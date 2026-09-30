
/*
 *  K R O N O S   Link  Editor    06-Feb-87
 *
 *  13.Feb.87  names allocated from pool; maxhash & pool_blk from
 *        command line
 *  18.Feb.87  calloc for tab (hash-table must be set to zero)
 *  23.Feb.87  NOLINK (files not to link for multiple linking)
 *  26.Feb.87  mullnk always (for all files not marked as NOLINK)
 *  24.Mar.87  some message texts changed
 *  31.Mar.87  /usr1/kronos/clib & c_run by default; nolib; times
 *  03.Apr.87  code modifications in _main (-h & -b)
 *  05.Apr.87  object file header changed (time now 2., 2 extra words)
 *  09.Apr.87  -o, libraries
 *  10.Apr.87  /usr1/kronos/clib.lib; no pool block option
 *  15.Apr.87  stderr -> stdout for non-error messages 
 *  22.Apr.87  -q for checking only
 *  05.May.87  -m (no messages for multiply defined library symbols by
 *     default)
 *  06.May.87  object modules listed explicitly are ignored if found in
 *        libraries
 * NEWLINK
 *  24.Sep.87  void functions marked in object files
 *  08.Oct.87  work with Kronos2 object files also; symbols not referred to
 *  18.Oct.87  C_HOME_DIR for Kronos; -h & -b limits
 *  20.Oct.87  getenv() for homedir; help
 *  09.Nov.87  new time (Kronos)
 *  20.Nov.87  fixed bug related to getenv(), returns NULL pointer
 *  11.Dec.87  message if modbyte modification unsuccessful
 *  23.Dec.87  MAXHASH 1013 -> 2011
 *
 *  05.Feb.88   New Version: LPC logic changed; new object file versions
 *        (Asm.h)
 *  12.Feb.88  object file structure changed: pool now before symbols
 */

#include <stdio.h>
#include <ctype.h>
#include <fcntl.h>
#include "asm.h"
#include "mkinstr.h"

#define  MAXHASH 2011
#define  MAXFILE 100
#define  MAXLIB  10

#define STEP 4

#define POOL_BLK 1024

#define  S_VAR 010
#define  S_FUN 020
#define  S_FLAG  030
#define S_VOID 040

/* Default values for _main's getargs parameters */

#define  DEF_HEAP 20
#define  DEF_FILES 4

/*  hash table for external symbols of all files */

typedef struct {
 char * name;
 int fno;
 int num;
 int flag;
 }
  TAB;

TAB * tab;
int maxhash; /* actual size of tab - 1; NB! should be 
      a prime number */
int tabnr;

/*  table of files */

struct {
 char * name;
 int num;
 int lib;
 }
  files [MAXFILE + 1];

int filenr, nolink, nolib, libmsg, libr, kron2 = -1;

/* table of imported symbols of one file */

TAB * imptab [EXTVAR + EXTFUN];

int imptno;

typedef  struct {
 char name [MAXIDENT + 1];
 int num;
 }
  EXPTSYM;

typedef  struct  {
 char name [MAXIDENT + 1];
 int creat_time;
 int varno;
 int funno;
 int offset;
 }
  FILEREC;

int debug, verbose, pool_incr, onefile, quiet;

char tempname [80], * pool, * pool_end;
int pool_blk, bufnum, heapsize, modbyte;

void error(), getsymbols();
char * alloc (), * malloc(), * calloc(), * strrchr();

long mytime;
extern long time ();

static char *release = "Kronos C Link Editor [2.5]  /23-Feb-88/";

/*
 * ----- M A I N  -----
 */

