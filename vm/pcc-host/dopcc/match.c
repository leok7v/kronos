/* Revision log:
 * 05.Apr.87 SS  introduced shape SICON and template for unsigned shifts
 * 07.Apr.87 SS  load the char value for testing using "lodt" in reclaim()
 * 03.Sep.87 SS  added template SCONV for int -> char conversion
 * 08.sep.87 SS  fixed bug in SCONV template: if we have char -> int conversion
 *   in the conditional context (while ( (int)func() ){ )
 * 25.Sep.87 SS  removed qflag related stuff
 */

# include "mfile2.h"

extern int mdebug; /* SS. 30.apr.86 for testing template matching */

 /* SS. 29.apr.86 added cookie FORADR, changed other cookies */
struct respref respref[] = {
 INTAREG,INTAREG,
 FORADR, FORADR, 
 INTEMP, INTEMP,
 FORARG, FORARG,
 INTEMP, INTAREG,
 0, 0 };

NODE resc[3];

int busy[REGSZ];

int maxa, mina;

static int mamask[] = { /* masks for matching dope with shapes */
 SIMPFLG, /* OPSIMP */
 SIMPFLG|ASGFLG, /* ASG OPSIMP */
 COMMFLG, /* OPCOMM */
 COMMFLG|ASGFLG, /* ASG OPCOMM */
 MULFLG,  /* OPMUL */
 MULFLG|ASGFLG,  /* ASG OPMUL */
 DIVFLG,  /* OPDIV */
 DIVFLG|ASGFLG,  /* ASG OPDIV */
 UTYPE,   /* OPUNARY */
 TYFLG,   /* ASG OPUNARY is senseless */
 LTYPE,   /* OPLEAF */
 TYFLG,   /* ASG OPLEAF is senseless */
 0,  /* OPANY */
 ASGOPFLG|ASGFLG, /* ASG OPANY */
 LOGFLG,  /* OPLOG */
 TYFLG,   /* ASG OPLOG is senseless */
 FLOFLG,  /* OPFLOAT */
 FLOFLG|ASGFLG,  /* ASG OPFLOAT */
 SHFFLG,  /* OPSHFT */
 SHFFLG|ASGFLG,  /* ASG OPSHIFT */
 SPFLG,   /* OPLTYPE */
 TYFLG,   /* ASG OPLTYPE is senseless */
 };

/* table.c */

/* SS. 13.02.87 the types which do not exist for Kronos were removed, but
   intermediate results may have such types and I got the compiler loop error.
*/

# define WRDPTR TPTRTO|TINT|TFLOAT|TPOINT|TUNSIGNED|TSTRUCT|TLONG|TDOUBLE|TUSHORT|TULONG
# define CHRPTR TPTRTO|TCHAR|TUCHAR
# define ANYSIGNED TPOINT|TINT|TCHAR|TLONG|TSHORT
# define ANYUSIGNED TUNSIGNED|TUCHAR|TULONG|TUSHORT
# define ANYFIXED ANYSIGNED|ANYUSIGNED
# define IWORD TPOINT|TINT|TLONG|TSHORT|TUNSIGNED|TULONG|TUSHORT
/* SS. 03.sep.87 IWORD added */

struct optab *rwtable;

struct optab *opptr[DSIZE];

