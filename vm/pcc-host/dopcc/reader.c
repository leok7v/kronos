/* Revision log:
 * 25.feb.87 SS  ncopy() replaced with structure assignment
 * 27.Feb.87 SS  oreg2(): don't make oreg node from aggregate address subtrees
 * 11.Mar.87 SS  cdebug is now only one for Pass1 and Pass2
 * 30.Mar.87 SS  vdebug replaced with mdebug
 *   verbose added: if set output function names as processed
 * 09.Apr.87 SS  uflag added: if given set the sethi-ullman computations off
 * 22.Apr.87 MK   outline (asflag) added in p2compile
 * 09.Jun.87 SS  asflag brought here from scan.c, 
 * 25.Sep.87 SS   control the werror: A-stack ovfl, meaningful only if uflag > 0 
 *   the stkflag "S" introduced for this purpose
 * 20.Oct.87 SS  multiple changes related with the bug reported by Novosibirsk
 * 21.Oct.87 SS  introduced optflag for optimizing function calls,if given set off
 * 23.Dec.87 SS  added function help()
 * 28.Sep.88 SS  use Modula-2 function call method if flag m2call is set 
 */

# include "mfile2.h"

/* some storage declarations */

#ifdef STACK15
int stacksize = 15; /* SS 28.sep.88 for KRONOS P2.5 only */
#else
int stacksize = 7; /* SS. 07.07.86  size of KC Expression stack 0-6 */
#endif

NODE *deltrees[DELAYS];
int deli;
int fregs;
int callflag;

int nrecur;
int lflag;
     /* negatives of relationals */
int negrel[] = { NE, EQ, GT, GE, LT, LE, UGT, UGE, ULT, ULE } ;  

OFFSZ tmpoff;  /* offset for first temporary, in bits for current block */
OFFSZ maxoff;  /* maximum temporary offset over all blocks in current ftn, in bits */
OFFSZ baseoff;

NODE *stotree;
int stocook;

OFFSZ maxtemp;
int maxtreg;

extern int Wflag;
#ifndef BUG4
int edebug;
int xdebug; 
int udebug;
int mdebug;
int odebug;
#endif
int verbose; /* output function names, line numbers */
int m2calls; /* SS 28.sep.88 if set then pass args on the E-stack */
int uflag = 1; /* default do the sethi-ullman computations */
int asflag; /* if set then output C source lines too */
int stkflag; /* every occurence in the command line reduce the E-stacksize by one*/
int optflag= 1; /* default attempt to optimize parameter passing for calls */

p2init( argc, argv ) char *argv[];{ 
 /* set the values of the pass 2 arguments */

 register int c;
 register char *cp;
 register files;

 allo0();  /* free all regs */
 files = 0;

 for( c=1; c<argc; ++c ){
  if( *(cp=argv[c]) == '-' ){
   while( *++cp ){
    switch( *cp ){

    case 'X':  /* pass1 flags */
     while( *++cp ) { /* VOID */ }
     --cp;
     break;

    case 'l':  /* linenos */
     ++lflag;
     break;

    case 'v':  /* verbose mode */
     ++verbose;
     break;

    case 'f':  /* function calls as in Modula-2 */
     ++m2calls;  /* SS 28.sep.88 */
     break;

    case 'O': /* don't do sethi-ullman computations  */
     uflag = 0;
     break;

    case 'S': /* reduce the E-stacksize  */
     ++stkflag;
     break;

    case 'p': /* SS. 21.Oct.87 set off optimizing func calls */
     optflag = 0;
     break;

    case 'i': /* generate C and asm interlisting */
     ++asflag;
     break;

    case 'H':
    case 'h': /* help */
     help();
     break;
#ifndef BUG4
    case 'e':  /* expressions */
     ++edebug;
     break;

    case 'o':  /* orders */
     ++odebug;
     break;

    case 'r':  /* register allocation */
     ++rdebug;
     break;

    case 'm':  /* SS. 29.apr. template matching */
     ++mdebug;
     break;

    case 't':  /* ttype calls */
     ++tdebug;
     break;

    case 's':  /* shapes */
     ++sdebug;
     break;

    case 'u':  /* Sethi-Ullman testing (machine dependent) */
     ++udebug;
     break;

    case 'x':  /* general machine-dependent debugging flag */
     ++xdebug;
     break;
#endif

#ifdef VAX
    case 'a':  /* rallo */
     ++radebug;
     break;
#endif
    case 'c':  /* comments: var names and Pass2 title */
     ++cdebug;
     ++lflag; /* SS. 09.jan.87 */
     break;

    case 'w':
    case 'W':  /* shut up warnings */

     ++Wflag;
     break;

    default:
     fprintf( stderr, 
  "\nCommand line warning: ignoring unknown p2 flag %c\n", *cp );
     break;
/*       cerror( 254, *cp ); /* bad option: %c */
     }
    }
   }
  else files = 1;  /* assumed to be a filename */
  }

 if( stkflag ) /* SS. 25.Sep.87 we must perform the sethi-ullman comp.*/
  uflag = 1; 
 stacksize -= stkflag; /* SS. 25.Sep.87 reduce the E-stack size */
 mkdope();
 setrew();
 return( files );

}  /* p2init */


