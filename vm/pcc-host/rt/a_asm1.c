/*
 * 23.Jan.86 li -<const>  => li <const> / neg
 * 26.Jan.87 entr M%d
 * 29.Jan.87 pcop
 * 02.Feb.87 _main included
 * 02.Feb.87 version for new linker: LPC & header
 * 09.Feb.87 function's static variables -> $
 * 11.Feb.87 no more 0-procedure jump labels; A_EQU -> A_DAT
 *       .int
 * 12.Feb.87 ROR, ROL
 * 04.Mar.87 an attempt to remove unnecessary STORE/LODFV pairs
 * 10.Mar.87 _main : _stargs as modula, 3 extra parameters (Henn)
 * 11.Mar.87 call to _setup removed from _main
 * 18.Mar.87 STORE/LODFV optimization rewritten (stack)
 * 19.Mar.87 bug in STORE/LODFV optimization; 
 *      -u (user program => no optimization)
 * 20.Mar.87 STORE/LODFV optimization left out ( => SS)
 * 24.Mar.87 changes in _main
 * 25.Mar.87 labels processing changed (getlabel)
 * 27.Mar.87 Kronos FP numbers (KRONOS_BIT)
 * 01.Apr.87 li /neg only for |0..65535| (special)
 * 04.Apr.87 BYTE macro
 * 23.Apr.87 accept ; as comment separator (along with %); long lines (200)
 * 25.May.87 new instructions (unused by Kronos C)
 * 12.Jun.87 Kronos 2.5; SWAP
 * 15.Jun.87 some new 2.5 instructions
 * 18.Jun.87 bug in string processing
 * 28.Aug.87 debug option for SS
 * 17.Sep.87 new directives (.alb, .ale)
 * 18.Sep.87 multiple files
 * 24.Sep.87 statics of modules marked after processing
 * NEWASM
 * 24.Sep.87 .void & .vfn directives
 * 26.Sep.87 statics again: CheckExternals brought here (after each module)
 * 28.Sep.87 longer lines (500)
 * 30.Sep.87 .void without names
 * 20.Oct.87 jlabels, dlabels
 * 21.Oct.87 dump in cerror
 * 27.Oct.87 entr changes in PCC => changes in _main; SWAP for Kronos 2
 * 29.Oct.87 SS fixed bug in _main
 * 19.Nov.87 0-proc. li 0, if stksize == 0
 * 19.Nov.87 SS restored LLW4 for saving 0 in case of switching processes
 * 20.Nov.87 SS new macro for SWAP using LLW4; DEBUG1
 * 30.Dec.87 LI 0.0 -> li0
 *
 * 05.Feb.88  New Version (LPC et al., cf. asm.c; MAXLINE)
 * 12.Feb.88  pool directly to object file
 * 25.Feb.88  -o ../file error
 * 15.Apr.88  error messages at EOF now correct
 */

#include <stdio.h>
#include <ctype.h>
#include "asm.h"
#include "mkinstr.h"

#ifdef DEBUG
#define DEBUG1
#endif

#define MAXLINE   1000 /* max. length of asm line */

/* Assembler line types  */

#define  A_INSTR 1
#define  A_DIR 2
#define  A_LAB 3
#define  A_DAT 4
#define  A_EMPTY 5

/* Operand cookies  */

#define  O_VAR 01  /* variable name  */
#define  O_FUN 02  /* function name  */
#define  O_MOD 04  /* op.code has modifications  */
#define  O_NUM 010  /* number 0..255  */
#define  O_JLAB  020  /* jump label   */
#define  O_DLAB  040  /* data label   */
#define O_SPEC 0100  /* LI,ENTR: data label or number*/
#define  O_MACRO 0200  /* SHL2, SHR2   */
#define O_EXT 0400  /* LPC : external function */
#define  O_01 01000  /* FFCT : number 0 or 1  */

/* Macros  */

#define  MACRO1  putb(LI2); putb(SHL)
#define  MACRO2  putb(LI2); putb(SHR)
#define MACRO3  { putb(SLW4); putb(STOT); putb(LLW4); putb(LODT); }
#define MACRO4  putb(SWAP) 

extern STAB globvar [], globfun [], extvar [], extfun [];
extern int gv, gf, ev, ef, filegv, filegf, fileef, fileev;

