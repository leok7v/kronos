/***********************************************
*                                              *
*      C language run-time subsystem for       *
*         SOVIET microcomputer KRONOS          *
*                                              *
*          Version 2.0  (c) KDG                *
*                                              *
***********************************************/
/*
* Revision log:
* SS  04.sep.87 modified code in abs() to avoid Kronos interrupt 41h
* SS  07.sep.87 new functions: lsearch(),di(),ei(),ioctl()
* SS  09.sep.87 disable/enable interrupts in number(), cvt() to avoid overflow
* SS  18.Oct.87 renamed functions: di() -> disable_intr(), ei() -> enable_intr()
* SS  19.Oct.87 added functions: getenv(),putenv(),getopt()
* SS  20.Oct.87 added functions: chdir(),mkdir(),rmdir()
* SS  10.Dec.87 qsort replaced from Unix Portable library
* SS  06.Jan.88 added test condition to strcat() and strncat()
* SS  09.Jan.88 introduced srand(),rand()
* MK  05.Feb.88 fixed char * bug in error() (getargs -> )
* SS  14.Jan.88 removed disable_intr/enable_intr calls from cvt()
* SS  14.Apr.88 strcmp() replaced with function using inline "asm" 
* SS  14.Apr.88 new versions of setjmp() & longjmp() planned to add
* SS  15.Apr.88 removed disable_intr/enable_intr calls from number()
* SS  25.Apr.88 removed all the HIBITL related stuff from number()
* LEO 01.Mar.89 unsigned arithmetic emulation in number()
* SS  02.Mar.89 MAXARGS set to 40
*/

#include "stdio.h"
#include "stdiom.h"
#include "ctype.h"
#include "varargs.h"
#include "values.h"
#include "sys_errno.h"
#include "fcntl.h"
#include "nan.h"
#include "print.h"
#include "string.h"
#include "memory.h"
#include "assert.h"
#undef clearerr
#undef getchar
#undef putchar

/* SS. 09.Jun.87 NB! some functions added to end of file */


/* malloci"-s op. systeemilt korraga kysitava maluloigu yhiku pikkus */
#define  BLOCK 512 /* SS 14.Apr.88 was (85*sizeof(struct store))  */

/* "_getarg"-i valjade suurused */
#define  MAXARGS 40  /* max number of arguments */
#define  MAXLEN  50  /* max length of an argument */

/****************************
*    globaalsed muutujad     *
*****************************/

int _argc_ = 0;

/****************************
*        funktsioonid       *
*****************************/

#ifdef kronos

char *   sbrk(incr)
int incr;
{ 
 modula char *_sbrk();
 return(_sbrk(incr));
}
 
int open (name,oflag,mode)
char *name;
int oflag, mode;
{
 modula  int  _open();
 return(_open(name,oflag));
}

int close(fd)
int fd;
{
 modula  int  _close();
 return (_close(fd));
}
int creat (name,mode)
char *name;
int   mode;
{
 modula int _open();
 return (_open(name, O_TRUNC | O_CREAT | O_WRONLY));
}
int read(fd,buf,cnt)
int fd;
char *buf;
unsigned cnt;
{
 modula  int  _read();
 return (_read(fd,buf,cnt));
}
int write(fd,buf,cnt)
int fd;
char *buf;
unsigned cnt;
{
 modula  int _write();
 return (_write(fd,buf,cnt));
}
long lseek(fd,offs,wh)
int fd,wh;
long offs;
{
 modula  long _lseek();
 return(_lseek(fd,offs,wh));
}
int isatty(fildes)
int fildes;
{
 modula  int _isatty();
 return(_isatty(fildes));
}
void _exit(status)
int status;
{
 modula  void __exit();
 __exit(status);
}

int access(path,amode)
char *path;
int amode;
{
 modula  int _access();
 return (_access(path,amode));
}

int unlink(path)
char *path;
{
 modula  int _unlink();
 return (_unlink(path));
}

int rename(old,new)
char *old, *new;
{
 modula  int _rename();
 return (_rename(old,new));
}

int getpid()
{
 modula int _getpid();
 return (_getpid());
}

char * ctime(clock)
long *clock;
{
 modula char * _ctime();
 return (_ctime(clock));
}

long  time(tloc)
long *tloc;
{
 modula long _time();
 return (_time(tloc));
}

int system(string)
char *string;
{
 modula int _system();
 return  (_system(string));
}

#endif

/*                  exit.c       */

void _cleanup();

void exit(v)
{
 _cleanup();
 _exit(v);
}


/*                  calloc.c   */
/*
 * calloc - alloc space for n items of size s, and clear it to nulls
*/


extern char * malloc();
extern char *memcpy();

char *
calloc(n, s)
unsigned n, s; 
{
/* MK */int  i;
 modula void _zero();
 register char *cp;
/* MK
 cp = malloc((unsigned int)(n *= s));
*/
 n *= s;
 if (i = n % 4) n += (4 - i);
 cp = malloc((unsigned int)n);

 if(cp == (char *)0)
  return((char *)0);
/* MK
 *cp = 0;
 (void) memcpy(cp, &cp[1], n-1);
*/
 _zero ((int *) cp, n/4);
 return(cp);
}

/*                  malloc.c   */

/* C storage allocator for Z80 and other 8 bit machines
 * circular first-fit strategy
 * works with noncontiguous, but monotonically linked, arena
 * each block is preceded by a ptr to the (pointer of) 
 * the next following block and a busy flag
 * bit in flag is 1 for busy, 0 for idle
 * gaps in arena are merely noted as busy blocks
 * last block of arena (pointed to by alloct) is empty and
 * has a pointer to first
 * idle blocks are coalesced during space search
 *
*/
#define BUSY 1
#define  testbusy(p) ((p).flag & BUSY)
#define  sbusy(p) (p).flag |= BUSY
#define  cbusy(p) (p).flag &= ~BUSY

struct store
{
 struct store *  ptr;
 char  flag;
};

static struct store * allocs = 0; /*initial arena*/
static struct store * allocp;  /*search ptr*/
static struct store * alloct;  /*arena top*/
static struct store allocx;  /* for realloc */

char *
malloc(nw)
unsigned nw;
{
 register struct store *p, *q;
 static unsigned temp; /*coroutines assume no auto*/

 if(allocs==(struct store *)0) { /*first time*/
  allocs = (struct store *) sbrk (2 * sizeof (struct store));
  sbrk(1);
  if ((int)allocs == -1)
   return (NULL);
  alloct = allocs[0].ptr = &allocs[1];
  allocp = allocs[1].ptr = &allocs[0];
  sbusy(allocs[0]);
  sbusy(allocs[1]);
 }
 nw = ((nw - 1 + sizeof(struct store)*2)/sizeof(struct store)) * sizeof(struct store);
 assert(allocp>=allocs && allocp<=alloct);
/* assert(allock()); */
 for(p=allocp; ; ) {
  for(temp=0; ; ) {
   if(!testbusy(*p)) {
    while(!testbusy(*(q=p->ptr))) {
     assert(q>p&&q<alloct);
     p->ptr = q->ptr;
    }
    if(q>=(struct store *)((char *)p+nw) && (struct store *)((char *)p+nw)>=p)
     goto found;
   }
   q = p;
   p = p->ptr;
   if(p>q)
    assert(p<=alloct);
   else if(q!=alloct || p!=allocs) {
    assert(q==alloct&&p==allocs);

    return(NULL);

   } else if(++temp>1)
    break;
  }
  temp = ((nw+sizeof(struct store)-1+BLOCK)/BLOCK)*BLOCK;
  q = (struct store *)sbrk(0);
  if((struct store *)((char *)q+temp) < q) {

   return(NULL);
  }
  q = (struct store *)sbrk(temp);
  if((int)q == -1) {

   return(NULL);
  }
  assert(q>alloct);
  alloct->ptr = q;
  if(q!=alloct+1)
   sbusy(*alloct);
  else
   cbusy(*alloct);
  alloct = q->ptr = (struct store *)((char *)q+temp-sizeof(struct store));;
  alloct->ptr = allocs;
  sbusy(*alloct);
  cbusy(*q);
 }
found:
 allocp = (struct store *)((char *)p + nw);
 assert(allocp<=alloct);
 if(q>allocp) {
  allocx = *allocp;
  allocp->ptr = p->ptr;
  allocp->flag = 0;
 }
 p->ptr = allocp;
 sbusy(*p);

 return((char *)(p+1));
}

/* freeing strategy tuned for LIFO allocation
*/

void free(ap)
char *ap;
{
 register struct store *p;

 p = ((struct store *)ap)-1;

 assert(p>=allocs[1].ptr&&p<=alloct);
/* assert(allock()); */
 allocp = p;
 assert(testbusy(*p));
 cbusy(*p);
 assert(p->ptr > allocp && p->ptr <= alloct);
}

char *
realloc(p, nbytes)
char *   p;
unsigned  nbytes;
{
 register struct store * xp, * q;
 unsigned short   ons;
 unsigned short   ns;

 xp = (struct store *)p;
 ns = (nbytes + sizeof(struct store) - 1)/sizeof(struct store);
 ons = xp[-1].ptr - xp;
 if(testbusy(xp[-1]))
  free((char *)xp);
 if(!(q = (struct store *)malloc(nbytes)) || q == xp){

  return (char *)q;
 }
 ns = q[-1].ptr - q;
 if(ons > ns)
  ons = ns;
 (void) memcpy((char *)q, (char *)xp,(int) (ons * sizeof(struct store)));
 if(q < xp && q+ns > xp)
  q[q+ns-xp] = allocx;

 return (char *)q;
}


#ifdef DEBUG
showall()
{
 struct store *p, *q;
 int i, used = 0, ifree = 0; /* SS. 17.dec. free -> ifree */
 return;
/*
 for(p = &allocs[0] ; p && p!= alloct ; p = q) {
  q = p->ptr;
  printf("%4.4x %5d %s\n", p, i = (int)((char *)q - (char *)p),
   testbusy(*p) ? "BUSY" : "FREE");
  if(testbusy(*p))
   used += i;
  else
   ifree += i;
 }
 printf("%d used, %d free, %4.4x end\n", used, ifree, alloct);
*/
}
#endif

#define  isterminator(c) ((c) == 0)
#define  look()   (*str)



static char * name = 0, * str = 0, * bp = 0;


static int
_puts(s)
register char *  s;
{
 while(*s) {
  fputc(*s++,stderr);
 }
}

/*VARARGS*/
static int
error(va_alist)
va_dcl
{
 va_list ape;
/* MK 05.Feb.88    char * -> int *  (conversion to char * took place)
 register char * sp;
*/
 register int  * sp;

 va_start(ape);
/* MK
 char * for va_arg replaced with int * (no changes are necessary)
*/

 while((sp = va_arg(ape, int*)) != (int *)0)
  _puts(sp);
 _puts("\n");
 exit(-1);
}


static char *
alloc(n)
short n;
{
 char *  bpp;  /* SS. 17.dec. bp -> bpp, to avoid redefinition warning */

 if((bpp = sbrk(n)) == (char *)-1)
  error("no room for arguments",(char *) 0);
 return bpp;
}

static char
nxtch()
{
 if(*str)
  return *str++;
 return 0;
}

static int
redirect(str_name, file_name, mode, stream)
char * str_name, * file_name, * mode;
FILE * stream;
{
 if(freopen(file_name, mode, stream) != stream)
  error("Can't open ", file_name, " for ", str_name,(char *) 0);
}

static char
isspecial(c)
char c;
{
 return c == '<' || c == '>';
}

static char
isseparator(c)
char c;
{
 return c == ' ' || c == '\t' || c == '\n';
}

char ** _getargs(_str, _name)
char * _str, * _name;
{
 char **  argv;
 register char * ap;
 char *   cp;
 short   argc;
 char  c, quote;
 unsigned short  i, j;
 char *   argbuf[MAXARGS];
 char  buf[MAXLEN];

 bp = (char *)0;
 quote = 0;
 name = _name;
 str = _str;
 argbuf[0] = name;
 argc = 1;

 /* first step - process arguments */

 while(look()) {

  if(argc == MAXARGS)
   error("too many arguments",(char *) 0);
  while(isseparator(c = nxtch()))
   continue;
  if(isterminator(c))
   break;
  ap = buf;
  if(isspecial(c)) {
   *ap++ = c;
   if(c == '>' && look() == '>')
    *ap++ = nxtch();
  } else {
   while(!isterminator(c) && 
    (quote || !isspecial(c) && !isseparator(c))) {
    if(ap == &buf[MAXLEN])
     error("argument too long",(char *) 0);
    if(c == quote) /* end of quoted string */
     quote = 0;
    else if(!quote && (c == '\'' || c == '"'))
     quote = c; /* start of quoted string */
    else {
     /* if(quote); */
     *ap++ = c;
    }
    if(!quote && isspecial(look()))
     break;
    c = nxtch();
   }
  }
 *ap = 0;
 argbuf[argc++] = ap = alloc(ap-buf+1);
 cp = buf;
 do
  *ap++ = *cp;
 while(*cp++);
 }

 /* now do redirection */

 for(i = j = 0 ; j < argc ; j++)
  if(isspecial(c = argbuf[j][0])) {
   if(j == argc-1)
    error("no name after ", argbuf[j],(char *) 0);
   if(c == '<')
    redirect("input", argbuf[j+1], "r", stdin);
   else {
    ap = argbuf[j][1] == '>' ? "a" : "w";
    redirect("output", argbuf[j+1], ap, stdout);
   }
   j++;
  }
  else
   argbuf[i++] = argbuf[j];
 _argc_ = i;
 argbuf[i++] = (char *)0;
 argv = (char **)alloc((short )(i * sizeof *argv));
 (void) memcpy((char *)argv, (char *)argbuf, (int) (i * sizeof(* argv)));
 return argv;

}
/* data.c */


/* some slop is allowed at the end of the buffers in case an upset in
 * the synchronization of _cnt and _ptr (caused by an interrupt or other
 * signal) is not immediately detected.
 */
unsigned char _sibuf[BUFSIZ+8], _sobuf[BUFSIZ+8];
/*
 * Ptrs to start of preallocated buffers for stdin, stdout.
 */
unsigned char *_stdbuf[] = { _sibuf, _sobuf };

unsigned char _smbuf[_NFILE+1][_SBFSIZ];

FILE _iob[_NFILE] = {
 { 0, NULL, NULL, _IOREAD, 0},
 { 0, NULL, NULL, _IOWRT, 1},
 { 0, _smbuf[2], _smbuf[2], _IOWRT+_IONBF, 2}
};
/*
 * Ptr to end of io control blocks
 */
FILE *_lastbuf = { &_iob[_NFILE] };

/*
 * Ptrs to end of read/write buffers for each device
 * There is an extra bufend pointer which corresponds to the dummy
 * file number _NFILE, which is used by sscanf and sprintf.
 */
unsigned char *_bufendtab[_NFILE+1] = { NULL, NULL, _smbuf[2]+_SBFSIZ, };

/*  clearerr.c */
void
clearerr(iop)
register FILE *iop;
{
 iop->_flag &= ~(_IOERR | _IOEOF);
}
/* doscan.c 2.6  */

#define NCHARS ((unsigned)1 << BITSPERBYTE)

extern double atof();
extern char *memset();
extern int ungetc();


