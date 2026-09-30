/*
 *  25.Jan.87 Optimizing flags
 *  02.Feb.87 Jump optimization always
 *  02.Feb.87 Version for new linker: LPC etc.
 *  09.Feb.87 statics of a function: GlobVar rewritten
 *  11.Feb.87 no more 0-procedure jump labels; bug in DLabel
 *  03.Mar.87 tmpfile() for temporary files
 *  04.Mar.87 an attempt to remove unnecessary STORE/LODFV pairs
 *  10.Mar.87 changes in main() (mk1)
 *  16.Mar.87 processing of labels changed (string -> int)
 *  19.Mar.87 -u (user program) => no otimization; statistics() removed
 *  25.Mar.87 first symbol of labels taken into account
 *  31.Mar.87 time in header; VERSION 2.1
 *  03.Apr.87 modbyte in object file
 *  04.Apr.87 BYTE macro
 *  05.Apr.87 object file header changed (time now 2., two extra words)
 *  23.Apr.87 ; along with %; long lines
 *  12.Jun.87 version for 2.5; SWAP
 *  28.Aug.87 debug version for SS
 *  17.Sep.87 new directives (.alb, .ale)
 *  18.Sep.87 multiple files, -o
 *  24.Sep.87 statics marked for processed modules
 * NEWASM
 *  24.Sep.87 .void & .vfn directives
 *  28.Sep.87 -k flag for Kronos 2
 *  15.Oct.87 bug in ExtFun (message was issued too often)
 *  20.Oct.87 tables for jump & data labels allocated from command line;
 * MAJOR REVISION:
 *        bug: module's statics (FILE1: .var pc; FILE2: .svr pc --
 *       this caused 'var declared twice' error); this applies to
 *       functions also => search in all global tables backwards
 *  21.Oct.87 dump & ifdef DEBUG
 *  23.Oct.87 fileev & fileef introduced
 *  09.Nov.87 fuss with stksize & lla 6; new (Kronos) time
 *  20.Nov.87 DEBUG1
 *  30.Dec.87 ENTR/ENTR optimization; li 0.0 -> li 0
 *  02.Jan.88 ENTR/ENTR rewritten
 *
 *  05.Feb.88 New Version:
 *   EXTFUN, EXTVAR - 1000
 *   ENTR/ENTR discarded temporarily
 *   LPC logic changed (no more EXTFUN dependancy)
 *   new object file versions (cf. Asm.h)
 *  12.Feb.88 pool directly to object file
 *  25.Feb.88 -o ../file went to pool ...
 *  01.Mar.89 SS new release & max number of files set to 25
*/

#include <stdio.h>
#include "Asm.h"

#ifdef DEBUG
#define  DEBUG1
#endif

#define  MAXFILE 25

char *files[MAXFILE];
int filenr;

STAB globvar [256], globfun [256], extvar [EXTVAR], extfun [EXTFUN];
int gv = 1, gf, ev, ef,
 filegv = 2,  /* index of file's 1st variable */
 filegf = 1;  /* index of file's 1st function */
int extv, extf, imptno,
 fileef = 1,  /* index of file's 1st external function */
 fileev = 1;  /* index of file's 1st external variable */

DLAB * dlabels;
int datlab = DATLAB;  /* dlabels size */
int dl,
 filedl = 1;  /* index of file's 1st data label */

JLAB * jlabels;
int jumplab = JUMPLAB;   /* jlabels size */
int jl;

FTAB funtab [256];
int maxfun;

int lineno, nerrors;

int label_let, label_num;

int loc;
int fsize, codesize;

int poolsize, stksize;

#ifdef DEBUG1
int debug;
#endif

int verbose, kron2, jump0, jumpfs, jumpjj, entr,
 stld0, stld1, pcc = 1;

int nmain, st;

int pc, pc0;

FILE * fptext, * fpcode, * fpinit, *fpobj, *fpallo, * fopen(), * tmpfile();

int funvar;

extern void werror(), uerror(), cerror();
void error ();

extern void Pass1(), Pass2();

char fname [80], ofname[80];
extern char *malloc(), *calloc();