help(){
 extern char *release;
 char *namep =
#if unix
 "mypcc";
#else
 "pcc";
#endif
 char *datep = release + strlen(release) - 12;  /* SS set to date substring */

fprintf(stderr,"Portable C Compiler v1.0\t%s\n",datep);
fprintf(stderr,"\nUsage : %s [file] [options]\n\n", namep);
fprintf(stderr,
"  -v   verbose       -ic  C and assembler interlisting\n");
fprintf(stderr,
"  -S   stricter stack overflow check -O   don't perform stack optimizations\n");
fprintf(stderr,
"  -w   shut up warnings                 -p   don't optimize parameter passing\n");
fprintf(stderr,
"  -f   Modula-2 function calls\n"); /* SS 28.sep.88 new protocol */
exit (1);
}

p2compile( p ) NODE *p; { 
/* MK 22.Apr.87 if asflag, output the last line */
extern void outline();
 if (asflag) outline();
 else if( lflag )
  printf( "; #%d\n",lineno );
/* SS. 19.Nov.87  printf( "%% line: %d, file: %s\n",lineno,filename );  */
/* if( lflag ) lineid( lineno, filename );  */
 tmpoff = baseoff;  /* expression at top level reuses temps */
 /* generate code for the tree p */
# ifndef BUG4
 if( edebug > 1 ) fwalk( p, eprint, 0 );  /* print expr after Pass1 */
# endif

# ifdef MYREADER
 MYREADER(p);  /* do your own laundering of the input */
# ifndef BUG4
 if( edebug ){
  printf( "after myreader:\n" );
  fwalk( p,eprint, 0 );  /* SS. 15.mai  expr after canon */
 }
# endif
# endif
 callflag = nrecur = 0;  /* SS. 21.Oct.87 added callflag init */
 delay( p );  /* do the code generation */

 reclaim( p, RNULL, 0 );
 allchk();
 /* can't do tcheck here; some stuff (e.g., attributes) may be around from first pass */
 /* first pass will do it... */
} /* p2compile */


p2bbeg( aoff, myreg ) {   
 static int myftn = -1;

 tmpoff = baseoff = (unsigned int) aoff;
 maxtreg = myreg;
 if( myftn != ftnno ){ /* beginning of function */
  maxoff = baseoff;
  myftn = ftnno;
  maxtemp = 0;
  }
 else {
  if( baseoff > maxoff ) maxoff = baseoff;
  /* maxoff at end of ftn is max of autos and temps over all blocks */
  }
 setregs();
} /* p2bbeg */

p2bend(){ 
 SETOFF( maxoff, ALSTACK );
 eobl2();
 }



delay( p ) register NODE *p; { 
 /* look in all legal places for COMOP's and ++ and -- ops to delay */
 /* note; don't delay ++ and -- within calls or things like getchar */
 /* (in their macro forms) will start behaving strangely.           */
 register i;

 /* look for visible COMOPS, and rewrite repeatedly */

 while( delay1( p ) ) { /* VOID */ }

 /* look for visible, delayable ++ and -- */

 deli = 0;
 delay2( p );
 codgen( p, FOREFF );  /* do what is left */
 for( i = 0; i<deli; ++i ) codgen( deltrees[i], FOREFF );  /* do the rest */
} /* delay */


delay1( p ) register NODE *p;{ 
 /* look for COMOPS */

 if( optype(p->in.op) == LTYPE ) return( 0 );
 else if( optype(p->in.op) == UTYPE ) return( delay1( p->in.left ) );

 switch( p->in.op ){

 case QUEST:
 case ANDAND:
 case OROR:
  /* don't look on RHS */
  return( delay1(p->in.left ) );

 case COMOP:  /* the meat of the routine */
  delay( p->in.left );  /* completely evaluate the LHS */
  /* rewrite the COMOP */
  { register NODE *q;
   q = p->in.right;
   *p = *q; /* was ncopy() */ 
   q->in.op = FREE;
   }
  return( 1 );
  }

 return( delay1(p->in.left) || delay1(p->in.right ) );
} /* delay1 */


delay2( p ) register NODE *p;{ 

 /* look for delayable ++ and -- operators */

 switch( p->in.op ){

 case NOT:
 case QUEST:
 case ANDAND:
 case OROR:
 case CALL:
 case UNARY CALL:
 case STCALL:
 case UNARY STCALL:
 case FORTCALL:
 case UNARY FORTCALL:
 case COMOP:
 case CBRANCH: 
  /* for the moment, don't delay past a conditional context, or */
  /* inside of a func call */
  return;

 case UNARY MUL:
  /* if *p++, do not rewrite */
/* SS. 09-Feb-87  Sorry, Kronos has not autoincrement addressing mode.
  if( autoincr( p ) ) return;
*/
  break;

 case INCR:
 case DECR:
  if( deltest( p ) ){
   if( deli < DELAYS ){
    register NODE *q;
    deltrees[deli++] = tcopy(p);
    q = p->in.left;
    p->in.right->in.op = FREE;  /* zap constant */
    *p = *q;  /* was ncopy() */
    q->in.op = FREE;
    return;
    }
   }

  }

 if( optype(p->in.op) == BITYPE ) delay2( p->in.right );
 if( optype(p->in.op) != LTYPE ) delay2( p->in.left );

} /* delay2 */