struct optab  table[] = {

PCONV, INTAREG|FORCC,  /* convert wordptr to charptr */
 STAREG, WRDPTR,
 SANY, CHRPTR,
  0, RLEFT|RESCC,
  " shl2\n", /* this is really "li 2; shl" */

PCONV, INTAREG|FORCC,  /* convert charptr to wordptr */
 STAREG, CHRPTR,
 SANY, WRDPTR,
  0, RLEFT|RESCC,
  " shr2\n", /* this is really "li 2; shr" */

PCONV, INTAREG|FORCC,  /* convert any scalar to pointer */
 STAREG, TANY, 
 SANY, TPOINT,
  0, RLEFT,
  "",

SCONV, INTAREG|FORCC,  /* convert int to float */
 STAREG, TANY,   /* was ANYFIXED */
 SANY, TFLOAT|TDOUBLE,
  0, RLEFT|RESCC,
  " ffct 0\n",

SCONV, INTAREG|FORCC,  /* convert float to integer */
 STAREG, TFLOAT|TDOUBLE,
 SANY, TANY,  /* was ANYFIXED */
  0, RLEFT|RESCC,
  " ffct 1\n",

SCONV, INTAREG|FORCC,  /* convert int to char */
 STAREG, IWORD,   /* SS. 03.sep.87 added */
 SANY, TCHAR|TUCHAR,  
  0, RLEFT|RESCC,
  " li 255\n and\n",

SCONV, INTAREG|FORCC,  /* convert any pointer to scalar */
 STAREG, TANY,   /* this was only TPOINT, but local passes */ 
 SANY, TANY,  /* int->char and other conversions yet */
  0, RLEFT|RESCC, /* SS. 07.sep.87 added "RESCC" */
  "",

/* No INITialization templates, letting the work be done by Pass 1 */


STASG, INTAREG|FOREFF,
 STAREG, TANY,   /* adr of destination struct */
 STAREG, TANY,   /* adr of source struct */
  0, RRIGHT,
  "ZS",

/*  FLD types not implemented yet */

CCODES,  INTAREG,    /* output unconditional jump, label,*/
 SANY, TANY,  /*   "li 0\n"  and label          */
 SANY, TANY,
  NAREG, RESC1,
  " li 1\nZN",
      
UNARY CALL, INTAREG, /* includes also Modula call */
 SCON, TANY,  /* extern or global func name */
 SANY, TANY,
  NAREG, RESC1,   /* really the top of stack */
  " cx CL\n", /* 11.dec.  this was "ZC\n call CL\n" */

UNARY CALL, INTAREG,  /* PTR to FTN is ext/global, arg/local*/
 SNAME|SOREG|STAREG, TANY, /*  or resides on the top of stack    */
 SANY, TANY,
  NAREG, RESC1,   /* really top of stack */
  "ZW stot\n cf\n",  /* 11.dec. this was "ZC\nZW stot\n pcall\n" */


OPSHFT,  INTAREG|FORCC,   /* RS and LS for signed and unsigned operands */
 STAREG, TANY,   /* SS. 12.feb.87 was ANYFIXED */
 STAREG, TANY,   /* was ANYFIXED */
  0, RLEFT|RESCC,
  "O",

OPSHFT,  INTAREG|FORCC,   /* RS and LS for const unsigned shifts */
 STAREG, TUNSIGNED, 
 SICON,  TANY, 
  0, RLEFT|RESCC,
  "ZU",

INCR, FOREFF,
 STAREG, TANY,
 SCON, TANY,
  0, RLEFT,  /* 04.june rnull->rleft */
  "ZE",

DECR, FOREFF,
 STAREG, TANY,
 SCON, TANY,
  0, RLEFT,  /* 04.june rnull->rleft */
  "ZE",

REG, FOREFF,  /* to remove unneeded result from top of stack */
 STAREG, TANY,
 SANY, TANY,
  0, RNULL,
  " drop\n",

/* cookie INTEMP - storing to temporary location not implemented yet */

#ifdef VAX
REG, INTEMP,  /* to store intermediate result from top of stack */
 STAREG, TANY,
 SANY, TANY,
  NTEMP, RESC1,
  " stot\n", /* reverse to "lodt" instr. */
#endif

OPLEAF,  FOREFF,
 SANY, TANY,  /* left and rigth child exchanged for OPLTYPE */
 SANY, TANY,
  0, RRIGHT, /* rleft->rright because childs are exchanged */
  "",

OPLTYPE, INTAREG|FORCC,  /* 13.may.86  check REG too! */
 SANY,   TANY,   
 SANY, TANY,
  NAREG, RESC1|RESCC,
  "ZW",   /* load LTYPE value on top of stack */

OPLTYPE, FORADR,
 SNAME|SOREG, TANY, /* 23.06 removed STARREG then do only "copt" */
 SANY, TANY,
  NAREG, RESC1,
  "ZA",     /* load LTYPE address on top of stack */
   
OPLTYPE, FORARG,
 SANY, TANY,  /* if REG then do only "stot"  */
 SANY, TANY,
  0, RNULL,  /* the tree must be freed */
  "ZW stot\n",

UNARY MINUS, INTAREG|FORCC,  /* integer and float negate ops */
 STAREG, TANY,
 SANY, TANY,
  0, RLEFT|RESCC,
  "O",    /* "fneg|neg" */

COMPL, INTAREG|FORCC,
 STAREG, TANY,    /* was ANYFIXED */
 SANY, TANY,
  0, RLEFT|RESCC,
  " li -1\n xor\n", /* 19.sep.86 patched by Mari */

MOD, INTAREG|FORCC,
 STAREG, TANY,    /* was ANYFIXED */
 STAREG, TANY,    /* was ANYFIXED */
  0, RLEFT|RESCC,
  " mod\n",  /* integer modulo */


ASSIGN,  INTAREG|FOREFF|FORCC,
 SOREG|SNAME|STAREG, TANY, /* local,global scalar or address */
 STAREG, TANY,    /* value */
  0, RRIGHT|RESCC,
  "ZM",    /* gen. "sw|slw|ssw|sxb" instr. */

/*  ASSIGN to FLD not implemented yet  */


ASG PLUS, INTAREG|FOREFF|FORCC,
 STAREG,  ANYFIXED, /* address on top of stack */
 SCON|STAREG, ANYFIXED,  /* rhs is const 1,ICON or REG */
  0, RLEFT|RESCC,
  "ZE",    /* "inc1|inc" */

ASG MINUS, INTAREG|FOREFF|FORCC,
 STAREG,  ANYFIXED, /* address on top of stack */
 SCON|STAREG, ANYFIXED,  /* rhs is const 1,ICON or REG */
  0, RLEFT|RESCC,
  "ZE",    /* "dec1|dec" */

PLUS, INTAREG|FORCC,   /* 26.sep.86 added */
 STAREG, ANYFIXED,  /* value on top of stack */
 SCCON,  ANYFIXED,  /* RHS is 0 < const < 256 */
  0, RLEFT|RESCC,
  "ZD",    /* add on top of stack "lsa" */


/* this is the place for address templates */

#ifdef OLDARGS
PLUS, INTAREG|FORCC,  /* template to generate code for trees which */
 SAREG,  TPOINT,  /* represent address with offset from AP     */
 SCON, TINT,  /* the result is instr. "lpa"                */
  NAREG, RESC1|RESCC,
  "ZK",   /* 11.dec.  this was "ZX" */
#endif

MINUS, INTAREG|FORCC,  /* template to generate code for trees which */
 SAREG,  TPOINT,  /* represent address with offset from FP     */
 SICON,  TINT,   /* the result is instr. "lla or llw"         */
  NAREG, RESC1|RESCC,
  "ZX",

OPLOG, FORCC,    /* all logical instructions */
 STAREG, TANY,
 STAREG, TANY,
  0, RESCC,
  "ZP",   /* output compare instr. and conditional jump */

OPSIMP,  INTAREG|FORCC,   /* 12.Feb.87 removed cookie FOREFF, because */
 STAREG, ANYFIXED, /* the unneeded result must be dropped */
 STAREG, ANYFIXED,
  0, RLEFT|RESCC,
  "O",   /* all simple ops for integers */

OPMUL, INTAREG|FORCC,  /*  DIV and MUL for integers */
 STAREG, ANYFIXED, /*  div and mul for floats matched by OPFLOAT */
 STAREG, ANYFIXED,
  0, RLEFT|RESCC,
  "O",   /* "mul|div" */

OPFLOAT, INTAREG|FORCC,   /* 12.Feb.87 removed cookie FOREFF, because */
 STAREG, TFLOAT|TDOUBLE, /* the unneeded result must be dropped */
 STAREG, TFLOAT|TDOUBLE,
  0, RLEFT|RESCC,
  "O",   /* all simple ops for floats */


/* Default actions for hard trees ... */

# define DF(x) FORREW,SANY,TANY,SANY,TANY,REWRITE,x,""

UNARY MUL, DF( UNARY MUL ),

INCR, DF(INCR),

DECR, DF(INCR),

ASSIGN, DF(ASSIGN),

STASG, DF(STASG),

FLD, DF(FLD),

OPLEAF, DF(NAME),

OPLOG, FORCC,
 SANY, TANY,
 SANY, TANY,
  REWRITE, BITYPE,
  "",

OPLOG, DF(NOT),

COMOP, DF(COMOP),

INIT, DF(INIT),

OPUNARY, DF(UNARY MINUS),

ASG OPANY, DF(ASG PLUS),

OPANY, DF(BITYPE),

FREE,  FREE,  FREE,  FREE,  FREE,  FREE,  FREE,  FREE,  "Help me!\n" };