int
_doscan(iop, fmt, args)
register FILE *iop;
register unsigned char *fmt;
va_list args;
{
 extern unsigned char *setup();
 char tab[NCHARS];
 register int ch;
 int nmatch = 0, len, inchar, stow, size;

 /*******************************************************
  * Main loop: reads format to determine a pattern,
  *  and then goes to read input stream
  *  in attempt to match the pattern.
  *******************************************************/
 for( ; ; ) {
  if((ch = *fmt++) == '\0')
   return(nmatch); /* end of format */
  if(isspace(ch)) {
   while(isspace(inchar = getc(iop)))
    ;
   if(ungetc(inchar, iop) != EOF)
    continue;
   break;
  }
  if(ch != '%' || (ch = *fmt++) == '%') {
   if((inchar = getc(iop)) == ch)
    continue;
   if(ungetc(inchar, iop) != EOF)
    return(nmatch); /* failed to match input */
   break;
  }
  if(ch == '*') {
   stow = 0;
   ch = *fmt++;
  } else
   stow = 1;

  for(len = 0; isdigit(ch); ch = *fmt++)
   len = len * 10 + ch - '0';
  if(len == 0)
   len = MAXINT;

  if((size = ch) == 'l' || size == 'h')
   ch = *fmt++;
  if(ch == '\0' ||
      ch == '[' && (fmt = setup(fmt, tab)) == NULL)
   return(EOF); /* unexpected end of format */
  if(isupper(ch)) { /* no longer documented */
   size = 'l';
   ch = _tolower(ch);
  }
  if(ch != 'c' && ch != '[') {
   while(isspace(inchar = getc(iop)))
    ;
   if(ungetc(inchar, iop) == EOF)
    break;
  }
  if((size = (ch == 'c' || ch == 's' || ch == '[') ?
 /* NS */    strin(stow, ch, len, tab, iop, &args) :
      number(stow, ch, len, size, iop, &args)) != 0)
   nmatch += stow;
  if(args == NULL) /* end of input */
   break;
  if(size == 0)
   return(nmatch); /* failed to match input */
 }
 return(nmatch != 0 ? nmatch : EOF); /* end of input */
}

/***************************************************************
 * Functions to read the input stream in an attempt to match incoming
 * data to the current pattern from the main loop of _doscan().
 ***************************************************************/


static int
number(stow, type, len, size, iop, listp)
int stow, type, len, size;
register FILE *iop;
va_list *listp;
{
 char numbuf[64];
 register char *np = numbuf;
 register int c, base;
 int digitseen = 0, dotseen = 0, expseen = 0, floater = 0, negflg = 0;

 long lcval = 0;
 long old_lcval, mult;

 switch(type) {
 case 'e':
 case 'f':
 case 'g':
  floater++;
 case 'd':
 case 'u':
  base = 10;
  break;
 case 'o':
  base = 8;
  break;
 case 'x':
  base = 16;
  break;
 default:
  return(0); /* unrecognized conversion character */
 }
 switch(c = getc(iop)) {
 case '-':
  negflg++;
 case '+': /* fall-through */
  len--;
  c = getc(iop);
 }
/* SS. 09.sep.87 NB! introduced call to disable_intr(), to avoid int overflow */


/* SSS  19.Apr.88  fprintf(stderr,"len: %d %x\n", len,len);   */

 disable_intr(0x41); /* disable integer overflow */

 for( ; --len >= 0; *np++ = c, c = getc(iop)) {
  if(isdigit(c) || base == 16 && isxdigit(c)) {
   int digit = c - (isdigit(c) ? '0' :
       isupper(c) ? 'A' - 10 : 'a' - 10);
   if(digit >= base)
    break;
   if(stow && !floater){
/* LEO 01-Mar-89 unsigned arithmetic emulation */

     if ( lcval <= (0x7FFFFFFF-digit)/base )
       lcval = base * lcval + digit;
     else
     { mult=base; old_lcval=lcval;
       do { lcval=lcval+old_lcval; mult--; } while (mult>1);
       lcval = lcval + digit;
     }

   }
   digitseen++;
/* SSS  19.Apr.88  fprintf(stderr,"lcval: %d %x  digitseen: %d\n",lcval, lcval,digitseen);  */
   continue;
  }
  if(!floater)
   break;
  if(c == '.' && !dotseen++)
   continue;
  if((c == 'e' || c == 'E') && digitseen && !expseen++) {
   *np++ = c;
   c = getc(iop);
   if(isdigit(c) || c == '+' || c == '-')
    continue;
  }
  break;
 }  /* ..for */

 enable_intr(0x41); /* enable integer overflow */

 if(stow && digitseen)
  if(floater) {
   register double dval;
 
   *np = '\0';
   dval = atof(numbuf);
   if(negflg)
    dval = -dval;
   if(size == 'l')
    *va_arg(*listp, double *) = dval;
   else
    *va_arg(*listp, float *) = (float)dval;
  } else {
   /* suppress possible overflow on 2's-comp negation */
   if(negflg && lcval != HIBITL)
    lcval = -lcval;
   if(size == 'l')
    *va_arg(*listp, long *) = lcval;
   else if(size == 'h')
    *va_arg(*listp, short *) = (short)lcval;
   else
    *va_arg(*listp, int *) = (int)lcval;
  }
 if(ungetc(c, iop) == EOF)
  *listp = NULL; /* end of input */
 return(digitseen); /* successful match if non-zero */
}

static int
strin(stow, type, len, tab, iop, listp) /* NS */
register int stow, type, len;
register char *tab;
register FILE *iop;
va_list *listp;
{
 register int ch;
 register char *ptr;
 char *start;

 start = ptr = stow ? va_arg(*listp, int *) : NULL;
 if(type == 'c' && len == MAXINT)
  len = 1;
 while((ch = getc(iop)) != EOF &&
     !(type == 's' && isspace(ch) || type == '[' && tab[ch])) {
  if(stow)
   *ptr = ch;
  ptr++;
  if(--len <= 0)
   break;
 }
 if(ch == EOF || len > 0 && ungetc(ch, iop) == EOF)
  *listp = NULL; /* end of input */
 if(ptr == start)
  return(0); /* no match */
 if(stow && type != 'c')
  *ptr = '\0';
 return(1); /* successful match */
}

static unsigned char *
setup(fmt, tab)
register unsigned char *fmt;
register char *tab;
{
 register int b, c, d, t = 0;

 if(*fmt == '^') {
  t++;
  fmt++;
 }
 (void)memset(tab, !t, NCHARS);
 if((c = *fmt) == ']' || c == '-') { /* first char is special */
  tab[c] = t;
  fmt++;
 }
 while((c = *fmt++) != ']') {
  if(c == '\0')
   return(NULL); /* unexpected end of format */
  if(c == '-' && (d = *fmt) != ']' && (b = fmt[-2]) < d) {
   (void)memset(&tab[b], t, d - b + 1);
   fmt++;
  } else
   tab[c] = t;
 }
 return(fmt);
}

/* ctermid.c  */

extern char *strcpy();
static char res[L_ctermid];

char *
ctermid(s)
register char *s;
{
 return (strcpy(s != NULL ? s : res, "/dev/tty"));
}

/* fdopen.c  */

/*
 * Unix routine to do an "fopen" on file descriptor
 * The mode has to be repeated because you can't query its
 * status
 */


extern long lseek();
extern FILE *_findiop();

FILE *
fdopen(fd, mode)
int fd;
register char *mode;
{
 register FILE *iop;

 if((iop = _findiop()) == NULL)
  return(NULL);

 iop->_cnt = 0;
 iop->_flag = 0;
 iop->_file = fd;
 _bufend(iop) = iop->_base = NULL;
 iop->_ptr = NULL;
 switch(*mode) {

  case 'r':
   iop->_flag |= _IOREAD;
   break;
  case 'a':
   (void) lseek(fd, 0L, 2);
   /* No break */
  case 'w':
   iop->_flag |= _IOWRT;
   break;
  default:
   return(NULL);
 }

 if(mode[1] == '+') {
  iop->_flag &= ~(_IOREAD | _IOWRT);
  iop->_flag |= _IORW;
 }

 return(iop);
}
/* fgetc.c  */

int
fgetc(fp)
register FILE *fp;
{
 return(getc(fp));
}
/* fgets.c  */
/*
 * This version reads directly from the buffer rather than looping on getc.
 * Ptr args aren't checked for NULL because the program would be a
 * catastrophic mess anyway.  Better to abort than just to return NULL.
 */

#define MIN(x, y) (x < y ? x : y)

extern int _filbuf();
extern char *memccpy();

char *
fgets(ptr, size, iop)
char *ptr;
register int size;
register FILE *iop;
{
 char *p, *ptr0 = ptr;
 register int n;

 for (size--; size > 0; size -= n) {
  if (iop->_cnt <= 0) { /* empty buffer */
   if (_filbuf(iop) == EOF) {
    if (ptr0 == ptr)
     return (NULL);
    break; /* no more data */
   }
   iop->_ptr--;
   iop->_cnt++;
  }
  n = MIN(size, iop->_cnt);
  if ((p = memccpy(ptr, (char *) iop->_ptr, '\n', n)) != NULL)
   n = p - ptr;
  ptr += n;
  iop->_cnt -= n;
  iop->_ptr += n;
  _BUFSYNC(iop);
  if (p != NULL)
   break; /* found '\n' in buffer */
 }
 *ptr = '\0';
 return (ptr0);
}
/* filbuf.c  */

extern _findbuf();
extern int read();
extern int fflush();
extern FILE *_lastbuf;

int
_filbuf(iop)
register FILE *iop;
{
 register FILE *diop;

 if (iop->_base == NULL)  /* get buffer if we don't have one */
  _findbuf(iop);

 if ( !(iop->_flag & _IOREAD) )
  if (iop->_flag & _IORW)
   iop->_flag |= _IOREAD;
  else
   return(EOF);

 /* if this device is a terminal (line-buffered) or unbuffered, then */
 /* flush buffers of all line-buffered devices currently writing */

 if (iop->_flag & (_IOLBF | _IONBF))
  for (diop = _iob; diop < _lastbuf; diop++ )
   if (diop->_flag & _IOLBF)
    (void) fflush(diop);

 iop->_ptr = iop->_base;
 iop->_cnt = read(fileno(iop), (char *)iop->_base,
     (unsigned)((iop->_flag & _IONBF) ? 1 : _bufsiz(iop) ));
 if (--iop->_cnt >= 0)   /* success */
  return (*iop->_ptr++);
 if (iop->_cnt != -1)  /* error */
  iop->_flag |= _IOERR;
 else {     /* end-of-file */
  iop->_flag |= _IOEOF;
  if (iop->_flag & _IORW)
   iop->_flag &= ~_IOREAD;
 }
 iop->_cnt = 0;
 return (EOF);
}
/* findiop.c  */

extern FILE *_lastbuf;

FILE *
_findiop()
{
 register FILE *iop;

 for(iop = _iob; iop->_flag & (_IOREAD | _IOWRT | _IORW); iop++)
  if(iop >= _lastbuf)
   return(NULL);
 return(iop);
}
/* flsbuf.c  */

extern void free();
extern int errno, write(), close(), isatty();
extern char *malloc();
extern FILE *_lastbuf;
#if !u370
extern unsigned char *_stdbuf[];
#endif
extern unsigned char _smbuf[][_SBFSIZ];

/*
 * Flush buffers on exit
 */

void
_cleanup()
{
 register FILE *iop;

 for(iop = _iob; iop < _lastbuf; iop++)
  (void) fclose(iop);
}
/*
 fclose() will flush (output) buffers for a buffered open
 FILE and then issue a system close on the _fileno.  The
 _base field will be reset to NULL for any but stdin and
 stdout, the _ptr field will be set the same as the _base
 field. The _flags and the _cnt field will be zeroed.
 If buffers had been obtained via malloc(), the space will
 be free()'d.  In case the FILE was not open, or fflush()
 or close() failed, an EOF will be returned, otherwise the
 return value is 0.
 */

int
fclose(iop)
register FILE *iop;
{
 register int rtn=EOF;

 if(iop == NULL)
  return(rtn);
 if(iop->_flag & (_IOREAD | _IOWRT | _IORW)) {
  rtn = (iop->_flag & _IONBF)? 0: fflush(iop);
  if(close(fileno(iop)) < 0) {
   rtn = EOF;
   errno = ENOENT;
  }
 }
 if(iop->_flag & _IOMYBUF) {
  free((char*)iop->_base);
  iop->_base = NULL;
 }
 iop->_flag = 0;
 iop->_cnt = 0;
 iop->_ptr = iop->_base;
 return(rtn);
}

/*
 The fflush() routine must take care because of the
 possibility for recursion. The calling program might
 do IO in an interupt catching routine that is likely
 to interupt the write() call within fflush()
 */

int
fflush(iop)
register FILE *iop;
{
 if (!(iop->_flag & _IOWRT)) {
  iop->_cnt = 0;
  return(0);
 }
 while(!(iop->_flag & _IONBF) && (iop->_flag & _IOWRT) &&
   (iop->_base != NULL) && (iop->_ptr > iop->_base) )
  (void) _xflsbuf(iop);
 return(ferror(iop) ? EOF : 0);
}

/* The routine _flsbuf may or may not actually flush the output buffer.  If
 * the file is line-buffered, the fact that iop->_cnt has run below zero
 * is meaningless: it is always kept below zero so that invocations of putc
 * will consistently give control to _flsbuf, even if the buffer is far from
 * full.  _flsbuf, on seeing the "line-buffered" flag, determines whether the
 * buffer is actually full by comparing iop->_ptr to the end-of-buffer pointer
 * _bufend(iop).  If it is full, or if an output line is completed (with a
 * newline), the buffer is flushed.  (Note: the character argument to _flsbuf
 * is not flushed with the current buffer if the buffer is actually full--
 * it goes into the buffer after flushing.)
 */

int
_flsbuf(c, iop)
unsigned char c;
register FILE *iop;
{
    unsigned char c1;

    do {
 /* check for linebuffered with write perm, but no EOF */
 if ( (iop->_flag & (_IOLBF | _IOWRT | _IOEOF)) == (_IOLBF | _IOWRT) ) {
  if ( iop->_ptr >= _bufend(iop) )  /* if buffer full, */
   break;      /* exit do-while, and flush buf. */
  if ( (*iop->_ptr++ = c) != '\n' )
   return(c);
  return(_xflsbuf(iop) == EOF ? EOF : c);
 }
 /* write out an unbuffered file, if have write perm, but no EOF */
 if ( (iop->_flag & (_IONBF | _IOWRT | _IOEOF)) == (_IONBF | _IOWRT) ) {
  c1 = c;
  iop->_cnt = 0;
  if (write(fileno(iop), (char *) &c1, 1) == 1)
   return(c);
  iop->_flag |= _IOERR;
  return(EOF);
 }
 /* The _wrtchk call is here rather than at the top of _flsbuf to re- */
 /* duce overhead for line-buffered I/O under normal circumstances.  */

 if (_WRTCHK(iop))   /* is writing legitimate? */
  return(EOF);
    } while ( (iop->_flag & (_IONBF | _IOLBF)) );


    (void) _xflsbuf(iop);   /* full buffer:  flush buffer */
    (void) (putc((char) c, iop));  /* then put "c" in newly emptied buf */
   /* (which, because of signals, may NOT be empty) */
    return( ferror(iop) ? EOF : c);

}

/* The function _xflsbuf writes out the current contents of the output
 * buffer delimited by iop->_base and iop->_ptr.
 * iop->_cnt is reset appropriately, but its value on entry to _xflsbuf
 * is ignored.
 *
 * The following code is not strictly correct.  If a signal is raised,
 * invoking a signal-handler which generates output into the same buffer
 * being flushed, a peculiar output sequence may result (for example,
 * the output generated by the signal-handler may appear twice).  At
 * present no means has been found to guarantee correct behavior without
 * resorting to the disabling of signals, a means considered too expensive.
 * For now the code has been written with the intent of reducing the
 * probability of strange effects and, when they do occur, of confining
 * the damage.  Except under extremely pathological circumstances, this
 * code should be expected to respect buffer boundaries even in the face
 * of interrupts and other signals.
 */