static   char * release = "Kronos C Assembler [2.5]  /01-Mar-89/";


/* M A I N    P R O G R A M */


main (argc, argv)
int   argc;
char  * argv[];
{
int num;
char * cp, * strrchr();

 if (argc < 2) {

printf("%s\n", release);
#ifdef DEBUG1
printf("(with debug flag -x)\n");
#endif
printf("\nUsage : asm file [file]... [options]\n\n");
printf("-v  verbose (-v -v is still more) \n");
printf("-o <file>  output file name: <file>.o\n");
printf("-u  user program (don't do PCC-flavoured optimizing)\n");
printf("-k  produce Kronos 2 object files (default 2.5)\n");
printf("-d <num> allocate space for <num> data labels (default %d)\n",
   DATLAB);
printf("-j <num> allocate space for <num> jump labels (default %d)\n",
   JUMPLAB);
printf("\nASM can assemble up to 25 assembler files into one object file.\n");
printf("If -o option is not used, the object file is named after first\n");
printf("assembler file listed in command line.\n\nLimits:\n");
printf("\tmax. no of undefined variables - %d\n", EXTVAR);
printf("\tmax. no of undefined functions - %d\n", EXTFUN);
exit (1);
 }

 while (*++argv) {
  if (argv[0][0] == '-') {
   switch (argv[0][1]) {
    case 'u' : 
     pcc = 0;
     break;
    case 'k':
     kron2 = 1;
     break;
    case 'd':
     if (!(*++argv) || sscanf(argv[0],
      "%d", &num) != 1) 
         error("-d : number expected\n\n");
     if (num > 100) datlab = num;
     break;
    case 'j':
     if (!(*++argv) || sscanf(argv[0], 
      "%d", &num) != 1)
         error("-j : number expected\n\n");
     if (num > 100) jumplab = num;
     break;
#ifdef DEBUG1
    case 'x' :
     debug ++;
     break;
#endif
    case 'v' :
     verbose ++;
     break;
    case 'o' :
     if (*fname)
      error("Duplicate -o\n\n");
     if (!(*++argv))
      error("No output file name\n\n");
     if (cp = strrchr (argv[0], '.')) {
         if (strcmp (cp, ".o"))
      error ("Illegal output file name : %s\n\n", argv[0]);
         else strncpy (fname, argv[0],
      strlen(argv[0]) - 2);
         }
     else strcpy (fname, argv[0]);
     break;
    default :
     error ("Unknown flag : %s\n\n", argv[0]);
    }
   continue;
   }
  addfile(argv[0]);
  }
 if (filenr == 0)
  error ("No files to assemble\n\n");

 if (verbose)
  fprintf (stderr, "%s\n", release);

 if ((dlabels = (DLAB *) calloc(sizeof(DLAB), datlab)) == (DLAB *) 0)
  error("No memory for data labels\n\n");

 if ((jlabels = (JLAB *) calloc(sizeof(JLAB), jumplab)) == (JLAB *) 0)
  error("No memory for jump labels\n\n");

#ifndef DEBUG
 if ((fpcode = tmpfile()) == (FILE *) 0 ||
     (fpinit = tmpfile()) == (FILE *) 0 ||
     (fpallo = tmpfile()) == (FILE *) 0)
  error ("Can't create temporary file\n\n");
#else
 fpcode = fopen("tmp.1", "w+");
 fpinit = fopen("tmp.2", "w+");
 fpallo = fopen("tmp.3", "w+");
 if ((fpcode == (FILE *) 0) || (fpinit == (FILE *) 0) ||
     (fpallo == (FILE *) 0))
  error("Can't create temporary file\n");
#endif

 /* Create object file (pool will be written directly to it).
    If there was no -o flag, use the first name given */

 if (!(*fname)) strcpy (fname, files[0]);
 strcpy(ofname, fname);
 strcat(ofname, SUF_OBJ);

 if ((fpobj = fopen(ofname, "w")) == (FILE *) 0)
  error ("Can't create object file\n\n");

 Pass1 ();

 if (nerrors) {
  fclose (fpobj);
  unlink (ofname);
  error ("Assembly terminated.\n\n");
  }

 Pass2 ();

 if (verbose) {
  fprintf (stderr, "Code\t%d\tPool\t%d\tStack\t\t%d\n",
    codesize/4, poolsize/4, stksize);
  fprintf (stderr, "Functions %d\tGlobals\t%d\tExternals\t%d\n",
    gf, gv - 1, imptno);
  fprintf (stderr, "File size %d bytes\n", fsize);
  }
 if (verbose > 1 &&
       (jumpfs || jump0 || jumpjj || stld0 || stld1 || entr)) {
  fprintf(stderr, "** Optimizations :\n");
  if (jumpfs)
   fprintf(stderr, "\tShort jumps : %d\n", jumpfs);
  if (jump0)
   fprintf(stderr, "\tJumps to next instruction : %d\n",
    jump0);
  if (jumpjj)
   fprintf(stderr, "\tJumps to jumps : %d\n", jumpjj);
  if (stld0 || stld1) {
   fprintf(stderr, "\tStore/Load : ");
   if (stld0) fprintf(stderr,"short %d ", stld0);
   if (stld1) fprintf(stderr, "long %d ", stld1);
   fprintf(stderr, "\n");
   }
  if (entr)
   fprintf(stderr, "\tEntr/entr : %d\n", entr);
  num = jumpfs + 2*jump0 + 3*jumpjj + stld1 + 2*entr;
  num = num * 100 / (codesize + num);
  if (num)
   fprintf(stderr, "\tTotal %2d %% of code size\n", num);
  }

 fclose (fpinit);
 fclose (fpcode);
 fclose (fpallo);
 fclose (fpobj);

 exit (0);
}