/* match.c */

# define NOINDIRECT


tshape( p, shape ) NODE *p; { /* F1 */
/* return true if shape is appropriate for the node p
   side effect for SFLD is to set up fldsz,etc */

# ifndef BUG3
 if( sdebug )
  printf( "tshape( %o, %o), op = %d\n", p, shape, p->in.op );
# endif
 if( shape & SPECIAL ){

  switch( shape ){

  case SZERO:
  case SONE:
  case SMONE:
  case SCCON:
  case SICON: /* SS. 5.apr.87 added for unsigned shifts */
   if( p->in.op != ICON || p->in.name[0] ) return(0);
   if( p->tn.lval >= 0 && shape == SICON ) return(1);
   if( p->tn.lval == 0 && shape == SZERO ) return(1);
   else if( p->tn.lval == 1 && shape == SONE ) return(1);
   else if( p->tn.lval == -1 && shape == SMONE ) return(1);
  /* SS. 26.sep.86 patch to replace "lib,add" with "lsa"  */
   else if( p->tn.lval > 0 && p->tn.lval < 256 
    && shape == SCCON ) return(1);
   else return(0);

 /* SS. 15.10.86  removed SSCON and SSOREG shapes */

  default:
   return( special( p, shape ) );
  }
 } /* special shape */

 if( shape & SANY ) return(1);

 if( (shape&INTEMP) && shtemp(p) ) return(1);

  /* SS. 29.apr.86 special action in case of cookie FORADR */
 if( (shape&FORADR) && ( p->in.op == REG ) ) return(1);

 if( (shape&SWADD) && (p->in.op==NAME || p->in.op==OREG) ){
  if( BYTEOFF(p->tn.lval) ) return(0);
 }

 /* SS. 15.10.86  removed WCARD1 and WCARD2 shape testing */

 switch( p->in.op ){

 case NAME:
  return( shape & SNAME );
 case ICON:
  return( shape & SCON );
 case FLD:
  if( shape & SFLD ){
   int o = p->tn.rval;
   if( !flshape( p->in.left ) ) return(0);
   /* it is a FIELD shape; make side-effects */
   fldsz = UPKFSZ(o);
# ifdef RTOLBYTES
 /* SS. 8.apr.87 UPKFOFF performs >>, may be unsigned cast is needed */
   fldshf = UPKFOFF(o);
# else
   fldshf = SZINT - fldsz - UPKFOFF(o);
# endif
   return(1);
  }
  return(0);

 case CCODES:
  return( shape&SCC );

 case REG: {
  /* distinctions:
  SAREG  any scalar register
  STAREG any temporary scalar register
  SBREG  any lvalue (index) register
  STBREG any temporary lvalue register
  */
  int mask;
  mask = isbreg( p->tn.rval ) ? SBREG : SAREG;

   /* SS. NB! we must distinct SAREG from STAREG   */
/* SS. 04.feb.87 if the tree was rewrited, then the REG node has busy value > 1
   and the tshape() returns value > 0, which produces compiler loop */
/*   if( istreg( p->tn.rval ) && busy[p->tn.rval]<=1 )  */
  if( istreg( p->tn.rval ) )
   mask =  STAREG;
  return( shape & mask );
 }

 case OREG:
  return( shape & SOREG );

#ifndef NOINDIRECT
 /* SS. 26.sep.86 there is not indirect nodes in the KCC */
 case UNARY MUL:  
  /* return STARNM or STARREG or 0 */
  return( shumul(p->in.left) & shape );
# endif

 } /* switch */

 return(0);
} /* tshape */