int
_xflsbuf(iop)
register FILE *iop;
{
 register unsigned char *base;
 register int n;

 n = iop->_ptr - (base = iop->_base);
 iop->_ptr = base;
 iop->_cnt = (iop->_flag &(_IONBF | _IOLBF)) ? 0 : _bufsiz(iop);
 _BUFSYNC(iop);
 if (n > 0 && n != write(fileno(iop),(char*)base,(unsigned)n) )  {
  iop->_flag |= _IOERR;
  return(EOF);
 }
 return(0);
}

/* The function _wrtchk checks to see whether it is legitimate to write
 * to the specified device.  If it is, _wrtchk sets flags in iop->_flag for
 * writing, assures presence of a buffer, and returns 0.  If writing is not
 * legitimate, EOF is returned.
 */

int
_wrtchk(iop)
register FILE *iop;
{
 if ( (iop->_flag & (_IOWRT | _IOEOF)) != _IOWRT ) {
  if (!(iop->_flag & (_IOWRT | _IORW)))
   return(EOF);  /* bogus call--read-only file */
  iop->_flag = iop->_flag & ~_IOEOF | _IOWRT; /* fix flags */
 }
 if (iop->_base == NULL)    /* this is first I/O to file--get buffer */
  _findbuf(iop);
 if (iop->_ptr == iop->_base && !(iop->_flag & (_IONBF | _IOLBF)) )  {
  iop->_cnt = _bufsiz(iop); /* first write since seek--set cnt */
  _BUFSYNC(iop);
 }
 return(0);
}

/*
 * _findbuf, called only when iop->_base == NULL, locates a predefined buffer
 * or allocates a buffer using malloc.  If a buffer is obtained from malloc,
 * the _IOMYBUF flag is set in iop->_flag.
 */

_findbuf(iop)
register FILE *iop;
{
 register int fno = fileno(iop); /* file number */

 /* allocate a small block for unbuffered, large for buffered */
 if (iop->_flag & _IONBF)  {
  _bufend(iop) = (iop->_base = _smbuf[fno]) + _SBFSIZ;
 }  else  {
#if !u370
  if (fno < 2)  /* use existing bufs for stdin, stdout */
   _bufend(iop) = (iop->_base = _stdbuf[fno]) + BUFSIZ;
  else 
#endif
  if ((iop->_base = (unsigned char *) malloc(BUFSIZ+8)) != NULL) {
   /* if  we got a buffer */
   iop->_flag |= _IOMYBUF;
   _bufend(iop) = iop->_base + BUFSIZ;
  } else
   /* if no room for buffer, use small buffer */
   _bufend(iop) = (iop->_base = _smbuf[fno]) + _SBFSIZ;
 }
 iop->_ptr = iop->_base;
 if ( isatty(fno) && !(iop->_flag & _IONBF) )
  iop->_flag |= _IOLBF;
}

/* The function _bufsync is called because interrupts and other signals
 * which occur in between the decrementing of iop->_cnt and the incrementing
 * of iop->_ptr, or in other contexts as well, may upset the synchronization
 * of iop->_cnt and iop->ptr.  If this happens, calling _bufsync should
 * resynchronize the two quantities (this is not always possible).  Resyn-
 * chronization guarantees that putc invocations will not write beyond
 * the end of the buffer.  Note that signals during _bufsync can cause
 * _bufsync to do the wrong thing, but usually with benign effects.
 */

_bufsync(iop)
register FILE *iop;
{
 register int spaceleft;

 if ((spaceleft = _bufend(iop) - iop->_ptr) < 0)
  iop->_ptr = _bufend(iop);
 else if (spaceleft < iop->_cnt)
  iop->_cnt = spaceleft;
}
/* fopen.c  */

extern int open(), fclose();
extern FILE *_findiop(), *_endopen();

FILE *
fopen(file, mode)
char *file, *mode;
{
 return (_endopen(file, mode, _findiop()));
}

FILE *
freopen(file, mode, iop)
char *file, *mode;
register FILE *iop;
{
 (void) fclose(iop); /* doesn't matter if this fails */
 return (_endopen(file, mode, iop));
}

static FILE *
_endopen(file, mode, iop)
char *file, *mode;
register FILE *iop;
{
 register int plus, oflag, fd;

 if (iop == NULL || file == NULL || file[0] == '\0')
  return (NULL);
 plus = (mode[1] == '+');
 switch (mode[0]) {
 case 'w':
  oflag = (plus ? O_RDWR : O_WRONLY) | O_TRUNC | O_CREAT;
  break;
 case 'a':
  oflag = (plus ? O_RDWR : O_WRONLY) | O_APPEND | O_CREAT;
  break;
 case 'r':
  oflag = plus ? O_RDWR : O_RDONLY;
  break;
 default:
  return (NULL);
 }
 if ((fd = open(file, oflag, 0666)) < 0)
  return (NULL);
 iop->_cnt = 0;
 iop->_file = fd;
 iop->_flag = plus ? _IORW : (mode[0] == 'r') ? _IOREAD : _IOWRT;
 if (mode[0] == 'a')   {
  if (!plus)  {
   /* if update only mode, move file pointer to the end
      of the file */
   if ((lseek(fd,0L,2)) < 0)  {
    return NULL;
   }
  }
 }
 _bufend(iop) = iop->_base = NULL;
 iop->_ptr = NULL;
 return (iop);
}
/* fputc.c  */

int
fputc(c, fp)
int c;
register FILE *fp;
{
 return(putc(c, fp));
}

/*      fputs.c   */

/*
 * This version writes directly to the buffer rather than looping on putc.
 * Ptr args aren't checked for NULL because the program would be a
 * catastrophic mess anyway.  Better to abort than just to return NULL.
 */

extern char *memccpy();

int
fputs(ptr, iop)
char *ptr;
register FILE *iop;
{
 register int ndone = 0, n;
 register unsigned char *cptr, *bufend;
 char *p;

 if (_WRTCHK(iop))
  return (0);
 bufend = _bufend(iop);

 if ((iop->_flag & _IONBF) == 0)  {
  for ( ; ; ptr += n) {
   while ((n = bufend - (cptr = iop->_ptr)) <= 0)  
    /* full buf */
    if (_xflsbuf(iop) == EOF)
     return(EOF);
   if ((p = memccpy((char *) cptr, ptr, '\0', n)) != NULL)
    n = (p - (char *) cptr) - 1;
   iop->_cnt -= n;
   iop->_ptr += n;
   _BUFSYNC(iop);
   ndone += n;
   if (p != NULL)  { 
    /* done; flush buffer if line-buffered */
           if (iop->_flag & _IOLBF)
            if (_xflsbuf(iop) == EOF)
             return(EOF);
           return(ndone);
          }
  }
 }  else  {
  /* write out to an unbuffered file */
  register unsigned int cnt = strlen(ptr);

  (void)write(iop->_file, ptr, cnt);
  return cnt;
 }
}
/* fread.c  */

/*
 * This version reads directly from the buffer rather than looping on getc.
 * Ptr args aren't checked for NULL because the program would be a
 * catastrophic mess anyway.  Better to abort than just to return NULL.
 */

#define MIN(x, y) (x < y ? x : y)

extern int _filbuf();
extern _bufsync();
extern char *memcpy();

int
fread(ptr, size, count, iop)
char *ptr;
int size, count;
register FILE *iop;
{
 register int nleft;  /* NS */
 register int n;

 if (size <= 0 || count <= 0) return 0;
 nleft = count * size;

 /* Put characters in the buffer */
 /* note that the meaning of n when just starting this loop is
    irrelevant.  It is defined in the loop */
 for ( ; ; ) {
  if (iop->_cnt <= 0) { /* empty buffer */
   if (_filbuf(iop) == EOF)
    return (count - (nleft + size - 1)/size);
   iop->_ptr--;
   iop->_cnt++;
  }
  n = MIN(nleft, iop->_cnt);
  ptr = memcpy(ptr, (char *) iop->_ptr, n) + n;
  iop->_cnt -= n;
  iop->_ptr += n;
  _BUFSYNC(iop);
  if ((nleft -= n) <= 0)
   return (count);
 }
}
/* fseek.c  */

/*
 * Seek for standard library.  Coordinates with buffering.
 */

extern long lseek();
extern int fflush();

int
fseek(iop, offset, ptrname)
register FILE *iop;
long offset;
int ptrname;
{
 register int c;
 long p;

 iop->_flag &= ~_IOEOF;
 if(iop->_flag & _IOREAD) {
  if(ptrname < 2 && iop->_base && !(iop->_flag&_IONBF)) {
   c = iop->_cnt;
   p = offset;
   if(ptrname == 0)
    p += (long)c-lseek(fileno(iop), 0L, 1);
   else
    offset -= (long)c;
   if(!(iop->_flag&_IORW) && c > 0 && p <= c &&
     p >= iop->_base - iop->_ptr) {
    iop->_ptr += (int)p;
    iop->_cnt -= (int)p;
    return(0);
   }
  }
  if(iop->_flag & _IORW) {
   iop->_ptr = iop->_base;
   iop->_flag &= ~_IOREAD;
  }
  p = lseek(fileno(iop), offset, ptrname);
  iop->_cnt = 0;
 } else if(iop->_flag & (_IOWRT | _IORW)) {
  (void) fflush(iop);
  iop->_cnt = 0;
  if(iop->_flag & _IORW) {
   iop->_flag &= ~_IOWRT;
   iop->_ptr = iop->_base;
  }
  p = lseek(fileno(iop), offset, ptrname);
 }
 return((p == -1)? -1: 0);
}
/* ftell.c  */

/*
 * Return file offset.
 * Coordinates with buffering.
 */

extern long lseek();

long
ftell(iop)
FILE *iop;
{
 long tres;
 register int adjust;

 if(iop->_cnt < 0)
  iop->_cnt = 0;
 if(iop->_flag & _IOREAD)
  adjust = - iop->_cnt;
 else if(iop->_flag & (_IOWRT | _IORW)) {
  adjust = 0;
  if(iop->_flag & _IOWRT && iop->_base &&
     (iop->_flag & _IONBF) == 0)
   adjust = iop->_ptr - iop->_base;
 } else
  return(-1);
 tres = lseek(fileno(iop), 0L, 1);
 if(tres >= 0)
  tres += (long)adjust;
 return(tres);
}
/* fwrite.c  */

/*
 * This version writes directly to the buffer rather than looping on putc.
 * Ptr args aren't checked for NULL because the program would be a
 * catastrophic mess anyway.  Better to abort than just to return NULL.
 *
 * This version does buffered writes larger than BUFSIZ directly, when
 * the buffer is empty.
 */

#define MIN(x, y)       (x < y ? x : y)

extern char *memcpy();
extern char *memchr();

int
fwrite(ptr, size, count, iop)
char *ptr;
int size, count;
register FILE *iop;
{
 register unsigned nleft;
 register int n;
 register unsigned char *cptr, *bufend;

 if (size <= 0 || count <= 0 || _WRTCHK(iop))
         return (0);

 bufend = _bufend(iop);
 nleft = count*size;

 /* if the file is unbuffered, or if the iop->ptr = iop->base, and there
    is > BUFSZ chars to write, we can do a direct write */
 if (iop->_base >= iop->_ptr)  { /*this covers the unbuffered case, too*/
  if (((iop->_flag & _IONBF) != 0) || (nleft >= BUFSIZ))  {
   if ((n=write(fileno(iop),ptr,nleft)) != nleft)
       {
    iop->_flag |= _IOERR;
    n = (n >= 0) ? n : 0;
   }
   return n/size;
  }
 }
 /* Put characters in the buffer */
 /* note that the meaning of n when just starting this loop is
    irrelevant.  It is defined in the loop */
 for (; ; ptr += n) {
         while ((n = bufend - (cptr = iop->_ptr)) <= 0)  /* full buf */
                 if (_xflsbuf(iop) == EOF)
                         return (count - (nleft + size - 1)/size);
         n = MIN(nleft, n);
         (void) memcpy((char *) cptr, ptr, n);
         iop->_cnt -= n;
         iop->_ptr += n;
         _BUFSYNC(iop);
  /* done; flush if linebuffered with a newline */
         if ((nleft -= n) == 0)  { 
   if (iop->_flag & (_IOLBF | _IONBF)) {
                  if ((iop->_flag & _IONBF) || 
     (memchr((char *)iop->_base,
     '\n',count * size) != NULL))  {
          (void) _xflsbuf(iop);
    }
   }
                 return (count);
         }
 }
}
/* getchar.c  */

/*
 * A subroutine version of the macro getchar.
 */

int
getchar()
{
 return(getc(stdin));
}
/* gets.c  */

/*
 * This version reads directly from the buffer rather than looping on getc.
 * Ptr args aren't checked for NULL because the program would be a
 * catastrophic mess anyway.  Better to abort than just to return NULL.
 */

extern int _filbuf();
extern _bufsync();
extern char *memccpy();

char *
gets(ptr)
char *ptr;
{
 char *p, *ptr0 = ptr;
 register int n;

 for ( ; ; ) {
  if (stdin->_cnt <= 0) { /* empty buffer */
   if (_filbuf(stdin) == EOF) {
    if (ptr0 == ptr)
     return (NULL);
    break; /* no more data */
   }
   stdin->_ptr--;
   stdin->_cnt++;
  }
  n = stdin->_cnt;
  if ((p = memccpy(ptr, (char *) stdin->_ptr, '\n', n)) != NULL)
   n = p - ptr;
  ptr += n;
  stdin->_cnt -= n;
  stdin->_ptr += n;
  _BUFSYNC(stdin);
  if (p != NULL) { /* found '\n' in buffer */
   ptr--; /* step back over '\n' */
   break;
  }
 }
 *ptr = '\0';
 return (ptr0);
}
/* getw.c  */

/*
 * The intent here is to provide a means to make the order of
 * bytes in an io-stream correspond to the order of the bytes
 * in the memory while doing the io a `word' at a time.
 */

int
getw(stream)
register FILE *stream;
{
 int w;
 register char *s = (char *)&w;
 register int i = sizeof(int);

 while (--i >= 0)
  *s++ = getc(stream);
 return (feof(stream) || ferror(stream) ? EOF : w);
}
/* putchar.c  */

/*
 * A subroutine version of the macro putchar
 */

int
putchar(c)
register char c;
{
 return(putc(c, stdout));
}
/* puts.c  */

/*
 * This version writes directly to the buffer rather than looping on putc.
 * Ptr args aren't checked for NULL because the program would be a
 * catastrophic mess anyway.  Better to abort than just to return NULL.
 */

extern char *memccpy();

int
puts(ptr)
char *ptr;
{
 char *p;
 register int ndone = 0, n;
 register unsigned char *cptr, *bufend;

 if (_WRTCHK(stdout))
  return (0);

 bufend = _bufend(stdout);

 for ( ; ; ptr += n) {
  while ((n = bufend - (cptr = stdout->_ptr)) <= 0) /* full buf */
   if (_xflsbuf(stdout) == EOF)
    return(EOF);
  if ((p = memccpy((char *) cptr, ptr, '\0', n)) != NULL)
   n = p - (char *) cptr;
  stdout->_cnt -= n;
  stdout->_ptr += n;
  _BUFSYNC(stdout);
  ndone += n;
  if (p != NULL) {
   stdout->_ptr[-1] = '\n'; /* overwrite '\0' with '\n' */
   if (stdout->_flag & (_IONBF | _IOLBF)) /* flush line */
    if (_xflsbuf(stdout) == EOF)
     return(EOF);
   return(ndone);
  }
 }
}

