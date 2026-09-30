/* Revision log:
 * 14.Apr.87 SS  store/lodfv related changes in genargs()
 * 05.Jun.87 SS  void func as an arg: fixed bugs in genargs(),argsize()
 * 25.Sep.87 SS  removed qflag related stuff
 * 16.Oct.87 SS  some changes in sucomp() related with callops
 * 19.Oct.87 SS  fixed bug in sucomp(): in case of commop(o) put callop always 
 *   to the left side of operation.
 * 21.Oct.87 SS  code related with unsigned shift ops rewritten in setbin()
 * 21.Oct.87 SS  mark calls in argsize()
 * 18.Nov.87 SS  fixed bug in argsize()
 */

# include "mfile2.h"

/* 
   This version rewritten for Kronos computer.
   Significantly modified following functions:
 - setstr;
 - sucomp;
 - offstar;
 - setincr;
 - setbin;
 - setasg;
 - setasop;
 - argsize;
 - genargs;
    With minor differences from VAX version:
 - deltest;
 - notoff -> reader.c;
 - deflab;
    Added new functions:
 - canstore;
    Not used in current version:
 - stoasg;
 - rallo;
 - mkadrs;
    Removed functions:
 - autoincr;
*/

int maxargs = { -1 }; /* max size of arguments in current block */

/* SS. 07.Apr.87 some more used code sequences : */

char *li0 = "\tli\t0\n"; /* tbl */
char *copt = "\tcopt\n"; /* tbl */
char *copt_swap = "\tcopt\n\tli\t0\n\tswap\n"; /* tbl */


#ifdef STORE

stoasg( p, o )  register NODE *p; {
/* should the assignment op p be stored, given that it lies 
as the right operand of o (or the left, if o==UNARY MUL) */
/*
 if( p->in.op == INCR || p->in.op == DECR ) return;
 if( o==UNARY MUL && p->in.left->in.op == REG 
   && !isbreg(p->in.left->tn.rval) ) SETSTO(p,INAREG);
*/
} /* stoasg */
#endif


deltest( p ) register NODE *p; {
/* should we delay the INCR or DECR operation p */
/* SS. removed REG node as candidate for delay  */

 return( p->in.left->in.op == NAME || p->in.left->in.op == OREG );
}


#ifdef STORE
mkadrs(p)  register NODE *p;
{
 register o;

 o = p->in.op;

 if( asgop(o) ){
  if( p->in.left->in.su >= p->in.right->in.su ){
   if( p->in.left->in.op == UNARY MUL ){
    SETSTO( p->in.left->in.left, INTEMP );
    }
   else if( p->in.left->in.op == FLD 
                               && p->in.left->in.left->in.op == UNARY MUL ){
    SETSTO( p->in.left->in.left->in.left, INTEMP );
    }
   else { /* should be only structure assignment */
    SETSTO( p->in.left, INTEMP );
    }
   }
  else SETSTO( p->in.right, INTEMP );
  }
 else {
  if( p->in.left->in.su > p->in.right->in.su ){
   SETSTO( p->in.left, INTEMP );
   }
  else {
   SETSTO( p->in.right, INTEMP );
   }
  }
} /* mkadrs */
#endif


# define max(x,y) ((x)<(y)?(y):(x))


canstore( p,cookie ) NODE *p; {
/* SS.  25.jun.86  is node p candidate for direct store instruction */
 register int r;

 r = p->tn.rval;

 if( p->in.op==OREG && r==FP )  return(1);     /* "slw" */
 if( p->in.op==OREG && istreg(r) && cookie==FOREFF && !ISCHAR(p->tn.type))
  return(1); /* "ssw" */
 if( p->in.op==NAME && r != NONAME && !ISAGGREGATE(stab[r].stype) ) 
  return(1); /* "sw", NB! char node is always member of array */
         /* r=NONAME means absolute address */
 if( p->in.op==OREG && r==SP && cookie==FOREFF ) return(1);  /* 16.dec."sxw" */
   /* in the future we hope to include "spw" too */
 return(0);
}  /* canstore */