ttype( t, tword ) TWORD t; { /* F2 */
/* does the type t match tword */

 if( tword & TANY ) return(1);

 if( t == UNDEF ) t=INT; /* void functions eased thru tables */

# ifndef BUG3
 if( tdebug )
  printf( "ttype( %o, %o )\n", t, tword );
# endif
 if( ISPTR(t) && (tword&TPTRTO) ) {
  do {
   t = DECREF(t);
  } while ( ISARY(t) );
   /* arrays that are left are usually only
      in structure references... */
  return( ttype( t, tword&(~TPTRTO) ) );
 }
 if( t != BTYPE(t) ) return( tword & TPOINT ); /* TPOINT means not simple! */
 if( tword & TPTRTO ) return(0);

 switch( t ){

 case UCHAR:
  return( tword & TUCHAR );
 case CHAR:
  return( tword & TCHAR );
 case STRTY:
 case UNIONTY:
  return( tword & TSTRUCT );
 case INT:
  return( tword & TINT );
 case UNSIGNED:
  return( tword & TUNSIGNED );
 case FLOAT:
  return( tword & TFLOAT );

/* SS. 10.02.87 the following types does not exist for Kronos,but the inter- */ 
/* mediate result may have such type in current state. This must be fixed */
 case SHORT:
  return( tword & TSHORT );
 case USHORT:
  return( tword & TUSHORT );
 case ULONG:
  return( tword & TULONG );
 case LONG:
  return( tword & TLONG );
 case DOUBLE:
  return( tword & TDOUBLE );

  }

 return(0);
} /* ttype */



