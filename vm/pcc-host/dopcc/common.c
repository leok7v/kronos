/* Revision log:
/* 15.Oct.86 MK defines removed in the beginning, indope[]: char[8]->char
 * 25.Dec.86 SS the max offset value set to  512K bytes
 * 10.Jan.87 MK msg.c included here
 * 03.Feb.87 MK node[] brought here from xdefs, messages stuff moved to end
 * 06.Feb.87 SS where() moved here from code.c
 * 25.Feb.87 SS tcheck() rewritten to increace effectivity
 * 11.Mar.87 SS this is now main file, not include file for comm1.c
 * 16.Oct.87 SS msginit() now returns 1, if it can't open message file
 */

#include "mfile1.h"

NODE node [TREESZ];

int dope[ DSIZE ];
char *opst[DSIZE];


unsigned caloff(){   /* return the max offset in bits */
 return ( 4194304 ); /* SS. 25.dec.86 512K max memory in case of Kronos */
 }


NODE *lastfree;  /* pointer to last free node; (for allocator) */


tinit(){  /* initialize expression tree search */

register NODE *p;

 for( p=node; p<= &node[TREESZ-1]; ++p ) p->in.op = FREE;
 lastfree = node;

 }

# define TNEXT(p) (p== &node[TREESZ-1]?node:p+1)


NODE *
talloc(){
register NODE *p, *q;
 q = lastfree;
 for( p = TNEXT(q); p!=q; p= TNEXT(p))
  if( p->in.op ==FREE ) return(lastfree=p);
 cerror( 208 );  /* out of tree space; simplify expression */
 /* NOTREACHED */
 }


tcheck(){ /* ensure that all nodes have been freed */

register NODE *p;
static NODE *first = node;  /* SS. 25.feb.87 added */

 if( !nerrors )
 /* for( p=node; p<= &node[TREESZ-1]; ++p )  */
  for( p=first; p != lastfree; p= TNEXT(p) ){
   if( p->in.op != FREE ){
#ifndef BUG4
    extern int e2print();
    printf( "\ntree causing cerror <wasted space>:\n" );
    fwalk( p, e2print, 0 );
#endif
    cerror( 209, p );  /* wasted space: %o */
   }
   first = lastfree; /* SS. remember the location */
  }
 else
  tinit(); /* SS. call the tinit() only if there were errors */
 freetstr();

} /* tcheck */


tfree( p )  NODE *p; {   /* free the tree p */
 extern tfree1();

 if( p->in.op != FREE ) walkf( p, tfree1 );

 }


tfree1(p)  NODE *p; {
 if( p == 0 ) 
  cerror( 210 );   /* freeing blank tree! */
 else p->in.op = FREE;
 }


fwalk( t, f, down ) register NODE *t; int (*f)(); {

 int down1, down2;

 more:
 down1 = down2 = 0;

 (*f)( t, down, &down1, &down2 );

 switch( optype( t->in.op ) ){

 case BITYPE:
  fwalk( t->in.left, f, down1 );
  t = t->in.right;
  down = down2;
  goto more;

 case UTYPE:
  t = t->in.left;
  down = down1;
  goto more;

  }
 }


walkf( t, f ) register NODE *t;  int (*f)(); {
 register opty;

 opty = optype(t->in.op);

 if( opty != LTYPE ) walkf( t->in.left, f );
 if( opty == BITYPE ) walkf( t->in.right, f );
 (*f)( t );
 }


struct dopest { int dopeop; char *opst; int dopeval; } indope[] = {