sucomp( p ) register NODE *p; {
/* set the su field in the node to the sethi-ullman
   number, or local equivalent */
/* SS. 17.june 1986  rewritten for stack machine like Kronos */

 register o = p->in.op;
 NODE *pl, *pr;
 register sul, sur;
 register charnode = ISCHAR(p->in.type); /* is it a char type node */

 p->in.su = 1;   /* this was szty( p->in.type ),2 for float or double */
   /* default all REG nodes have su value 1 */

 if( optype(o) == LTYPE ){
  register r = p->tn.rval;
#ifdef OLDARGS
  if( o==OREG && r==AP ) ++p->tn.su;  /* lpw, lpa */
#endif
  if( o==NAME && r!=NONAME && ISAGGREGATE(stab[r].stype) ){
   if( charnode )       /* additional "li 0" */
    p->tn.lval > 255 ? p->tn.su +=2 : ++p->tn.su;
   else     /* not a char node */
    if( p->tn.lval > 255 ) ++p->tn.su;
   return;  /* SS. 16.Oct.87 added */
  }   
  if(( o==ICON && r<0 ) ||   /* string adr must be multiplied */
   ( o==ICON && *p->tn.name && r>=0 && !ISFTN(DECREF(p->tn.type))
    && ISAGGREGATE(stab[r].stype) && (p->tn.lval > 255) ))
   ++p->tn.su;
  return;
 } /* LTYPE */

 pl = p->in.left;

 if( optype(o) == UTYPE ){
  if( callop(o) ){  /* room for ret value */
   p->in.su = pl->in.su;  /* SS. 16.Oct.87 was 2 */
   return;
  }

         /* don't forget the "copt" or "copt;li 0;swap" in case of char */
  if( asgop(pl->in.op) && (pl->in.op != STASG && pl->in.op != INCR && pl->in.op != DECR) )      
   if( !charnode ) ++pl->in.su; 

/* SS. 7.apr. was: ISCHAR(p->in.type) ? pl->in.su +=2 : ++pl->in.su;  */

  /* can we create oreg node and eliminate the "add" instr.? */
  /* this problem was solved when PLUS node was processed */

  if( o==PCONV ){  /* save room for "li 2; shl" */
   p->in.su = max( pl->in.su, 2 );
   return;
  }
  if( o==UNARY MUL && charnode ){   /* save room for "li 0" */
   p->in.su = max( pl->in.su, 2 ); 
   return;
  } 
   /* the usual case su computation */
  p->in.su = pl->in.su;
  return;
 }  /* if UTYPE */


 /*  if BITYPE then start here */

 /* If rhs needs n, lhs needs m, regular su computation */

 pr = p->in.right;

 if( o == ASSIGN && canstore(pl,INTAREG))   /* assume the worst case */
  pl->in.su = 0;  /* slw, sw */
 
 /* we must save room for "copt" or for "copt;li 0;swap" if char asg */
 /* 9.apr.87 char value is restored now using "copt,stot", no "swap" */

 if( asgop( pl->in.op )  /* 25.nov.86 check lhs too, for e.g. CBRANCH */
   && ( (pl->in.left->in.su != 0) || !asgop(pl->in.right->in.op)) )
  if( !ISCHAR(pl->in.type) ) ++pl->in.su;
/* 9.apr.87 was:   ISCHAR(pl->in.type) ? pl->in.su +=2 : ++pl->in.su;  */

 if( asgop( pr->in.op ) 
   && ( (pr->in.left->in.su != 0) || !asgop(pr->in.right->in.op)) )
  if( !ISCHAR(pr->in.type) ) ++pr->in.su; 
/* 9.apr.87 was:   ISCHAR(pr->in.type) ? pr->in.su +=2 : ++pr->in.su;  */

 if( shiftop(o) && p->in.type == UNSIGNED && !(pr->in.op == ICON
   && pr->tn.name[0]=='\0' && pr->tn.lval >= 0) ) /* simulating shift */
  pr->in.su += 2;  /* save room for "li 1;neg;..copt" */

 sul = pl->in.su;
 sur = pr->in.su;


 /* if lhs is candidate for store, then computed by doing right, then doing it, 
    else computed by doing left, then right, then doing it */
 if( o == ASSIGN ){
  p->in.su = ( sul==0 ? sur : max( sul, sur+1 ) );
/* lhs has "li 0",which resides in stack do the end of computing the whole op,not only the lhs */
  if( charnode ) ++p->in.su; 
  return;
 }

 if( callop(o) ){ /* SS. 16.Oct.87 test condition changed */
  /* room for computing func arguments )         */
  p->in.su = max( sul,sur );
  return;
 }

 if( o == STASG ){
  /* if sur > 1 then room for "li size" is reserved already */
  p->in.su = ( sur > 1 ? max( sul,sur+1 ) : max( sul, sur+2 ) );
  return;
 }

  /* su computation for =ops which belong to rewriting */
 if( opasg(o) && !canopasg(p) ){  /* 9.apr.87 !asgop() added */
  if( canstore(pl,INTAREG) )
   p->in.su = ( commop(o) && !asgop(o) ? max(1,sur) : max(sul, sur+1) );
  else
   p->in.su = charnode ? max(sul,sur+3) : max(sul,sur+2);
  return;
 }

  /* NB! In current version we do not consider the situ-
     ation when ++/-- operator is out of conditional context and
     therefore belongs to delay. The su number is > than min */

  /* So we compute the only effective =ops in case of KC */
 if( ( pr->in.op == ICON && pr->tn.lval == 1 ) 
   && ( o==INCR || o==DECR || o==ASG PLUS || o==ASG MINUS ) ){
  sur = pr->tn.su = 0; /* there is no need to load const 1 */
  p->in.su = sul;    /*  max( sul,1 ); */
  return;
 }

 if( o==ANDAND || o==OROR || o==QUEST || o==COLON || o==COMOP ){
  p->in.su = max( max( sul,sur ), 1 );
  return;
 }

 if( o==CM ){ /* stack room needed computing func arguments */
  p->in.su = max( sul,sur );
  return;
 }
  /* SS. 26.sep.86  patch caused from attempt to eliminate LIB */
 if( o==PLUS && !isapreg(pl) && isonode(pr) ) /* arg adr has su value 2*/
  sur = pr->in.su = 0;   /* adding with lsa */

  /* in case of address templates we need room only for "llw" */
 if( o==MINUS && isfpreg(pl) )
  sur = pr->in.su = 0;   /* NB! REG node has su value 1*/

 /* commutative ops: AND,MUL,PLUS,OR,ER;  put harder on left */
          /* we must gen. the "lsw" instr. first*/
 if( commop(o) && !asgop(o) && !(pl->in.op==OREG && istreg(pl->tn.rval))
    && !callop(pl->in.op) ){ 
  /* SS. 19.Oct.87 compute the call first, this saves "store" */
  /* fixed bug reported by Novosibirsk: call without "store" */
  /* lvar = (lvar << 2) | f()        */
    if( (sur > sul) || (callop(pr->in.op)) ){
   register NODE *temp;
   register int sut;
   temp = pl;
   p->in.left = pr;
   p->in.right = temp;
   sut = sul;
   sul = sur;
   sur = sut;
     }
 }  /* commop.. */

 /* all remaining binary ops, computed by left, then right, then do op */
 p->in.su = max( sul, sur+1 );

} /* sucomp */