extern DLAB * dlabels;
extern int dl, filedl;

extern JLAB * jlabels;
extern int jl;

extern FTAB funtab [];
extern int maxfun;

extern char * files[];
extern int filenr;

char line [MAXLINE], * lp;

extern int lineno, nerrors, hd[];

extern int label_let, label_num;

extern int loc, pc, pc0, funvar, st;

static int poolpc, pooldata;
extern int poolsize, stksize;

#ifdef DEBUG1
extern int debug;
#endif

extern int nmain, verbose, pcc, kron2;

extern int GlobVar(), GlobFun(), ExtVar(), ExtFun(), Label(), DLabel();

extern void error();

extern FILE * fptext, * fpcode, * fpinit, * fpobj, *fpallo;

static FILE * fp;
static int * pcpnt;
static int fno;

static int srcflag;

static int over;

extern char fname[], ofname[];

/* Functions */

 void Pass1 ();
static void where ();
 void werror ();
 void uerror ();
 void cerror ();
static char * getname ();
static int getnum ();
static char * getlabel();
static char * skipspace ();
static void putb ();
static void putd ();
static void put ();
static void putpool ();
static int poolreset ();
static void putli ();
static void special ();
static void procdata ();
static int scan ();
static void procline ();
static void p_dir ();
static void p_num ();
static void p_equ ();
static void p_dat ();
static int find ();
static void p_instr ();
static void ftnend ();
static void CheckExternals();
static void fileend ();
static void modend ();


/*
 *  P A S S   1
 *  ===========
 */

void  Pass1 ()

{
int i, n;
char *s, *strrchr();

 if (verbose > 1)
  fprintf (stderr, "Pass 1  ---  Code Generation\n");

 /* leave space for header in the object file */
 fwrite((char *)&hd[0], HDSIZE, sizeof(int), fpobj);

 /* put file name into the string pool (first item) */
 if (s = strrchr(fname, '/')) s++; else s = fname;
 n = strlen (s);
 for (i = 0; i < n; i ++)
  putpool (s[i]);
 putpool (0);

 /* insert function _zero into extfun[] */
 if (pcc) ExtFun ("_zero", VOID);

 /* process all files */
 for (fno = 0; fno < filenr; fno ++) {
  strcat (files[fno], ".a");
  if (verbose && filenr > 1)  {
   if (verbose > 1 && fno) fprintf(stderr, "\n");
   fprintf(stderr,"%s:\n", files[fno]);
   }
  if ((fptext = fopen(files[fno], "r")) == (FILE *) 0) {
   unlink (ofname);
   error ("No file : %s\n\n", files[fno]);
   }
  fp = fpcode;
  pcpnt = & pc;
  lineno = 0;
  srcflag = FIND;

  while (fgets(line, MAXLINE, fptext) != NULL) {
   lineno ++;
#ifdef DEBUG1
   if (debug > 1) 
    fprintf(stderr, "%3d : %s", lineno, line);
#endif
   procline ();
   }

  fileend ();
  fclose (fptext);
  }
 modend ();
#ifdef DEBUG
 if (debug)
  dump();
#endif
}


static void where ()
{
    if (over) 
 fprintf(stderr, "%s.o : ", fname);
    else {
 if (filenr > 1 && ! verbose) {
  if (fno == filenr) fno --;
  fprintf(stderr, "%s: ", files[fno]);
  }
 fprintf(stderr, "%3d : %s", lineno, line);
 }
}

void werror (s)
char * s;
{
 where();
 fprintf(stderr, "*** warning : %s\n", s);
}

void uerror (s)
char * s;
{
 if (++ nerrors > 10)
  cerror ("Can't continue. Bye!");
 where();
 fprintf (stderr, "*** error : %s\n", s);
}

void cerror (s)
char * s;
{
 where();
 fprintf (stderr, "\n*** fatal error : %s\n", s);
#ifdef DEBUG
 if (debug)
  dump();
#endif
 exit (1);
}


static char * getname (s)
char * s;
{
static char name [MAXIDENT+1];
int i = 0, c;

 s = skipspace (s);
 c = * s;
 if ((isalpha(c)) || c == '_')
  for (; i < MAXIDENT; i ++) {
   if (isspace(c)) 
    break;
   *(name + i) = c;
   c = *++s;
   }
 name[i] = '\0';
 if (!i)
  uerror ("identifier missing");
 return (i ? name : (char *)0);
}

