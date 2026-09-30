/* Revision log:
 * 26.Jan.87 MK  entr now  in the beginning of func., no jumps -><- 
 * 20.Feb.87 MK  prints introduced for non-formatted output 
 * 30.Mar.87 SS  quiet mode added
 * 07.Apr.87 SS  in case of NAME node output "li0" in adrload()
 *   don't gen code in zzzcode() for "W" case label
 *   hardops() removed, code improved in cbgen() 
 * 08.May.87 SS  bug fixed in gencall(), void type illegal in expression
 * 04.Jun.87 SS  added warning to gencall(): void func as an argument
 * 03.Sep.87 SS  gencall(): rewritten code gen. for func returning struct
 * 25.Sep.87 SS  wrdload(): added generating directive .void before "lpc"
 *   gencall(): added generating directive .void before "cx .."
 *               removed qflag related stuff
 * 16.Oct.87 SS  added uerror(131) to gencall(): expr too complicated for modula
 * 21.Oct.87 SS  rewritten code related with unsigned shifts in hopcode()
 * 21.Oct.87 SS  introduced optimizing function calls in gencall()
 * 28.Dec.87 SS  fixed bug in zzzcode(), changed call to werror() in wrdload()
 */

/* a lot of the machine dependent parts of the second pass */

/*  This version written for Kronos computer by S.Siibak 25.apr.86
    Significantly modified functions:
 - eobl2;
 - hopcode;
 - tlen;
 - lineid;
 - zzzcode;
 - setregs;
 - gencall;
 - cbgen;
 - nextcook -> reader.c;
 - szty;
 - adrput;
 - optim2;
    Added functions:
 - canstore -> order.c;
 - wrdload;
 - adrload;
 - wrdstore;
 - genfcall;
    Removed functions:
 - conput;
 - where2;
 - adrcon;
 - canaddr;
 - mixtypes;
 - rmove;
 - callreg;
 - insput;

 - prtype;
 - base; 
 - offset;
 - makeor2;
 - shltype;
 - shumul;
 - hardops; (*
  ------------------------------
(* func may be included in the future

*/

# include "mfile2.h"

# define abs(x)    (x>=0 ? x : -(x))

/* # define BITMASK(n) ((1L<<n)-1) */

extern int ischarary();  /* SS. 16.nov.86 char ary type test from local.c */
extern int cdebug; /* SS. 25.jun.86 added for writing out var names */

/* SS. status of regs r0-r11 changed to temporary */
int rstatus[] = {
 STAREG, STAREG,
 STAREG, STAREG, STAREG, STAREG,
 STAREG, STAREG, STAREG, STAREG, STAREG, STAREG,
 SAREG, SAREG, SAREG, SAREG,

};

int maxlocals = 252; /* SS. 13.10.86 max # of locals in KC */
/* int gc_numbytes; /* really was a # of args in case of KC */


#ifdef VAX
lineid( l, fn ) char *fn; {
/* identify line l and file fn */
 printf( "%%  line: %d, file: %s\n", l, fn );
}
#endif

eobl2(){
 OFFSZ spoff; /* offset from stack pointer */
 extern int ftlab1, ftlab2;

  /* SS. 21.Oct.87 +1 for # of args saved with "store"  */
  spoff = maxoff/SZCHAR - 3;
  if( spoff > 252 )
   cerror( 225 );  /* too many local variables */
     /* ftlab1 is end of function label */
/* tbl */ printf("M%d\t.equ\t%d\n", ftlab1, spoff);
  maxargs = -1;
}  /* eobl2 */


struct hoptab { int opmask; char *opstring; } ioptab[] = {

 PLUS,   "add",
 MINUS,   "sub",
 MUL,  "mul",
 DIV,  "div",
 OR,  "or",
 ER,  "xor",
 AND,  "and",
 UNARY MINUS, "neg",
 LS,  "shl",
 LS,  "rol", /* SS. 12.Feb.87  for unsigned operands */
 RS,  "shr",
 RS,  "ror", /* SS. 12.Feb.87  for unsigned operands */
 -1, ""    };