offstar( p )  register NODE *p; {
/* SS. p is a direct descendant of U*. Our job is to make it addressable... */
/* In this version we simply compute the tree p into scratch register. */

#ifndef BUG4
 if (odebug)   printf( "offstar( %o )\n",p);
#endif
  /* later we must gen. char load or store instr. */
 if( p->in.type == PTR+CHAR || p->in.type == PTR+UCHAR )
/* tbl */ prints( li0 );  /* SS. 7.apr.87 prepare for "sxb|lxb" */

  /*  Indirect addressing with offsets from stack */
 if( p->in.op==PLUS && isonode(p->in.right) ){ /* don't check the MINUS op */
  order( p->in.left , INTAREG);
  return;
 }

 order( p, INTAREG );

} /* offstar */


setincr( p,cookie ) register NODE *p; {
/* setup for postfix incr and decr operators */
 int lo=p->in.left->in.op;

#ifndef BUG4
 if (odebug){
  printf( "setincr( %o, ",p );
  prcook( cookie );
  printf( " )\n" );
 }
#endif
 if( ISCHAR(p->in.type) ) return(0); /* rewrite tree */

 /* default rewriting, but goto canon() first of all, when OREG(r0) */ 
 if( lo==UNARY MUL ){
  offstar( p->in.left->in.left );
  return(1); 
 }

 if( cookie & FOREFF ){
  if( lo == NAME || lo == OREG ){
   order( p->in.left,FORADR );  /* put the adr to stack */
   return(1);
  }
  cerror( 267 );  /* setincr - illegal lhs */
 } /* foreff */

 if ( cookie & (INTAREG|FORCC|FORARG) ){
  register NODE  *p1;
  if( lo==NAME || ( lo==OREG && !istreg(p->in.left->tn.rval)) ){
   p1 = tcopy(p);  /* make a fresh copy of p */
# ifndef BUG4
   if( odebug )
    fwalk( p1, eprint, 0 );
# endif
   codgen( p1->in.left,INTAREG );  /* gen. load instr. */
   codgen( p,FOREFF );   /* incr in memory */
   p->in.op = REG;  /* replace tree with REG node */
   p->tn.rval = p1->in.left->tn.rval;
   p->tn.lval = 0;
   tfree( p1 );    /* leaving nothing */
   return(1);
  }
   /* we have OREG from stack and we must load value
   and then incr in memory, but we havn't "swap" instr.
   and this is the reason for rewriting ++/-- to +=/-= */

  return( 0 ); /* come back and perform default rewriting */
 } /* intareg */  

#ifndef BUG4
 cerror( 268 );   /* setincr - illegal cookie */
#endif
} /* setincr */


