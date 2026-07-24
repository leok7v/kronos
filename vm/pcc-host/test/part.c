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