codgen( p, cookie ) NODE *p; { 

 /* generate the code for p;    */
 /* order may call codgen recursively      */
 /* cookie is used to describe the context */

 for(;;){
  canon(p);  /* creats OREG from U* if possible and does sucomp */
  stotree = NIL;
# ifndef BUG4
  if( edebug > 2 ){ /* SS. 11.mai.86  don't print always */
   printf( "store called on:\n" );
   fwalk( p, eprint, 0 );
   }
# endif

/* SS. 01.07.86  storing subtrees commented out in current state of things */
 /* store(p); */

  if( stotree==NIL ) break;

  /* because it's minimal, can do w.o. stores */

  order( stotree, stocook );
  }
/* SS. 01.07.86  patch dealing with su numbers and E-stack overflow */

/* SS. 09.Jun.87 if we don't perform sethi-ullman computations then su fields have garbage */
 if( uflag && p->in.su >= stacksize ){ 
  werror( 27,stacksize );  /* expr too complicated:  E-stack overflow! */
#ifndef BUG4
  if( cdebug > 1 ){
   printf( "tree causing E-stack(%d) overflow:\n",stacksize);
   fwalk( p, eprint, 0 );
  }
#endif
 }

 order( p, cookie );

}  /* codgen */

# ifndef BUG4
  /* SS. 28.apr.86 SWADD replaced with FORREW */
  /* SS. 16.sep.86 SANY replaced with  FOREFF */
char *cnames[] = {
 "FOREFF",
 "SAREG",
 "STAREG",
 "SBREG",
 "STBREG",
 "SCC",
 "SNAME",
 "SCON",
 "SFLD",
 "SOREG",
 "STARNM",
 "STARREG",
 "INTEMP",
 "FORARG",
 "FORREW", 
 0,
 };

prcook( cookie ){

/* print a nice-looking description of cookie */

 int i, flag;

 if( cookie & SPECIAL ){
  if( cookie == SZERO ) printf( "SZERO" );
  else if( cookie == SONE ) printf( "SONE" );
  else if( cookie == SMONE ) printf( "SMONE" );
  else printf( "SPECIAL+%d", cookie & ~SPECIAL );
  return;
 }

 flag = 0;
 for( i=0; cnames[i]; ++i ){
  if( cookie & ((unsigned)1<<i) ){
   if( flag ) printf( "|" );
   ++flag;
   printf( cnames[i] );
  }
 }
 if( cookie&FORADR )  /* SS. 28.apr.86 added new cookie */
  printf( "FORADR" ); 

}  /* prcook */
# endif


nextcook( p, cookie ) NODE *p; {
/* we have failed to match p with cookie; try another */
/* SS. 24.jun.86  removed senseless cookies  */  

 if( cookie == FORREW ) return( 0 );  /* hopeless! */

 else if( cookie==FORARG ) cookie = INTAREG;
  else if( (cookie == FOREFF ) && !asgop(p->in.op) )
    cookie = INTAREG;

/* SS. 08.jun.86  remove at current state of things */
#ifdef VAX
  else if( !(cookie & INTEMP) && asgop(p->in.op) )
    cookie = INTEMP|INTAREG;
#endif
    else cookie = FORREW;
#ifndef BUG4
 if( mdebug ){
  printf( "-->" );
  prcook( cookie );
  printf( "\n" );
 }
#endif
 return( cookie );
}  /* nextcook */

int storeflag;