main (argc, argv)
char * argv [];
{
int i, num;

 fprintf (stderr, "%s\n", release);
 maxhash  = MAXHASH;
 pool_blk = POOL_BLK;
 heapsize = DEF_HEAP;
 bufnum   = DEF_FILES;

 if (argc < 2) {

printf("\nUsage : link file [file].. [options]\n\n");
printf("-v  verbose\n");
printf("-f <file> input file (contains names of files)\n");
printf("-o <file> link only <file> ( => <file>.cod )\n");
printf("-h <num> set runtime heap to <num> Kb (default: %d Kb)\n",
   DEF_HEAP);
printf("-b <num> set runtime no. of disk file buffers to <num>\
\n  (default: %d buffers)\n", DEF_FILES);
printf("-t <num> symbol table size (default %d, must be a prime number)\n",
   MAXHASH);
printf("-l  do not use standard library clib.lib\n");
printf("-q  quiet mode (do not create code files)\n");
printf("-m  print messages about multiply defined library symbols\n");
printf("\nIf -l option is not used, LINK will search for clib.lib first in the");
printf("\ncurrent directory, then in the directory CDIR supplied by getenv().\n");
printf("\nLimits:\n");
printf("\tmax. no of files - 100\n");
printf("\tmax. no of external (undefined) symbols of one file - 1000\n");
exit (1);
   }

 /* set time */
 mytime = time ((long *)0);
#if unix
 mytime -= MAGICTIME;
#endif
 for (i = 1; i < argc; i ++) {
  if (argv[i][0] == '-') {
      switch (argv[i][1]) {
   case 'd':
#ifdef DEBUG
    debug ++;
#else
    fprintf(stderr, "No debugging\n");
#endif
    break;
   case 'v':
    verbose ++;
    break;
   case 'f':
    if (++i == argc)
      error("No input file after -f \n");
    inpfile (argv[i]);
    break;
   case 'o':
    if (++i == argc)
      error("No file name after -o\n");
    onefile = addfile (argv[i]);
    if (!onefile) error("Can't link %s\n", argv[i]);
    break;
   case 'h':
    if (++i == argc)
      error("Syntax error: -h\n");
    if (sscanf(argv[i], "%d", & num) != 1 ||
     num < 1 || num > 255)
      error("Illegal heap size: %s\n", argv[i]);
    heapsize = num;
    break;
   case 'b':
    if (++i == argc)
      error("Syntax error: -b\n");
    if (sscanf(argv[i], "%d", & num) != 1 ||
     num < 0 || num > 15)
      error("Illegal number of buffers: %s\n", 
         argv[i]);
    bufnum = num;
    break;
   case 't':
    if (++i == argc)
      error("Syntax error: -t\n");
    if (sscanf(argv[i], "%d", & num) != 1 ||
     num < 128)
      error("Illegal table size: %s\n", argv[i]);
    maxhash = num;
    break;
   case 'l':
    nolib = 1;
    break;
   case 'q':
    quiet = 1;
    break;
   case 'm':
    libmsg = 1;
    break;
   default:
    error ("Unknown flag : %s\n", argv[i]);
   }
      continue;
      }
  addfile (argv[i]);
  }
 if (filenr == 0 || filenr == nolink)
  error ("No files to link\n");
 if (! nolib) {
#if excII
  extern char * getenv();
  char fname[100], *penv;
#endif
  if (addlib("clib.lib")) {
#if unix
      if (addlib ("/usr1/kronos/clib.lib"))  
   error("No standard library clib.lib\n");
#endif
#if excII
      if (penv = getenv("CDIR")) { /* SS. 20.Nov.87 */
       strcpy (fname, penv);
       strcat (fname, "clib.lib");
   }
      if (!penv || addlib(fname))
   error("No standard library clib.lib\n");
#endif
      }
  }

 if ((tab = (TAB *)calloc((maxhash + 1), sizeof(TAB))) == (TAB *) 0)
  error("No memory for symbol table\n");

 tab[0].fno = MAXFILE;

#ifdef DEBUG
 if (debug)
  for (i = 0; i < filenr; i ++)
   printf("%d - %s num=%d nolink=%d\n", i,
    files[i].name, files[i].num, files[i].lib);
#endif
 libr = 0;
 getsymbols ();
 libr = 1;
 getlibraries ();

#ifdef DEBUG
 if (debug) 
  for (i = 0; i <= maxhash; i ++)
     if (tab[i].name)
     printf("%d - %s fno=%d num=%d flag=%o\n", i,
       tab[i].name, tab[i].fno, tab[i].num, tab[i].flag);
#endif
 fprintf(stderr, "Symbol table usage : %d %%\n",
   tabnr * 100 / maxhash);

 /*  Link one file or all non-library files */
 if (!quiet && kron2 > 0)
  fprintf(stderr, "\nWARNING: Kronos 2 code files !\n");
 if (onefile)
  linkfile (onefile - 1);
 else {
  for (i = 0; i < filenr; i ++)
   if (! files[i].lib)
    linkfile (i);
  }

 printf ("\n");

 return (0);
}