addfile (s) /* add a file to files' table */
char * s;
{
char * cp, *strrchr();
int i;

 if (cp = strrchr(s, '.')) {
  if (strcmp(cp, ".a"))
   error ("Illegal file name : %s\n", s);
  * cp = '\0';
  }
 /* check if file listed already */
 for (i = 0; i < filenr; i ++)
  if (!strcmp(s, files[i]))
   error ("File occurs twice : %s\n", s);
 if (++ filenr == MAXFILE)
  error ("Too many files\n");
 files[filenr - 1] = malloc (strlen(s) + 3);
 if (!(files[filenr - 1]))
  error ("Not enough heap\n");
 strcpy(files[filenr - 1], s);
 return (filenr);
}

void error (s, t)
char * s, * t;
{
 fprintf (stderr, s, t);
 exit (1);
}


int GlobVar (s, flag)

/* Find & insert global variable in the table globvar. Filegv is the
   no. of 1st global of current file (funvar is the same for current function).
   gv - # of last variable in globvar.
*/
char * s;
int flag; /* FIND -- INSERT -- INSERT + STAT */
{
int i, found = 0;
STAB * st0, * st1;
unsigned char c;

 if (!s)
  return (0);
 c = *s;

 /* statics of a function must be distinguished from file's
    variables; statics of a file - from module's variables. 
    That's why we look in the table downwards : FIND will then 
    find the last occurrence of the variable (possibly 
    function's or file's static); INSERT will look only among
    function's or file's statics if we are inside a function or a
    file and a new static variable is inserted. */

 if (flag & STAT) {  /* INSERTing.. */
  if (loc & L_FUN)   /* inside a function,... */
   st0 = & globvar [funvar]; /* start from the function's
           first */
  else st0 = & globvar [filegv]; /* else look in the file */
  }
 else    st0 = & globvar [2]; /* for non-statics, search in the module */
 for (i = gv, st1 = & globvar [gv]; st1 >= st0; st1 --, i --) 
  if (c == st1->name[0] && !strcmp(st1->name, s)) {
   found = 1;
   break;
   }
 if (flag & INSERT) {
  if (found) {
   uerror ("variable declared twice");
   return (i);
   }
  if (++gv == 256)
   cerror ("too many global variables: limit 254");
  st1 = & globvar[gv];
  strcpy (st1->name, s);
  st1->flag = flag & ~INSERT;
  return (gv);
  }
 return (found ? i : 0);
}


int GlobFun (s, flag)