static int getnum (s)
char * s;
{
int num = -1;

 s = skipspace (s);
 if (*s)
  if (sscanf (s, "%d", & num) && num >= 0 && num < 256)
   return (num);
 uerror ("number 0..255 required");
 return (-1);
}

static char * getlabel (s)
char * s;
{
int c, num = 0;

 s = skipspace (s);
 c = * s;
 if ((isalpha(c))) {
  label_let = c;
  while (isdigit(c = *++s))
   num = num * 10 + (c - '0');
  if (num) {
   label_num = num;
   return (s);
   }
  }
 uerror ("missing or incorrect label");
 return ((char *)0);
}

static char * skipspace (p)
char * p;
{
 while (isspace(*p)) p++;
 return (p);
}


static void putb (c)
int c;
{
 (* pcpnt) ++;
 putc (c, fp);
}

static void putd (c)
unsigned c;          /* Zenja & SS 01-Mar-89 */
{
 putb ((c >> 8) & 0377);
 putb ( c & 0377);
}

static void put (c)
unsigned c;          /* Zenja & SS 01-Mar-89 */
{
 putb (c & 0377);
 putb ((c >> 8) & 0377);
 putb ((c >> 16) & 0377);
 putb ((c >> 24) & 0377);
}

static void putpool (c)
int c;
{
 putc (c, fpobj);
 poolpc ++;
}

static int poolreset ()
{
int incr, new;

 if (poolpc % 4)  {
  new = (poolpc/4 + 1) * 4;
  incr = new - poolpc;
  while (incr--)
   putpool (0);
  poolpc = new;
  }
 return (poolpc);
}

static void putli (num)
{
int absnum, neg;

 /* Make  LI/NEG only for numbers |0..65535|; for greater abs.values
    neg is redundant (LIW 4bytes anyway).
    This also escapes the 
  li 0x80000000  -> li0 / neg
    error (big numeric values are better no to be modified...)
       1.Apr.87
 */
 absnum = abs (num);
 /* host-port: use the short LI/NEG form only for NON-NEGATIVE small values.
    On the 32-bit native asm, a mask like ~15 arrives as -16 -> LIB 16 NEG,
    but the 64-bit host asm sees it as 4294967280 -> LIW (full word).  The
    two encodings shifted the code and mis-aligned a call target -> the
    native-built pcc trapped `49h invalid instruction`.  Emitting LIW for any
    negative makes native codegen match the (working) host. */
 if (num >= 0 && num < 65536) {
  if (num < 16)
   putb (LI0 + num);
  else if (num < 256) {
   putb (LIB);
   putb (num);
   }
  else  {  /* num < 65536 */
   putb (LID);
   putb (num & 0377);
   putb ((num >> 8) & 0377);  /* low byte first ! */
   }
  }
 else {
  putb (LIW);
  put (num);
  }
}

static void special (code)
{
char c, *p;
int num;
union {
 float f;
 int i;
 }
  FP_buf;

 p = lp = skipspace (lp);
 if (p) {
  if (isalpha(*p)) {
   if (! getlabel(p)) return;
   num = DLabel (FIND | D_EQU);
   putb (0xFF);
   putb (code);
   putb (0);
   putd (num);
   return;
   }
  if (code == ENTR) {
   if ((num = getnum(lp)) == -1)
    return;
   putb (ENTR);
   putb (num);
   return;
   }
  while ((c = *p++) && c != ';')
   if (c == '.' || c == 'e' || c == 'E') {
    if (sscanf (lp, "%f", &FP_buf.f)) {
     if (FP_buf.f == 0.0)
      putb(LI0);
     else {
#if unix
      FP_buf.i += KRONOS_BIT;
#endif
      putb (LIW);
      put (FP_buf.i);
      }
     }
    else uerror("incorrect FP number");
    return;
    }
  if (sscanf (lp, "%d", &num)) {
   putli (num);
   return;
   }
  }
 uerror ("Data label or constant expected");
 return;
}