hopcode( t, o ) TWORD t; {
/* output the appropriate string from the above table */
/* t - type of operands;  o - operator.  */

 register struct hoptab *q;

 for( q = ioptab;  q->opmask>=0; ++q ){
  if( q->opmask == o ){
   if( shiftop(o) && t==UNSIGNED ) /* simulate logical shift */
/* tbl */   ++q, prints( "\tand\n\tlodt\n" ); /* SS. 21.Oct.87 */
/* tbl */  printf( "%s%s\n",( ISFLOAT(t) ? "\tf" : "\t"), q->opstring );
   return;
  }
 }
 cerror( 275, opst[o] );  /* no hoptab for %s */

}  /* hopcode */

#ifndef BUG4
char *
rnames[] = {  /* keyed to register number tokens */

 "r0", "r1",
 "r2", "r3", "r4", "r5",
 "r6", "r7", "r8", "r9", "r10", "r11",
 "ap", "fp", "sp", "pc",

 };
#endif


tlen(p) NODE *p; {
/* SS. 27.apr.86  length of types changed */

 return(4);

#ifdef DIFFTYPES
 if( ISCHAR(p->in.type) )
  return(4); /* NB! in current state */
 else if( p->in.type==DOUBLE )
  return(4); /* NB! also in current state */
 else return(4);
#endif
} /* tlen */


/* SS. 5.apr.87  masks for implementing unsigned constant shift ops */
/* array represents results of shifting -1 by i, where i=0..31  */

static unsigned int
shift_mask[32] = {  0xffffffff, 0xfffffffe, 0xfffffffc, 0xfffffff8, 
   0xfffffff0, 0xffffffe0, 0xffffffc0, 0xffffff80,
   0xffffff00, 0xfffffe00, 0xfffffc00, 0xfffff800,
   0xfffff000, 0xffffe000, 0xffffc000, 0xffff8000,
   0xffff0000, 0xfffe0000, 0xfffc0000, 0xfff80000,
   0xfff00000, 0xffe00000, 0xffc00000, 0xff800000,
   0xff000000, 0xfe000000, 0xfc000000, 0xf8000000,
   0xf0000000, 0xe0000000, 0xc0000000, 0x80000000
   };