/* Find or insert global function in the table globfun.
   Filegf is the no. of first function of current file, gf - no. of
   last function in globfun.
 */
char * s;
int flag; /* FIND -- FIND + VOID -- INSERT -- INSERT + STAT --
            INSERT + VOID; note that STAT overrules VOID !! */
{
int i, found = 0;
STAB * st0, * st1;
unsigned char c;

 if (flag & INSERT) {
  if (loc & L_FUN)
   uerror ("function inside of function");
  else if (loc & L_INIT)
   uerror ("function inside initialization code");
  loc |= L_FUN;
  st = 0;
  funvar = gv + 1;
  pc = jl = 0;
  }
 if (!s)
  return (0);
 c = *s;
 if (flag & STAT) /* INSERTing.. */
  st0 = & globfun [filegf]; /* for statics, search only inside
          current file */
 else st0 = & globfun [1]; /* else search within all files */
 for (i = gf, st1 = & globfun[gf]; st1 >= st0; st1 --, i --) 
  if (c == st1->name[0] && !strcmp(st1->name, s)) {
   found = 1;
   break;
   }
 if (flag & INSERT) {
  if (found) {
   uerror ("function declared twice");
   return (i);
   }
  if (++gf == 256)
   cerror ("too many functions: limit 255");
  if (!strcmp (s, MAIN)) {
   if (nmain)
    uerror("duplicate main");
   nmain = gf;
   }
  st1 = & globfun[gf];
  strcpy (st1->name, s);
  st1->flag = flag & ~INSERT;
  return (gf);
  }
 else {  /* FIND: check void compatibility */
  if (found && !(st1->flag & STAT)) /* for statics, PCC has
       done the job */
   if ((flag & VOID) != (st1->flag & VOID)){
    char msg[100];
    sprintf(msg, st1->flag & VOID ? 
     "non-void call to void function %s" :
     "void call to non-void function %s",
      st1->name);
    uerror (msg);
    }
  }
 return (found ? i : 0);
}

int ExtVar (s)

/* Find (if not found, insert) external variable in the table extvar.
   We deal with current file's externals only (see ExtFun). Fileev is
   the no. of file's 1st external variable. */
char * s;
{
int i;
STAB * st0, * st1;
unsigned char c;

 if (!s)
  return (0);
 c = *s;
 st1 = & extvar[ev] + 1;
 for (i = fileev, st0 = & extvar[fileev]; st0 < st1; st0 ++, i ++) {
  if (c == st0->name[0] && !strcmp(st0->name, s))
    break;
  }
 if (i > ev) {
  if (++ev == EXTVAR)
   cerror ("too many external variables");
  strcpy (st1->name, s);
  st1->flag = 0;
  }
 return (i);
}

int ExtFun (s, flag)

/* Find (if not found, insert) external function in the table extfun.
   We deal with current file's externals only (fileef..ef), so that we
   won't mess up situations like this:
  File 1:  a();
  File 2:  a();
    static a(){}
*/
char * s;
int flag; /* VOID or none */
{
int i;
STAB * st0, * st1;
unsigned char c;

 if (!s)
  return (0);
 c = *s;
 st1 = & extfun[ef] + 1;
 for (i = fileef, st0 = & extfun[fileef]; st0 < st1; st0 ++, i ++) {
  if (c == st0->name[0] && !strcmp(st0->name, s))
    break;
  }
 if (i > ef) {
  if (++ef == EXTFUN)
   cerror ("too many external functions");
  strcpy (st1->name, s);
  st1->flag = flag;
  }
/* this check is not necessary, because we don't look at other files'
   externals
 else {  
  if (st0->flag >= 0  still remains external 
    && (flag & VOID) != (st0->flag & VOID)) {
   char msg[100];
   sprintf(msg, "void & non-void calls to function %s",
    st0->name);
   uerror(msg);
   }
  }
*/
 return (i);
}

int Label (flag)