/* tempnam.c  */

#define max1(A,B) (((A)<(B))?(B):(A))

extern char *malloc(), *mktemp(), *getenv();
extern int access();

static char *pcopy(), *seed1="AAA";

char *
tempnam(dir, pfx)
char *dir;  /* use this directory please (if non-NULL) */
char *pfx;  /* use this (if non-NULL) as filename prefix */
{
 register char *p, *q, *tdir;
 int x=0, y=0, z;

#if unix
/* SS. 19.Oct.87 in excII env. P_tmpdir is equal to call getenv("TMPDIR") */
 z=strlen(P_tmpdir);
#endif
 if((tdir = getenv("TMPDIR")) != NULL) {
  x = strlen(tdir);
 }
 if(dir != NULL) {
  y=strlen(dir);
 }
#if excII
 if((p=malloc((unsigned)(max1(x,y)+16))) == NULL)
#else
 if((p=malloc((unsigned)(max1(max1(x,y),z)+16))) == NULL)
#endif
  return(NULL);
 if(x > 0 && access(pcopy(p, tdir), 3) == 0)
  goto OK;
 if(y > 0 && access(pcopy(p, dir), 3) == 0)
  goto OK;
#if unix
 /* SS. 19.Oct.87 no need to twice call getenv() in excII environment */
 if(access(pcopy(p, P_tmpdir), 3) == 0)
  goto OK;
#endif
 if(access(pcopy(p, "/tmp"), 3) != 0)
  return(NULL);
OK:
 (void)strcat(p, "/");
 if(pfx) {
  *(p+strlen(p)+5) = '\0';
  (void)strncat(p, pfx, 5);
 }
 (void)strcat(p, seed1);
 (void)strcat(p, "XXXXXX");
 q = seed1;
 while(*q == 'Z')
  *q++ = 'A';
 ++*q;
 if(*mktemp(p) == '\0')
  return(NULL);
 return(p);
}

/* tmpfile.c  */

/*
 * tmpfile - return a pointer to an update file that can be
 *  used for scratch. The file will automatically
 *  go away if the program using it terminates.
 */

extern FILE *fopen();
extern int unlink();
extern char *tmpnam();
extern void perror();

FILE *
tmpfile()
{
 char tfname[L_tmpnam];
 register FILE *p;

 (void) tmpnam(tfname);
 if((p = fopen(tfname, "w+")) == NULL)
  return NULL;
 else
  (void) unlink(tfname);
 return(p);
}

/* tmpnam.c  */

extern char *mktemp(), *strcpy(), *strcat();
static char str1[L_tmpnam], seed2[] = { 'a', 'a', 'a', '\0' };

char *
tmpnam(s)
char *s;
{
 register char *p, *q;

 p = (s == NULL)? str1: s;
/* SS. 20.Nov.87 NB! getenv() returns NULL pointer if TMPDIR not found */
 (void) strcpy(p, P_tmpdir);
 (void) strcat(p, seed2);
 (void) strcat(p, "XXXXXX");

 q = seed2;
 while(*q == 'z')
  *q++ = 'a';
 ++*q;

 (void) mktemp(p);
 return(p);
}
static char*
pcopy(space, arg)
char *space, *arg;
{
 char *p;

 if(arg) {
  (void)strcpy(space, arg);
  p = space-1+strlen(space);
  if(*p == '/')
   *p = '\0';
 }
 return(space);
}
/* putw.c  */

/*
 * The intent here is to provide a means to make the order of
 * bytes in an io-stream correspond to the order of the bytes
 * in the memory while doing the io a `word' at a time.
 */

int
putw(w, stream)
int w;
register FILE *stream;
{
 register char *s = (char *)&w;
 register int i = sizeof(int);

 while (--i >= 0)
  (void) (putc(*s++, stream));
 return (ferror(stream));
}
/* rew.c  */

extern int fflush();
extern long lseek();

void
rewind(iop)
register FILE *iop;
{
 (void) fflush(iop);
 (void) lseek(fileno(iop), 0L, 0);
 iop->_cnt = 0;
 iop->_ptr = iop->_base;
 iop->_flag &= ~(_IOERR | _IOEOF);
 if(iop->_flag & _IORW)
  iop->_flag &= ~(_IOREAD | _IOWRT);
}

/* scanf.c  */

extern int _doscan();

/*VARARGS1*/
int
scanf(fmt, va_alist)
char *fmt;
va_dcl
{
 va_list ap;

 va_start(ap);
 return(_doscan(stdin,(unsigned char *) fmt, ap));
}

/*VARARGS2*/
int
fscanf(iop, fmt, va_alist)
FILE *iop;
char *fmt;
va_dcl
{
 va_list ap;

 va_start(ap);
 return(_doscan(iop,(unsigned char *) fmt, ap));
}

/*VARARGS2*/
int
sscanf(str100, fmt, va_alist)
register char *str100;
char *fmt;
va_dcl
{
 va_list ap;
 FILE strbuf;

 va_start(ap);
 strbuf._flag = _IOREAD;
 strbuf._ptr = strbuf._base = (unsigned char*)str100;
 strbuf._cnt = strlen(str100);
 strbuf._file = _NFILE;
 return(_doscan(&strbuf,(unsigned char *) fmt, ap));
}
/* setbuf.c  */

extern void free();
extern int isatty();
extern unsigned char _smbuf[][_SBFSIZ];
#if !u370
extern unsigned char *_stdbuf[];
#else
extern char *malloc();
#endif

void
setbuf(iop, buf)
register FILE *iop;
char *buf;
{
 register int fno = fileno(iop);  /* file number */

 if(iop->_base != NULL && iop->_flag & _IOMYBUF)
  free((char*)iop->_base);
 iop->_flag &= ~(_IOMYBUF | _IONBF | _IOLBF);
 if((iop->_base = (unsigned char*)buf) == NULL) {
  iop->_flag |= _IONBF; /* file unbuffered except in fastio */
#if u370
  if ( (iop->_base = (unsigned char *) malloc(BUFSIZ+8)) != NULL){
   iop->_flag |= _IOMYBUF;
   _bufend(iop) = iop->_base + BUFSIZ;
  }
#else
  if (fno < 2)  /* for stdin, stdout, use the existing bufs */
   _bufend(iop) = (iop->_base = _stdbuf[fno]) + BUFSIZ;
#endif

  else   /* otherwise, use small buffers reserved for this */
   _bufend(iop) = (iop->_base = _smbuf[fno]) + _SBFSIZ;
 }
 else {  /* regular buffered I/O, standard buffer size */
  _bufend(iop) = iop->_base + BUFSIZ;
  if (isatty(fno))
   iop->_flag |= _IOLBF;
 }
 iop->_ptr = iop->_base;
 iop->_cnt = 0;
}

/* ungetc.c  */

int
ungetc(c, iop)
int c;
register FILE *iop;
{
 if(c == EOF)
  return(-1);
 if((iop->_flag & _IOREAD) == 0 || iop->_ptr <= iop->_base)
  if(iop->_ptr == iop->_base && iop->_cnt == 0)
   ++iop->_ptr;
  else
   return(-1);
 *--iop->_ptr = c;
 ++iop->_cnt;
 return(c);
}
/* doprnt.c  */

/*
 * _doprnt: common code for printf, fprintf, sprintf
 */


#define PUT(p, n)     { register unsigned char *newbufptr; \
   if ((newbufptr = bufptr + n) > bufferend) { \
    _dowrite(p, n, iop, &bufptr); \
   } else { \
    (void) memcpy((char *) bufptr, p, n); \
    bufptr = newbufptr; \
   } \
        }

#define PAD(s, n)     { register int nn; \
   for (nn = n; nn > 20; nn -= 20) \
    _dowrite(s, 20, iop, &bufptr); \
   PUT(s, nn); \
        }

/* bit positions for flags used in doprnt */
#define LENGTH 1 /* l */
#define FPLUS 2  /* + */
#define FMINUS 4 /* - */
#define FBLANK 8 /* blank */
#define FSHARP 16 /* # */
#define PADZERO 32 /* padding zeroes requested via '0' */
#define DOTSEEN 64 /* dot appeared in format specification */
#define SUFFIX 128 /* a suffix is to appear in the output */
#define RZERO 256 /* there will be trailing zeros in output */
#define LZERO 512 /* there will be leading zeroes in output */

/*
 * C-Library routines for floating conversion
 */

extern char *fcvt(), *ecvt(), *memchr(), *memcpy();
extern int strlen(), fwrite();


static int
_lowdigit(valptr)
long *valptr;
{ /* This function computes the decimal low-order digit of the number */
 /* pointed to by valptr, and returns this digit after dividing   */
 /* *valptr by ten.  This function is called ONLY to compute the */
 /* low-order digit of a long whose high-order bit is set. */

 int lowbit = *valptr & 1;
/* long value = (*valptr >> 1) & ~HIBITL; */
 long value = (long unsigned)(*valptr >> 1);

 *valptr = value / 5;
 return(value % 5 * 2 + lowbit + '0');
}

/* The function _dowrite carries out buffer pointer bookkeeping surrounding */
/* a call to fwrite.  It is called only when the end of the file output */
/* buffer is approached or in other unusual situations. */
static
_dowrite(p, n, iop, ptrptr)
register char *p;
register int n;
register FILE *iop;
register unsigned char **ptrptr;
{
 if (iop->_file != _NFILE) {
  iop->_cnt -= (*ptrptr - iop->_ptr);
  iop->_ptr = *ptrptr;
  _bufsync(iop);
  (void) fwrite(p, 1, n, iop);
  *ptrptr = iop->_ptr;
 } else
  *ptrptr = (unsigned char *) memcpy((char *) *ptrptr, p, n) + n;
}