void error (s, t)  /* fatal error: can't continue */  /* host-port: was static, conflicts with K&R fwd decl */
char * s, * t;
{
 fprintf (stderr, s, t);
 exit (1);
}


char * alloc (s)     /* allocate memory for strings (names) */
char * s;
{
char * t;
int l;

 l = strlen(s) + 1;
 t = pool;
 if ((pool += l) > pool_end) {
  if ((t = malloc(pool_blk)) == (char *)0)
   error ("No memory for string pool\n");
  pool_end = t + pool_blk;
  pool = t + l;
  pool_incr ++;
  }
 strcpy (t, s);
 return (t);
}

addlib (s)
char * s;
{
int fd, i, num, hd[3];

 /* check if this library listed already */
 for (i = 0; i < filenr; i ++)
  if (!strcmp(s, files[i].name))
   return(0);
 
 if ((fd = open (s, O_RDONLY)) == -1)
  return(1);
 read (fd, (char *) & hd[0], 12);
 close (fd);
 if (hd[0] != LIB_VERSION)
  error("Not a Kronos C library file: %s\n", s);
 num = hd[2];
 if (filenr + num >= MAXFILE)
  error("Too many files: %s\n", s);
 files[filenr].name = alloc (s);
 for (i = filenr; i < filenr + num; i ++)
  files[i].lib = num;
 filenr += num;
 nolink += num;
 return(0);
}


addfile (s) /* add a file to files' table */
char * s;
{
char * cp, *strchr();
int i;

 if (cp = strchr(s, '.')) {
  if (!strcmp(cp, SUF_LIB)){
   if (addlib (s)) error("No file: %s\n", s);
   return (0);
   }
  if (strcmp(cp, SUF_OBJ))
   error ("Illegal file name : %s\n", s);
  * cp = '\0';
  }
 /* check if file listed already */
 for (i = 0; i < filenr; i ++)
  if (!strcmp(s, files[i].name))
   return (i + 1);
 if (++ filenr == MAXFILE)
  error ("Too many files: %s\n", s);
 files[filenr - 1].name = alloc (s);
 files[filenr - 1].lib = 0;
 return (filenr);
}


inpfile (s)
char * s;
{
FILE * fp, * fopen();
int j, c;

 if ((fp = fopen(s, "r")) == (FILE *)0)
  error ("No file : %s\n", s);
 if ((c = getc(fp)) == EOF)
  error ("Empty file : %s\n", s);
 j = 0;
 while (c != EOF) {
  if ((isspace(c)) || c == ',') {
   if (j) {
    tempname[j] = '\0';
    addfile (tempname);
    j = 0;
    }
   }
  else tempname[j++] = c;
  c = getc (fp);
  }
 if (j) {
  tempname[j] = '\0';
  addfile (tempname);
  }
 fclose (fp);
}


static char * m_var = "\t(VAR)", * m_fun = "\t(FUN)";

static void err_muldef (index, flag, mod)
  /* 'Multiple Definition' error */
{TAB * t;
static int msg0 = 0;

 if (libr && ! libmsg) /* no messages for library symbols */
  return;
 if (!msg0) {
  msg0 = 1;
  printf ("*** Multiple definition :\n");
  }
 t = & tab[index];
 printf ("%s in files:\n", t->name);
 printf ("\t%s :", files[t->fno].name);
 printf (t->flag & S_VAR ? m_var : m_fun);
 printf ("\n\t%s :", files[mod].name);
 printf (flag & S_VAR ? m_var : m_fun);
 printf ("\n");
}

static int msgf = 0;

static void err_conflict (index)
  /* 'Function Type Conflict' error */
{TAB *t;

 if (!msgf) {
  msgf = 1;
  printf("\n");
  }
 t = & tab[index];
 printf("*** Conflict :  (FUN) %s defined in ", t->name);
 printf("%s as %s\n", files[t->fno].name,
   t->flag & S_VOID ? "void" : "non-void");
}

static void err_notfound (name, flag)
  /* 'Symbol Not Found' error */
char * name;
{

 if (!msgf) {
  msgf = 1;
  printf ("\n");
  }
 printf ("*** Not found: %s %s\n", flag & S_VAR ? m_var : m_fun, name);
}


int hash (s) /* compute hash value for string */
char * s;
{
int hashval = 0;

 while (*s)
  hashval += *s++;
 return (hashval % maxhash + 1);
  /* NB!  return 1..MAXHASH ! , entry #0 in tab is for 
          unresolved references */
}