setbin( p ) register NODE *p; {
/* Generate code for binary operators ...*/

 if( p->in.left->in.op != REG){
  order( p->in.left, INTAREG );  /* try putting LHS into a reg. */
  return(1);
 }
 if( shiftop(p->in.op) && p->in.type == UNSIGNED )
/* tbl */ prints( "\tli\t-1\n" );

 if( p->in.right->in.op != REG ){
  order( p->in.right, INTAREG ); /* try putting RHS into a reg.*/
  if( shiftop(p->in.op) && p->in.type == UNSIGNED ){
  /* SS. 21.Oct.87 "slw4/llw4" replaced with "lodt/stot" */
/* tbl */  prints( "\tcopt\n\tstot\n\tshl\n" );  
   if( p->in.op == LS )
/* tbl */   prints( "\tlodt\n\tcopt\n\tstot\n\tror\n" );
  }
  return(1);
 }
 return(0); /* LHS and RHS are in the registers */

} /* setbin */


setstr( p,cookie ) register NODE *p; {
/* setup for structure assignment */

 register lo=p->in.left->in.op;

#ifndef BUG4
 if (odebug){
  printf( "setstr( %o, ",p );
  prcook( cookie );
  printf( " )\n" );
 }
#endif
 /* Try to make left side addressable ... */

 if( lo==UNARY MUL ){  /* lhs is not reg and not candidate for store */
  offstar( p->in.left->in.left );
  return(1);
 }

 if ( lo==NAME || lo==OREG ){ /* lhs is OREG(fp),OREG(r0) or NAME */
  order( p->in.left,FORADR );
  if( cookie & (INTAREG|FORCC|FORARG) ){
#ifndef BUG4
   if(odebug) printf( "setasop( %o )\n", p->in.left );
#endif
/* tbl */  prints( copt );
  }
  return(1);
 }

 /* adr with copy resides on top of stack, try putting RHS into a reg */
 if( lo==REG ){   
  p->in.right->in.op==UNARY MUL ? offstar(p->in.right->in.left) : order(p->in.right,INTAREG);
  return(1);
 }   
   
 cerror( 269 );   /* setstr - illegal assignment */
 
} /* setstr */