zzzcode( p, c ) register NODE *p; {
 register m;
 CONSZ val;

 switch( c ){
  
       /*  code generating for CCODES template  */
 case 'N':       /* logical ops, turned into 0-1 */
  cbgen( 0, m=getlab(), 'I' ); /* output unconditional jump */
          /* to the next label        */
  deflab( p->bn.label );  /* output CCODES node label value */
/* tbl */ prints( "\tli\t0\n" );     /* condition is false */ 
  deflab( m );           /* output the current label */
  return;

 case 'I':
 case 'P':  /* SS 28.Dec.87 fixed bug: we must test the type of child */
  if( ISFLOAT(p->in.left->tn.type) || ISFLOAT(p->in.right->tn.type))
/* tbl */  prints( "\tfcmp\n" );
  cbgen( p->in.op, p->bn.label, 'I' );
  return;

#ifdef OLDARGS
 case 'C': /* num words pushed on arg stack */
/* tbl */ printf("\tli %d\n stot", gc_numbytes );
  return;
#endif

 case 'D': /* SS. 26.sep.86 added to gen. "lsa" */
/* tbl */ printf( "\tlsa %d\n", p->in.right->tn.lval );
  return;

 case 'E': /* SS. 14.nov.86  INCR/DECR,(FOREFF),ASG PLUS/MINUS */
  if( p->in.right->in.op==ICON && p->in.right->tn.lval==1 ){
   prints((p->in.op==INCR || p->in.op==ASG PLUS ?
      "\tinc1\n" : "\tdec1\n") );
   return;
  }
  if( p->in.right->in.op != REG )  /* we must load it */
   wrdload( getlr( p, 'R' ) );
/* tbl */ prints((p->in.op==INCR || p->in.op==ASG PLUS ? "\tinc\n":"\tdec\n"));
  return;

 case 'U' :   /* SS. 5.apr.87 constant unsigned shift op */
     {
   int ic = p->in.right->tn.lval % 32;
   if( ic == 0 ) return; /* no need to shift by 0 */
/* tbl */  printf( "\tli\t%d\n\tand\n\tli\t%d\n\t",(p->in.op==LS ? 
   shift_mask[ic] >> ic : shift_mask[ic]), ic );
/* tbl */  prints( p->in.op == LS ? "rol\n" : "ror\n" );
   return;
  }
   
 case 'X' : /* SS. code generation for address template */
  {
  int r;
  TWORD t;
  t = DECREF( p->in.left->tn.type );
  r = p->in.left->tn.rval; /* base ptr reg. */
  if ( ISAGGREGATE(t) ){ /* ARY,STRTY or UNIONTY */
   if ( r == FP ){
/* tbl */        printf( "\tllw  %d",p->in.right->tn.lval );
/* tbl */      if(cdebug) printf( "\t%%&%s", stab[p->in.left->tn.stalign].sname );
       prints( "\n" );
       return;
   }
   cerror( 276 );  /* zzzcode - aggr. base ptr illegal */
  }
  if (r != FP && r != AP ) 
   cerror( 277 );   /* zzzcode - illegal base ptr */
/* tbl */ printf( "\tlla %d", p->in.right->tn.lval );
#ifdef OLDARGS
/* tbl */ printf( "\t%s\t%d\n",
    ( r == FP ? "lla" : "lpa" ), p->in.right->tn.lval );
#endif
/* tbl */ if(cdebug ) printf( "\t%%&%s", stab[p->in.left->tn.stalign].sname);
  prints( "\n" );
  return;
  }

 case 'W' : { /* SS. generate code for load Word instruction */
  NODE *q;
  q = getlr( p, 'L' );
  if( ISCHAR(q->in.type) && (q->in.op==NAME || q->in.op==OREG) ){
   adrload( q );
   prints( "\tlxb\n" );
  }
  else
   wrdload( q ); /* SS. 8.apr.87 was getlr() */
  return;
  }

 case 'A' : /* SS. generate code for load Address instruction */
  adrload( getlr( p,'L' ) );
  return;

 case 'M' : /* SS. generate code for store word instruction */
  wrdstore( getlr( p,'L' ) );
  return;

 case 'S':  /* structure assignment */
   if( p->stn.stsize > 65536 )  /* 12.feb.87 ? */
    werror( 31 );  /* struct size > 64K */

/* tbl */  printf("\tli\t%d\n\tmove\n", p->stn.stsize );
   return;
 
#ifndef BUG4
 case 'K':
  werror( 30 );  /* wrong template catched! */
  return;
#endif

#ifndef BUG4
 default:
  cerror( 280 );  /* illegal zzzcode */
#endif
 }  /* switch */
}  /* zzzcode */