order(p,cook) NODE *p; { 

 register o, ty, m;
 int m1;
 int cookie;
 NODE *p1, *p2;

 cookie = cook;
 rcount(p);
 canon(p);
/* rallo( p, p->in.rall );  */
 goto first;
 /* by this time, p should be able to be generated without stores;
    the only question is how */

 again:

 if ( p->in.op == FREE )
  return;  /* whole tree was done */
 cookie = cook;
 rcount(p);
 canon(p);
/* rallo( p, p->in.rall );  */
 /* if any rewriting and canonicalization has put
  * the tree (p) into a shape that cook is happy
  * with (exclusive of FOREFF, FORREW, and INTEMP)
  * then we are done.
  * this allows us to call order with shapes in
  * addition to cookies and stop short if possible.
  */
 if( tshape(p, cook &(~(FOREFF|FORREW|INTEMP))) )return;

 first:
# ifndef BUG4
 if( odebug ){
  printf( "order( %o, ", p );
  prcook( cook );  /* SS. 17.sep.86  NB! initial value */
  if( callflag ) /* SS. 21.Oct.87 related with optimize calls */
   printf(" ), CALLFLAG= %d\n", callflag);
  else
   printf( " )\n" );
  fwalk( p, eprint, 0 );
  }
# endif

 o = p->in.op;
 ty = optype(o);

 /* first of all, for most ops, see if it is in the table */

 /* look for ops */

 switch( m = p->in.op ){

 default:
  /* look for op in table */
  for(;;){
   if( (m = match( p, cookie ) ) == MDONE ) goto cleanup;
   else if( m == MNOPE ){
    if( !(cookie = nextcook( p, cookie ) ) ) goto nomat;
    continue;
    }
   else break;
   }
  break;

 case COMOP:
 case FORCE:
 case CBRANCH:
 case QUEST:
 case ANDAND:
 case OROR:
 case NOT:
 case UNARY CALL:
 case CALL:
 case UNARY STCALL:
 case STCALL:
 case UNARY FORTCALL:
 case FORTCALL:
  /* don't even go near the table... */
  ;

  }
 /* get here to do rewriting if no match or
    fall through from above for hard ops */

 p1 = p->in.left;
 if( ty == BITYPE ) p2 = p->in.right;
 else p2 = NIL;
 
# ifndef BUG4
 if( odebug ){
  printf( "order( %o, ", p );
  prcook( cook );
  printf( " ), cookie " );
  prcook( cookie );
  printf( ", rewrite %s\n", opst[m] );
  }
# endif
 switch( m ){
 default:
  nomat:
  cerror( 255, opst[p->in.op] );  /* no table entry for op %s */

 case COMOP:
  /* SS. 20.Oct.87 fix the bug reported by Novosibirsk  */
  /* int c,k,i,q, ar[10];
  m(){ ar[i] = q ? (c= f(),k): 22; }
  the adr resides on top of stack, but f() was called without "store" */
  if( cook&(INTAREG|FORCC|FORARG) )
   p1->in.rall |= SPECIAL; /* NB! "store/lodfv" is needed*/
  codgen( p1, FOREFF );
  p2->in.rall = p->in.rall;
  codgen( p2, cookie );
  *p = *p2;  /* was ncopy() */
  p2->in.op = FREE;
  goto cleanup;

 case FORCE:
  /* recurse, letting the work be done by rallo */
  storeflag = p->in.rall & SPECIAL; /* SS. 20.Oct.87 */  
  p = p->in.left;
  cook = INTAREG;
  if( storeflag )  /* SS. 20.Oct.87 don't optimize store */
   goto again;
  /* SS. 16.sep.86  so we inform the code gen., that there is */
  /* no need to save the E-stack before the function call. */
  /* return( f() )   */
  if( callop(p1->in.op) )  /* SS. 17.sep.86  NB! STCALL */
   p1->in.rall |= MUSTDO;
  else /* return( (float) f() )  */
   if( (optype(p1->in.op)==UTYPE) && callop(p1->in.left->in.op) ) 
    p1->in.left->in.rall |= MUSTDO;
   else  /* return( f1() ? f2() : f3() ) */
    if( p1->in.op==QUEST ){
     NODE *p1r = p1->in.right;   
     if( callop(p1->in.left->in.op) )
      p1->in.left->in.rall |= MUSTDO;
     else  /* return( f()==2 ? 1 : 2 )  */
      if( logop(p1->in.left->in.op)
        && callop(p1->in.left->in.left->in.op) )
       p1->in.left->in.left->in.rall |= MUSTDO;
     if( callop(p1r->in.left->in.op) )
      p1r->in.left->in.rall |= MUSTDO;
     if( callop(p1r->in.right->in.op) )
      p1r->in.right->in.rall |= MUSTDO;
    }
  goto again;

 case CBRANCH:
  o = p2->tn.lval;
  if( !(p->in.rall & SPECIAL) ){ /* SS. 20.Oct.87 optimize stores */
  /* SS. 17.sep.86 ... there is no need to save the E-stack ... */
  /* if( f() ) */
  if( callop(p1->in.op) ) 
   p1->in.rall |= MUSTDO;
  /* SS. 9.apr.87 additonal store/lodfv tests added */
  else
   if( logop(p1->in.op) ){
    NODE *p1l = p1->in.left;
    /* if( f() < q )  */
    if( callop(p1l->in.op) )
     p1l->in.rall |= MUSTDO;
    /* if( ( l = f() ) == const )  */
   /* 09.Jun.87 p1l->in.left->in.su==0 ->canstore() */
    if(p1l->in.op==ASSIGN && canstore(p1l->in.left,INTAREG) 
      && callop( p1l->in.right->in.op) )
     p1l->in.right->in.rall |= MUSTDO; 
   }
   else  /* if( f() ? a : b )  */
    if( p1->in.op==QUEST && callop(p1->in.left->in.op) )
     p1->in.left->in.rall |= MUSTDO;
  } /* 20.Oct.87 end of optimizing */
  cbranch( p1, -1, o );
  p2->in.op = FREE;
  p->in.op = FREE;
  return;

 case QUEST:
  storeflag = p->in.rall & SPECIAL;  /* SS. 20.Oct.87 */
  cbranch( p1, -1, m=getlab() );
  /* SS. 16.sep.86  we cannot overwrite the MUSTDO set by ASSIGN*/
  /* p2->in.left->in.rall = p->in.rall; */
 
  /* SS. 16.sep.86 ... there is no need to save the E-stack ... */
  if( !storeflag && (cook&FOREFF) && callop(p2->in.left->in.op) )
   p2->in.left->in.rall |= MUSTDO;

  codgen( p2->in.left, INTAREG|INTBREG );
  /* force right to compute result into same reg used by left */

  /* SS. 16.sep.86  we cannot overwrite the MUSTDO set by ASSIGN*/
  /* p2->in.right->in.rall = p2->in.left->tn.rval|MUSTDO;  */

  /* SS. 17.sep.86 ... there is no need to save the E-stack ... */
  if( !storeflag && (cook&FOREFF) && callop(p2->in.right->in.op) )
   p2->in.right->in.rall |= MUSTDO;

  reclaim( p2->in.left, RNULL, 0 );
  cbgen( 0, m1 = getlab(), 'I' );
  deflab( m );
  codgen( p2->in.right, INTAREG|INTBREG );
  deflab( m1 );
  p->in.op = REG;  /* set up node describing result */
  p->tn.lval = 0;
  p->tn.rval = p2->in.right->tn.rval;
  p->in.type = p2->in.right->in.type;
  tfree( p2->in.right );
  p2->in.op = FREE;
  goto cleanup;

 case ANDAND:
 case OROR:
 case NOT:  /* logical operators */
  /* if here, must be a logical operator for 0-1 value */
  cbranch( p, -1, m=getlab() );
  p->in.op = CCODES;
  p->bn.label = m;
  order( p, INTAREG );
  goto cleanup;

 case FLD: /* fields of funny type */
  /* SS. 17.sep.86  fields are not implemented yet in KCC */
/*  cerror( 256 );    bit fields not implemented */
  if ( p1->in.op == UNARY MUL ){
   offstar( p1->in.left, cook );
   goto again;
   }

 case UNARY MINUS:
  order( p1, INTAREG );  /* SS. 28.apr. cookie changed */
  goto again;

 case NAME:
  /* all leaves end up here ... */
  if( o == REG ) goto nomat;
  order( p, INTAREG ); /* SS. 28.apr. cookie changed */
  goto again;

 case INIT:
  uerror( 74 );  /* illegal initialization */
  return;

 case UNARY FORTCALL:
  p->in.right = NIL;
 case FORTCALL:
  o = p->in.op = UNARY FORTCALL;
  if( genfcall( p, cookie ) ) goto nomat;
  goto cleanup;

 case UNARY CALL:
  p->in.right = NIL;
 case CALL:
  o = p->in.op = UNARY CALL;
  if( gencall( p, cookie ) ) goto nomat;
  goto cleanup;

 case UNARY STCALL:
  p->in.right = NIL;
 case STCALL:
  o = p->in.op = UNARY STCALL;
  if( genscall( p, cookie ) ) goto nomat;
  goto cleanup;

  /* if arguments are passed in register, care must be taken that 
  reclaim not throw away the register which now has the result... */

 case UNARY MUL:
  if( cook == FOREFF ){
   /* do nothing */
   order( p->in.left, FOREFF );
   p->in.op = FREE;
   return;
   }
  offstar( p->in.left, cook );
  goto again;

 case INCR:  /* INCR and DECR */
  if( setincr(p, cook) ) goto again;

  /* x++ becomes (x += 1) -1; */

  if( cook & FOREFF ){  /* result not needed so inc or dec and be done with it */
   /* x++ => x += 1 */
   p->in.op = (p->in.op==INCR)?ASG PLUS:ASG MINUS;
   goto again;
   }

  p1 = tcopy(p);
  reclaim( p->in.left, RNULL, 0 );
  p->in.left = p1;
  p1->in.op = (p->in.op==INCR)?ASG PLUS:ASG MINUS;
  p->in.op = (p->in.op==INCR)?MINUS:PLUS;
  goto again;

 case STASG:
  if( setstr( p, cook ) ) goto again;
  goto nomat;

 case ASG PLUS:  /* and other assignment ops */
  if( setasop(p, cook) ) goto again;

  /* there are assumed to be no side effects in LHS */
  /* try replacing =ops by binary ops */

  p2 = tcopy(p);
  p->in.op = ASSIGN;
  reclaim( p->in.right, RNULL, 0 );
  p->in.right = p2;
  p2->in.op = NOASG o;    /* SS. 28.apr.86 added this line  */
     /* its missing is serious error ! */

/*   SS. 28.apr.86  commented out because following actions produce illegal
      code in case of stack machine like Kronos       */ 
#ifdef VAX
  canon(p);
/*  rallo( p, p->in.rall );  */

# ifndef BUG4
  if( odebug ) fwalk( p, eprint, 0 );
# endif
  order( p2->in.left, INTBREG|INTAREG );
  order( p2, INTBREG|INTAREG );
#endif
  goto again;

 case ASSIGN:

  if( !(p->in.rall & SPECIAL)){ /* SS. 20.Oct.87 optimize store */
  /* SS. 17.sep.86 ... there is no need to save the E-stack ... */
  /* because we may assign the func result using "sw,slw"  */
  /* 09.Jun.87 p1->in.su==0 replaced with canstore(p1,INTAREG) */
  if( (cook == FOREFF) && canstore(p1,INTAREG) ){
   /* q = f()  */
   if( callop(p2->in.op) )
    p2->in.rall |= MUSTDO;
   else  /*  q = (char *)f();  */
   if( (p2->in.op==PCONV || p2->in.op==SCONV )
      && callop(p2->in.left->in.op) )
    p2->in.left->in.rall |= MUSTDO;
   else  /* q = f() ? f1() : f2()  */
   if( p2->in.op==QUEST ){
    NODE *p2r = p2->in.right;
    if( callop(p2->in.left->in.op) )
     p2->in.left->in.rall |= MUSTDO;
    if( callop(p2r->in.left->in.op) ) 
     p2r->in.left->in.rall |= MUSTDO;
    if( callop(p2r->in.right->in.op) ) 
     p2r->in.right->in.rall |= MUSTDO;
   }
   /* SS. 19.Oct.87 we may put the callop to left side */ 
   /* q = f() + w;    */
   else
   if( optype(p2->in.op)==BITYPE && callop(p2->in.left->in.op) )
    p2->in.left->in.rall |= MUSTDO;
  }
  } /* end of optimizing */
  if( setasg( p, cook ) ) goto again;
  goto nomat;


 case BITYPE:
  if( setbin( p, cook ) ) goto again;
  /* try to replace binary ops by =ops */
  switch(o){

  case PLUS:
  case MINUS:
  case MUL:
  case DIV:
  case MOD:
  case AND:
  case OR:
  case ER:
  case LS:
  case RS:
   p->in.op = ASG o;
   goto again;
   }
  goto nomat;

  }

 cleanup:
   /* SS. 21.mai my patch for debugging */
# ifndef BUG4
 if( odebug ){  
  printf( "cleanup: " );
  prcook( cook );
  printf( "\n" );
  fwalk( p, eprint, 0 );
 }
# endif
 /* if it is not yet in the right state, put it there */

 /* SS. 22.mai 86 in case of KC , patch to fix the problem that */
  /* there are unneeded result on  the top of stack -> "drop" it */
 /* NB! excluding results produced by func of type "void"(type=0) */
 if( cook & FOREFF ){    
  if( (p->tn.op == REG) && (p->tn.type != 0) )   
   match( p, FOREFF );
  else         
   reclaim( p, RNULL, 0 );
  return;
 }

 if( p->in.op==FREE ) return;

 if( tshape( p, cook ) ) return;

 /* SS. 16.sep.86 we had expr as argument and we computed it into the */
 /* REG, here we must go and generate the "stot" (in case of C call)  */
 if( (m=match(p,cook) ) == MDONE ) return; 

 /* we are in bad shape, try one last chance */
 if( lastchance( p, cook ) ) goto again;

 goto nomat;

} /* order */