int
_doprnt(format, args, iop)
register char *format;
va_list  args;
register FILE *iop;
{
 static char _blanks[] = "                    ";
 static char _zeroes[] = "00000000000000000000";

 /* bufptr is used inside of doprnt instead of iop->_ptr; */
 /* bufferend is a copy of _bufend(iop), if it exists.  For */
 /* dummy file descriptors (iop->_file = _NFILE), bufferend */
 /* may be meaningless.     */

 unsigned char *bufptr, *bufferend;

 /* This variable counts output characters. */
 int count = 0;

 /* Starting and ending points for value to be printed */
 register char *bp1;
 char *p;

 /* Field width and precision */
 int width, prec;

 /* Format code */
 register int fcode;

 /* Number of padding zeroes required on the left and right */
 int lzero, rzero;

 /* Flags - bit positions defined by LENGTH, FPLUS, FMINUS, FBLANK, */
 /* and FSHARP are set if corresponding character is in format */
 /* Bit position defined by PADZERO means extra space in the field */
 /* should be padded with leading zeroes rather than with blanks */
 register int flagword;

 /* Values are developed in this buffer */
 char buf[max(MAXDIGS, 1+max(MAXFCVT+MAXEXP, MAXECVT))];

 /* Pointer to sign, "0x", "0X", or empty */
 char *prefix;

 /* Exponent or empty */
 char *suffix;

 /* Buffer to create exponent */
 char expbuf[MAXESIZ + 1];

 /* Length of prefix and of suffix */
 int prefixlength, suffixlength;

 /* combined length of leading zeroes, trailing zeroes, and suffix */
 int  otherlength;

 /* The value being converted, if integer */
 long val;

 /* The value being converted, if real */
 double  dval;

 /* Output values from fcvt and ecvt */
 int decpt, sign;

 /* Pointer to a translate table for digits of whatever radix */
 char *tab;

 /* Work variables */
 int k, lradix, mradix;

 /* MK 03.Mar.87 variable for fetching FP numbers from va_arg */
 union {
  int ival;
  float  fval;
  }
   FP_buf;

 /* initialize buffer pointer and buffer end pointer */
 { register int fno = iop->_file;
 /* host-port 2026-07: prime the stdio buffer if this is the first I/O to
    the file (e.g. printf as the very first output, with no preceding
    putc/puts).  The stock _doprnt below assumes iop->_ptr and
    _bufendtab[fno] are already set up, but they are allocated lazily by
    putc->_flsbuf->_wrtchk->_findbuf.  Mirror that priming here so printf
    works standalone.  Guarded on _base==NULL so the sprintf dummy-file
    path (whose _base is the caller's string) is left untouched. */
 if (iop->_base == NULL)
  _wrtchk(iop);
 bufptr = iop->_ptr;
 bufferend = (fno == _NFILE) ?
   (unsigned char *)((long) bufptr | (-1L & ~HIBITL))
     : _bufendtab[fno];
 }

 /*
  * The main loop -- this loop goes through one iteration
  * for each string of ordinary characters or format specification.
  */
 for ( ; ; ) {
  register int n;

  if ((fcode = *format) != '\0' && fcode != '%') {
   bp1 = format;
   do {
    format++;
   } while ((fcode = *format) != '\0' && fcode != '%');
  
   count += (n = format - bp1); /* n = no. of non-% chars */
   PUT(bp1, n);
  }
  if (fcode == '\0') {  /* end of format; return */
   register int nn = bufptr - iop->_ptr;
   iop->_cnt -= nn;
   iop->_ptr = bufptr;
/* MK 25.03.87 check for _NFILE now first - caused integer overflow */
   if (iop->_file != _NFILE
   && bufptr + iop->_cnt > bufferend ) /* in case of */
   _bufsync(iop); /* interrupt during last several lines */
   if (iop->_flag & (_IONBF | _IOLBF) &&
        (iop->_flag & _IONBF ||
         memchr((char *)(bufptr-count), '\n', count) != NULL))
    (void) _xflsbuf(iop);
   return(ferror(iop) ? EOF : count);
  }

  /*
   * % has been found.
   * The following switch is used to parse the format
   * specification and to perform the operation specified
   * by the format letter.  The program repeatedly goes
   * back to this switch until the format letter is
   * encountered.
   */
  width = prefixlength = otherlength = flagword = 0;
  format++;

 charswitch:

  switch (fcode = *format++) {

  case '+':
   flagword |= FPLUS;
   goto charswitch;
  case '-':
   flagword |= FMINUS;
   goto charswitch;
  case ' ':
   flagword |= FBLANK;
   goto charswitch;
  case '#':
   flagword |= FSHARP;
   goto charswitch;

  /* Scan the field width and precision */
  case '.':
   flagword |= DOTSEEN;
   prec = 0;
   goto charswitch;

  case '*':
   if (!(flagword & DOTSEEN)) {
    width = va_arg(args, int);
    if (width < 0) {
     width = -width;
     flagword ^= FMINUS;
    }
   } else {
    prec = va_arg(args, int);
    if (prec < 0)
     prec = 0;
   }
   goto charswitch;

  case '0': /* obsolescent spec:  leading zero in width */
    /* means pad with leading zeros */
   if (!(flagword & (DOTSEEN | FMINUS)))
    flagword |= PADZERO;
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
        { register num = fcode - '0';
   while (isdigit(fcode = *format)) {
    num = num * 10 + fcode - '0';
    format++;
   }
   if (flagword & DOTSEEN)
    prec = num;
   else
    width = num;
   goto charswitch;
        }

  /* Scan the length modifier */
  case 'l':
   flagword |= LENGTH;
   /* No break */
  case 'h':
   goto charswitch;

  /*
   * The character addressed by format must be
   * the format letter -- there is nothing
   * left for it to be.
   *
   * The status of the +, -, #, and blank
   * flags are reflected in the variable
   * "flagword".  "width" and "prec" contain
   * numbers corresponding to the digit
   * strings before and after the decimal
   * point, respectively. If there was no
   * decimal point, then flagword & DOTSEEN
   * is false and the value of prec is meaningless.
   *
   * The following switch cases set things up
   * for printing.  What ultimately gets
   * printed will be padding blanks, a
   * prefix, left padding zeroes, a value,
   * right padding zeroes, a suffix, and
   * more padding blanks.  Padding blanks
   * will not appear simultaneously on both
   * the left and the right.  Each case in
   * this switch will compute the value, and
   * leave in several variables the informa-
   * tion necessary to construct what is to
   * be printed.  
   *
   * The prefix is a sign, a blank, "0x",
   * "0X", or null, and is addressed by
   * "prefix".
   *
   * The suffix is either null or an
   * exponent, and is addressed by "suffix".
   * If there is a suffix, the flagword bit
   * SUFFIX will be set.
   *
   * The value to be printed starts at "bp1"
   * and continues up to and not including
   * "p".
   *
   * "lzero" and "rzero" will contain the
   * number of padding zeroes required on
   * the left and right, respectively.
   * The flagword bits LZERO and RZERO tell
   * whether padding zeros are required.
   *
   * The number of padding blanks, and
   * whether they go on the left or the
   * right, will be computed on exit from
   * the switch.
   */



  
  /*
   * decimal fixed point representations
   *
   * HIBITL is 100...000
   * binary, and is equal to the maximum
   * negative number.
   * We assume a 2's complement machine
   */

  case 'd':
   /* Fetch the argument to be printed */
   if (flagword & LENGTH)
    val = va_arg(args, long);
   else
    val = va_arg(args, int);

   /* Set buffer pointer to last digit */
   p = bp1 = buf + MAXDIGS;

   /* If signed conversion, make sign */
   if (val < 0) {
    prefix = "-";
    prefixlength = 1;
    /*
     * Negate, checking in
     * advance for possible
     * overflow.
     */
    if (val != HIBITL)
     val = -val;
    else     /* number is -HIBITL; convert last */
      /* digit now and get positive number */
     *--bp1 = _lowdigit(&val);
   } else if (flagword & FPLUS) {
    prefix = "+";
    prefixlength = 1;
   } else if (flagword & FBLANK) {
    prefix = " ";
    prefixlength = 1;
   }

  decimal:
   { register long qval = val;
    if (qval <= 9) {
     if (qval != 0 || !(flagword & DOTSEEN))
      *--bp1 = qval + '0';
    } else {
     do {
      n = qval;
      qval /= 10;
      *--bp1 = n - qval * 10 + '0';
     } while (qval > 9);
     *--bp1 = qval + '0';
    }
   }

   /* Calculate minimum padding zero requirement */
   if (flagword & DOTSEEN) {
    register leadzeroes = prec - (p - bp1);
    if (leadzeroes > 0) {
     otherlength = lzero = leadzeroes;
     flagword |= LZERO;
    }
   }

   break;

  case 'u':
   /* Fetch the argument to be printed */
   if (flagword & LENGTH)
    val = va_arg(args, long);
   else
    val = va_arg(args, unsigned);

   p = bp1 = buf + MAXDIGS;

   if (val & HIBITL)
    *--bp1 = _lowdigit(&val);

   goto decimal;

  /*
   * non-decimal fixed point representations
   * for radix equal to a power of two
   *
   * "mradix" is one less than the radix for the conversion.
   * "lradix" is one less than the base 2 log
   * of the radix for the conversion. Conversion is unsigned.
   * HIBITL is 100...000
   * binary, and is equal to the maximum
   * negative number.
   * We assume a 2's complement machine
   */

  case 'o':
   mradix = 7;
   lradix = 2;
   goto fixed;

  case 'X':
  case 'x':
   mradix = 15;
   lradix = 3;

  fixed:
   /* Fetch the argument to be printed */
   if (flagword & LENGTH)
    val = va_arg(args, long);
   else
    val = va_arg(args, unsigned);

   /* Set translate table for digits */
   tab = (fcode == 'X') ?
       "0123456789ABCDEF" : "0123456789abcdef";

   /* Develop the digits of the value */
   p = bp1 = buf + MAXDIGS;
   { register long qval = val;
    if (qval == 0) {
     if (!(flagword & DOTSEEN)) {
      otherlength = lzero = 1;
      flagword |= LZERO;
     }
    } else
     do {
      *--bp1 = tab[qval & mradix];
 /*      qval = ((qval >> 1) & ~HIBITL)
          >> lradix; */
     qval = ((long unsigned)(qval >> 1))
          >> lradix;
     } while (qval != 0);
   }

   /* Calculate minimum padding zero requirement */
   if (flagword & DOTSEEN) {
    register leadzeroes = prec - (p - bp1);
    if (leadzeroes > 0) {
     otherlength = lzero = leadzeroes;
     flagword |= LZERO;
    }
   }

   /* Handle the # flag */
   if (flagword & FSHARP && val != 0)
    switch (fcode) {
    case 'o':
     if (!(flagword & LZERO)) {
      otherlength = lzero = 1;
      flagword |= LZERO;
     }
     break;
    case 'x':
     prefix = "0x";
     prefixlength = 2;
     break;
    case 'X':
     prefix = "0X";
     prefixlength = 2;
     break;
    }

   break;

  case 'E':
  case 'e':
   /*
    * E-format.  The general strategy
    * here is fairly easy: we take
    * what ecvt gives us and re-format it.
    */

   /* Establish default precision */
   if (!(flagword & DOTSEEN))
    prec = 6;

   /* Fetch the value */
  /* MK 03.Mar.87 */
   FP_buf.ival = va_arg(args, int);
   dval = (double) FP_buf.fval;

   /* Develop the mantissa */
   bp1 = ecvt(dval, min(prec + 1, MAXECVT), &decpt, &sign);

   /* Determine the prefix */
  e_merge:
   if (sign) {
    prefix = "-";
    prefixlength = 1;
   } else if (flagword & FPLUS) {
    prefix = "+";
    prefixlength = 1;
   } else if (flagword & FBLANK) {
    prefix = " ";
    prefixlength = 1;
   }

   /* Place the first digit in the buffer*/
   p = &buf[0];
   *p++ = (*bp1 != '\0') ? *bp1++ : '0';

   /* Put in a decimal point if needed */
   if (prec != 0 || (flagword & FSHARP))
    *p++ = '.';

   /* Create the rest of the mantissa */
   { register rz = prec;
    for ( ; rz > 0 && *bp1 != '\0'; --rz)
     *p++ = *bp1++;
    if (rz > 0) {
     otherlength = rzero = rz;
     flagword |= RZERO;
    }
   }

   bp1 = &buf[0];

   /* Create the exponent */
   *(suffix = &expbuf[MAXESIZ]) = '\0';
   if (dval != 0) {
    register int nn = decpt - 1;
    if (nn < 0)
        nn = -nn;
    for ( ; nn > 9; nn /= 10)
     *--suffix = todigit(nn % 10);
    *--suffix = todigit(nn);
   }

   /* Prepend leading zeroes to the exponent */
   while (suffix > &expbuf[MAXESIZ - 2])
    *--suffix = '0';

   /* Put in the exponent sign */
   *--suffix = (decpt > 0 || dval == 0) ? '+' : '-';

   /* Put in the e */
   *--suffix = isupper(fcode) ? 'E'  : 'e';

   /* compute size of suffix */
   otherlength += (suffixlength = &expbuf[MAXESIZ]
          - suffix);
   flagword |= SUFFIX;

   break;

  case 'f':
   /*
    * F-format floating point.  This is a
    * good deal less simple than E-format.
    * The overall strategy will be to call
    * fcvt, reformat its result into buf,
    * and calculate how many trailing
    * zeroes will be required.  There will
    * never be any leading zeroes needed.
    */

   /* Establish default precision */
   if (!(flagword & DOTSEEN))
    prec = 6;

   /* Fetch the value */
  /* MK 03.Mar.87 */
   FP_buf.ival = va_arg(args, int);
   dval = (double) FP_buf.fval;

   /* Do the conversion */
   bp1 = fcvt(dval, min(prec, MAXFCVT), &decpt, &sign);

   /* Determine the prefix */
  f_merge:
   if (sign && decpt > -prec && *bp1 != '0') {
    prefix = "-";
    prefixlength = 1;
   } else if (flagword & FPLUS) {
    prefix = "+";
    prefixlength = 1;
   } else if (flagword & FBLANK) {
    prefix = " ";
    prefixlength = 1;
   }

   /* Initialize buffer pointer */
   p = &buf[0];

   { register int nn = decpt;

    /* Emit the digits before the decimal point */
    k = 0;
    do {
     *p++ = (nn <= 0 || *bp1 == '\0'
      || k >= MAXFSIG) ?
          '0' : (k++, *bp1++);
    } while (--nn > 0);

    /* Decide whether we need a decimal point */
    if ((flagword & FSHARP) || prec > 0)
     *p++ = '.';

    /* Digits (if any) after the decimal point */
    nn = min(prec, MAXFCVT);
    if (prec > nn) {
     flagword |= RZERO;
     otherlength = rzero = prec - nn;
    }
    while (--nn >= 0)
     *p++ = (++decpt <= 0 || *bp1 == '\0' ||
             k >= MAXFSIG) ? '0' : (k++, *bp1++);
   }

   bp1 = &buf[0];

   break;

  case 'G':
  case 'g':
   /*
    * g-format.  We play around a bit
    * and then jump into e or f, as needed.
    */
  
   /* Establish default precision */
   if (!(flagword & DOTSEEN))
    prec = 6;
   else if (prec == 0)
    prec = 1;

   /* Fetch the value */
  /* MK 03.Mar.87 */
   FP_buf.ival = va_arg(args, int);
   dval = (double)FP_buf.fval;

   /* Do the conversion */
   bp1 = ecvt(dval, min(prec, MAXECVT), &decpt, &sign);
   if (dval == 0)
    decpt = 1;

   { register int kk = prec;
    if (!(flagword & FSHARP)) {
     n = strlen(bp1);
     if (n < kk)
      kk = n;
     while (kk >= 1 && bp1[kk-1] == '0')
      --kk;
    }
    
    if (decpt < -3 || decpt > prec) {
     prec = kk - 1;
     goto e_merge;
    }
    prec = kk - decpt;
    goto f_merge;
   }

  case '%':
   buf[0] = fcode;
   goto c_merge;

  case 'c':
   buf[0] = va_arg(args, int);
  c_merge:
   p = (bp1 = &buf[0]) + 1;
   break;

  case 's':
   bp1 = va_arg(args, int);
   if (!(flagword & DOTSEEN))
    p = bp1 + strlen(bp1);
   else { /* a strnlen function would  be useful here! */
    register char *qp = bp1;
    while (*qp++ != '\0' && --prec >= 0)
     ;
    p = qp - 1;
   }
   break;

  default: /* this is technically an error; what we do is to */
   /* back up the format pointer to the offending char */
   /* and continue with the format scan */
   format--;
   continue;

  }

  /* Calculate number of padding blanks */
  k = (n = p - bp1) + prefixlength + otherlength;
  if (width <= k)
   count += k;
  else {
   count += width;

   /* Set up for padding zeroes if requested */
   /* Otherwise emit padding blanks unless output is */
   /* to be left-justified.  */

   if (flagword & PADZERO) {
    if (!(flagword & LZERO)) {
     flagword |= LZERO;
     lzero = width - k;
    }
    else
     lzero += width - k;
    k = width; /* cancel padding blanks */
   } else
    /* Blanks on left if required */
    if (!(flagword & FMINUS))
     PAD(_blanks, width - k);
  }

  /* Prefix, if any */
  if (prefixlength != 0)
   PUT(prefix, prefixlength);

  /* Zeroes on the left */
  if (flagword & LZERO)
   PAD(_zeroes, lzero);
  
  /* The value itself */
  if (n > 0)
   PUT(bp1, n);

  if (flagword & (RZERO | SUFFIX | FMINUS)) {
   /* Zeroes on the right */
   if (flagword & RZERO)
    PAD(_zeroes, rzero);

   /* The suffix */
   if (flagword & SUFFIX)
    PUT(suffix, suffixlength);

   /* Blanks on the right if required */
   if (flagword & FMINUS && width > k)
    PAD(_blanks, width - k);
  }
 }
}
/* fprintf.c  */

extern int _doprnt();

/*VARARGS2*/
int
fprintf(iop, format, va_alist)
FILE *iop;
char *format;
va_dcl
{
 register int count;
 va_list ap;

 va_start(ap);
 if (!(iop->_flag | _IOWRT)) {
  /* if no write flag */
  if (iop->_flag | _IORW) {
   /* if ok, cause read-write */
   iop->_flag |= _IOWRT;
  } else {
   /* else error */
   return EOF;
  }
 }
 count = _doprnt(format, ap, iop);
 va_end(ap);
 return(ferror(iop)? EOF: count);
}
/* printf.c  */


extern int _doprnt();

/*VARARGS1*/
int
printf(format, va_alist)
char *format;
va_dcl
{
 register int count;
 va_list ap;

 va_start(ap);
 if (!(stdout->_flag | _IOWRT)) {
  /* if no write flag */
  if (stdout->_flag | _IORW) {
   /* if ok, cause read-write */
   stdout->_flag |= _IOWRT;
  } else {
   /* else error */
   return EOF;
  }
 }
 count = _doprnt(format, ap, stdout);
 va_end(ap);
 return(ferror(stdout)? EOF: count);
}
/* sprintf.c  */

extern int _doprnt();

/*VARARGS2*/
int
sprintf(string, format, va_alist)
char *string, *format;
va_dcl
{
 register int count;
 FILE siop;
 va_list ap;

 siop._cnt = MAXINT;
 siop._base = siop._ptr = (unsigned char *)string;
 siop._flag = _IOWRT;
 siop._file = _NFILE;
 va_start(ap);
 count = _doprnt(format, ap, &siop);
 va_end(ap);
 *siop._ptr = '\0'; /* plant terminating null character */
 return(count);
}
/* vfprintf.c  */

extern int _doprnt();

/*VARARGS2*/
int
vfprintf(iop, format, ap)
FILE *iop;
char *format;
va_list ap;
{
 register int count;

 if (!(iop->_flag | _IOWRT)) {
  /* if no write flag */
  if (iop->_flag | _IORW) {
   /* if ok, cause read-write */
   iop->_flag |= _IOWRT;
  } else {
   /* else error */
   return EOF;
  }
 }
 count = _doprnt(format, ap, iop);
 return(ferror(iop)? EOF: count);
}
/* vprintf.c  */

extern int _doprnt();

/*VARARGS1*/
int
vprintf(format, ap)
char *format;
va_list ap;
{
 register int count;

 if (!(stdout->_flag | _IOWRT)) {
  /* if no write flag */
  if (stdout->_flag | _IORW) {
   /* if ok, cause read-write */
   stdout->_flag |= _IOWRT;
  } else {
   /* else error */
   return EOF;
  }
 }
 count = _doprnt(format, ap, stdout);
 return(ferror(stdout)? EOF: count);
}
/* vsprintf.c  */

extern int _doprnt();

/*VARARGS2*/
int
vsprintf(string, format, ap)
char *string, *format;
va_list ap;
{
 register int count;
 FILE siop;

 siop._cnt = MAXINT;
 siop._base = siop._ptr = (unsigned char *)string;
 siop._flag = _IOWRT;
 siop._file = _NFILE;
 count = _doprnt(format, ap, &siop);
 *siop._ptr = '\0'; /* plant terminating null character */
 return(count);
}
/* abort.c  */
/*
 * abort() - terminate current process with dump via SIGIOT
 */


static pass = 0;  /* counts how many times abort has been called*/

int
abort()
{
 /* increment first to avoid any hassle with interupts */
 if (++pass == 1)  {
  _cleanup();
 }
 /*return(*/ _exit(-1) /*)*/ ;
}
/* abs.c  */