setasg( p,cookie ) register NODE *p; {
/* setup for assignment operator */
 register NODE *l = p->in.left;
 int charnode = ISCHAR(p->in.type);

#ifndef BUG4
 if (odebug){
  printf( "setasg( %o, ",p );
  prcook( cookie );
  printf( " )\n" );
 }
#endif
 /* Try to make left side addressable ... */

 if( l->in.op==UNARY MUL ){ 
  offstar( l->in.left );
  return(1);
 }

 /* SS. 7.apr.87 || added to copy the char value if later needed */

 if( canstore(l,cookie)  /* if OREG(r0)|OREG(sp) then must be FOREFF*/
   || (l->in.op==REG && charnode && (cookie&(INTAREG|FORCC|FORARG))) ){
  if( p->in.right->in.op == UNARY MUL )
   offstar( p->in.right->in.left );
  else {
   order( p->in.right,INTAREG );  /* try putting rhs into a reg */
   if( cookie & (INTAREG|FORCC|FORARG) ){
#ifndef BUG4
    if(odebug) printf( "setasg( %o )\n", p->in.right );
#endif
/* tbl */   prints( copt ); /* copy the value */
    if( charnode ) 
/* tbl */    prints( "\tstot\n" );  /* SS. 7.apr.87 save for later "lodt" */
   }
  }
  return(1);
 }  /* lhs is candidate for store */ 
 
 if( l->in.op != REG ){  /* lhs is OREG(ap),NAME+ofs,OREG(r0),abs adr or char*/
  if( l->in.op==NAME && l->tn.rval != NONAME && (cookie&FOREFF) ){
   if( !charnode ){
    int val;
    val = l->tn.lval;   /* save the offset */
    l->tn.lval = 0;
    order( l,FORADR );  /* gen. only "lw name" */
    l->in.op = OREG;
    if( isitofs(val) )  /* can we make OREG(r0)? */
     l->tn.lval = val; /* restore offset */
    else {
#ifndef BUG4
     if(odebug) printf("setasg( %o )\n", l );
#endif
/* tbl */    printf( "\tli\t%d\n", val);
     rfree( l->tn.rval );
     rbusy( SP );
     l->tn.rval = SP;  /* make OREG(sp) for "sxw" */
    }
    return(1);  /* lhs is now canstore, goto compute rhs */
   }
  } /* Abs adr,char name or cookie intareg with oreg|name node */
  order( l,FORADR );
  if( cookie & (INTAREG|FORCC|FORARG) ){
#ifndef BUG4
   if(odebug) printf( "setasg( %o )\n", l );
#endif
  /* SS. 7.apr.87 copy the address only for word variables */
   if( !charnode )
/* tbl */          prints( copt );    /* copy the address */
   return(1);
  }
 }

 /* adr with copy resides on top of stack, try putting rhs into a reg */
 if( l->in.op==REG ){
  p->in.right->in.op==UNARY MUL ? 
      offstar( p->in.right->in.left ) : order( p->in.right,INTAREG);
  return(1);
 }

 cerror( 270 );  /* setasg - illegal assignment */
 
}  /* setasg */


canopasg( p ) NODE *p; {
/* returns true if node is candidate for =op instructions "inc,inc1/dec,dec1" */
TWORD t = p->in.type;

 return( myopasg(p->in.op) && !ISCHAR(t) && !ISFLOAT(t) );
}