static void procdata()
{
int c, num, neg;

 for (;;) {
  lp = skipspace (lp);
  c = * lp ++;
  if (!c || c == '%' || c == ';')
   return;
  if (c == '-') {
   neg = 1;
   c = * lp ++;
   }
  else neg = 0;
  if (!(isdigit(c))) {
   pooldata = 0; /* stop data processing */
   uerror("integer expected");
   return;
   }
  num = c - '0';
  while (isdigit(c = *lp)) {
   num = num * 10 + c - '0';
   lp ++;
   }
  if (neg) num = - num;
  putw (num, fpobj);
  poolpc += 4;
  lp = skipspace (lp);
  c = * lp ++;
  if (!c || c == '%' || c == ';') {
   /* it's over */
   pooldata = 0;
   return;
   }
  if (c != ',') {
   pooldata = 0; /* stop data processing */
   uerror(" , expected between numbers");
   return;
   }
  }
}

static int scan()
{
int n, jump = 0;

 if (isalpha (*lp)) {
  if (!(lp = getlabel(lp))) return (A_EMPTY);
  if (*lp == ':') 
   jump = 1, lp ++;
  lp = skipspace (lp);
  if (jump) {
   if (*lp)
    uerror("jump label error");
   return (A_LAB);
   }
  return (A_DAT);
  }
 lp = skipspace (lp);
 if ((n = * lp) == '%' || n == ';')
  return (A_EMPTY);
 if (n == '.') {
  lp ++;
  return (A_DIR);
  }
 return (n ? A_INSTR : A_EMPTY);
}

static void procline()
{
int c;

 lp = line;
 if ((c = *lp) == '\n' || c == '%' || c == ';')
  return;
 if (pooldata)
  procdata();
 else switch (scan()) {
  case A_DIR:
   p_dir();
   break;
  case A_INSTR:
   p_instr();
   break;
  case A_LAB:
   Label (INSERT);
   break;
  case A_DAT:
   p_equ();
   break;
  }
}

static void p_dir ()
{int num;

 switch (*lp) {
  case 'v':
   if (!strncmp(lp, "var", 3)) {
    GlobVar(getname(lp+3), INSERT);
    return;
    }
   if (!strncmp(lp, "vfn", 3)) {
    GlobFun (getname(lp+3), INSERT | VOID);
    return;
    }
   if (!strncmp(lp, "void", 4)) {
    srcflag = FIND | VOID;
    return;
    }
   break;
  case 's':
   if (!strncmp (lp, "svr", 3)) {
    GlobVar(getname(lp+3), INSERT | STAT);
    return;
    }
   if (!strncmp (lp, "sfn", 3)) {
    GlobFun(getname(lp+3), INSERT | STAT);
    return;
    }
   if (!strncmp (lp, "stk", 3)) {
       if (!sscanf(lp+3, "%d", &num) || num < 0) 
    uerror ("illegal stack : number > 0");
       else stksize += num;
       return;
       }
   break;
  case 'f':
   if (!strncmp(lp, "fun", 3)) {
    GlobFun (getname(lp+3), INSERT);
    return;
    }
   break;
  case 'm':
   if (!strncmp(lp, "mod", 3))
    return;
   break;
  case 'd':
   if (!strncmp(lp, "dat", 3)) {
    uerror("label missing for .dat directive");
    return;
    }
   break;
  case 'e':
   if (!strncmp(lp, "equ", 3)) { 
    uerror ("label missing for .equ directive");
    return;
    }
   if (!strncmp(lp, "end", 3)) {
    if (loc & L_FUN)
     ftnend();
    else if (loc & L_INIT)
     werror("excessive .end ignored");
    return;
    }
   break;
  case 'i':
   if (!strncmp(lp, "inb", 3)) {
    fp = fpinit;
   initbegin:
    if (loc & L_INIT)
     uerror(".inb/.alb conflict");
    loc |= L_INIT;
    pcpnt = & pc0;
    return;
    }
   if (!strncmp(lp, "ine", 3)) {
   initend:
    if (! (loc & L_INIT))
     werror("unmatched .ine: ignored");
    loc &= ~L_INIT;
    fp = fpcode;
    pcpnt = & pc;
    return;
    }
   if (!strncmp(lp, "int", 3)) {
    uerror("label missing for .int directive");
    return;
    }
   break;
  case 'a':
   if (!strncmp(lp, "alb", 3)) {
    fp = fpallo;
    goto initbegin;
    }
   if (!strncmp(lp, "ale", 3))
    goto initend;
   break;
  }
 uerror ("illegal directive");
}