/* Find or insert jump label in the table jlabels. Labels go 1 .. jl
   and beling to one function only; GlobFun(INSERT) sets jl = 0.
*/
int flag;  /* FIND or INSERT */
{
int i, found = 0;
JLAB * l0, * l1;

 if (!(loc & L_FUN)) {
  uerror("jump label outside of function");
  return (0);
  }
 l0 = jlabels + 1; /* Lower limit for search: entry #0 is not used */
 for (i = jl, l1 = jlabels + jl; l1 >= l0; l1 --, i --) 
  if (l1->num == label_num && l1->letter == label_let) {
   found = 1;
   break;
   }
 if ((flag & INSERT) || !found) {
  if (found) {
   if (jlabels[i].pc != -1) 
    uerror ("jump label declared twice");
   else jlabels[i].pc = pc;
   return (i);
   }
  if (++jl == jumplab)
   cerror ("jump table overflow");
  l1 = jlabels + jl;
  l1->num = label_num;
  l1->letter = label_let;
  l1->pc = (flag & INSERT) ? pc : -1;
  return (jl);
  }
 return (i);
}

int DLabel (flag)

/* Find or insert .dat, .equ or .int label in the table dlabels.
   Labels start at 1; those at filedl .. dl belong to current module.
   Fileend() sets filedl = dl + 1.
*/
int flag; /* FIND or INSERT; D_EQU or D_DAT (this for .int also) */
{
int i, found = 0;
DLAB * l0, * l1;

 l0 = dlabels + filedl;
 for (i = dl, l1 = dlabels + dl; l1 >= l0; l1 --, i --) 
  if (l1->num == label_num && l1->letter == label_let) {
   found = 1;
   break;
   }
 if ((flag & INSERT) || !found) {
  flag &= ~INSERT;
  if (found) {
   if (!(l1->flag & D_NVAL))
    uerror ("data label declared twice");
   else if (flag != (l1->flag & ~D_NVAL))
    uerror("data label conflict");
   return (i);
   }
  if (++dl == datlab)
   cerror ("data labels table overflow");
  l1 = dlabels + dl;
  l1->num = label_num;
  l1->val = 0;
  l1->letter = label_let;
  l1->flag = flag | D_NVAL;
  return (dl);
  }
 if (flag != (l1->flag & ~D_NVAL))
  uerror("data label conflict");
 return (i);
}

#ifdef DEBUG
dump()
{
int i;

 printf("\nGLOBAL VARIABLES: (filegv = %d)\n", filegv);
 for (i = 2; i <= gv; i ++){
  printf("%s \t%4x\t", globvar[i].name, globvar[i].flag);
  if (globvar[i].flag & STAT) printf("STATIC ");
  printf("\n");
  }
 printf("\nGLOBAL FUNCTIONS: (filegf = %d)\n", filegf);
 for (i = 1; i <= gf; i ++){
  printf("%s \t%4x\t", globfun[i].name, globfun[i].flag);
  if (globfun[i].flag & STAT) printf("STATIC ");
  if (globfun[i].flag & VOID) printf("VOID");
  printf("\n");
  }
 printf("\nEXTERNAL VARIABLES:\n");
 for (i = 1; i <= ev; i ++){
  printf("%s \t%4x\t", extvar[i].name, extvar[i].flag);
  printf("\n");
  }
 printf("\nEXTERNAL FUNCTIONS:\n");
 for (i = 1; i <= ef; i ++) {
  printf("%s \t%4x\t", extfun[i].name, extfun[i].flag);
  if (extfun[i].flag & VOID) printf(" VOID");
  printf("\n");
  }
 printf("\nDATA LABELS: (filedl = %d)\n", filedl);
 for (i = 1; i <= dl; i ++){
  printf("%c%d\t%4x\t", dlabels[i].letter, dlabels[i].num,
     dlabels[i].flag);
  if (dlabels[i].flag & D_NVAL) printf("NO VALUE ");
  else {
   if (dlabels[i].flag & D_DAT) printf("DATA ");
   else printf("EQU ");
   printf("val=%d", dlabels[i].val);
   }
  printf("\n");
  }
 printf("\nJUMP LABELS:\n");
 for (i = 1; i <= jl; i ++){
  printf("%c%d\t %d", jlabels[i].letter, jlabels[i].num,
     jlabels[i].pc);
  printf("\n");
  }
 exit (0); 
}
#endif