setrew(){ 
/* set rwtable to first value which allows rewrite */
/* SS. 27.apr.86 modified OPLTYPE check  */
 register struct optab *q;
 register int i;

 for( q = table; q->op != FREE; ++q ){
  if( q->needs == REWRITE ){
   rwtable = q;
   goto more;
  }
 }
 cerror( 265 );  /* bad setrew */ 

 more:
 for( i=0; i<DSIZE; ++i ){
  if( dope[i] ){ /* there is an op... */
   for( q=table; q->op != FREE; ++q ){
    if( q->op < OPSIMP ){  /* simple template */
     if( q->op==i ) break;
    }
    else {      /* this template can  */
          /* match multiple nodes */
       register opmtemp;  /* OPgroup mask  */

/* SS. 27.apr.86 added REG as leaf and removed call to shltype */

       if((opmtemp=mamask[q->op - OPSIMP])&SPFLG){
        if( i==NAME || i==ICON || i==OREG 
                   || i==REG || i==FCON ) break;
       }     /* is it OPLTYPE */
       else
          if( (dope[i]&(opmtemp|ASGFLG)) == opmtemp)
           break;
           }
   } /* for q.. */
   opptr[i] = q; /* save ptr to the first possible temp.*/
  } /* if .. */
 } /* for i .. */

/* SS. 30.apr.86  added only for debugging purposes  */

# ifndef BUG3
 if (mdebug > 1){
  printf( "Setup for templates:\n" );
  for( i=0; i<DSIZE; ++i ){
   printf("%s -> %d..",opst[i],opptr[i] );
   if( opptr[i]->op < DSIZE ) 
    printf( "( %s )\n", opst[opptr[i]->op] );
   else 
    printf( "( 0%o )\n", opptr[i]->op );
  }
 }
# endif

}  /* setrew */


match( p, cookie ) NODE *p; { 
/* called by: order, gencall
   look for match in table and generate code if found,
   unless entry specified REWRITE.
   returns MDONE, MNOPE, or rewrite specification from table */
/* SS. modified 27.apr.86  OPLTYPE check */

 register struct optab *q;
 register NODE *r;
 register o;

 o = p->in.op;
 rcount( p ); /* SS. 19.nov.86 added p as param for printing tree*/
 if( cookie == FORREW ) q = rwtable;  /* set ptr to 1. rewriting temp.*/
 else q = opptr[o];

 for( ; q->op != FREE; ++q ){

  if( q->op < OPSIMP ){  /* simple template */
   if( q->op != o ) continue;
  }
  else {      /* template for group of nodes */

  /* SS. 26.sep.86  added REG as leaf and removed the call to shltype  */
   register opmtemp;
   if((opmtemp=mamask[q->op - OPSIMP])&SPFLG){ /*OPLTYPE?*/
    if(  o != NAME  &&  o != ICON 
      && o != OREG  &&  o != REG  && o != FCON )
     continue;
   }
   else if( (dope[o]&(opmtemp|ASGFLG)) != opmtemp )
     continue;
  }

  /* opcode is ok! */
#ifndef BUG4
 if (mdebug)   /* SS. 29.apr.86  why not! */
  printf( " match op= %d, 0%o\n", q->op,q->op );
#endif
  if( !(q->visit & cookie ) ) continue;
  r = getlr( p, 'L' );   /* see if left child matches */
  if( !tshape( r, q->lshape ) ) continue;
  if( !ttype( r->in.type, q->ltype ) ) continue;
  r = getlr( p, 'R' );   /* see if right child matches */
  if( !tshape( r, q->rshape ) ) continue;
  if( !ttype( r->in.type, q->rtype ) ) continue;

   /* REWRITE means no code from this match but go ahead
      and rewrite node to help future match */
  if( q->needs & REWRITE ) return( q->rewrite );

  /* template is ok! */
  if( !allo( p, q ) ) continue;   /* if can't generate code */

  /* resources are available */
  expand( p, cookie, q->cstring ); /* generate code */
  reclaim( p, q->rewrite, cookie );

  return(MDONE);  /* we do it! */

 } /* for .. */

 return(MNOPE);   /* let's try next cookie */

}  /* match */

 