 NAME, "NAME", LTYPE,
 STRING, "STRING", LTYPE,
 REG, "REG", LTYPE,
 OREG, "OREG", LTYPE,
 ICON, "ICON", LTYPE,
 FCON, "FCON", LTYPE,
 CCODES, "CCODES", LTYPE,
 UNARY MINUS, "U-", UTYPE,
 UNARY MUL, "U*", UTYPE,
 UNARY AND, "U&", UTYPE,
 UNARY CALL, "UCALL", UTYPE|CALLFLG,
 UNARY FORTCALL, "UFCALL", UTYPE|CALLFLG,
 NOT, "!", UTYPE|LOGFLG,
 COMPL, "~", UTYPE,
 FORCE, "FORCE", UTYPE,
 INIT, "INIT", UTYPE,
 SCONV, "SCONV", UTYPE,
 PCONV, "PCONV", UTYPE,
 PLUS, "+", BITYPE|FLOFLG|SIMPFLG|COMMFLG,
 ASG PLUS, "+=", BITYPE|ASGFLG|ASGOPFLG|FLOFLG|SIMPFLG|COMMFLG,
 MINUS, "-", BITYPE|FLOFLG|SIMPFLG,
 ASG MINUS, "-=", BITYPE|FLOFLG|SIMPFLG|ASGFLG|ASGOPFLG,
 MUL, "*", BITYPE|FLOFLG|MULFLG|COMMFLG,  /* SS.30.06.86 COMMFLG*/
 ASG MUL, "*=", BITYPE|FLOFLG|MULFLG|ASGFLG|ASGOPFLG,
 AND, "&", BITYPE|SIMPFLG|COMMFLG,
 ASG AND, "&=", BITYPE|SIMPFLG|COMMFLG|ASGFLG|ASGOPFLG,
 QUEST, "?", BITYPE,
 COLON, ":", BITYPE,
 ANDAND, "&&", BITYPE|LOGFLG,
 OROR, "||", BITYPE|LOGFLG,
 CM, ",", BITYPE,
 COMOP, ",OP", BITYPE,
 ASSIGN, "=", BITYPE|ASGFLG,
 DIV, "/", BITYPE|FLOFLG|MULFLG|DIVFLG,
 ASG DIV, "/=", BITYPE|FLOFLG|MULFLG|DIVFLG|ASGFLG|ASGOPFLG,
 MOD, "%", BITYPE|DIVFLG,
 ASG MOD, "%=", BITYPE|DIVFLG|ASGFLG|ASGOPFLG,
 LS, "<<", BITYPE|SHFFLG,
 ASG LS, "<<=", BITYPE|SHFFLG|ASGFLG|ASGOPFLG,
 RS, ">>", BITYPE|SHFFLG,
 ASG RS, ">>=", BITYPE|SHFFLG|ASGFLG|ASGOPFLG,
 OR, "|", BITYPE|COMMFLG|SIMPFLG,
 ASG OR, "|=", BITYPE|COMMFLG|SIMPFLG|ASGFLG|ASGOPFLG,
 ER, "^", BITYPE|COMMFLG|SIMPFLG,
 ASG ER, "^=", BITYPE|COMMFLG|SIMPFLG|ASGFLG|ASGOPFLG,
 INCR, "++", BITYPE|ASGFLG,
 DECR, "--", BITYPE|ASGFLG,
 STREF, "->", BITYPE,
 CALL, "CALL", BITYPE|CALLFLG,
 FORTCALL, "FCALL", BITYPE|CALLFLG,
 EQ, "==", BITYPE|LOGFLG,
 NE, "!=", BITYPE|LOGFLG,
 LE, "<=", BITYPE|LOGFLG,
 LT, "<", BITYPE|LOGFLG,
 GE, ">=", BITYPE|LOGFLG, /* SS. 29.apr.86 patch ">" */
 GT, ">", BITYPE|LOGFLG,
 UGT, "UGT", BITYPE|LOGFLG,
 UGE, "UGE", BITYPE|LOGFLG,
 ULT, "ULT", BITYPE|LOGFLG,
 ULE, "ULE", BITYPE|LOGFLG,
 ARS, "A>>", BITYPE,
 TYPE, "TYPE", LTYPE,
 LB, "[", BITYPE,
 CBRANCH, "CBRANCH", BITYPE,
 FLD, "FLD", UTYPE,
 PMCONV, "PMCONV", BITYPE,
 PVCONV, "PVCONV", BITYPE,
 RETURN, "RETURN", BITYPE|ASGFLG|ASGOPFLG,
 CAST, "CAST", BITYPE|ASGFLG|ASGOPFLG,
 GOTO, "GOTO", UTYPE,
 STASG, "STASG", BITYPE|ASGFLG,
 STARG, "STARG", UTYPE,
 STCALL, "STCALL", BITYPE|CALLFLG,
 UNARY STCALL, "USTCALL", UTYPE|CALLFLG,

-1, 0
};


mkdope(){
 register struct dopest *q;

 for( q = indope; q->dopeop >= 0; ++q ){
  dope[q->dopeop] = q->dopeval;
  opst[q->dopeop] = q->opst;
  }
 }


# ifndef BUG4
tprint( t )  TWORD t; { /* output a nice description of the type of t */

 static char * tnames[] = {
  "undef",
  "farg",
  "char",
  "short",
  "int",
  "long",
  "float",
  "double",
  "strty",
  "unionty",
  "enumty",
  "moety",
  "uchar",
  "ushort",
  "unsigned",
  "ulong",
  "?", "?"
  };

 for(;; t = DECREF(t) ){

  if( ISPTR(t) ) printf( "PTR " );
  else if( ISFTN(t) ) printf( "FTN " );
  else if( ISARY(t) ) printf( "ARY " );
  else {
   printf( "%s", tnames[t] );
   return;
   }
  }
 }