int hashname (name, flag, mod) 
  
  /* enter or find a name in table tab.
     For incremental rehash algorithm, no. of entries in
     hash table (MAXHASH) must be a prime number. Actually,
     table contains MAXHASH + 1 entries, entry #0 is not
     used. */
char * name;
int flag, mod;
{
TAB * t;
int hashval, index, c;

 c = * name;
 hashval = index = hash (name);
 t = & tab [index];

 while (t->name) {
  if (c == *(t->name) && !strcmp (name, t->name)) {
   if (flag & INSERT) {
    err_muldef (index, flag, mod);
    return (0);
    }
   else if ((flag & S_FLAG) != (t->flag & S_FLAG)) {
   /* name exists, but has wrong type */
    err_notfound (name, flag);
    return (0);
    }
   if ((flag & S_VOID) != (t->flag & S_VOID))  
   err_conflict (index);
   t->flag |= USED;
   return (index);
   }
  if ((index += STEP) > maxhash)
   index -= maxhash;
  if (index == hashval) {  /* here we started ... */
   if (flag & INSERT)
    error ("Symbol table overflow\n");
   return (0);
   }
  t = & tab [index];
  }
 if (flag & INSERT) {
  tabnr ++;
  t->name = alloc (name);
  t->fno = mod;
  t->flag = flag & ~INSERT;
  return (index);
  }
 err_notfound (name, flag);
 return (0);
}

getlibraries ()   /* read export symbols of all library files */
{
int num, i, j, l, fd, not, flag;
FILEREC  mod[MAXLIB];
EXPTSYM  sym;
char * s = (char *) & sym;

 for (i = 0; i < filenr; i ++)
     if ((num = files[i].lib) > 0) {
  fd = open(files[i].name, O_RDONLY);
  lseek (fd, 12L, 0);
  read (fd, (char *) & mod[0], num * sizeof (FILEREC));
  for (j = 0; j < num; j ++) {
   not = 0;
   for (l = 0; l < filenr; l ++)
       if (files[l].name && !strcmp(files[l].name, mod[j].name)) {  /* host-port: unnamed lib-module slots leave name=NULL; glibc strcmp(NULL,..) segfaults */
    if (files[l].lib)
        error("Library module occurs twice: %s\n", 
       mod[j].name);
    not = 1;
    break;
    }
   files[i].name = alloc (mod[j].name);
   if (not) files[i].lib = -1;
   else {
       lseek (fd, (long)mod[j].offset, 0);

       /* read & insert export variables */
       for (l = 0; l < mod[j].varno; l ++) {
    read (fd, s, sizeof (EXPTSYM));
    tab[hashname(sym.name, INSERT|S_VAR, i)].num =
         sym.num;
    }
       /* read & insert export functions */
       for (l = 0; l < mod[j].funno; l ++) {
    read (fd, s, sizeof (EXPTSYM));
    if (sym.num < 0) {
     sym.num = - sym.num;
     flag = INSERT | S_FUN | S_VOID;
     }
    else flag = INSERT | S_FUN;
    tab[hashname(sym.name, flag, i)].num = 
         sym.num;
    }
       }
   i ++;
   }
  i --;  /* NB!  i points to first file after this library's 
      files already */
  close (fd);
  }
}

void getsymbols () /* read export symbols of all object files */
{
int i, j, fd, num, varno, funno, flag, header[LHDSIZE], poolsize;
EXPTSYM  sym;
char * s = (char *) & sym;

 for (i = 0; i < filenr; i ++) 
     if (!files[i].lib) {
 /* open file */
  strcpy (tempname, files[i].name);
  strcat (tempname, SUF_OBJ);
  if ((fd = open (tempname, O_RDONLY)) == -1)
   error ("No file: %s\n", tempname);
 /* read header, check version, extract necessary data */
  read (fd, (char *)&header[0], 4 * HDSIZE);
  num = header[0];
  if (num != OBJ2_VERSION && num != OBJ25_VERSION)
   error ("Not a Kronos C object file: %s\n", tempname);
  if (kron2 == -1) kron2 = num == OBJ2_VERSION;
  else if (kron2 != (num == OBJ2_VERSION))
   error  ("Version conflict: %s\n", tempname);
  varno    = header[2];
  funno    = header[3];
  poolsize = header[6];
 /* skip pool */
  lseek (fd, (long)(HDSIZE * 4 + poolsize * 4), 0);
 /* read & insert export variables */
  for (j = 0; j < varno; j ++) {
   read (fd, s, sizeof (EXPTSYM));
   tab[hashname(sym.name, INSERT|S_VAR, i)].num = sym.num;
   }
 /* read & insert export functions */
  for (j = 0; j < funno; j ++) {
   read (fd, s, sizeof (EXPTSYM));
   if (sym.num < 0) {
    sym.num = - sym.num;
    flag = INSERT | S_FUN | S_VOID;
    }
   else flag = INSERT | S_FUN;
   tab[hashname(sym.name, flag, i)].num = sym.num;
   }
 /* close file */
  close (fd);
  }
}