/* SS. 13.oct.86  Start of store related stuff, commented out in current state */

#ifdef STORE
store( p ) register NODE *p; {

 /* find a subtree of p which should be stored */

 register o, ty;

 o = p->in.op;
 ty = optype(o);

 if( ty == LTYPE ) return;

 switch( o ){

 case UNARY CALL:
 case UNARY FORTCALL:
 case UNARY STCALL:
  ++callflag;
  break;

 case UNARY MUL:
  if( asgop(p->in.left->in.op) ) stoasg( p->in.left, UNARY MUL );
  break;

 case CALL:
 case FORTCALL:
 case STCALL:
  store( p->in.left );
  stoarg( p->in.right, o );
  ++callflag;
  return;

 case COMOP:
  markcall( p->in.right );
  if( p->in.right->in.su > fregs ) SETSTO( p, INTEMP );
  store( p->in.left );
  return;

 case ANDAND:
 case OROR:
 case QUEST:
  markcall( p->in.right );
  if( p->in.right->in.su > fregs ) SETSTO( p, INTEMP );
 case CBRANCH:   /* to prevent complicated expressions on the LHS from being stored */
 case NOT:
  constore( p->in.left );
  return;

  }

 if( ty == UTYPE ){
  store( p->in.left );
  return;
  }

 if( asgop( p->in.right->in.op ) ) stoasg( p->in.right, o );

 if( p->in.su>fregs ){ /* must store */
  mkadrs( p );  /* set up stotree and stocook to subtree
     that must be stored */
  }

 store( p->in.right );
 store( p->in.left );

} /* store */