static void p_num (s)
char * s;
{
int val;

 s = skipspace (s);
 if (*s)
  uerror("data after .int directive ignored");
 val = DLabel (INSERT | D_DAT);
 dlabels[val].val = poolreset() / 4;
 dlabels[val].flag = D_DAT;
 pooldata = 1;
}

static void p_equ ()
{
int val;
DLAB *d;

 if (!strncmp(lp, ".dat", 4)) {
  p_dat (lp + 4);
  return;
  }
 if (!strncmp (lp, ".int", 4)) {
  p_num (lp + 4);
  return;
  }
 if (strncmp (lp, ".equ", 4)) {
  uerror (".equ, .dat or colon expected.. (?)");
  return;
  }
 lp += 4;
 lp = skipspace (lp);
 if (*lp && sscanf (lp, "%d", & val)) {
  d = dlabels + DLabel (INSERT | D_EQU);
  d->val = val;
  d->flag = D_EQU;
  return;
  }
 uerror ("integer expected");
}


static void p_dat (s)
char * s;
{
int addr, c, val;

 s = skipspace (s);
 if (*s != '"') {
  uerror ("String constant missing");
  return;
  }
 s ++;
 addr = poolreset() / 4;

 /*  Collect string */

 while ((c = *s) != '"') {
     switch (c) {
  case '\0': case '\n':
   uerror ("Incorrect string");
   return;
  case '\\':
      switch (c = *++s) {
   case '\0':
    continue;
   case '\n':
       if (fgets (line, MAXLINE, fptext) == NULL) {
     uerror ("Incorrect string");
     return;
     }
       ++ lineno;
       s = line;
       continue;
   case 'n':
    val = '\n';
    break;
   case 't':
    val = '\t';
    break;
   case '0': case '1': case '2': case '3':
   case '4': case '5': case '6': case '7':
    val = c - '0';
    if ((c = *++s) >= '0' && c < '8') {
     val = (val << 3) | (c - '0');
     if ((c = *++s) >= '0' && c < '8')
         val = (val << 3) | (c - '0');
     else s--;
     }
    else s--;
    break;
   default:
    val = c;
    break;
   }
   break;
  default:
   val = c;
   break;
  }
     putpool (val);
     s ++;
     }

 putpool (0);
 val = DLabel (INSERT | D_DAT);
 dlabels[val].val = addr;
 dlabels[val].flag = D_DAT;
}


/*
 *  The following table is used for code generation.
 *  op == 0 means that instruction has no operands; machine code is
 *  taken from code[0].
 *  op != 0 determines the type of operand and what is to be
 *  done with it. Code[] then contains machine code modifications.
 *  l is instruction's (text) length (for speed).
 *  
 *  Function find() performs binary search on this table.
 *  Code is generated by p_instr().
 */

struct instrtab {
  char * mnemo;  /* assembler instruction */
  int op;  /* operand cookies  */
  int l;  /* length of asm. instruction  */
  int code [3];  /* instruction codes  */
  };

typedef  struct  instrtab INSTR;


#define  BEG  0
#define  END  94