FILE * fpobj, * fpld, * fopen();


/* ---------- LINK  ------------*/


linkfile (n) /*  link file # n   */

{

int header [LHDSIZE], i, j, exptsize, imptvar, imptfun, poolsize, psize,
 procsize, codesize, funno, varno, stksize, extrsize, flag;
char id [MAXIDENT + 1], * s, buf[1024];
TAB * t;

 printf(quiet ? "\n...Checking %s" : "\n...Linking %s", files[n].name);
 fflush (stdout);

   /* open object file */
 strcpy (tempname, files[n].name);
 strcat (tempname, SUF_OBJ);
 fpobj = fopen (tempname, "r");
   /* read header */
 fread ((char *) & header[0], 4, HDSIZE, fpobj);
 exptsize = header[2] + header[3];
 imptvar  = header [4];
 imptfun  = header [5];
 poolsize = header [6];
 procsize = header [7];
 codesize = header [8];
 funno    = header [9];
 varno    = header [10];
 modbyte  = header [11];
 stksize  = header [12];
   /* skip pool and export symbols */
 fseek (fpobj, (long)(poolsize * 4 + exptsize * sizeof (EXPTSYM)), 1);
   /* prepare for new file being linked */
 for (i = 0; i < filenr; i ++)
  files[i].num = 0;
 msgf = 0;
   /* read import symbols & resolve external references; imptab[i] points to
      correspondent entry in tab */
 imptno = imptvar + imptfun;
 flag = S_VAR;
 for (i = 0; i < imptno; i ++) {
  fread (id, 1, MAXIDENT + 1, fpobj);
  if (i >= imptvar) {
   flag = S_FUN;
   if (id[MAXIDENT] == '\1') {
    id[MAXIDENT] = '\0';
    flag |= S_VOID;
    }
   }
  imptab[i] = t = & tab[hashname(id, FIND | flag, n)];
  files[t->fno].num ++;
  }
   /* check which files were actually needed & compute extrsize */
 files[n].num = 0;
 tab[0].fno = n;
 tab[0].num = 0; /* set unresolved references to zero in same module */
 for (i = 0, extrsize = 1; i < filenr; i ++)
  if (files[i].num)
   files[i].num = extrsize ++;
 if (! quiet) {
    /* create output (load) file */
  strcpy (tempname, files[n].name);
  strcat (tempname, SUF_LD);
  if ((fpld = fopen(tempname, "w+")) == (FILE *) 0)
   error ("\nCan't create code file\n");
    /* fill and write out new header */
  for (i = 0; i < LHDSIZE; i ++) header[i] = 0;
  header [0] = 0x02010002; /* host-port: 1991 dcf.code_rec vers {1}+cpu2, check_time OFF (was COD25 0x101) */
  header [1] = header [2] = mytime;
  header [3] = poolsize;
  header [4] = procsize + codesize;
  header [5] = header [6] = stksize;
  header [7] = varno + extrsize;
  header [8] = extrsize;
  header [9] = funno;
  header[12] = MOD0;
  fwrite ((char *) & header [0], 4, LHDSIZE, fpld);
    /* copy pool to output file */
  fseek(fpobj, (long)HDSIZE * 4, 0);
  psize = poolsize;
  while (psize > 256) {
   fread  (buf, 256, 4, fpobj);
   fwrite (buf, 256, 4, fpld);
   psize -= 256;
   }
  fread  (buf, psize, 4, fpobj);
  fwrite (buf, psize, 4, fpld);
 /* copy procedure table to output file */
  fseek(fpobj, (long)(exptsize * sizeof(EXPTSYM) + 
    (imptvar + imptfun)*(MAXIDENT + 1)), 1);
  fread  (buf, procsize, 4, fpobj);
  fwrite (buf, procsize, 4, fpld);
    /* insert right external references and write code */
  getcode (codesize);
  }
   /* write external symbols without pathnames */
 if (verbose) printf ("  ");
 for (i = filenr -1; i >= 0; i --) {
  if (files[i].num ) {
   s = strrchr (files[i].name, '/');
   if (s == (char *) 0) s = files[i].name;
   else s ++;
   if (verbose)
    printf (" %s", s);
   j = strlen(s) + 1;
   if (! quiet) {
    /* host-port: 1991 import list = [def_time][name]; def_time = -1 (0xffffffff)
       => loader skips the time check.  Write the 4 bytes explicitly via putc:
       fwrite(&local,4,1) mis-handles &local on native Kronos (wrote a real
       timestamp instead of -1 -> "time conflict" at load). */
    putc (0377, fpld); putc (0377, fpld); putc (0377, fpld); putc (0377, fpld);
    fwrite (s, 1, j, fpld);
    if (j % 4) {
     j = 4 - (j % 4);
     while (j--)
      putc (0, fpld);
     }
    }
   }
  }
    /* modify _main (if -b or -h) */
 if (modbyte && ! quiet)
  if (heapsize != DEF_HEAP || bufnum != DEF_FILES) {
   unsigned char word[4];
   modbyte += ((LHDSIZE + poolsize + procsize) * 4);
   fflush (fpld);
   lseek (fileno(fpld), modbyte, 0);
   read (fileno(fpld), word, 4);
   if (word[0] == LIB) {
    if (heapsize != DEF_HEAP) 
     word [1] = heapsize;
    if (bufnum != DEF_FILES) 
     word[2] = LI0 + bufnum;
    lseek (fileno(fpld), (long) (-4), 1);
    write (fileno(fpld), word, 4);
    }
   else printf("\nHELP!! Can't pass parameters: %d %02x\n",
      modbyte, word[0]);
   }
    /* close both files */
 fclose (fpobj);
 fclose (fpld);
}