constore( p ) register NODE *p; {

 /* store conditional expressions */
 /* the point is, avoid storing expressions in conditional conditional
    context, since the evaluation order is predetermined */

 switch( p->in.op ) {

 case ANDAND:
 case OROR:
 case QUEST:
  markcall( p->in.right );
 case NOT:
  constore( p->in.left );
  return;

  }

 store( p );
} /* constore */

markcall( p ) register NODE *p; {  /* mark off calls below the current node */

 again:
 switch( p->in.op ){

 case UNARY CALL:
 case UNARY STCALL:
 case UNARY FORTCALL:
 case CALL:
 case STCALL:
 case FORTCALL:
  ++callflag;
  return;

  }

 switch( optype( p->in.op ) ){

 case BITYPE:
  markcall( p->in.right );
 case UTYPE:
  p = p->in.left;
  /* eliminate recursion (aren't I clever...) */
  goto again;
 case LTYPE:
  return;
  }

} /* markcall */

stoarg( p, calltype ) register NODE *p; {
 /* arrange to store the args */

 if( p->in.op == CM ){
  stoarg( p->in.left, calltype );
  p = p->in.right ;
  }
 if( calltype == CALL ){
  STOARG(p);
  }
 else if( calltype == STCALL ){
  STOSTARG(p);
  }
 else {
  STOFARG(p);
  }
 callflag = 0;
 store(p);
# ifndef NESTCALLS
 if( callflag ){ /* prevent two calls from being active at once  */
  SETSTO(p,INTEMP);
  store(p); /* do again to preserve bottom up nature....  */
  }
#endif
} /* stoarg */

#endif
/* SS. 13.oct 86  End of store related stuff, commented out in current state */