expand( p, cookie, cp ) NODE *p;  register char *cp; { /* F5 */
/* generate code by interpreting table entry */

 CONSZ val;

 rtyflg = 0;

 for( ; *cp; ++cp ){
  switch( *cp ){

  default:
   PUTCHAR( *cp );
   continue;  /* this is the usual case... */

  case 'Z':  /* special machine dependent operations */
   zzzcode( p, *++cp );
   continue;

  case 'C': {  /* for constant value only, output the func name */
   NODE *q;
   q =  getlr( p, *++cp );
   if( q->in.op == ICON )
/* tbl */   prints( q->in.name );
#ifndef BUG4
   else
    cerror( 281 );  /* illegal conput */
#endif
 /*  conput( getlr( p, *++cp ) ); */
   continue;
   }

  case 'O': { /* opcode string */
   hopcode( p->in.type, p->in.op );
   continue;
   }

#ifdef VAX
  case 'T':
   /* rewrite register type is suppressed */
   rtyflg = 1;
   continue;

  case 'F':  /* this line deleted if FOREFF is active */
   if( cookie & FOREFF ) while( *++cp != '\n' ); /* VOID */
   continue;

  case 'S':  /* field size */
   printf( "%d", fldsz );
   continue;

  case 'H':  /* field shift */
   printf( "%d", fldshf );
   continue;

  case 'M':  /* field mask */
  case 'N':  /* complement of field mask */
  /* SS. 8.apr.87 unsigned casts added */
   val = 1;
   val = (unsigned)val << fldsz;
   --val;
   val = (unsigned)val << fldshf;
   adrcon( *cp=='M' ? val : ~val );
   continue;

  case 'L':  /* output special label field */
   printf( "%d", p->bn.label );
   continue;

  case 'B':  /* byte offset in word */
   val = getlr(p,*++cp)->tn.lval;
   val = BYTEOFF(val);
   printf( CONFMT, val );
   continue;

  case 'I': /* in instruction */
   insput( getlr( p, *++cp ) ); /* SS. 26.sep.86 removed */
   continue;

  case 'A': /* address of */
   adrput( getlr( p, *++cp ) ); /* SS. may be called  */
   continue;       /* only from e2print  */

  case 'U': /* for upper half of address, only */
   upput( getlr( p, *++cp ) ); /* SS. 26.sep.86 removed */
   continue;
#endif

  }  /* switch */

 } /* for .. */

}  /* expand */


NODE *
getlr( p, c ) NODE *p; { /* F6 */
/* return the pointer to the left or right side of p, or p itself,
   depending on the optype of p */

   /* SS. 26.sep.86  removed the call to shumul */
 if( c == 'L' )
  return( optype( p->in.op ) == LTYPE ? p : p->in.left );

 else if( c == 'R' ) 
  return( optype( p->in.op ) != BITYPE ? p : p->in.right );

  else if( c== '1' || c == '2' || c == '3' )
   return( &resc[c-'1'] );
   else
    cerror( 266, c );  /* bad getlr: %c */

} /* getlr */


/* allo.c --  SS. 28.apr.86  modified */

allo( p, q ) NODE *p; struct optab *q; {

 register n, i, j, o;
 int either;

 n = q->needs;

#ifdef VAX
 either = ( EITHER & n );
#endif
 i = 0;

 while( n & NACOUNT ){
  resc[i].in.op = REG;
  resc[i].tn.rval = freereg( p, n&NAMASK );
  resc[i].tn.lval = 0;
  resc[i].in.name = "";
  n -= NAREG;
  ++i;
  }

  /* SS. 01.07.86 either modifier commented out */
#ifdef VAX
 if (either) { /* all or nothing at all */
  for( j = 0; j < i; j++ )
   if( resc[j].tn.rval < 0 ) { /* nothing */
    i = 0;
    break;
   }
  if( i != 0 ) goto ok; /* all */
 } /* either */
#endif

 o = p->in.op;
 if( n & NTMASK ){
  resc[i].in.op = OREG;
  resc[i].tn.rval = TMPREG;
  if( o==STCALL || o==STARG || o==UNARY STCALL || o==STASG )
           resc[i].tn.lval = freetemp( (SZCHAR*p->stn.stsize + (SZINT-1))/SZINT);
  else
   resc[i].tn.lval = freetemp( (n&NTMASK)/NTEMP );
  resc[i].in.name = "";
  /* SS. 8.apr.87 BITOOR performs >>, may be unsigned cast is needed*/
  resc[i].tn.lval = BITOOR(resc[i].tn.lval); 
  ++i;
 } /* ntmask */

 /* turn off "temporarily busy" bit */

 ok:
 REGLOOP(j){
  busy[j] &= ~TBUSY;
 }

 for( j=0; j<i; ++j ) if( resc[j].tn.rval < 0 ) return(0);
 return(1);

}  /* allo */



