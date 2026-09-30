/* Revision log:
 * 05.Apr.87 SS  SICON shape added for ICON node without name 
 * 28.Sep.87 SS added directives .vfn, .void for identifying void functions
 */

# include "macdefs.h"
# include "mac2defs.h"
# include "manifest.h"

/* SS. 28.apr.86  modified for Kronos */

/* bunch of stuff for putting the passes together... */
# define crslab crs2lab
# define where  where2
# define xdebug x2debug
# define tdebug t2debug
# define deflab def2lab
# define edebug e2debug
# define eprint e2print
# define getlab get2lab
# define filename ftitle

/* cookies, used as arguments to codgen */

# define FOREFF 01 /* compute for effects only */
# define INAREG 02 /* compute into a register */
# define INTAREG 04 /* compute into a scratch register */
# define INBREG 010 /* compute into a lvalue register */
# define INTBREG 020 /* compute into a scratch lvalue register */
# define FORCC 040 /* compute for condition codes only */
# define INTEMP 010000 /* compute into a temporary location */
# define FORARG 020000 /* compute for an argument of a function */
# define FORREW 040000 /* search the table, looking for a rewrite rule */
# define FORADR 0200000 /* SS. 28.04.86 added for generating address load instr.*/

 /* OP descriptors */
 /* the ASG operator may be used on some of these */

# define OPSIMP 010000    /* +, -, &, |, ^ */
# define OPCOMM 010002  /* +, &, |, ^ */
# define OPMUL 010004  /* *, / */
# define OPDIV 010006 /* /, % */
# define OPUNARY 010010  /* unary ops */
# define OPLEAF 010012  /* leaves */
# define OPANY 010014  /* any op... */
# define OPLOG 010016 /* logical ops */
# define OPFLOAT 010020 /* +, -, *, or / (for floats) */
# define OPSHFT 010022  /* <<, >> */
# define OPLTYPE 010024  /* leaf type nodes (e.g, NAME, ICON, etc. ) */

 /* match returns */

# define MNOPE 010000
# define MDONE 010001

 /* shapes */

# define SANY 01 /* same as FOREFF */
# define SAREG 02 /* same as INAREG */
# define STAREG 04 /* same as INTAREG */
# define SBREG 010 /* same as INBREG */
# define STBREG 020 /* same as INTBREG */
# define SCC 040 /* same as FORCC */
# define SNAME 0100
# define SCON 0200 /* ICON node with or without name */
# define SFLD 0400 /* FLD node */
# define SOREG 01000 /* matches indexed and non-indexed OREG node */
# define STARNM 02000 /* U* with descendants */
# define STARREG 04000 /* U* with descendants */


/* SS. 15.10.86  removed indirection or wild card shapes */

# define SWADD 040000  /* same as FORREW */
# define SPECIAL 0100000
# define SZERO SPECIAL   /* const 0 */
# define SONE (SPECIAL|1) /* const 1 */
# define SMONE (SPECIAL|2) /* const -1 */
# define SCCON (SPECIAL|3) /* 0 <const< 256    SS. 26.sep.86 for "lsa" */
# define SICON (SPECIAL|4) /* const >= 0, ICON with no name  */

/* SS. 15.10.86  removed SSCON and SSOREG shapes */


   /*  FORADR, FORARG and INTEMP are carefully not conflicting with shapes */

 /* types */

# define TCHAR 01
# define TSHORT 02
# define TINT 04
# define TLONG 010
# define TFLOAT 020
# define TDOUBLE 040
# define TPOINT 0100
# define TUCHAR 0200
# define TUSHORT 0400
# define TUNSIGNED 01000
# define TULONG 02000
# define TPTRTO 04000     /* pointer to one of the above */
# define TANY 010000     /* matches anything within reason */
# define TSTRUCT 020000   /* structure or union */

 /* reclamation cookies */

# define RNULL 0    /* clobber result */
# define RLEFT 01
# define RRIGHT 02
# define RESC1 04 /* scratch reg. */
# define RESC2 010
# define RESC3 020
# define RESCC 04000 /* result is in the cc reg. */
# define RNOP 010000   /* DANGER: can cause loops.. */

 /* needs */

 /* SS. 15.10.86 removed needs NBREG,NBMASK,NBSL,NBSR */