cbranch( p, true, false ) NODE *p; { 
/* evaluate p for truth value, and branch to true or false      */
/* accordingly: label <0 means fall through    */
/* SS. 11.mai 86 modified to produce optimal code in case of KC */

 register o, lab, flab, tlab;

#ifndef BUG4
 if( odebug > 1 ) printf( "cbranch( %o, %d %d )\n", p, true, false );
#endif
 lab = -1;

 o  = p->in.op;

 switch( o ){

 case ULE:
 case ULT:
 case UGE:
 case UGT:
 case EQ:
 case NE:
 case LE:
 case LT:
 case GE:
 case GT:
  if( true < 0 ){
   o = p->in.op = negrel[ o-EQ ];
   true = false;
   false = -1;
   }
#ifndef NOOPT
  if( p->in.right->in.op == ICON && p->in.right->tn.lval == 0 
    && p->in.right->in.name[0] == '\0' ){
   switch( o ){

   case UGT:
   case ULE:
    o = p->in.op = (o==UGT)?NE:EQ;
   case EQ:
   case NE:
    if( logop(p->in.left->in.op) ){
    /* strange situation:e.g.,(a!=0)==0, we must */
    /* prevent reference to p->in.left->lable,so get 0/1 */
    /* we could optimize, but why bother */

      /* SS. 28.apr. cookie changed */
     codgen( p->in.left, INTAREG ); 
    }
  /* SS. 11.mai we make the next stmt. conditional, because */
  /*    we have not template for REG node with cookie FORCC */
    else
     codgen( p->in.left, FORCC );

 /* SS. 28.apr.86 NOTE : mode changed to "T" to distinct situation,
    when compare instruction is not needed.
     cbgen must only output jump instruction.  */
  
    cbgen( o, true, 'T' );  /* conditional branch */
    break;

 /* SS. 11.mai the following code is to fix the problem that there are
    not conditional jump instr. other than test top of stack for ZERO 
    in the instruction set of Kronos computer */

   case LE:  /* e.g. a<0 => llw a; li 0; lss; jcnd label.*/
   case LT:  /* This is bad code, but what I can do !    */
   case GE:
   case GT:
    p->bn.label = true;
    codgen( p, FORCC );
    break;

  /* SS. 25.feb.87 UGE & ULT: Don't set the condition codes,
  do side effects and branch or fall through */
   case UGE:
    codgen(tcopy(p->in.left), FOREFF); /* was FORCC */
    cbgen( 0, true, 'I' );  /* unconditional branch */
    break;
   case ULT:
    codgen( tcopy(p->in.left), FOREFF); /* was FORCC*/
    }
   }
  else
#endif
   {
   p->bn.label = true;
   codgen( p, FORCC );
   }
  if( false>=0 ) cbgen( 0, false, 'I' );
  reclaim( p, RNULL, 0 );
  return;

 case ANDAND:
  lab = false<0 ? getlab() : false ;
  cbranch( p->in.left, -1, lab );
  cbranch( p->in.right, true, false );
  if( false < 0 ) deflab( lab );
  p->in.op = FREE;
  return;

 case OROR:
  lab = true<0 ? getlab() : true;
  cbranch( p->in.left, lab, -1 );
  cbranch( p->in.right, true, false );
  if( true < 0 ) deflab( lab );
  p->in.op = FREE;
  return;

 case NOT:
  cbranch( p->in.left, false, true );
  p->in.op = FREE;
  break;

 case COMOP:
  codgen( p->in.left, FOREFF );
  p->in.op = FREE;
  cbranch( p->in.right, true, false );
  return;

 case QUEST:
  flab = false<0 ? getlab() : false;
  tlab = true<0 ? getlab() : true;
  cbranch( p->in.left, -1, lab = getlab() );
  cbranch( p->in.right->in.left, tlab, flab );
  deflab( lab );
  cbranch( p->in.right->in.right, true, false );
  if( true < 0 ) deflab( tlab);
  if( false < 0 ) deflab( flab );
  p->in.right->in.op = FREE;
  p->in.op = FREE;
  return;

 case ICON:
  if( p->in.type != FLOAT && p->in.type != DOUBLE ){

   if( p->tn.lval || p->in.name[0] ){
    /* addresses of C objects are never 0 */
    if( true>=0 ) cbgen( 0, true, 'I' );
    }
   else if( false>=0 ) cbgen( 0, false, 'I' );
   p->in.op = FREE;
   return;
   }
  /* fall through to default with other strange constants */

 default:
  /* get condition codes */
  codgen( p, FORCC );
   /* SS. 28.apr.86 mode arg changed to "T", see note
          on line 987.
   */
  if( true >= 0 ) cbgen( NE, true, 'T' );
  if( false >= 0 ) cbgen( true >= 0 ? 0 : EQ, false, 'T' );
  reclaim( p, RNULL, 0 );
  return;

  }

}  /* cbranch */

rcount(p) NODE *p; { 
  /* count recursions */
 if( ++nrecur > NRECUR ){
#ifndef BUG4
  printf( "tree causing compiler loop:\n" );
  fwalk( p, eprint, 0 );
#endif
  cerror( 257 );  /* expr causes compiler loop: try simplifying */
 }

}

 /* SS. 05.feb.87 ncopy(q,p) moved here from allo.c */

 /* copy the contents of p into q, without any feeling for the contents*/
 /* SS. 25.feb.87 there are no need for ncopy(), we use the struct asg */

  
NODE *     /* SS. 05.feb.87 moved here from allo.c */
tcopy( p ) register NODE *p; { /* make a fresh copy of p */
/* SS. 29.apr.86  removed double indexing */

 register NODE *q;

 q = talloc();
 *q = *p; /* was ncopy() */

 if( p->in.op == REG || p->in.op == OREG )
  rbusy( p->tn.rval ); /* SS. 31.mar.87 no need to test node type */
     /* the rbusy has only one argument now */

 switch( optype(q->in.op) ){

 case BITYPE:
  q->in.right = tcopy(p->in.right);
 case UTYPE:
  q->in.left = tcopy(p->in.left);
 }
 return(q);
} /* tcopy */


# ifndef BUG4
eprint( p, down, a, b ) NODE *p; int *a, *b; {

 *a = *b = down+1;
 while( down >= 2 ){
  printf( "\t" );
  down -= 2;
  }
 if( down-- ) printf( "    " );


 printf( "%o) %s", p, opst[p->in.op] );
 switch( p->in.op ) { /* special cases */

 case REG:
  printf( " %s", rnames[p->tn.rval] );
   /* SS. 4.june 86 following line added for debugging */
  printf( ", (l=%d,n=%s,sta=%d)", p->tn.lval, p->tn.name, p->tn.stalign );
  break;

 case FCON: /* SS. 13.mai 86 this node appears now in Pass 2 */
 case ICON:
 case NAME:
 case OREG:
  printf( " " );
  adrput( p ); /* SS. 11.mai this is the only place where we */
  break;  /* can call func "adrput" in case of KC */

 case STCALL:
 case UNARY STCALL:
 case STARG:
 case STASG:
  printf( " size=%d", p->stn.stsize );
  printf( " align=%d", p->stn.stalign );
  break;
 } /* case */

 printf( ", " );
 tprint( p->in.type );
 printf( "(%o), ", p->in.type );    /* SS. 15.nov. show type in octal */
 if( p->in.rall == NOPREF ) 
  ;
/*  printf( "NOPREF" ); */  /* SS. 15.nov.86 don't print NOPREF */
 else {
  register j;
  if( p->in.rall & SPECIAL ) printf( "STORE," );
  if( p->in.rall & MUSTDO ) printf( "MUSTDO," );
  j = (p->in.rall & ~MUSTDO) & ~SPECIAL;   /* SS. 20.Oct.87  */
  if( j != NOPREF )   /* so we control the printing */
   printf( "PREF,%s", rnames[j] );
  }
 printf( "SU=%d\n", p->in.su );

} /* eprint */
# endif