wrdload( p ) register NODE *p; {
/* SS. output an word load instruction, with offsets, from p */

 register int r,lv;

 lv = p->tn.lval;
 r  = p->tn.rval;
 
 switch( p->in.op ){

 case NAME :
     if( r==NONAME ){ /* absolute address e.g. *(int *)100=... */
/* tbl */ printf( "\tli\t%d\t%%&\n\tlsw\t0\n", lv );
  return;
     }
/* tbl */   printf( "\tlw %s\n", p->tn.name );
     if( ISAGGREGATE( stab[r].stype ) ){ /* ARY,STRTY or UNIONTY */
  if( lv < 0 )   
   cerror( 282 );  /* wrdload - illegal NAME ofs */

  if( isitofs( lv ) )
/* tbl */  printf( "\tlsw %d\n", lv );
  else
/* tbl */  printf( "\tli\t%d\n\tlxw\n",lv);  /* 8.dec.86 */
     }
     return;

 case FCON :
/* tbl */    prints( " li " ); /* simple float const */
/* tbl */ printf( FCONFMT, p->fpn.dval );
/* tbl */  prints( "\n" );
  return;
     
 case ICON :
     if( p->in.name[0] == '\0' ){ /* simple const */
/* tbl */    prints( " li " );
/* tbl */ printf( CONFMT, lv );
/* tbl */ prints( "\n" );
  return;
     }
     else if(  r < 0 ){    /* string label,cvt to byteadr */
/* tbl */ printf( " lsta %s\n shl2\n", p->tn.name );
  return;
     }
  /* in the following cases there must be entry in symbol table */
  /* this means that the name[0] != '\0' and rval >= 0  */

     if( ISFTN( DECREF(p->tn.type)) ){ /* ICON as PTR to FTN */
/* SS. 25.sep.87 we must test type from stab, type in tree may be different */
  if( BTYPE(stab[r].stype) == UNDEF )  /* SS. 25.Sep.87 for identifying void func */  
    D_VOIDCALL;  
/* tbl */     printf( "\tlpc %s\n", p->tn.name );
  return;
     } 
       
          /* ICON as global or extern adr const */
     if( ISAGGREGATE(stab[r].stype)){  /* base type is ARY,STRTY or UNIONTY */
/* tbl */ printf( "\tlw  %s\n",p->tn.name );
      if( !lv ) return;
  if( lv < -1 )  /* SS. 28.Dec.87 was 0,but so I avoid some silly warnings */
   werror( 29 );   /* negative subscript */
  if( isitofs( lv ) )
/* tbl */  printf( "\tlsa %d\n", lv );
  else
/* tbl */  printf( "\tli %d\n add\n", lv );
  return;
     }
/* tbl */   printf( "\tla %s\n", p->tn.name ); /* scalar adr const */
     return;
   
 case REG :
     ;   /* called from OPLTYPE with cookie FORARG or from UNARY CALL
       in the case when the PTR to FTN resides on top of stack */
     return;


 case OREG :
     if( istreg(r) ){  /* load stack addressed var */
  if( isitofs(lv) )
/* tbl */     printf( "\tlsw %d\n", lv );
  else cerror( 284, p->in.op );
  return;
     }
     
     if( r == FP ){
/* tbl */     printf( "\tllw\t%d", abs(lv) );  /* SS. 09.jan.87 */
#ifdef OLDARGS
     if( r == FP || r == AP ){ /* load the local var or argument */
/* tbl */ printf( "\t%s\t%d", ( r==FP ? "llw" : "lpw" ), abs(lv) );
#endif
/* tbl */ if(cdebug) printf( "\t%%%s",stab[p->tn.stalign].sname );
  prints( "\n" );
  return;
     }
     cerror( 283 );  /* wrdload - illegal base ptr */
 default :
     cerror( 284, p->in.op );  /* wrdload - illegal node: %d */
 }  /* switch */

}  /* wrdload */


wrdstore( p ) register NODE *p; {
/* SS. 19.jun.86  output an store word instruction */

 register int r,lv;

 lv = p->tn.lval;
 r  = p->tn.rval;
 
 switch( p->in.op ){

 case NAME :
/* tbl */   printf( "\tsw %s\n", p->tn.name );
     if( ISAGGREGATE( stab[r].stype ) )  /* ARY,STRTY or UNIONTY */
   cerror( 285 );  /* wrdstore - illegal lhs */
     return;

 case OREG :
     if ( r == FP ){     /* store local var */
/* tbl */ printf( "\tslw %d", -lv ); 
/* tbl */ if(cdebug) printf( "\t%%%s",stab[p->tn.stalign].sname );
  prints( "\n" );
  return;
     }
     if( istreg(r) ){  /* store stack addressed wrd */
/* tbl */ printf( "\tssw %d\n", lv );
  return;
     }
     if( r == SP ){
/* tbl */ prints( "\tsxw\n" ); /* 16.dec.86 */
  return;
     }
     cerror( 286 );  /* wrdstore - illegal base ptr */

 case REG:
/* tbl */   prints( (ISCHAR(p->tn.type) ? "\tsxb\n" : "\tssw\t0\n"));
     return;
 
 default :
     cerror( 287, p->in.op );  /* wrdstore - illegal node: %d */
 }  /* switch */

}  /* wrdstore */


adrload( p ) register NODE *p; {
/* SS. output an address load instruction, with offsets, from p */
 register int lv,r;

 lv = p->tn.lval; /* save the offset */
 r  = p->tn.rval;

 switch( p->in.op ){

 case NAME :
     if( ISCHAR(p->in.type) )
/* tbl */ prints( "\tli\t0\n" ); /* SS. 7.apr.87 prepare for "sxb" */

     if( r==NONAME ){ /* absolute address for e.g. *(int *)100=... */
/* tbl */ printf( "\tli\t%d\t%%&\n", lv );
  return;
     }
     if( ISAGGREGATE(stab[r].stype) ){  /* base type:ARY|STRTY|UNIONTY */
/* tbl */ printf( "\tlw  %s\n",p->tn.name );
      if( !lv )  return;
  if( lv < 0 )   
   cerror( 288 );   /* adrload - illegal ofs */
  if( isitofs( lv ) )
/* tbl */  printf( "\tlsa %d\n", lv );
  else
/* tbl */  printf( "\tli %d\n\tadd\n", lv );
  return;
     }
/* tbl */   printf( "\tla\t%s\n", p->tn.name );  /* scalar adr const */
     return;

 case REG :
     cerror( 289 );  /* adrload - adr. of stack, may be include ? */

 case OREG :
     if( istreg(r) ){  /* load stack address */
  if( lv ) /* non Zero ofs */
/* tbl */      printf( "\tlsa %d\n", lv );
  return;
     }
     /* SS. 25.Sep.87 if OLDARGS then must be test for "AP" also,removed */
     if( r == FP ){  /* local var or struct adr */
/* tbl */ printf( "\t%s\t%d", 
/* tbl */  ( (p->tn.type==STRTY || p->tn.type==UNIONTY) ? "llw" : "lla" ), -lv ); /* STASG */

#ifdef OLDARGS
      else     /* argument address */
/* tbl */        printf( " lpa %d", lv );
#endif

/* tbl */ if(cdebug) printf( "\t%%&%s",stab[p->tn.stalign].sname );
  prints( "\n" );
  return;
     }    
     cerror( 290 );  /* adrload - illegal base ptr */

 default :
     cerror( 291, p->in.op );  /* adrload - illegal adr node: %d */
 } /* switch */
}  /* adrload */


# ifndef BUG4

adrput( p ) register NODE *p; {
/* output an address, with offsets, from p */
/* SS. 28.apr.86  in case of Kronos called only from e2print for debugging. 
   removed double indexing test and UNARY MUL nodes: STARNM and STARREG. 
   Added printing of rval value and base type for aggregates.
*/
 register int r,lv;
 struct symtab *q;

 r =  p->tn.rval;
 lv = p->tn.lval;

 if( p->in.op == FLD ){
  p = p->in.left;
 }
 switch( p->in.op ){

 case NAME:
  acon( p );
  if( r != NONAME ){     /* not a absolute address*/
   printf( ", r=%d (", r );  /* or debug */
   if(ISAGGREGATE( stab[r].stype )){
    tprint( stab[r].stype );  /* print base type */
    printf( " %o", stab[r].stype );
   }
   printf( ")" );
  }
  return;

 case FCON: /* SS. 13.mai 86 this node now reaches to code gen. */
  printf( "%f", p->fpn.dval ); 
  return;

 case ICON:
  /* addressable value of the constant */
  printf( "$" );
  acon( p );
  if( r != NONAME ){            /* not simple const */
   printf( ", r=%d (", r );          /* for debug */
   if(ISAGGREGATE( stab[r].stype )){
    tprint( stab[r].stype );  /* print base type */
    printf( " %o", stab[r].stype );
   }
   printf( ")" );
  }
  return;

 case REG:
  printf( "%s", rnames[r] );
  return;

 case OREG:
  if( r == AP ){    /* in the argument region */
   if( lv <= 0 || p->in.name[0] != '\0' ) 
  /*  werror( 31 );   /* bad arg temp */
   printf( CONFMT, lv );
   printf( "(ap)" );
   return;
  }
  if( lv != 0 || p->in.name[0] != '\0') acon( p );
  printf( "(%s)", rnames[r] );
  return;

 default:
  cerror( 292, p->in.op );  /* adrput - illegal adr node: %d */

 } /* switch */

}  /* adrput */


acon( p ) register NODE *p; { /* print out a constant */

 if( p->in.name[0] == '\0' ){
  printf( CONFMT, p->tn.lval);
 }
 else if( !p->tn.lval ) {
   printf( "%s", p->in.name );
 }  
  else {
   printf( "%s+", p->in.name );
   printf( CONFMT, p->tn.lval );
  }
}  /* acon */
#endif


genscall( p, cookie ) register NODE *p; {
/* structure valued call */
 return( gencall( p, cookie ) );
}

extern int uflag; /* true if we perform sethi-ullman computations */
extern int optflag; /* attempt to optimize calls, set in reader.c */

gencall( p, cookie ) register NODE *p; {
/* generate the  "C Call & Modula Call"  given by tree p */

 register temp;
 register m, ccall, stcall,loc_callflag;
 int optcall; /* 21.Oct.87 true if we optimizing function call */

#ifndef BUG4
 if (odebug){
  printf( "gencall( %o, %s, ", p, opst[p->in.op] );
  prcook( cookie );
  if( p->in.rall & MUSTDO )  /* SS. 16.sep.86 we don't save E-stack */
   printf( ", MUSTDO" );
  printf( " ), CALLFLAG= %d\n", callflag );
/*  printf( " )\n" ); */
 }
#endif
 optcall = 0; /* SS. 21.Oct.87 */
 if( cookie&(INTAREG|FORCC) && p->in.type==UNDEF ){ /* SS. 4.jun.87 */
   uerror( 109 );  /* void type illegal in expression */
   return(0);
 }
 ccall = !(p->in.op == UNARY FORTCALL);  /* 12.dec. NB! STCALL also */
 stcall = (p->in.op == UNARY STCALL);

 loc_callflag = callflag;  /* SS. 21.Oct.87 save the previous state */
 /*  get # of arguments KC ver. */
 temp = p->in.right ? argsize( p->in.right ) : 0;
#ifndef BUG4
 if( odebug > 1 )
  printf("args= %d, loc_callflag= %d, callflag= %d\n",temp,loc_callflag,callflag);
#endif

 if( ccall && optflag && uflag && (loc_callflag==callflag) && (p->in.su + temp) <= stacksize+1 && temp > 1 ){ 
  temp += 1; /* 21.Oct.87 save room for # of args to, which stored using "store" */
  ++optcall;
 }

 if( !ccall ){   /* 11.dec.86 only for MODULA call */
  if( uflag && (p->in.su + temp) > stacksize){ /* SS. 16.Oct.87 */
   uerror( 131,stacksize ); /* expr too complicated for modula call ... */
   return(0);
  }
  else
   if( temp > stacksize ){
    uerror( 125,stacksize );  /* modula call with too many args... */
    return(0);
   }
 }

 if( cookie&(INTAREG|FORCC|FORARG) && !(p->in.rall&MUSTDO) )
/* tbl */  prints( "\tstore\n" );  /* SS. 16.sep.86 E-stack isn't empty, save it */

 if( ccall && temp ) /* 11.dec.86 only for C call */
 /* if there are args, then allocate space for proc activation record */
/* tbl */ printf( "\tentr\t%d\n", (AUTOINIT/SZINT)+1 ); /* 6 words,23.oct.87 now 4 */

 if( p->in.right )  /* SS. 21.Oct.87 added optcall arg */
  genargs( p->in.right,ccall,optcall ); /* gen. code for the args */

 if( optcall ){  /* SS. 21.Oct.87 another method for passing args */
/* tbl */ prints( "\tstore\t\t;$args\n" );
 }

 if( p->in.left->in.op  != ICON  /* extern func name */
   && p->in.left->in.op != REG   /* ptr to ftn is on top of stack */
   && p->in.left->in.op != OREG  /* ptr to ftn is arg. or local  */
   && p->in.left->in.op != NAME )
  order( p->in.left, INTAREG ); /* must gen. code for the ptr to ftn */

 /* 30.jan.87 was used as # of args, now includes the struct ret addr */
/*  gc_numbytes = temp;  */ 

 if( ccall && temp ) /* 11.dec.86 only for C call */
 /* if there are args, then decrement the S register: args+4,was 6 */
/* tbl */ printf( "\tli\t%d\n\tdecs\n", temp+(AUTOINIT/SZINT)+1 );

/* SS. 28.Sep.87 void func call, if left is not ICON then there is a call by  */
/* ptr to func, which gen.code "lw ptr_ftn;stot;cf" and the func name is not known */
 if( p->in.type == UNDEF && p->in.left->in.op == ICON )
/* tbl */ D_VOIDCALL;
  
 p->in.op = UNARY CALL;  /* to change STCALL or UNARY STCALL */
 m = match( p, INTAREG ); /* generate code for call */

 if( m != MDONE ) /* We cannot find the template */
  return( 1 );

 if( cookie&(INTAREG|FORCC|FORARG) && !(p->in.rall&MUSTDO) )
/* tbl */  prints( "\tlodfv\n" );  /* SS. 16.sep.86 E-stack wasn't empty, restore it */
 return( 0 ); /* We do it! */

}  /* gencall */


/* SS. Conditional jump  instructions are replaced by 
       logical instructions. */

/* tbl */
char *
ccbranches[] = {
 "\tequ\n", /* equal signed */ 
 "\tneq\n", /* not equal signed */ 
 "\tleq\n", /* less than or equal signed */
 "\tlss\n", /* less than signed */
 "\tgeq\n", /* greater than or equal signed */
 "\tgtr\n", /* greater than signed */
 "\tleq\n", /* less than or equal unsigned */
 "\tlss\n", /* less than unsigned */
 "\tgeq\n", /* greater than or equal unsigned */
 "\tgtr\n", /* greater than unsigned */
 };
           
extern int negrel[];    /* negatives of relationals(from reader.c) */

cbgen( o, lab, mode ) { 
/* printf conditional and unconditional branches */
/* SS. We must use the mode argument to decide whether to generate
       code for logical op. between two operands or only test top of stack. */

 if( o == 0 ){ 
/* tbl */ printf( "\tjump M%d\n", lab ); /* unconditional jump */
  return;
 } 

#ifndef BUG4
 if( o > UGT ) 
  cerror( 293, opst[o] );  /* bad conditional branch: %s */ 
 if( mode != 'T' && mode != 'I' )
  cerror( 294 );   /* cbgen - illegal mode */
#endif
 if( mode == 'I' ){        /* the OPLOG with two op. */
  o = negrel[o-EQ];
/* tbl */ prints( ccbranches[o-EQ] );  /* output log.instr. */
 }
 else if( o == NE )
/* tbl */ prints( "\tnot\n" ); 

/* tbl */ printf( "\tjcnd\tM%d\n", lab );

}  /* cbgen */



optim2( p ) register NODE *p; {
/* SS. 06.nov.86  do local tree transformations and optimizations */

 register NODE *l = p->in.left;

 if( p->in.op==MINUS )
  /* the following code is for patching the illegal offset
     nodes passed from pass1: local char array adr subtree */
  if( isfpreg(l) && ischarary(stab[l->tn.stalign].stype) 
     && p->in.right->in.op==ICON && p->in.right->tn.name[0]=='\0'){
   p->in.right->tn.lval >>= 2;
   return;
  }
 if( p->in.op == CALL || p->in.op == UNARY CALL )
  /* SS. 16.sep.86  patch to fix the problem, that we cannot get 
     the FORTCALL nodes from pass1 if we have ptr to fort func */
  if( l->in.op == NAME && ( stab[l->tn.rval].sclass == FORTRAN )){
   p->in.op += (FORTCALL-CALL);
   return;
  }
}  /* optim2 */


lastchance( p, cook ) NODE *p; {
/* forget it! */
 return(0);
}


setregs(){ /* set up temporary registers */
/* SS. give all the regs(0-11) free for code generation */
 fregs = 11; /* tbl- 6 free regs on VAX (0-5) */
 ;
}


szty(t){ /* size, in registers, needed to hold thing of type t */
/* SS. 28.apr.86  modified all types to the same length  */
 return( ( t == DOUBLE ) ? 1 : 1 );
}


shtemp( p ) register NODE *p; {
/* called from tshape, related with storing intermediate results */
/* SS. 26.sep.86  removed the call to shumul */
 register int o = p->in.op;

 return( o==NAME || o==ICON || o==OREG );
}


rewfld( p ) NODE *p; {
/* called from ffld, to test is it legal to rewrite the FLD node */ 
 return(1);
}

flshape( p ) register NODE *p; {
/* called from tshape, related with fields */
 register int o = p->in.op;

 return( o==REG || o==NAME || o==ICON || o==OREG );
}


myreader(p) register NODE *p; {
/* SS. 30.apr.86  canon must be included  */

/* walkf( p, hardops );  /* convert ops to function calls */

 canon( p );  /* expands r-vals for fileds */

 walkf( p, optim2 ); /* do local optimizations    */
 /* jwf toff = 0;  /* stack offset swindle */

}  /* myreader */