/* The following table contains KRONOS instruction lengths. Instruction
   code is used as index into this table. 
   Instructions LEA, LEW, SEW, CX, LPC are marked as EXR - their 2-byte
   operands must be modified.
*/

#define  EXR 8

static int  instr[256] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
       1,2,4,0,1,1,1,EXR,2,2,1,1,2,2,1,1,
       1,1,EXR,1,0,0,0,0,0,0,0,0,0,0,0,0,
       1,1,EXR,1,0,0,0,0,0,0,0,0,0,0,0,0,  
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  
       0,0,0,0,0,0,1,1,3,3,2,0,2,0,1,1,
       0,0,2,0,1,0,0,0,0,1,0,0,EXR,1,0,1,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
       0,0,0,0,0,0,0,0,0,0,0,EXR,0,0,0,0,
       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};


/*  ---------  GETCODE  -------- */

getcode (codesize) /* read, modify and write function code */

int codesize;
{
int i, j, a, b, c, oper;

 codesize *= 4;
 i = 0;
 while (i < codesize) {

  c = getc (fpobj);
#if unix
  c &= 0377;
#endif
  i ++;
  putc (c, fpld);
  if (!(j = instr[c])) /* no operands */
   continue;
  if (j < EXR) {       /* copy operands as they are */
   i += j;
   while (j--) {
    c = getc (fpobj);
    putc (c, fpld);
    }
   continue;
   }
  /* operands must be modified */
  i += 2;
  a = getc (fpobj);
  b = getc (fpobj);
  oper = (b << 8) + a;
#ifdef DEBUG
  if (debug > 1) 
   printf("code byte %x operand (%d %d) %d\n", 
       c, a, b, oper);
#endif
  if (c == LPC) {
  /* LPC is special: if operand > 255, then it denotes 
     external procedure, otherwise local */
      if (oper < 256) {
   putc (0, fpld);
   putc (oper, fpld);
   continue;
   }
      oper -= 255; /* external procedure */
      }
  a = imptab[oper]->fno;
  b = files[a].num;
#ifdef DEBUG
  if (debug > 1) 
   printf("file no.%d (%d) item no.%d\n", a, b, 
       imptab[oper]->num);
#endif
  a = imptab[oper]->num;
  putc (b, fpld);
  putc (a, fpld);
  }
}