#define FIELDOPS /* SS. 04.jun.87 fields not implemented */

#ifndef FIELDOPS
 /* do this if there is no special hardware support for fields */

ffld( p, down, down1, down2 ) NODE *p; int *down1, *down2; {
 /* look for fields that are not in an lvalue context, and rewrite them... */
 register NODE *shp;
 register s, o, v, ty;

 *down1 =  asgop( p->in.op );
 *down2 = 0;

 if( !down && p->in.op == FLD ){ /* rewrite the node */

  if( !rewfld(p) ) return;

  ty = (szty(p->in.type) == 2)? LONG: INT;
  v = p->tn.rval;
  s = UPKFSZ(v);
# ifdef RTOLBYTES
 /* SS. 8.apr.87 UPKFOFF performs <<, may be unsigned cast is needed */
  o = UPKFOFF(v);  /* amount to shift */
# else
  o = szty(p->in.type)*SZINT - s - UPKFOFF(v); /* amount to shift */
#endif

  /* make & mask part */

  p->in.left->in.type = ty;

  p->in.op = AND;
  p->in.right = talloc();
  p->in.right->in.op = ICON;
  p->in.right->in.rall = NOPREF;
  p->in.right->in.type = ty;
  p->in.right->tn.lval = 1;
  p->in.right->tn.rval = 0;

  p->in.right->in.name = "";

/* SS. 8.apr.87 the next line was: p->in.right->tn.lval <<= s;  */
  p->in.right->tn.lval = (unsigned)p->in.right->tn.lval << s;
  p->in.right->tn.lval--;

  /* now, if a shift is needed, do it */

  if( o != 0 ){
   shp = talloc();
   shp->in.op = RS;
   shp->in.rall = NOPREF;
   shp->in.type = ty;
   shp->in.left = p->in.left;
   shp->in.right = talloc();
   shp->in.right->in.op = ICON;
   shp->in.right->in.rall = NOPREF;
   shp->in.right->in.type = ty;
   shp->in.right->tn.rval = 0;
   shp->in.right->tn.lval = o;  /* amount to shift */

   shp->in.right->in.name = "";
   p->in.left = shp;
   /* whew! */
   }
  }
} /* ffld */
#endif


notoff( off ) CONSZ off; {
/* is it legal to make an OREG or NAME entry which has an offset of off,
   (from a register of r), if the resulting thing had type t */

 if( off<0 )  off = -off;       /* SS. 21.mai the sign is senseless */

        return( !isitofs( off ) ? 1 : 0 );   /* don't make OREG, if ofs<0 or >255 */
}


oreg2( p ) register NODE *p; { 

/* look for situations where we can turn * into OREG */

 NODE *q;
 register r;
 register char *cp;
 register NODE *ql, *qr;
 CONSZ temp;
 TWORD t;

 if( p->in.op == UNARY MUL ){
  q = p->in.left;
  if( q->in.op == REG ){
   temp = q->tn.lval;
   r = q->tn.rval;
   cp = q->in.name;
   goto ormake;
   }

  if( q->in.op != PLUS && q->in.op != MINUS ) return;
  ql = q->in.left;
  qr = q->in.right;

  /* SS. 27.feb.87 we can't build OREG node 
     from aggregate address subtree */
  t = DECREF(ql->in.type), temp = ISAGGREGATE(t);  
  if( q->in.op == MINUS && isfpreg(ql) && qr->in.op==ICON
    && qr->in.name[0]=='\0' && temp ) return;


  if( (q->in.op==PLUS || q->in.op==MINUS) && qr->in.op == ICON &&
    ql->in.op==REG && szty(qr->in.type)==1) {
   temp = qr->tn.lval;
   if( q->in.op == MINUS ) temp = -temp;
   r = ql->tn.rval;
   temp += ql->tn.lval;
   cp = qr->in.name;
   if( *cp && ( q->in.op == MINUS || *ql->in.name ) ) return;
   if( !*cp ) cp = ql->in.name;

   ormake:

   if( notoff( temp ) ) return;
   p->in.op = OREG;
   p->tn.rval = r;
   p->tn.lval = temp;
    /* SS. 30.mai 86 attempt to use field stalign */
    /* for stab index for locals and args with    */
    /* purpose to output local var names  (debug) */
    /* see also func clocal() in local.c          */
   p->tn.stalign = ql->tn.stalign; 

   p->in.name = cp;
   tfree(q);
   return;
   }
  }

} /* oreg2 */

canon(p) NODE *p; { 
/* put p in canonical form */
 int oreg2(), sucomp();

#ifndef FIELDOPS
 int ffld();
 fwalk( p, ffld, 0 );  /* look for field operators */
# endif
 walkf( p, oreg2 );   /* look for and create OREG nodes */
#ifdef MYCANON
 MYCANON(p);     /* your own canonicalization routine(s) */
#endif
 if( uflag )
  walkf( p, sucomp );    /* do the Sethi-Ullman computation */

}