# define NAREG 01 /* class A register */
# define NACOUNT 03 /* mask for determining # of A regs */
# define NAMASK 017 /* mask for setting off NTEMP and NBREG */
# define NASL 04   /* share left register */
# define NASR 010  /* share right register */
# define NTEMP 0400 /* temporary storage needed */
# define NTMASK 07400
# define REWRITE 010000  /* the tree is candidate for rewriting */
# define EITHER 040000   /* "either" modifier for needs */


# define MUSTDO 010000   /* force register requirements */
# define NOPREF 020000  /* no preference for register assignment */

  /*  SS. 21.mai.86 some storage classes added to Pass2 for debugging purposes */
# define AUTO 1
# define STATIC 3
# define REGISTER 4
# define PARAM 9
# define FORTRAN 14
# define UFORTRAN 17
# define USTATIC 18

 /* register allocation */
extern int rstatus[];
extern int busy[];

extern struct respref { int cform; int mform; } respref[];

/* SS. 05.feb.87  check only for temporary scalar regs(stareg) */
# define isbreg(r) (rstatus[r]&SBREG)
# define istreg(r) (rstatus[r]&STAREG)
# define istnode(p) (p->in.op==REG && istreg(p->tn.rval))


/* SS. 27.apr. added to Pass2 */

# define myopasg(o)  (o==ASG PLUS || o==ASG MINUS)
# define isonode(p) (p->tn.op==ICON && (p->tn.name[0]=='\0') && (p->tn.lval >=0 && p->tn.lval <=255) )

/* end of my patch */


struct symtab {   /* SS. 16.mai 86 we must take it from Pass1 */
 char *sname;
 TWORD stype;  /* type word */

 char sclass;  /* storage class */
 char slevel;  /* scope level */
 char sflags;  /* flags for set, use, hidden, mos, etc. */
 int offset;   /* offset or value */
 short dimoff; /* offset into the dimension table */
 short sizoff; /* offset into the size table */
 short suse;   /* line number of last use of the variable */
};

extern struct symtab stab[]; /* SS. 16.mai 86  help from Pass1 */

# define NONAME 040000

# define TBUSY 01000
# define REGLOOP(i) for(i=0;i<REGSZ;++i)

# define SETSTO(x,y) (stotree=(x),stocook=(y))
extern int stocook;

# define DELAYS 20
extern NODE *deltrees[DELAYS];
extern int deli;   /* mmmmm */

extern NODE *stotree;
extern int callflag;

extern int fregs;

extern NODE node[];

extern struct optab {
 int op;
 int visit;
 int lshape;
 int ltype;
 int rshape;
 int rtype;
 int needs;
 int rewrite;
 char * cstring;
 }
 table[];

extern NODE resc[];

extern OFFSZ tmpoff;
extern OFFSZ maxoff;
extern OFFSZ baseoff;
extern OFFSZ maxtemp;
extern OFFSZ paramoff; /* SS 28.sep.88 offset after processing arguments */
extern int maxtreg;
extern int ftnno;
extern int rtyflg;

extern int nrecur;  /* flag to keep track of recursions */
extern int stacksize; /* expression stack size of Kronos computer 7 regs */

# define NRECUR (10*TREESZ)

extern NODE
 *talloc(),
 *tcopy(),
 *getlr(); /* SS. 15.10.86 removed *eread() and CONSZ rdin() */

extern int eprint();
extern void prints(); /* SS. 09.mar.87 from code.c, same as fputs() */

extern char *rnames[];

extern int lineno;
extern char filename[];
extern int fldshf, fldsz;
extern int lflag, xdebug, udebug, edebug, odebug, rdebug, radebug, 
    tdebug, sdebug, cdebug, verbose, m2calls;

# define PUTCHAR(x) putchar(x)

 /* SS. 30.06.86 removed - macros for doing double indexing */

 /* SS. 28.Sep.87 added asm directives for void functions */
#define  D_VOIDFUN prints("\t.vfn\t")
#define  D_VOIDCALL prints("\t.void\n")