int
abs(arg)
register int arg;
{
/* SS. 04.sep.87 modified: "NEG for 0x80000000" causes Kronos interrupt */
/* return (arg >= 0 ? arg : -arg);  */

 if( arg >= 0 || arg == HIBITL )
  return( arg );
 else
  return(-arg);
}

/* assert.c  */
/*
 * called from "assert" macro; prints without printf or stdio.
 */

#define WRITE(s, n) (void) write(2, (s), (n))
#define WRITESTR(s1, n, s2) WRITE((s1), n), \
    WRITE((s2), (unsigned) strlen(s2))
void _assert(assertion, filename, line_num)
char *assertion;
char *filename;
int line_num;
{
 static char linestr[] = ", line NNNNN\n";
 register char *p = &linestr[7];
 register int div, digit;

 WRITESTR("Assertion failed: ", 18, assertion);
 WRITESTR(", file ", 7, filename);
 for (div = 10000; div != 0; line_num %= div, div /= 10)
  if ((digit = line_num/div) != 0 || p != &linestr[7] || div == 1)
   *p++ = digit + '0';
 *p++ = '\n';
 *p = '\0';
 WRITE(linestr, (unsigned) strlen(linestr));
 (void) abort();
}
/* ctype.c  */

char _ctype[] = { 0,

/*  0  1  2  3  4  5  6   7  */

/* 0*/ _C, _C, _C, _C, _C, _C, _C, _C,
/* 10*/  _C, _S|_C, _S|_C, _S|_C, _S|_C, _S|_C,  _C, _C,
/* 20*/  _C, _C, _C, _C, _C, _C, _C, _C,
/* 30*/  _C, _C, _C, _C, _C, _C, _C, _C,
/* 40*/  _S|_B,  _P, _P, _P, _P, _P, _P, _P,
/* 50*/  _P, _P, _P, _P, _P, _P, _P, _P,
/* 60*/  _N|_X,  _N|_X,  _N|_X,  _N|_X,  _N|_X,  _N|_X,  _N|_X,  _N|_X,
/* 70*/  _N|_X,  _N|_X,  _P, _P, _P, _P, _P, _P,
/*100*/  _P, _U|_X, _U|_X, _U|_X, _U|_X, _U|_X,  _U|_X,  _U,
/*110*/  _U, _U, _U, _U, _U, _U, _U, _U,
/*120*/  _U, _U, _U, _U, _U, _U, _U, _U,
/*130*/  _U, _U, _U, _P, _P, _P, _P, _P,
/*140*/  _P, _L|_X, _L|_X, _L|_X, _L|_X, _L|_X,  _L|_X,  _L,
/*150*/  _L, _L, _L, _L, _L, _L, _L, _L,
/*160*/  _L, _L, _L, _L, _L, _L, _L, _L,
/*170*/  _L, _L, _L, _P, _P, _P, _P, _C,
/*200*/   0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0
};
/* ecvt.c  */

/*
 * ecvt converts to decimal
 * the number of digits is specified by ndigit
 * decpt is set to the position of the decimal point
 * sign is set to 0 for positive, 1 for negative
 *
 */
#define  NMAX ((DSIGNIF * 3 + 19)/10) /* restrict max precision */
#define  NDIG 80

extern char *cvt();

char *
ecvt(value, ndigit, decpt, sign)
double value;
int ndigit, *decpt, *sign;
{
 return (cvt(value, ndigit, decpt, sign, 0));
}

char *
fcvt(value, ndigit, decpt, sign)
double value;
int ndigit, *decpt, *sign;
{
 return (cvt(value, ndigit, decpt, sign, 1));
}

static char buf[NDIG];

static char *
cvt(value, ndigit, decpt, sign, f_flag)
double value;
int ndigit, *sign, f_flag;
register int *decpt;
{
 register char *p = &buf[0], *p_last = &buf[ndigit];

 KILLNaN(value); /* raise exception on Not-a-Number (3b only) */
 if (*sign = (value < 0.0))
  value = -value;
 buf[0] = '\0';
 *decpt = 0;
 if (value != 0.0) { /* rescale to range [1.0, 10.0) */
  /* in binary for speed and to minimize error build-up */
  /* even for the IEEE standard with its high exponents,
     it's probably better for speed to just loop on them */
  static struct s { double p10; int n; } s[] = {
   1e32, 32,
   1e16, 16,
   1e8,  8,
   1e4,  4,
   1e2,  2,
   1e1,  1,
  };
  register struct s *sp = s;

  ++*decpt;
  if (value >= 2.0 * MAXPOWTWO) /* can't be precisely integral */
   do {
    for ( ; value >= sp->p10; *decpt += sp->n)
     value /= sp->p10;
   } while (sp++->n > 1);
  else if (value >= 10.0) { /* convert integer part separately */
   register double pow10 = 10.0, powtemp;
   double ftmp1, ftmp2, minusnull;

   ftmp1 = ftmp2 = 0.0;  /* SS 14.Jan.88 */
   minusnull = ftmp1-ftmp2;

   while ((powtemp = 10.0 * pow10) <= value)
    pow10 = powtemp;

 /*  disable_intr(0x42);   disable floating overflow */

   for ( ; ; pow10 /= 10.0) {
/* SS. 28.aug.87 debugging code, NB! format %f not available,produce loop */
    register int digit;
/* SS. 09.sep.87 we can't compute expr.-> -0.0 / pow10 */
/* because "fdiv" with 80000000h/pow10 -> interrupt 42h, floating overflow   */
    digit = value/pow10;
    *p++ = digit + '0';
    value -= digit * pow10;
    if( value == minusnull )  /* SS 14.Jan.88 */
     value = 0.0;
    ++*decpt;
    if (pow10 <= 10.0)
     break;
   } /* ..for */

 /*  enable_intr(0x42);     enable floating overflow */

  } else if (value < 1.0)
   do {
    for ( ; value * sp->p10 < 10.0; *decpt -= sp->n)
     value *= sp->p10;
   } while (sp++->n > 1);
 }
 if (f_flag)
  p_last += *decpt;
 if (p_last >= buf) {
  if (p_last > &buf[NDIG - 2])
   p_last = &buf[NDIG - 2];
  for ( ; ; ++p) {
   if (value == 0 || p >= &buf[NMAX])
    *p = '0';
   else {
    register int intx; /* intx in [0, 9] */
    *p = (intx = (int)value) + '0';
    value = 10.0 * (value - (double)intx);
   }
   if (p >= p_last) {
    p = p_last;
    break;
   }
  }
  if (*p >= '5') /* check rounding in last place + 1 */
   do {
    if (p == buf) { /* rollover from 99999... */
     buf[0] = '1'; /* later digits are 0 */
     ++*decpt;
     if (f_flag)
      ++p_last;
     break;
    }
    *p = '0';
   } while (++*--p > '9'); /* propagate carries left */
  *p_last = '\0';
 }
 return (buf);
}
/* frexp.c  */

/*
 * frexp(value, eptr)
 * returns a double x such that x = 0 or 0.5 <= |x| < 1.0
 * and stores an integer n such that value = x * 2 ** n
 * indirectly through eptr.
 *
 */

double
frexp(value, eptr)
double value; /* don't declare register, because of KILLNan! */
register int *eptr;
{
 register double absvalue;

 KILLNaN(value); /* raise exception on Not-a-Number (3b only) */
 *eptr = 0;
 if (value == 0.0) /* nothing to do for zero */
  return (value);
 absvalue = (value > 0.0) ? value : -value;
 for ( ; absvalue >= 1.0; absvalue *= 0.5)
  ++*eptr;
 for ( ; absvalue < 0.5; absvalue += absvalue)
  --*eptr;
 return (value > 0.0 ? absvalue : -absvalue);
}

/* ldexp.c  */

/*
 * double ldexp (value, exp)
 *  double value;
 *  int exp;
 *
 * Ldexp returns value * 2**exp, if that result is in range.
 * If underflow occurs, it returns zero.  If overflow occurs,
 * it returns a value of appropriate sign and largest single-
 * precision magnitude.  In case of underflow or overflow,
 * the external int "errno" is set to ERANGE.  Note that errno is
 * not modified if no error occurs, so if you intend to test it
 * after you use ldexp, you had better set it to something
 * other than ERANGE first (zero is a reasonable value to use).
 */

/* Largest signed long int power of 2 */
#define MAXSHIFT (BITSPERBYTE * sizeof(long) - 2)

extern double frexp();

double
ldexp(value, exp)
register double value;
register int exp;
{
 int old_exp;

 if (exp == 0 || value == 0.0) /* nothing to do for zero */
  return (value);
#if !(pdp11 || u3b5) /* pdp11 "cc" can't handle cast of
       double to void on pdp11 or 3b5 */
 (void)
#endif
 frexp(value, &old_exp);
 if (exp > 0) {
  if (exp + old_exp > MAXBEXP) { /* overflow */
   errno = ERANGE;
   return (value < 0 ? -MAXFLOAT : MAXFLOAT);
  }
  for ( ; exp > MAXSHIFT; exp -= MAXSHIFT)
   value *= ((unsigned)1L << MAXSHIFT);
  return (value * ((unsigned)1L << exp));
 }
 if (exp + old_exp < MINBEXP) { /* underflow */
  errno = ERANGE;
  return (0.0);
 }
 for ( ; exp < -MAXSHIFT; exp += MAXSHIFT)
  value *= 1.0/((unsigned)1L << MAXSHIFT); /* mult faster than div */
 return (value / ((unsigned)1L << -exp));
}
/* memccpy.c  */

/*
 * Copy s2 to s1, stopping if character c is copied. Copy no more than n bytes.
 * Return a pointer to the byte after character c in the copy,
 * or NULL if c is not found in the first n bytes.
 */
char *
memccpy(s1, s2, c, n)
register char *s1, *s2;
register int c, n;
{
 while (--n >= 0)
  if ((*s1++ = *s2++) == c)
   return (s1);
 return (0);
}
/* memchr.c  */

/*
 * Return the ptr in sp at which the character c appears;
 *   NULL if not found in n chars; don't stop at \0.
 */
char *
memchr(sp, c, n)
register char *sp;
register int n, c;
{
 while (--n >= 0)
  if (*sp++ == c)
   return (--sp);
 return (0);
}

/* memcmp.c  */

/*
 * Compare n bytes:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */
int
memcmp(s1, s2, n)
register char *s1, *s2;
register int n;
{
 int diff;

 if (s1 != s2)
  while (--n >= 0)
   if (diff = *s1++ - *s2++)
    return (diff);
 return (0);
}

/* memcpy.c  */

/*
 * Copy s2 to s1, always copy n bytes.
 * Return s1
 */
char *
memcpy(s1, s2, n)
register char *s1, *s2;
register int n;
{
 register char *os1 = s1;

 while (--n >= 0)
  *s1++ = *s2++;
 return (os1);
}
/* memset.c  */

/*
 * Set an array of n chars starting at sp to the character c.
 * Return sp.
 */
char *
memset(sp, c, n)
register char *sp, c;
register int n;
{
 register char *sp0 = sp;

 while (--n >= 0)
  *sp++ = c;
 return (sp0);
}

/* mktemp.c  */

/****************************************************************
 * Routine expects a string of length at least 6, with
 * six trailing 'X's.  These will be overlaid with a
 * letter and the last (5) digigts of the proccess ID.
 * If every letter (a thru z) thus inserted leads to
 * an existing file name, your string is shortened to
 * length zero upon return (first character set to '\0').
 ***************************************************************/
extern int strlen(), access(), getpid();

char *
mktemp(as)
char *as;
{
 register char *s=as;
 register unsigned pid;

 pid = getpid();
 s += strlen(as); /* point at the terminal null */
 while(*--s == 'X') {
  *s = (pid%10) + '0';
  pid /= 10;
 }
 if(*++s) {  /* maybe there were no 'X's */
  *s = 'a';
  while(access(as, 0) == 0) {
   if(++*s > 'z') {
    *as = '\0';
    break;
   }
  }
 } else
  if(access(as, 0) == 0)
   *as = '\0';
 return(as);
}
/* modf.c  */

/*
 * modf(value, iptr) returns the signed fractional part of value
 * and stores the integer part indirectly through iptr.
 *
 */


double
modf(value, iptr)
double value; /* don't declare register, because of KILLNaN! */
register double *iptr;
{
 register double absvalue;

 if ((absvalue = (value >= 0.0) ? value : -value) >= MAXPOWTWO)
  *iptr = value; /* it must be an integer */
 else {
  *iptr = absvalue + MAXPOWTWO; /* shift fraction off right */
  *iptr -= MAXPOWTWO; /* shift back without fraction */
  while (*iptr > absvalue) /* above arithmetic might round */
   *iptr -= 1.0; /* test again just to be sure */
  if (value < 0.0)
   *iptr = -*iptr;
 }
 return (value - *iptr); /* signed fractional part */
}
/* perror.c  */

/*
 * Print the error indicated
 * in the cerror cell.
 */

extern int errno, sys_nerr, strlen(), write();
extern char *sys_errlist[];

void
perror(s)
char *s;
{
 modula char * _perror();
 register char *c;
 register int n;

/* c = "Unknown error";  */

 c = _perror();

 if(errno < sys_nerr)
  c = sys_errlist[errno];
 n = strlen(s);
 if(n) {
  (void) write(2, s, (unsigned)n);
  (void) write(2, ": ", 2);
 }
 (void) write(2, c, (unsigned)strlen(c));
 (void) write(2, "\n", 1);
}
/* strcat.c  */

/*
 * Concatenate s2 on the end of s1.  S1's space must be large enough.
 * Return s1.
 */

char *
strcat(s1, s2)
register char *s1, *s2;
{
 register char *os1;
 if( (os1 = s1) == s2 )
  error("strcat: bad address",(char *) 0);
/* SS 09-Jan-88 was only os1 = s1; */

 while(*s1++)
  ;
 --s1;
 while(*s1++ = *s2++)
  ;
 return(os1);
}

/* strchr.c  */

/*
 * Return the ptr in sp at which the character c appears;
 * NULL if not found
 */

#define  NULL 0

char *
strchr(sp, c)
register char *sp, c;
{
 do {
  if(*sp == c)
   return(sp);
 } while(*sp++);
 return(NULL);
}

/* strcmp.c
 *
 * Compare strings:  s1>s2: >0  s1==s2: 0  s1<s2: <0
 */

int
strcmp(s1, s2)
register char *s1, *s2;
{
 if(s1 == s2)
  return(0);

        if( !((int)s1 & 3) && !((int)s2 & 3) ){

                asm(" llw 5");    /* s1 - first arg is LLW5 */
  asm("  shr2");
  asm("  llw 6");    /* s2 - second arg is LLW6 */
  asm("  shr2");
  asm("  comp");  /* compare the strings */
  asm("  sub");   /* difference remain to top of stack */
  asm("  rtn");
 }
 /* SS 14.Apr.88 previously were only these statements */
        while(*s1 == *s2++)  
  if(*s1++ == '\0')
   return(0);
 return(*s1 - *--s2);
}

/* strcpy.c  */
/*
 * Copy string s2 to s1.  s1 must be large enough.
 * return s1
 */

char *
strcpy(s1, s2)
register char *s1, *s2;
{
 register char *os1;

 os1 = s1;
 while(*s1++ = *s2++)
  ;
 return(os1);
}
/* strcspn.c  */

/*
 * Return the number of characters in the maximum leading segment
 * of string which consists solely of characters NOT from charset.
 */