setasop( p,cookie ) register NODE *p; {
/* setup for =ops */
 register NODE *l = p->in.left;
 int validasop, charnode;

#ifndef BUG4
 if (odebug){
  printf( "setasop( %o, ",p );
  prcook( cookie );
  printf( " )\n" );
 }
#endif
 /* Try to rewrite the tree at least to the point
    where there are no side effects in the LHS.   */

 if( l->in.op==REG ){       /* try putting rhs into a reg */
  p->in.right->in.op==UNARY MUL ?
      offstar(p->in.right->in.left) : order(p->in.right,INTAREG);
  return(1);  /* search for template */
 }

 charnode = ISCHAR(p->in.type);  /* is it a char type node */
 validasop = canopasg( p ); /* is it valid =op for Kronos */

 if( cookie & FOREFF ){
 if( l->in.op==NAME ){
  if( validasop ){
   order( l,FORADR );
   return(1); /* search for template */
  }
  else { /* we must rewrite the =op to binary op */
   if( canstore(l,FOREFF) )    /* is it scalar name? */
    return(0);    /* rewrite without computing adr*/
   else {  /* compute the adr and the rewrite */
    int val, ofsname = 0;
    /*SS. 8.apr.87 don't gen "li0" in case of char*/
    if( !charnode && l->tn.rval != NONAME && isitofs(val=l->tn.lval))
     ofsname = 1; /* candidate for OREG(r0)*/
    if( ofsname )
     l->tn.lval = 0; /* gen.only "lw name" */
    order( l,FORADR );
    l->in.op = OREG; /* prepare for "lsw|lxb" after rewriting */ 
    if( ofsname )
     l->tn.lval = val; /* make OREG(r0) */
#ifndef BUG4
    if(odebug) printf( "setasop( %o )\n", l );
#endif
    /* copy the address for "lsw|lxb"*/
/* tbl */   prints( charnode ? copt_swap : copt );
    return(0);    /* and then rewrite tree */
   }
  }
 } /* name */
 
 if( l->in.op==OREG ){
#ifdef OLDARGS
  if( l->tn.rval==AP ){
   order( l,FORADR );
   if(validasop)
    return(1); /* search for template */
   else {   /* we must rewrite the tree */
   /* we didn't check canstore(l.), because we havn't "spw"
   instr. and for "sxb" we must completely compute address.
   The following produce code: "lpa,copt,lsw0,li,op,ssw0"
   This is better than: "lpa,lpw,li,op,ssw0" (if we don't
   compute adr first, but rewrite immediately). */
    l->in.op = OREG;  /* NB! aren't I clever: REG->OREG */
/* tbl */   prints( "\tcopt\n" );   /* copy the address */
    return(0);   /* rewrite the tree */
   }
  }
#endif
  if( istreg(l->tn.rval) ){  /* before reaching here was U* */
   if(validasop){
    order( l,FORADR ); /* completely compute the adr*/
    return(1);
   }
   else {
    if( charnode ){ /* completely compute the adr */
     order( l,FORADR );
     l->in.op = OREG;  /* to cause gen "lxb" */
    }
#ifndef BUG4
    if(odebug) printf( "setasop( %o )\n", l );
#endif
    /* copy the adr for "lsw|lxb" */
/* tbl */   prints( charnode ? copt_swap : copt );
    return(0); /* rewrite tree */
   }
  } /* oreg(r0) */
  if( l->tn.rval==FP ){
   if( validasop ){
    order( l,FORADR );
    return(1); /* search for template */
   }
   /* for locals don't compute the adr before rewriting,
   because the code: "llw,li,op,slw" is shorter than 
   "lla,copt,lsw0,li,op,ssw0" */
   else return(0);  
  }
 } /* oreg */

 if( l->in.op==UNARY MUL ){
  offstar( l->in.left );
  return(1);
 }

 cerror( 271 );  /* illegal =op */
 
 } /* cookie & foreff */   

 if( cookie & (INTAREG|FORCC|FORARG) ){

  if( l->in.op == UNARY MUL ){
   offstar( l->in.left );
   return(1);
  }

  /* completely compute the adr for "sxb" in case of charnode 
  or for "lsw0"(reclaim) in case of word node; char values are
  restored for later testing using "copt,stot" in setasg() */
  order( l,FORADR ); 
#ifndef BUG4
  if(odebug) printf( "setasop( %o )\n", l );
#endif
/* tbl */ if( !charnode ) prints( copt ); /* adr for "ssw0" */
  if( validasop )
   return(1); /* search for template */
  else {   /* completely compute the lhs */
#ifndef BUG4
   if(odebug) printf( "setasop( %o )\n", l );
#endif
/* tbl */  prints( charnode ? copt_swap : copt ); /* load the adr */
/* tbl */  prints( charnode ? "\tlxb\n" : "\tlsw\t0\n" ); /* load the value */
   return(0); /* rewrite to binary op */
  }
 } /* cookie & intareg... */
}  /* setasop */