freetemp( k ){ /* allocate k integers worth of temp space */
/* we also make the convention that, if the number of words is more than 1,
/* it must be aligned for storing doubles... */

# ifdef BACKTEMP
 tmpoff += k*SZINT; /* SS. we using this part .. */
 if( k>1 )
  SETOFF( tmpoff, ALDOUBLE );
 if( tmpoff > maxoff ) maxoff = tmpoff;
 if( tmpoff >= offsz )
  cerror( 258 );  /* stack overflow */
 if( tmpoff-baseoff > maxtemp ) maxtemp = tmpoff-baseoff;
 return( -tmpoff );
# endif
}


freereg( p, n ) NODE *p; {
/* allocate a register of type n */
/* p gives the type, if floating */
/* SS. 21.mai.86 removed B type reg. alloc and callop checking */

 register j;

 j = p->in.rall & ~MUSTDO;
 if( j != NOPREF && usable(j) )  /* needed and not allocated */
  return( j );
 if( n&NAMASK ){
  for( j=mina; j<=maxa; ++j )
   if( istreg(j) && usable(j) ) return( j );
 }
 return( -1 );
} /* freereg */


usable( r ){
/* decide if register r is usable in tree p to satisfy need n */
/* SS. 11.mai 86 removed additional args and pairing for floating regs */

 /* checks, for the moment */
#ifndef BUG4
 if( !istreg(r) ) cerror( 259 ); /* usable asked about nontemp register*/
#endif
 if( busy[r] == 0 ) {
  busy[r] |= TBUSY;
  return(1);
 }

 /* busy[r]>1 or busy[r] is 1: is there chance for sharing */
 else
  return( 0 );

} 


recl2( p ) register NODE *p; {
/* SS. 28.apr.86 removed conditional compilation  and double indexing */

 if( p->in.op == REG || p->in.op == OREG )
  rfree( p->tn.rval );
}


rfree( r ){
/* mark register r free, if it is legal to do so */
/* SS. 17.jun.86 removed type argument and register pair checking */

# ifndef BUG3
 if( rdebug ) printf( "rfree( %s )\n", rnames[r] );
# endif
 if( istreg(r) && (--busy[r] < 0) ) 
  cerror( 260 );  /* register overfreed */
}


rbusy(r){
/* mark register r busy, t is the type */
/* SS.  23.jun.86  removed type argument and register pair checking */

# ifndef BUG3
 if( rdebug ) printf( "rbusy( %s )\n", rnames[r] );
# endif
 if( istreg(r) ) ++busy[r];

}


# ifndef BUG3
rwprint( rw ){   /* print rewriting rule */
 register i, flag;
 static char * rwnames[] = {

  "RLEFT",
  "RRIGHT",
  "RESC1",
  "RESC2",
  "RESC3",
  0,
 };

 if( rw == RNULL ){
  printf( "RNULL" );
  return;
 }

 if( rw == RNOP ){
  printf( "RNOP" );
  return;
 }

 flag = 0;
 for( i=0; rwnames[i]; ++i ){
  if( rw & ((unsigned)1<<i) ){
   if( flag ) printf( "|" );
   ++flag;
   printf( rwnames[i] );
  }
 }
 
 if( ( rw&RESCC ) &&  (flag) ) printf( "|RESCC" ); /* SS. 28.apr.86 */
 else if( rw&RESCC ) printf( "RESCC" );

}  /* rwprint */
# endif