int
strcspn(string, charset)
char *string;
register char *charset;
{
 register char *p, *q;

 for(q=string; *q != '\0'; ++q) {
  for(p=charset; *p != '\0' && *p != *q; ++p)
   ;
  if(*p != '\0')
   break;
 }
 return(q-string);
}

/* strlen.c  */

/*
 * Returns the number of
 * non-NULL bytes in string argument.
 */

int
strlen(s)
register char *s;
{
 register char *s0 = s + 1;

 while (*s++ != '\0')
  ;
 return (s - s0);
}

/* strncat.c  */

/*
 * Concatenate s2 on the end of s1.  S1's space must be large enough.
 * At most n characters are moved.
 * Return s1.
 */

char *
strncat(s1, s2, n)
register char *s1, *s2;
register n;
{
 register char *os1;

 if( (os1 = s1) == s2 )
  error("strncat: bad address",(char *) 0);
/* SS 09-Jan-88 was only os1 = s1; */

 while(*s1++)
  ;
 --s1;
 while(*s1++ = *s2++)
  if(--n < 0) {
   *--s1 = '\0';
   break;
  }
 return(os1);
}

/* strncmp.c  */

/*
 * Compare strings (at most n bytes)
 * returns: s1>s2; >0  s1==s2; 0  s1<s2; <0
 */

int
strncmp(s1, s2, n)
register char *s1, *s2;
register n;
{
 if(s1 == s2)
  return(0);
 while(--n >= 0 && *s1 == *s2++)
  if(*s1++ == '\0')
   return(0);
 return((n < 0)? 0: (*s1 - *--s2));
}

/* strncpy.c  */

/*
 * Copy s2 to s1, truncating or null-padding to always copy n bytes
 * return s1
 */

char *
strncpy(s1, s2, n)
register char *s1, *s2;
register int n;
{
 register char *os1 = s1;

 while (--n >= 0)
  if ((*s1++ = *s2++) == '\0')
   while (--n >= 0)
    *s1++ = '\0';
 return (os1);
}

/* strpbrk.c  */

/*
 * Return ptr to first occurance of any character from `brkset'
 * in the character string `string'; NULL if none exists.
 */

#define  NULLP (char *) 0

char *
strpbrk(string, brkset)
register char *string, *brkset;
{
 register char *p;

 do {
  for(p=brkset; *p != '\0' && *p != *string; ++p)
   ;
  if(*p != '\0')
   return(string);
 }
 while(*string++);
 return(NULLP);
}
/* strrchr.c  */

/*
 * Return the ptr in sp at which the character c last
 * appears; NULL if not found
*/

#define NULLI 0

char *
strrchr(sp, c)
register char *sp, c;
{
 register char *r;

 r = NULLI;
 do {
  if(*sp == c)
   r = sp;
 } while(*sp++);
 return(r);
}

/* strspn.c  */

/*
 * Return the number of characters in the maximum leading segment
 * of string which consists solely of characters from charset.
 */
int
strspn(string, charset)
char *string;
register char *charset;
{
 register char *p, *q;

 for(q=string; *q != '\0'; ++q) {
  for(p=charset; *p != '\0' && *p != *q; ++p)
   ;
  if(*p == '\0')
   break;
 }
 return(q-string);
}

/* strtok.c  */

/*
 * uses strpbrk and strspn to break string into tokens on
 * sequentially subsequent calls.  returns NULL when no
 * non-separator characters remain.
 * `subsequent' calls are calls with first argument NULL.
 */

#define  NULLV (char*)0

extern int strspn();
extern char *strpbrk();

char *
strtok(string, sepset)
char *string, *sepset;
{
 register char *p, *q, *r;
 static char *savept;

 /*first or subsequent call*/
 p = (string == NULLV)? savept: string;

 if(p == 0)  /* return if no tokens remaining */
  return(NULLV);

 q = p + strspn(p, sepset); /* skip leading separators */

 if(*q == '\0')   /* return if no tokens remaining */
  return(NULLV);

 if((r = strpbrk(q, sepset)) == NULLV) /* move past token */
  savept = 0; /* indicate this is last token */
 else {
  *r = '\0';
  savept = ++r;
 }
 return(q);
}
/* tell.c  */

/*
 * return offset in file.
 */

extern long lseek();

long
tell(f)
int f;
{
 return(lseek(f, 0L, 1));
}
/* tolower.c  */

/*
 * If arg is upper-case, return the lower-case, else return the arg.
 */

int
tolower(c)
register int c;
{
 if(c >= 'A' && c <= 'Z')
  c -= 'A' - 'a';
 return(c);
}
/* toupper.c  */

/*
 * If arg is lower-case, return upper-case, otherwise return arg.
 */

int
toupper(c)
register int c;
{
 if(c >= 'a' && c <= 'z')
  c += 'A' - 'a';
 return(c);
}

/* atof.c  */

/*
 * C library - ascii to floating (atof) and string to double (strtod)
 *
 * This version compiles both atof and strtod depending on the value
 * of STRTOD, which is set in the file and may be overridden on the
 * "cc" command line.  The only difference is the storage of a pointer
 * to the character which terminated the conversion.
 * Long-integer accumulation is used, except on the PDP11, where
 * "long" arithmetic is simulated, so floating-point is much faster.
 */
#ifndef STRTOD
#define STRTOD 0
#endif

extern double ldexp();

#if u3b || u3b5
# define POW1_25LEN 9
#else
# define POW1_25LEN 6
#endif
static double pow1_25[POW1_25LEN] = { 0.0 };

#if STRTOD
#define STORE_PTR (*ptr = p)
#define GOT_DIGIT (got_digit++)
#define RET_ZERO(val) if (!got_digit) return (0.0)

double
strtod(p, ptr)
register char *p;
char **ptr;
#else
#define STORE_PTR
#define GOT_DIGIT
#define RET_ZERO(val) if (!val) return (0.0)

double
atof(p)
register char *p;
#endif
{
 register int c;
 int exp = 0, neg_val = 0;
 double fl_val;
#if STRTOD
 int got_digit = 0;
 char *dummy;
 if (ptr == (char **)0)
  ptr = &dummy; /* harmless dumping place */
 STORE_PTR;
#endif
 while (isspace(c = *p)) /* eat leading white space */
  p++;
 switch (c) { /* process sign */
 case '-':
  neg_val++;
 case '+': /* fall-through */
  p++;
 }
 { /* accumulate value */
#if pdp11
 /* "long" arithmetic on the PDP-11 is simulated using int's and
  * is outrageously slow, so the accumulation is done using double's */
  register int decpt = 0;

  fl_val = 0.0;
  while (isdigit(c = *p++) || c == '.' && !decpt++) {
   if (c == '.')
    continue;
   GOT_DIGIT;
   exp -= decpt; /* decr exponent if dec point seen */
   if (fl_val < 2.0 * MAXPOWTWO)
    fl_val = 10.0 * fl_val + (double)(c - '0');
   else
    exp++;
  }
  RET_ZERO(fl_val);
#else
  register long high = 0, low = 0, scale = 1;
  register int decpt = 0, nzeroes = 0;

  while (isdigit(c = *p++) || c == '.' && !decpt++) {
   if (c == '.')
    continue;
   GOT_DIGIT;
   if (decpt) { /* handle trailing zeroes specially */
    if (c == '0') { /* ignore zero for now */
     nzeroes++;
     continue;
    }
    while (nzeroes > 0) { /* put zeroes back in */
     exp--;
     if (high < MAXLONG/10) {
      high *= 10;
     } else if (scale < MAXLONG/10) {
      scale *= 10;
      low *= 10;
     } else
      exp++;
     nzeroes--;
    }
    exp--; /* decr exponent if decimal pt. seen */
   }
   if (high < MAXLONG/10) {
    high *= 10;
    high += c - '0';
   } else if (scale < MAXLONG/10) {
    scale *= 10;
    low *= 10;
    low += c - '0';
   } else
    exp++;
  }
  RET_ZERO(high);
  fl_val = (double)high;
  if (scale > 1)
   fl_val = (double)scale * fl_val + (double)low;
#endif
 }
 STORE_PTR; /* in case there is no legitimate exponent */
 if (c == 'E' || c == 'e') { /* accumulate exponent */
  register int e_exp = 0, neg_exp = 0;

  switch (*p) { /* process sign */
  case '-':
   neg_exp++;
  case '+': /* fall-through */
  case ' ': /* many FORTRAN environments generate this! */
   p++;
  }
  if (isdigit(c = *p)) { /* found a legitimate exponent */
   do {
    /* limit outrageously large exponents */
    if (e_exp < DMAXEXP)
     e_exp = 10 * e_exp + c - '0';
   } while (isdigit(c = *++p));
   if (neg_exp)
    exp -= e_exp;
   else
    exp += e_exp;
   STORE_PTR;
  }
 }
#if STRTOD
 if (!fl_val) /* atof will already have returned, but strtod had */
  return (fl_val); /* to find the end of the exponent first */
#endif
 /*
  * The following computation is done in two stages,
  * first accumulating powers of (10/8), then jamming powers of 8,
  * to avoid underflow in situations like the following (for
  * the DEC representation): 1.2345678901234567890e-37,
  * where exp would be about (-37 + -18) = -55, and the
  * value 10^(-55) can't be represented, but 1.25^(-55) can
  * be represented, and then 8^(-55) jammed via ldexp().
  */
 if (exp != 0) { /* apply exponent */
  register double *powptr = pow1_25, fl_exp = fl_val;

  if (*powptr == 0.0) { /* need to initialize table */
   *powptr = 1.25;
   for (; powptr < &pow1_25[POW1_25LEN - 1]; powptr++)
    powptr[1] = *powptr * *powptr;
   powptr = pow1_25;
  }
  if ((c = exp) < 0) {
   c = -c;
   fl_exp = 1.0;
  }
  if (c > DMAXEXP/2) /* outrageously large exponents */
   c = DMAXEXP/2; /* will be handled by ldexp */
  for ( ; ; powptr++) {
   /* binary representation of ints assumed; otherwise
    * replace (& 01) by (% 2) and (>>= 1) by (/= 2) */
   if (c & 01)
    fl_exp *= *powptr;
   if ((c >>= 1) == 0)
    break;
  }
  fl_val = ldexp(exp < 0 ? fl_val/fl_exp : fl_exp, 3 * exp);
 }
 return (neg_val ? -fl_val : fl_val); /* apply sign */
}
/* atoi.c  */

#define ATOI

#ifdef ATOI
typedef int TYPE;
#define NAME atoi
#else
typedef long TYPE;
#define NAME atol
#endif

TYPE
NAME(p)
register char *p;
{
 register TYPE n;
 register int c, neg = 0;

 if (!isdigit(c = *p)) {
  while (isspace(c))
   c = *++p;
  switch (c) {
  case '-':
   neg++;
  case '+': /* fall-through */
   c = *++p;
  }
  if (!isdigit(c))
   return (0);
 }
 for (n = '0' - c; isdigit(c = *++p); ) {
  n *= 10; /* two steps to avoid unnecessary overflow */
  n += '0' - c; /* accum neg to avoid surprises at MAX */
 }
 return (neg ? n : -n);
}

long atol (p)
char * p;
{
 return ((long) atoi(p));
}

/* strtod.c  */

/*
 * C library - ascii to floating (atof) and string to double (strtod)
 *
 * This version compiles both atof and strtod depending on the value
 * of STRTOD, which is set in the file and may be overridden on the
 * "cc" command line.  The only difference is the storage of a pointer
 * to the character which terminated the conversion.
 * Long-integer accumulation is used, except on the PDP11, where
 * "long" arithmetic is simulated, so floating-point is much faster.
 */
#undef STRTOD
#undef STORE_PTR
#undef GOT_DIGIT
#undef RET_ZERO
#define STRTOD 1

extern double ldexp();

#if u3b || u3b5
# define POW1_25LEN 9
#else
# define POW1_25LEN 6
#endif
static double ppow1_25[POW1_25LEN] = { 0.0 };

#if STRTOD
#define STORE_PTR (*ptr = p)
#define GOT_DIGIT (got_digit++)
#define RET_ZERO(val) if (!got_digit) return (0.0)

double
strtod(p, ptr)
register char *p;
char **ptr;
#else
#define STORE_PTR
#define GOT_DIGIT
#define RET_ZERO(val) if (!val) return (0.0)

double
atof(p)
register char *p;
#endif
{
 register int c;
 int exp = 0, neg_val = 0;
 double fl_val;
#if STRTOD
 int got_digit = 0;
 char *dummy;
 if (ptr == (char **)0)
  ptr = &dummy; /* harmless dumping place */
 STORE_PTR;
#endif
 while (isspace(c = *p)) /* eat leading white space */
  p++;
 switch (c) { /* process sign */
 case '-':
  neg_val++;
 case '+': /* fall-through */
  p++;
 }
 { /* accumulate value */
#if pdp11
 /* "long" arithmetic on the PDP-11 is simulated using int's and
  * is outrageously slow, so the accumulation is done using double's */
  register int decpt = 0;

  fl_val = 0.0;
  while (isdigit(c = *p++) || c == '.' && !decpt++) {
   if (c == '.')
    continue;
   GOT_DIGIT;
   exp -= decpt; /* decr exponent if dec point seen */
   if (fl_val < 2.0 * MAXPOWTWO)
    fl_val = 10.0 * fl_val + (double)(c - '0');
   else
    exp++;
  }
  RET_ZERO(fl_val);
#else
  register long high = 0, low = 0, scale = 1;
  register int decpt = 0, nzeroes = 0;

  while (isdigit(c = *p++) || c == '.' && !decpt++) {
   if (c == '.')
    continue;
   GOT_DIGIT;
   if (decpt) { /* handle trailing zeroes specially */
    if (c == '0') { /* ignore zero for now */
     nzeroes++;
     continue;
    }
    while (nzeroes > 0) { /* put zeroes back in */
     exp--;
     if (high < MAXLONG/10) {
      high *= 10;
     } else if (scale < MAXLONG/10) {
      scale *= 10;
      low *= 10;
     } else
      exp++;
     nzeroes--;
    }
    exp--; /* decr exponent if decimal pt. seen */
   }
   if (high < MAXLONG/10) {
    high *= 10;
    high += c - '0';
   } else if (scale < MAXLONG/10) {
    scale *= 10;
    low *= 10;
    low += c - '0';
   } else
    exp++;
  }
  RET_ZERO(high);
  fl_val = (double)high;
  if (scale > 1)
   fl_val = (double)scale * fl_val + (double)low;
#endif
 }
 STORE_PTR; /* in case there is no legitimate exponent */
 if (c == 'E' || c == 'e') { /* accumulate exponent */
  register int e_exp = 0, neg_exp = 0;

  switch (*p) { /* process sign */
  case '-':
   neg_exp++;
  case '+': /* fall-through */
  case ' ': /* many FORTRAN environments generate this! */
   p++;
  }
  if (isdigit(c = *p)) { /* found a legitimate exponent */
   do {
    /* limit outrageously large exponents */
    if (e_exp < DMAXEXP)
     e_exp = 10 * e_exp + c - '0';
   } while (isdigit(c = *++p));
   if (neg_exp)
    exp -= e_exp;
   else
    exp += e_exp;
   STORE_PTR;
  }
 }
#if STRTOD
 if (!fl_val) /* atof will already have returned, but strtod had */
  return (fl_val); /* to find the end of the exponent first */
#endif
 /*
  * The following computation is done in two stages,
  * first accumulating powers of (10/8), then jamming powers of 8,
  * to avoid underflow in situations like the following (for
  * the DEC representation): 1.2345678901234567890e-37,
  * where exp would be about (-37 + -18) = -55, and the
  * value 10^(-55) can't be represented, but 1.25^(-55) can
  * be represented, and then 8^(-55) jammed via ldexp().
  */
 if (exp != 0) { /* apply exponent */
  register double *powptr = ppow1_25, fl_exp = fl_val;

  if (*powptr == 0.0) { /* need to initialize table */
   *powptr = 1.25;
   for (; powptr < &ppow1_25[POW1_25LEN - 1]; powptr++)
    powptr[1] = *powptr * *powptr;
   powptr = ppow1_25;
  }
  if ((c = exp) < 0) {
   c = -c;
   fl_exp = 1.0;
  }
  if (c > DMAXEXP/2) /* outrageously large exponents */
   c = DMAXEXP/2; /* will be handled by ldexp */
  for ( ; ; powptr++) {
   /* binary representation of ints assumed; otherwise
    * replace (& 01) by (% 2) and (>>= 1) by (/= 2) */
   if (c & 01)
    fl_exp *= *powptr;
   if ((c >>= 1) == 0)
    break;
  }
  fl_val = ldexp(exp < 0 ? fl_val/fl_exp : fl_exp, 3 * exp);
 }
 return (neg_val ? -fl_val : fl_val); /* apply sign */
}