static INSTR instr [] = {
 
 "abs",   0, 3,  {ABS},
 "add",    0, 3, {ADD},
 "alloc", 0, 5,  {ALLOC},
 "and",   0, 3,  {AND},
 "bblt",  0, 4,  {BBLT},
 "bbp",   0, 3,  {BBP},
 "bbu",   0, 3,  {BBU},
 "bic",   0, 3,  {BIC},
 "bit",   0, 3,  {BIT},
 "cf",   0, 2, {CF},
 "comp",  0, 4,  {COMP},
 "copt",  0, 4,  {COPT},
 "cx",   O_FUN|O_MOD, 2, {CX, CL0, CL},
 "dec",   0, 3,  {DEC},
 "dec1",  0, 4,  {DEC1},
 "decs",  0, 4,  {DECS},
 "div",   0, 3,  {DIV},
 "drop",  0, 4,  {DROP},
 "entr",      O_SPEC, 4, {ENTR},
 "equ",   0, 3,  {EQU},
 "excl",  0, 4,  {EXCL},
 "fabs",  0, 4,  {FABS},
 "fadd",  0, 4,  {FADD},
 "fcmp",  0, 4,  {FCMP},
 "fdiv",  0, 4,  {FDIV},
 "ffct",   O_NUM|O_01, 4, {FFCT},
 "fmul",  0, 4,  {FMUL},
 "fneg",  0, 4,  {FNEG},
 "fsub",  0, 4,  {FSUB},
 "geq",   0, 3,  {GEQ},
 "getm",  0, 4,  {GETM},
 "gtr",   0, 3,  {GTR},
 "idle",  0, 4,  {IDLE},
 "in",   0, 2, {IN},
 "inc",   0, 3,  {INC},
 "inc1",  0, 4,  {INC1},
 "incl",  0, 4,  {INCL},
 "jcnd",     O_JLAB, 4,  {JFLC, JBLC},
 "jump",     O_JLAB, 4,  {JFL, JBL},
 "la",      O_VAR, 2, {LEA, LGA},
 "leq",   0, 3,  {LEQ},
 "li",     O_SPEC, 2, {0},
 "lla",       O_NUM, 3,  {LLA},
 "llw",    O_NUM|O_MOD,  3, {LLW, LLW4-4},
 "lodfv",     0, 5, {LODFV},
 "lodt",  0, 4,  {LODT},
 "lpa",       O_NUM, 3,  {LPA},
 "lpc",    O_FUN|O_EXT,  3, {LPC},
 "lpw",       O_NUM, 3,  {LPW},
 "lsa",       O_NUM, 3,  {LSA},
 "lss",   0, 3,  {LSS},
 "lsta",     O_DLAB, 4,  {LSTA},
 "lsw",    O_NUM|O_MOD,  3, {LSW, LSW0},
 "lw",   O_VAR|O_MOD, 2, {LEW, LGW2-2, LGW},
 "lxa",   0, 3,  {LXA},
 "lxb",   0, 3,  {LXB},
 "lxw",   0, 3,  {LXW},
 "mod",   0, 3,  {MOD},
 "move",  0, 4,  {MOVE},
 "mul",   0, 3,  {MUL},
 "neg",   0, 3,  {NEG},
 "neq",   0, 3,  {NEQ},
 "nop",   0, 3,  {NOP},
 "not",   0, 3,  {NOT},
 "or",   0, 2, {OR},
 "pcop",      O_NUM, 4,  {PCOP},
 "quit",  0, 4,  {QUIT},
 "reset", 0, 5,  {RESET},
 "rol",   0, 3,  {ROL},
 "ror",   0, 3,  {ROR},
 "rtn",       0, 3, {RTN},
 "setm",  0, 4,  {SETM},
 "sgeq",  0, 4,  {SGEQ},
 "shl",   0, 3,  {SHL},
 "shl2",    O_MACRO, 4,  {1},
 "shr",   0, 3,  {SHR},
 "shr2",    O_MACRO, 4,  {2},
 "sleq",  0, 4,  {SLEQ},
 "slw",     O_NUM|O_MOD, 3, {SLW, SLW4-4},
 "spw",        O_NUM, 3, {SPW},
 "ssw",     O_NUM|O_MOD, 3, {SSW, SSW0},
 "sswu",         0,   4, {SSWU},
 "stofv", 0, 5,  {STOFV},
 "store",    0,  5, {STORE},
 "stot",  0, 4,  {STOT},
 "sub",   0, 3,  {SUB},
 "sw",    O_VAR|O_MOD, 2, {SEW, SGW2-2, SGW},
 "swap",      O_MACRO, 4, {3},
 "sxb",   0, 3,  {SXB},
 "sxw",   0, 3,  {SXW},
 "tr",   0, 2, {TR},
 "tra",   0, 3,  {TRA},
 "trap",  0, 4,  {TRAP},
 "trb",   0, 3,  {TRB},
 "xor",   0, 3,  {XOR}
 };

static int find ()
{
int beg, end, num, s;
INSTR * i;

 beg = BEG;
 end = END;

 for (;;) {

  num = (beg + end) / 2;

  i = & instr[num];
  s = strncmp (lp, i->mnemo, i->l);
  if (!s && !(isspace(*(lp + i->l))))
    s = 1;
  if (s == 0)
   return (num);
  if (s < 0)
   end = num - 1;
  else
   beg = num + 1;
  if (beg > end)
   return (-1);
  }
}

