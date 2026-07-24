/* Revision log:
 * 25.Apr.86 MK definitions of Kronos assembler special commands
 * 29.dec.86 SS added directive .stk for setting the size of global aggregates
 * 18.Sep.87 MK added directives .alb, .ale
 * 25.Sep.87 SS added directives .vfn, .void for identifying void functions
 * 13.Apr.88 SS reduced table sizes for the Kronos version:
  SYMTSZ 600->500; DIMTABSZ 1000->800; SWITSZ 250->150
 * 03.Mar.89 SS increased table sizes for Kronos version also NB! NUT
*/

# include "macdefs.h"
# include "manifest.h"

/* storage classes  */
# define SNULL 0
# define AUTO 1
# define EXTERN 2
# define STATIC 3
# define REGISTER 4
# define EXTDEF 5
# define LABEL 6
# define ULABEL 7
# define MOS 8
# define PARAM 9
# define STNAME 10
# define MOU 11
# define UNAME 12
# define TYPEDEF 13
# define FORTRAN 14
# define ENAME 15
# define MOE 16
# define UFORTRAN 17
# define USTATIC 18
 /* field size is ORed in */
# define FIELD 0100
# define FLDSIZ 077
# ifndef BUG1
extern char *scnames();
# endif

/* location counters */
# define PROG 0
# define DATA 1
# define ADATA 2
# define STRNG 3
# define ISTRNG 4
# define STAB 5


/* symbol table flags */
# define SMOS 01
# define SHIDDEN 02
# define SHIDES 04
# define SSET 010
# define SREF 020
# define SNONUNIQ 040
# define STAG 0100

# ifndef FIXDEF
# define FIXDEF(p)
#endif
# ifndef FIXARG
# define FIXARG(p)
# endif
# ifndef FIXSTRUCT
# define FIXSTRUCT(a,b)
# endif

 /* alignment of initialized quantities */
# ifndef AL_INIT
# define AL_INIT ALINT
# endif

struct symtab {
 char *sname;
 TWORD stype;  /* type word */

 char sclass;  /* storage class */
 char slevel;  /* scope level */
 char sflags;  /* flags for set, use, hidden, mos, etc. */
 int offset;  /* offset or value */
 short dimoff; /* offset into the dimension table */
 short sizoff; /* offset into the size table */
 short suse;  /* line number of last use of the variable */
 };


/* NOPREF must be defined for use in first pass tree machine */
# define NOPREF 020000  /* no preference for register assignment */

struct sw {
 CONSZ sval;
 int slab;
 };

extern struct sw swtab[];
extern struct sw *swp;
extern int swx;

extern int ftnno;
extern int blevel;
extern int instruct, stwart;

extern int lineno, nerrors;
typedef union {
 int intval;
 NODE * nodep;
 } YYSTYPE;
extern YYSTYPE yylval;

extern CONSZ lastcon;
extern double dcon;

extern char ftitle[];
extern char ititle[];
extern struct symtab stab[];
extern int curftn;
extern int curclass;
extern int curdim;
extern int dimtab[];
extern int paramstk[];
extern int paramno;
extern int autooff, argoff, strucoff;
extern int regvar;
extern int minrvar;
extern int brkflag;
extern char yytext[];

extern OFFSZ inoff;

extern int reached;

/* tunnel to buildtree for name id's */

extern int idname;

extern NODE node[];
extern NODE *lastfree;

extern int cflag, hflag, pflag;

/* various labels */
extern int brklab;
extern int contlab;
extern int flostat;
extern int retlab;
extern int retstat;
extern int asavbc[], *psavbc;

/* flags used in structures/unions */

# define SEENAME 01
# define INSTRUCT 02
# define INUNION 04
# define FUNNYNAME 010
# define TAGNAME 020

/* flags used in the (elementary) flow analysis ... */

# define FBRK 02
# define FCONT 04
# define FDEF 010
# define FLOOP 020

/* flags used for return status */

# define RETVAL 1
# define NRETVAL 2

/* used to mark a constant with no name field */

# define NONAME 040000

 /* mark an offset which is undefined */

# define NOOFFSET (-10201)

/* declarations of various functions */

extern NODE
 *buildtree(),
 *bdty(),
 *mkty(),
 *rstruct(),
 *dclstruct(),
 *getstr(),
 *tymerge(),
 *stref(),
 *offcon(),
 *bcon(),
 *bpsize(),
 *convert(),
 *pconvert(),
 *oconvert(),
 *ptmatch(),
 *tymatch(),
 *makety(),
 *block(),
 *doszof(),
 *talloc(),
 *optim(),
 *strargs(),
 *clocal();

void prints ();

OFFSZ tsize(),
 psize();

TWORD types();

extern int eprint(),  /* in trees.c */
    ischarary(),  /* in code.c */
    xdebug,  /* in code.c */
    cdebug;  /* in scan.c */

double atof();

# define checkst(x)

# ifndef CHARCAST
/* to make character constants into character connstants */
/* this is a macro to defend against cross-compilers, etc. */
/* SS. 16.Mar.87  char replaced with unsigned char to avoid illegal const */
# define CHARCAST(x) (unsigned char)(x)
# endif

# define BCSZ 100 /* size of the table to save break and continue labels */
#if unix
# define SYMTSZ 20000  /* host-port: was 1300 -- clib.c overflows it (no guard) -> cerror(214) */
# define DIMTABSZ 20000   /* host-port: was 2000 */
# define PARAMSZ 1000  /* host-port: was 150 */
# define SWITSZ 2000  /* host-port: was 500 */
#else
# define SYMTSZ 800  /* SS 03.Mar.89 was 500;  */
# define DIMTABSZ 1000  /* SS 03.Mar.89 was 800  */
# define PARAMSZ 100
# define SWITSZ 300  /* SS 03.Mar.89 was 150  */
#endif

/* special interfaces for yacc alone */
/* These serve as abbreviations of 2 or more ops:
 ASOP =, = ops
 RELOP LE,LT,GE,GT
 EQUOP EQ,NE
 DIVOP DIV,MOD
 SHIFTOP LS,RS
 ICOP ICR,DECR
 UNOP NOT,COMPL
 STROP DOT,STREF

 */
# define ASOP 25
# define RELOP 26
# define EQUOP 27
# define DIVOP 28
# define SHIFTOP 29
# define INCOP 30
# define UNOP 31
# define STROP 32

# define LP 50
# define RP 51
# define LC 52
# define RC 53

/*
* These defines control "while" and "for" loop code generation.
* wloop_level and floop_level each must be set to one of these values.
*/
#define LL_TOP 0 /* test at loop top */
#define LL_BOT 1 /* test at loop bottom */
#define LL_DUP 2 /* duplicate loop test at top and bottom */


#define D_STK  prints("\t.stk\t")
#define  D_MOD   prints("\t.mod\t")
#define  D_END   prints("\t.end\t")
#define  D_VAR   prints("\t.var\t")
#define  D_STATVAR prints("\t.svr\t")
#define  D_FUN   prints("\t.fun\t")
#define  D_STATFUN prints("\t.sfn\t")
#define  D_DATA   prints("\t.dat\t")
#define  D_EQU   prints("\t.equ\t")
#define  D_BEGINIT prints("\t.inb\n")
#define  D_ENDINIT prints("\t.ine\n")
#define D_BEGALLOC prints("\t.alb\n")
#define D_ENDALLOC prints("\t.ale\n")
#define  D_VOIDFUN prints("\t.vfn\t")
#define  D_VOIDCALL prints("\t.void\n")