int crslab = 9999;  /* Honeywell */


getlab(){
 return( crslab-- );
}


deflab( l ){
/* Label symbol is changed from L -> M */

/* tbl */   printf( "M%d:\n", l );
}


genargs( p, ccall, optcall ) register NODE *p; {
/* C or Modula function call - generate code for the arguments */

#ifndef BUG4
 if (odebug) 
    printf( "genargs( %o,ccall= %d,opt= %d )\n",p,ccall,optcall);
#endif
 /*  SS. 21.Oct.87 first, do the arguments on the right */
 if( optcall ){
  while( p->in.op == CM ){
   genargs( p->in.right, ccall,optcall );
   p->in.op = FREE;
   p = p->in.left;
  }
 }
 else {
 /*  first, do the arguments on the left */
  while( p->in.op == CM ){
   genargs( p->in.left, ccall,optcall );
   p->in.op = FREE;
   p = p->in.right;
  }
 }

 if( p->in.op == STARG ){ /* structure valued argument */
  p->in.op = FREE;
  p = p->in.left;
 }
 
 /* ordinary case, scalar argument */

 if( callop(p->in.op) && p->in.type == UNDEF ){
  werror( 34 );   /* void func used as an argument */
  order( p, FOREFF );  /* SS. 05.06.87 void func as arg, "stot" not needed */
 }
     /* SS. 21.Oct.87 don't optimize call */
 else if( ccall && !optcall ){   /* gen.code for C call, put arg into P-stack */
  p->in.rall |= MUSTDO;  /* 14.apr.87 store is not needed */
  order( p, FORARG );
 }
   /* SS. 21.Oct.87 NB! includes also optimized C call */
 else {   /* gen.code for Modula call, put arg into E-stack */
  order( p, INTAREG );
  reclaim( p, RNULL, 0 ); /* We don't need the REG node */
 }

} /* genargs */


argsize( p ) register NODE *p; {
/* SS. Do not deal with argument sizes, for Kronos we must
       only count the number of arguments. */

 register t;
 t = 0;
 if( p->in.op == CM ){
  t = argsize( p->in.left );
  p = p->in.right;
 }
#ifndef VAX
 /* SS 05.06.87 do not count void functions as arguments */
 if( callop(p->in.op) ) /* SS.21.Oct.87 mark calls for optimizing */
  ++callflag;
 if( p->in.type != UNDEF ) 
  return( t+1 );   /* That's all ! */
 else 
  return( t ); /* 18.Nov.87 SS added, was bug */
#else

 if( p->in.type == DOUBLE || p->in.type == FLOAT ){
  SETOFF( t, 4 );
  return( t+8 );
 }
 else if( p->in.op == STARG ){
   SETOFF( t, 4 );  /* alignment */
   return( t + ((p->stn.stsize+3)/4)*4 );  /* size */
        }
 else {
  SETOFF( t, 4 );
  return( t+4 );
              }
#endif
}  /* argsize */