static void p_instr()
{
int num, op;
INSTR * i;
char * s;

 if (! (loc & (L_FUN | L_INIT))) {
  uerror ("code outside of function");
  return;
  }
 if ((num = find()) == -1) {
  uerror("illegal instruction");
  return;
  }
 i = & instr[num];

 if (!(op = i->op)) {
  putb (i->code[0]);
  return;
  }
 lp += i->l;

 if (op & O_NUM) {
  if ((num = getnum(lp)) == -1) return;
  if (op & O_01)
   if (num > 1)
    uerror ("FFCT : number : 0 or 1");
  if ((op & O_MOD) && num < 16)
   putb (i->code[1]+num);
  else {
   putb (i->code[0]);
   putb (num);
   }
  return;
  }
 if (op & O_MACRO) {
  switch (i->code[0]) {
   case 1:
    MACRO1;
    return;
   case 2:
    MACRO2;
    return;
   case 3:
    if (kron2) 
         MACRO3  /* NB! no ; */
    else 
     MACRO4;
    return; 
   }
  return;
  }
 if (op & O_FUN) {
  if (!(s = getname(lp))) return;
  if (num = GlobFun (s, srcflag)) {
   if (op & O_EXT) {
    putb (i->code[0]);
    putd (num);
    }
   else { /* CX, CL.. */
    if (num < 16)
     putb (i->code[1]+num);
    else {
     putb (i->code[2]);
     putb (num);
     }
    }
   }
  else {
   putb (i->code[0]);
   num = ExtFun(s, srcflag);
   if (op & O_EXT) putd (255 + num);
    /* NB! > 255 for LPC undefined operands! */
   else putd (num);
   }
  srcflag = FIND;
  return;
  }
 if (op & O_VAR) {
  if (!(s = getname (lp))) return;
  if (op & O_MOD) {
   if (num = GlobVar(s, FIND)) {
    if (num < 16)
     putb (i->code[1]+num);
    else {
     putb (i->code[2]);
     putb (num);
     }
    }
   else {
    putb (i->code[0]);
    putd (ExtVar(s));
    }
   return;
   }
  if (num = GlobVar(s, FIND)) {
   putb (i->code[1]);
   putb (num);
   }
  else {
   putb (i->code[0]);
   putd (ExtVar(s));
   }
  return;
  }
 if (op & O_SPEC) {
  special (i->code[0]);
  return;
  }
 if (op & O_JLAB) {
  if (!getlabel(lp)) return;
  num = Label (FIND);
  putb (i->code[jlabels[num].pc == -1 ? 0 : 1]);
  putd (num);
  return;
  }
 if (op & O_DLAB) {
  if (!getlabel(lp)) return;
  num = DLabel (FIND | D_DAT);
  putb (i->code[0]);
  putd (num);
  return;
  }
}


static void ftnend()
{
char msg[100];
int i;
JLAB * jl1, * jl2;

 jl2 = jlabels + jl + 1;
 for (jl1 = jlabels + 1; jl1 < jl2; jl1 ++)
  if (jl1->pc == -1) {
   sprintf(msg, "undefined jump label : %c%d",
    jl1->letter, jl1->num);
   uerror (msg);
   }

 if (pc > maxfun) maxfun = pc;
 funtab [gf].length = pc;
 funtab[gf].labno = jl;
 for (jl1 = jlabels + 1; jl1 < jl2; jl1 ++) 
  put (jl1->pc);

 /* mark function's internal static variables in the table so
    that they will not be 'found' in the future */
 for (i = funvar; i <= gv; i ++)
  if (globvar[i].flag & STAT)
   *(globvar[i].name) = '$';

 if (verbose > 1)
  fprintf(stderr, "#");
 loc = st = 0;
}

static void CheckExternals ()
/*  --------------
    Check the tables of external functions and variables to see whether
    some of them are defined in the tables for globals */
{
int i;
STAB * p0, * p1;

 p1 = & extvar[ev] + 1;
 for (p0 = & extvar[fileev]; p0 < p1; p0 ++) {
     if (p0->flag >= 0)
  if (i = GlobVar(p0->name, FIND))
   p0->flag = -i;
     }
 p1 = & extfun[ef] + 1;
 for (p0 = & extfun[fileef]; p0 < p1; p0++) {
     if (p0->flag >= 0)
  if (i = GlobFun(p0->name, p0->flag & VOID ? FIND | VOID : FIND))
   p0->flag =  -i;
     }
}