# endif


#define  NTSTRBUF 40
#define  TSTRSZ   2048
char itstrbuf[TSTRSZ];
char *tstrbuf[NTSTRBUF] = { itstrbuf };
char **curtstr = tstrbuf;
int tstrused;
extern  void  free();
extern  char    *malloc();

char *
tstr(cp) register char *cp; { /* place the copy of string into temp storage */

 register int i = strlen(cp);
 register char *dp;
 
 if (tstrused + i >= TSTRSZ) {
  if (++curtstr >= &tstrbuf[NTSTRBUF])
   cerror( 211 );   /* out of temporary string space */
  tstrused = 0;
  if (*curtstr == 0) {
   if( (dp = (char *)malloc(TSTRSZ)) == 0 )
    cerror( 212 );  /* out of memory (tstr) */
   *curtstr = dp;
  }
 }
 strcpy(dp = *curtstr+tstrused, cp);
 tstrused += i + 1;
 return (dp);
}


int nerrors;   /* number of errors */
int nwarnings; /* SS. 29.dec.86  # of warnings */

long lseek();

static int msgfd,
  nmessages;

typedef struct {
 int  m_nr, m_len;
 int m_offset;
 } MSG;

static MSG *msg;

static int nofile;


msginit(fname) char *fname; {
 if ((msgfd = open(fname, 0)) == -1){
  nofile = 1;
  return(1); /* SS. 16.Oct.87 to note unsuccessful open */
  }
 else 
  nofile = 0;
 read (msgfd, (char *)&nmessages, sizeof(int));
 if ((msg = (MSG *) malloc(nmessages*sizeof(MSG))) == (MSG *) 0) {
  nofile = 1;
  return(0);
  }
 read (msgfd, (char *)msg, nmessages*sizeof(MSG));
 return(0);
}


msgterm(){

 if (!nofile){
  close (msgfd);
  free ((char *)msg);
 }
}


char *
msgget (code, message) int code; char *message; {
 register int beg, end, num, temp;

 if (nofile) {
  sprintf(message, "# %d", code);
  return (message);
  }
 beg = 0;
 end = nmessages - 1;
 for (;;) {
  num = (int) ((beg+end)/2);
  if (!(temp = code - msg[num].m_nr))
   break;
  if (temp < 0)
   end = num - 1;
  else
   beg = num + 1;
  if (beg > end) {
   sprintf(message, "# %d", code);
   return (message);
   }
  }
 if (lseek(msgfd, msg[num].m_offset, 0) == -1) {
  sprintf(message, "Error reading message # %d", code);
  return (message);
  }
 read (msgfd, message, msg[num].m_len);
 return (message);
}


where(c){ /* print location of error  */
 /* c is either 'u', 'c', or 'w' */
 /* GCOS version */
 fprintf( stderr, "%s, line %d, ", ftitle, lineno );
 }


 /* VARARGS1 */
uerror( code, a )  { /* nonfatal error message */
 /* the routine where is different for pass 1 and pass 2;
 /*  it tells where the error took place */
char msgbuf[80];

 ++nerrors;
 where('u');
 fprintf( stderr, "error:   " );
 fprintf( stderr, msgget(code, msgbuf), a );
 fprintf( stderr, "\n" );
#ifdef BUFSTDERR
 fflush(stderr);
#endif
 if( nerrors > 20 )  /* SS. 1.apr.87 # of errors was 30 ! */
  cerror( 207 );   /* too many errors */
 }


 /* VARARGS1 */
cerror( code, a, b, c ) { /* compiler error: die */
char msgbuf[80];
 where('c');
 if( nerrors && nerrors <= 30 ){ /* give the compiler the benefit of the doubt */
  fprintf( stderr, "cannot recover from earlier errors: goodbye!\n" );
  }
 else {
  fprintf( stderr, "compiler error: " );
  fprintf( stderr, msgget(code, msgbuf), a, b, c );
  fprintf( stderr, "\n" );
  }
#ifdef BUFSTDERR
 fflush(stderr);
#endif
 exit(1);
 }


int Wflag = 0;    /* Non-zero means do not print warnings */


 /* VARARGS1 */
werror( code, a, b ) {  /* warning */
char msgbuf[80];

 ++nwarnings;  /* SS. 29.dec.86  count warnings too */
 if(Wflag) return;
 where('w');
 fprintf( stderr, "warning: " );
 fprintf( stderr, msgget(code, msgbuf), a, b );
 fprintf( stderr, "\n" );
#ifdef BUFSTDERR
 fflush(stderr);
#endif
 }