#undef STRTOD
#define STRTOD 0

/* strtol.c  */

#define DIGIT(x) (isdigit(x) ? (x) - '0' : \
   islower(x) ? (x) + 10 - 'a' : (x) + 10 - 'A')
#define MBASE ('z' - 'a' + 1 + 10)

long
strtol(sstr, ptr, base)
register char *sstr;
char **ptr;
register int base;
{
 register long val;
 register int c;
 int xx, neg = 0;

 if (ptr != (char **)0)
  *ptr = sstr; /* in case no number is formed */
 if (base < 0 || base > MBASE)
  return (0); /* base is invalid -- should be a fatal error */
 if (!isalnum(c = *sstr)) {
  while (isspace(c))
   c = *++sstr;
  switch (c) {
  case '-':
   neg++;
  case '+': /* fall-through */
   c = *++sstr;
  }
 }
 if (base == 0)
  if (c != '0')
   base = 10;
  else if (sstr[1] == 'x' || sstr[1] == 'X')
   base = 16;
  else
   base = 8;
 /*
  * for any base > 10, the digits incrementally following
  * 9 are assumed to be "abc...z" or "ABC...Z"
  */
 if (!isalnum(c) || (xx = DIGIT(c)) >= base)
  return (0); /* no number formed */
 if (base == 16 && c == '0' && isxdigit(sstr[2]) &&
     (sstr[1] == 'x' || sstr[1] == 'X'))
  c = *(sstr += 2); /* skip over leading "0x" or "0X" */
 for (val = -DIGIT(c); isalnum(c = *++sstr) && (xx = DIGIT(c)) < base; )
  /* accumulate neg avoids surprises near MAXLONG */
  val = base * val - xx;
 if (ptr != (char **)0)
  *ptr = sstr;
 return (neg ? val : -val);
}

/* errlst.c  */

char *sys_errlist[] = {
 "Error 0",
 "Not owner",
 "No such file or directory",
 "No such process",
 "Interrupted system call",
 "I/O error",
 "No such device or address",
 "Arg list too long",
 "Exec format error",
 "Bad file number",
 "No child processes",
 "No more processes",
 "Not enough space",
 "Permission denied",
 "Bad address",
 "Block device required",
 "Device busy",
 "File exists",
 "Cross-device link",
 "No such device",
 "Not a directory",
 "Is a directory",
 "Invalid argument",
 "File table overflow",
 "Too many open files",
 "Not a typewriter",
 "Text file busy",
 "File too large",
 "No space left on device",
 "Illegal seek",
 "Read-only file system",
 "Too many links",
 "Broken pipe",
 "Argument out of domain",
 "Result too large",
 "No message of desired type",
 "Identifier removed",
 "Channel number out of range",
 "Level 2 not synchronized",
 "Level 3 halted",
 "Level 3 reset",
 "Link number out of range",
 "Protocol driver not attached",
 "No CSI structure available",
 "Level 2 halted",
};
int sys_nerr = { sizeof(sys_errlist)/sizeof(sys_errlist[0]) };

int errno;

/* SS. 09.Jun.87 some additional library functions: */

/* @(#)qsort.c 1.4 */
/* 3.0 SID # 1.2 */
/*LINTLIBRARY*/

#ifndef kronos
#ifdef vax         /* number is determined experimentally on vax-11/780 */
#define MINCPY 24 /* minimum number of characters worth using memcpy for */
#else              /* number is determined experimentally on 3b20s */
#define MINCPY 8  /* minimum number of characters worth using memcpy for */
#endif
#define NULL 0
#define CPY(i, j) ((void) memcpy(i, j, n))
extern char *malloc(), *realloc(), *memcpy();
static char *qsbuf = NULL;
#endif

static   qses, (*qscmp)();

void
qsort(a, n, es, fc)
char *a;  /* base of data */
unsigned n, es;   /* # of items to sort, width of an element */
int (*fc)(); /* key comparison function */
{
 void qs1();

#ifndef kronos
 {
  static unsigned qsbufsize;

  if (es >= MINCPY)
   if (qsbuf == NULL)
    qsbuf = malloc(qsbufsize = es);
   else if (qsbufsize < es)
    qsbuf = realloc(qsbuf, qsbufsize = es);
 }
#endif
 qscmp = fc;
 qses = es;
 qs1(a, a+n*es);
}

static void
qs1(a, l)
char *a, *l;
{
 register char *i, *j;
 register int es;
 char *lp, *hp;
 int c;
 void qsexc(), qstexc();
 unsigned n;

 es = qses;
start:
 if((n=l-a) <= es)
  return;
 n = es * (n / (2*es));
 hp = lp = a+n;
 i = a;
 j = l-es;
 while(1) {
  if(i < lp) {
   if((c = (*qscmp)(i, lp)) == 0) {
    qsexc(i, lp -= es);
    continue;
   }
   if(c < 0) {
    i += es;
    continue;
   }
  }

loop:
  if(j > hp) {
   if((c = (*qscmp)(hp, j)) == 0) {
    qsexc(hp += es, j);
    goto loop;
   }
   if(c > 0) {
    if(i == lp) {
     qstexc(i, hp += es, j);
     i = lp += es;
     goto loop;
    }
    qsexc(i, j);
    j -= es;
    i += es;
    continue;
   }
   j -= es;
   goto loop;
  }

  if(i == lp) {
   if(lp-a >= l-hp) {
    qs1(hp+es, l);
    l = lp;
   } else {
    qs1(a, lp);
    a = hp+es;
   }
   goto start;
  }

  qstexc(j, lp -= es, i);
  j = hp -= es;
 }
}

static void
qsexc(ri, rj)
register char *ri, *rj;
{
 register int n = qses;

#ifndef kronos
 if (n >= MINCPY && qsbuf != NULL) {
  CPY(qsbuf, ri);
  CPY(ri, rj);
  CPY(rj, qsbuf);
  return;
 }
#endif
 do {
  register char c = *ri;
  *ri++ = *rj;
  *rj++ = c;
 } while(--n);
}

static void
qstexc(ri, rj, rk)
register char *ri, *rj, *rk;
{
 register int n = qses;

#ifndef kronos
 if (n >= MINCPY && qsbuf != NULL) {
  CPY(qsbuf, ri);
  CPY(ri, rk);
  CPY(rk, rj);
  CPY(rj, qsbuf);
  return;
 }
#endif
 do {
  register char c = *ri;
  *ri++ = *rk;
  *rk++ = *rj;
  *rj++ = c;
 } while(--n);
}
/* @(#)bsearch.c 1.5 */
/*
 * Binary search algorithm, generalized from Knuth (6.2.1) Algorithm B.
 */

typedef char *POINTER;

POINTER
bsearch(key, base, nel, width, compar)
POINTER  key;    /* Key to be located */
POINTER  base;    /* Beginning of table */
unsigned nel;    /* Number of elements in the table */
unsigned width;    /* Width of an element (bytes) */
int (*compar)();  /* Comparison function */
{
 int two_width = width + width;
 POINTER last = base + width * (nel - 1); /* Last element in table */

 while (last >= base) {

  register POINTER p = base + width * ((last - base)/two_width);
  register int result = (*compar)(key, p);

  if (result == 0)
   return (p); /* Key found */
  if (result < 0)
   last = p - width;
  else
   base = p + width;
 }
 return ((POINTER) 0);   /* Key not found */
}

/* @(#)lsearch.c 1.8 */
/*
 * Linear search algorithm, generalized from Knuth (6.1) Algorithm Q.
 *
 * This version no longer has anything to do with Knuth's Algorithm Q,
 * which first copies the new element into the table, then looks for it.
 * The assumption there was that the cost of checking for the end of the
 * table before each comparison outweighed the cost of the comparison, which
 * isn't true when an arbitrary comparison function must be called and when the
 * copy itself takes a significant number of cycles.
 * Actually, it has now reverted to Algorithm S, which is "simpler."
 */

extern POINTER memcpy();

POINTER
lsearch(key, base, nelp, width, compar)
register POINTER key;  /* Key to be located */
register POINTER base;   /* Beginning of table */
unsigned *nelp;    /* Pointer to current table size */
register unsigned width; /* Width of an element (bytes) */
int (*compar)();  /* Comparison function */
{
 register POINTER next = base + *nelp * width; /* End of table */

 for ( ; base < next; base += width)
  if ((*compar)(key, base) == 0)
   return (base); /* Key found */
 ++*nelp;   /* Not found, add to table */
 return (memcpy(base, key, (int)width)); /* base now == next */
}

/* the rest of these functions are not often used */

#define MHZ 3 /* May be too much for Kronos ? */

unsigned
sleep(n)  unsigned n; {  /* n = xths of seconds on Z80 with speed of MHZ */
 
 register unsigned i;

 for( ; --n; )
  for(i = MHZ * 0x900; --i; )
   ;
}

char
peek(a) char *a; {

 return *a;
}

void
poke(a,c) char *a, c; {

 *a = c;
}


void
disable_intr(intr){ /* disable kronos interrupt number intr(hex) */

 modula void _setipt();  
 _setipt(intr,0); 
}


void
enable_intr(intr){ /* enable kronos interrupt number intr(hex) */

 modula void _setipt();  
 _setipt(intr,1); 
}


int
ioctl(fildes,request,arg) /* control device */
int fildes,request;
char *arg;  /* pointer to return area structure */
{
 modula int _iocntrl();
 return(_iocntrl(fildes,request,arg));
}

int
chdir(pathname) char *pathname; { /* change working directory */

 modula int _chdir();
 return(_chdir(pathname));
}

int
mkdir(pathname) char *pathname; { /* make new directory */

 modula int _mkdir();
 return(_mkdir(pathname));
}

int
rmdir(pathname) char *pathname; { /* remove working directory */

 modula int _rmdir();
 return(_rmdir(pathname));
}


/* @(#)getenv.c  1.2 */
/*
 * getenv(name)
 * returns ptr to value associated with name, if any, else NULL
 */
#define NULL 0
extern char **environ; 
static char *nvmatch();

char *
getenv(ename)  /* SS. 18.Oct.87 name -> ename */
register char *ename;
{
 register char *v, **p=environ;

 if(p == NULL)
  return(NULL);
 while(*p != NULL)
  if((v = nvmatch(ename, *p++)) != NULL)
   return(v);
 return(NULL);
}

/*
 * s1 is either name, or name=value
 * s2 is name=value
 * if names match, return value of s2, else NULL
 * used for environment searching: see getenv
 */

static char *
nvmatch(s1, s2)
register char *s1, *s2;
{
 while(*s1 == *s2++)
  if(*s1++ == '=')
   return(s2);
 if(*s1 == '\0' && *(s2-1) == '=')
  return(s2);
 return(NULL);
}

/* @(#)putenv.c  1.2 */
/* putenv - change environment variables

 input - char *change = a pointer to a string of the form
          "name=value"

 output - 0, if successful
   1, otherwise
*/
#define NULL 0
extern char **environ;    /* pointer to enviroment */
static reall = 0;  /* flag to reallocate space, if putenv is called more than once */  

int
putenv(change)
char *change;
{
 char **newenv;       /* points to new environment */
 register int which;     /* index of variable to replace */
 char *realloc(), *malloc(); /* memory alloc routines */

 if ((which = find(change)) < 0)  {
  /* if a new variable */
  /* which is negative of table size, so invert and
     count new element */
  which = (-which) + 1;
  if (reall)  {
   /* we have expanded environ before */
   newenv = (char **)realloc(environ,
      which*sizeof(char *));
   if (newenv == NULL)  return -1;
   /* now that we have space, change environ */
   environ = newenv;
  } else {
   /* environ points to the original space */
   reall++;
   newenv = (char **)malloc(which*sizeof(char *));
   if (newenv == NULL)  return -1;
   (void)memcpy((char *)newenv, (char *)environ,
     (int)(which*sizeof(char *)));
   environ = newenv;
  }
  environ[which-2] = change;
  environ[which-1] = NULL;
 }  else  {
  /* we are replacing an old variable */
  environ[which] = change;
 }
 return 0;
}

/* find - find where s2 is in environ
 *
 * input - str = string of form name=value
 *
 * output - index of name in environ that matches "name"
 *   -size of table, if none exists
*/
static
find(fstr)
register char *fstr;
{
 register int ct = 0; /* index into environ */

 while(environ[ct] != NULL)   {
  if (match(environ[ct], fstr)  != 0)
   return ct;
  ct++;
 }
 return -(++ct);
}
/*
 * s1 is either name, or name=value
 * s2 is name=value
 * if names match, return value of 1,
 * else return 0
 */

static
match(s1, s2)
register char *s1, *s2;
{
 while(*s1 == *s2++)  {
  if (*s1 == '=')
   return 1;
  s1++;
 }
 return 0;
}

/* @(#)getopt.c  1.5 */
/* 3.0 SID # 1.2 */
#define NULL 0
#define EOF (-1)
#define ERR(s, c) if(opterr){\
 extern int strlen(), write();\
 char errbuf[2];\
 errbuf[0] = c; errbuf[1] = '\n';\
 (void) write(2, argv[0], (unsigned)strlen(argv[0]));\
 (void) write(2, s, (unsigned)strlen(s));\
 (void) write(2, errbuf, 2);}

extern int strcmp();
extern char *strchr();

int opterr = 1;
int optind = 1;
int optopt;
char *optarg;

int
getopt(argc, argv, opts)
int argc;
char **argv, *opts;
{
 static int sp = 1;
 register int c;
 register char *cp;

 if(sp == 1)
  if(optind >= argc ||
     argv[optind][0] != '-' || argv[optind][1] == '\0')
   return(EOF);
  else if(strcmp(argv[optind], "--") == NULL) {
   optind++;
   return(EOF);
  }
 optopt = c = argv[optind][sp];
 if(c == ':' || (cp=strchr(opts, c)) == NULL) {
  ERR(": illegal option -- ", c);
  if(argv[optind][++sp] == '\0') {
   optind++;
   sp = 1;
  }
  return('?');
 }
 if(*++cp == ':') {
  if(argv[optind][sp+1] != '\0')
   optarg = &argv[optind++][sp+1];
  else if(++optind >= argc) {
   ERR(": option requires an argument -- ", c);
   sp = 1;
   return('?');
  } else
   optarg = argv[optind++];
  sp = 1;
 } else {
  if(argv[optind][++sp] == '\0') {
   sp = 1;
   optind++;
  }
  optarg = NULL;
 }
 return(c);
}


static long randx = 1;

void
srand(seed) 
unsigned seed; {

  randx = seed;
}

int
rand() {

 return (randx = (randx * 32719 + 3) % 32749);
}