static void fileend()
{STAB * start, * end;

 if (loc)
  uerror ("Unexpected EOF");

 CheckExternals();

 /* mark file's static variables & functions in the tables so
    that they will not be 'found' in other files */
 if (filenr > 1) {
  for (start = & globvar[filegv], end = & globvar[gv]; 
    start <= end; start ++)
   if (start->flag & STAT)
    *(start->name) = '$';
  for (start = & globfun[filegf], end = & globfun[gf]; 
    start <= end; start ++)
   if (start->flag & STAT)
    *(start->name) = '$';
  }
 filedl = dl + 1;
 filegv = gv + 1;
 filegf = gf + 1;
 fileef = ef + 1;
 fileev = ev + 1;
}

#define  MAINLENGTH 41

static char * main_proc [MAINLENGTH] = {
 " .fun  _main ",
 " entr  4 ",  /* SS. 19.Nov.87 was 3 */
 " li 64 ",
 " alloc ",
 " shl2 ",
 " copt ",
 " slw 6 ",  /* cl_ptr, SS. 19.Nov.87 was 5 */
 "  li 5 ",
 " alloc ",
 " shl2 ",
 " copt ",
 " slw 7 ",      /* program, SS. 19.Nov.87 was 6 */
 " li 512 ",  /* host-port: 512 Kb heap (was 20 -- too small for clink's symbol table on real programs; 32MB board) */
 " li 4 ",  /* 4 file buffers (default) */
 " la errno ", /* in clib */
 " .void _stargs ",
 " cx _stargs ", /* Modula function */
 " entr  5 ",  /* SS. 19.Nov.87 was 4 */
 " llw 7 ",  /* SS. 19.Nov.87 was 6 */
 " llw 6 ",  /* SS. 19.Nov.87 was 5 */
 " store ",
 " li 8 ",  /* SS. 19.Nov.87 was 7 */
 " decs ",
 " cx _getargs ",
/* NB!  return value from getargs (argv) remains on E-stack and is
        passed to main as 1st parameter */
/* " slw 4 ", */
 " entr  5 ",  /* SS. 19.Nov.87 was 4 */
/* " llw 4 ",  argv */
 " lw _argc_",
 " store ",
 " li 8 ", /* SS. 19.Nov.87 was 7 */
 " decs ",
 " cx main ", 
/* NB!  return value from main remains on E-stack and is passed to exit() */
 " entr  5 ", /* SS. 19.Nov.87 was 4 */
 " stot ",
 " li 6 ", /* SS. 19.Nov.87 was 5 */
 " decs ",
 " .void ",
 " cx exit ",
 " rtn ",
 " .end  _main ",
 " .inb ",
 " cx _main ",
 " .ine " };

static void modend()
{
int i;
char msg[100];

 over = 1;
 for (i = 1; i <= dl; i ++)
  if (dlabels[i].flag & D_NVAL) {
   sprintf (msg, "undefined data label : %c%d",
    dlabels[i].letter, dlabels[i].num);
   uerror (msg);
   }

 fileef = fileev = 1;

 if (filenr > 1) {
  /* check all externals once more */
  CheckExternals();
  }

 if (nerrors) return;

 if (nmain) {
  loc = 0;
  for (i = 0; i < MAINLENGTH; i ++) {
   strcpy (line, main_proc [i]);
   procline ();
   }
  }
 loc = L_INIT;
 fp = fpinit;
 pcpnt = & pc0;

 putli (stksize + 5); /* SS. 19.Nov.87 */
 putb (RTN);

 if (pcc && stksize) {
  fp = fpallo;
  strcpy (line, "\tlla\t5\n");  /* SS. 19.Nov.87 */
  procline();
  sprintf (line, "\tli\t%d\n", stksize);
  procline();
  strcpy (line, "\t.void\n");
  procline();
  strcpy (line, "\tcx\t_zero\n");
  procline();
  }

 poolsize = poolreset();

 if ((pc0 + 7) > maxfun) maxfun = pc0 + 7;
 funtab [0].length = pc0;
 funtab[0].labno = 0;
 
 if (verbose > 1)
  fprintf(stderr, "\n");
}