reclaim( p, rw, cookie ) NODE *p; {   /* get back stuff */
/* SS. 28.apr.86 modified for Kronos */ 

 register NODE **qq;
 register NODE *q;
 NODE *recres[5];
 struct respref *r;
 int o = p->in.op;

# ifndef BUG3
 if( rdebug ){
  printf( "reclaim( %o, ", p );
  rwprint( rw );
  printf( ", " );
  prcook( cookie );
  printf( " )\n" );
 }
# endif
  /* SS. 28.apr.86 patches dealing with asgops, when address
     resides on top of stack.           */

 if( cookie&(INTAREG|FORCC|FORARG) &&
     ( myopasg(o) || ( o==ASSIGN && !canstore(p->in.left,cookie)))) 
/* tbl */ prints( (ISCHAR(p->in.type)) ? "\tlodt\n" : "\tlsw\t0\n");

/* SS. 7.apr.87  this line was: 
  prints( (ISCHAR(p->in.type)) ? "\tli\t0\n\tswap\n\tlxb\n":
            "\tlsw\t0\n");
*/

 if( rw == RNOP || ( o==FREE && rw==RNULL ) ) return;  /* do nothing */

 walkf( p, recl2 );

  /* SS. 11.mai 86   callchk(p) removed to avoid 
     "register allocation error" in situations like fptr=func();
     when adr already resides on top of stack */

 if( rw == RNULL || (cookie&FOREFF) ){ /* totally clobber, leaving nothing */
  tfree(p);
  return;
 }

 /* handle condition codes specially */

 if( (cookie & FORCC) && (rw&RESCC)) {
  /* result is CC register */
  tfree(p);
  p->in.op = CCODES;
  p->tn.lval = 0;
  p->tn.rval = 0;
  return;
 }

 /* locate results */

 qq = recres;

 if( rw&RLEFT)   *qq++ = getlr( p, 'L' );;
 if( rw&RRIGHT ) *qq++ = getlr( p, 'R' );
 if( rw&RESC1 )  *qq++ = &resc[0];
 if( rw&RESC2 )  *qq++ = &resc[1];
 if( rw&RESC3 )  *qq++ = &resc[2];

 if( qq == recres ){
  cerror( 261 );   /* illegal reclaim */
 }

 *qq = NIL;

 /* now, select the best result, based on the cookie */

 for( r=respref; r->cform; ++r ){
  if( cookie & r->cform ){
   for( qq=recres; (q= *qq) != NIL; ++qq ){
    if( tshape( q, r->mform ) ) goto gotit;
   }
  }
 }

 /* we can't do it; die */
 cerror( 262 );   /* cannot reclaim */

 gotit:

 if( p->in.op == STARG ) p = p->in.left;  /* STARGs are still STARGS */

 q->in.type = p->in.type;  /* to make multi-register allocations work */
             /* maybe there is a better way! */
 q = tcopy(q);

 tfree(p);

 p->in.op = q->in.op;
 p->tn.lval = q->tn.lval;
 p->tn.rval = q->tn.rval;
 p->in.name = q->in.name;
 p->in.stalign = q->in.stalign;

 q->in.op = FREE;

 /* if the thing is in a register, adjust the type */

 /* SS. 12.nov.86  don't change the type of intermediate results,
    because we must distinct assignement to char("lxb") from
    assignement to word("slw,sw,ssw"). See func wrdstore() also. */

#ifdef VAX
 switch( p->in.op ){

 case REG:
  if( !rtyflg ){
   TWORD t;
   t = p->in.type;
   /* the C language requires intermediate results to change type */
   /* this is inefficient or impossible on some machines */
   /* the "T" command in match supresses this type changing */
   /* SS. 10.feb.87 char is unsigned, don't convert it */
   if( t==CHAR || t==SHORT ) p->in.type = INT;
   else if( t==UCHAR || t==USHORT ) p->in.type = UNSIGNED;
  }
  return;     /* SS. 21.mai don't care of rall field */

    /* SS. 21. mai removed call for rmove */
 case OREG:
  if( busy[p->tn.rval]>1 && istreg(p->tn.rval) ) 
   cerror( 263 );  /* potential register overwrite */

 } /* switch */
#endif

} /* reclaim */


allchk(){  /* check to ensure that all registers are free */

 register i;

 REGLOOP(i){
  if( istreg(i) && busy[i] )
   cerror( 264 );   /* register allocation error */
 }
} 


allo0(){ /* free everything */

 register i;
 maxa = -1; mina = 0;

 REGLOOP(i){
  busy[i] = 0;
  if( istreg(i) ){
   if( maxa<0 ) mina = i;
   maxa = i;
  }
 }
}  /* allo0 */

#ifndef BUG4
int sdebug, tdebug, rdebug;
#endif
int fldsz, fldshf, rtyflg;
