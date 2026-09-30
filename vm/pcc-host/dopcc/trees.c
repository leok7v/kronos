/* Revision log:
 *  15.Oct.86 MK  ADDROREG removed
 *  20.Oct.86 MK  STATIC names in p2tree()
 *  27.Oct.86 MK  call to exname removed in p2tree
 *  28.Oct.86 MK  switch -> IF in buildtree()
 *  29.Oct.86 MK  call to locctr removed in ecomp()
 *  09.Nov.86 SS  debugging prints
 *  11.Nov.86 MK  doszof(): / (SZCHAR / 4)
 *  03.Feb.87 MK  cflag, hflag brought here from xdefs
 *  04.Feb.87 MK  part of xdefs that cgram coudn't accommodate is here also
 *  26.Feb.87 SS  call to clocal() added in buildtree in CAST case
 *                restored initial state in stref()
 *  27.Feb.87 SS  fixed bugs in notlval() and opact()
 *                fixed bug in doszof(): sizeof must ret unsigned const
 *  16.Mar.87 SS  added check for useless cast, paintcast() and else if CAST..
 *    in buildtree()
 *  03.Apr.87 SS  unsigned const shifts in conval()
 *  04.Jun.87 SS  added check for return in opact()
 *  14.Oct.87 SS  added some debug info to chkstr()
 */

# include "mfile1.h"

     /* corrections when in violation of lint */

/* some special actions, used in finding the type of nodes */
# define NCVT 01
# define PUN 02
# define TYPL 04
# define TYPR 010
# define TYMATCH 040
# define LVAL 0100
# define CVTO 0200
# define CVTL 0400
# define CVTR 01000
# define PTMATCH 02000
# define OTHER 04000
# define NCVTR 010000

/* node conventions:

 NAME: rval>0 is stab index for external or static
  rval<0 is -inlabel number  (not used)
  lval is offset in bits
 ICON: lval has the value
  rval has the STAB index, or - label number,
   for strings only
  rval = NONAME means no name
 REG: rval is reg. identification cookie

 */

#ifndef BUG1
int edebug;
int tdebug;
int bdebug;
extern ddebug;
#endif

int cflag = 0;  /* do we check for funny casts ? */  /* host-port: was static, extern in mfile1.h */
int hflag = 1;     /* do we check for various heuristics that may
         indicate errors ? */

int brkflag = 1;   /* complain about break statements not reached */
struct sw swtab[SWITSZ]; /* table for cases within  a switch */
struct sw * swp; /* pointer to next free entry in swtab */
int swx;     /* index of beginning of cases for current switch */
int brklab, contlab, flostat, retstat;
int retlab = NOLAB;
int reached; /* true if statement can be reached */
int idname;
int regvar;  /* the next free register for register variables */
/* save array for break, continue labels, and flostat */
int asavbc [BCSZ];
int * psavbc = asavbc;
int stwart;  /* for addressing names that are structure members or names */
int curclass;  /* current storage class */


NODE *
buildtree( o, l, r ) register NODE *l, *r; {
 register NODE *p, *q;
 register actions;
 register opty;
 register struct symtab *sp;
 register NODE *lr, *ll;
 int i;
 extern int eprint();

# ifndef BUG1
 if( bdebug ) printf( "buildtree( %s, %o, %o )\n", opst[o], l, r );
# endif
 opty = optype(o);

 /* check for constants */

 if( opty == UTYPE && l->in.op == ICON ){

  if (o == NOT && hflag)
    werror( 7 );  /* constant argument to NOT */
  if (o == UNARY MINUS || o == COMPL || o == NOT)
   if( conval( l, o, l ) ) return(l);
  }

  /* SS. 16.Mar.87 check if cast can be painted down */
  /* cgram has to check what we return here */
 else if (o == CAST && r->in.op == ICON){
#ifndef BUG1
  if (bdebug) printf( "attempting to paint cast down\n" );
#endif
  if( paintcast(r, l->in.type) ){
   l->in.op = FREE;
   return (r);
  }
 }

 else if( o==UNARY MINUS && l->in.op==FCON ){
  l->fpn.dval = -l->fpn.dval;
  return(l);
  }

 else if( o==QUEST && l->in.op==ICON ) {
  l->in.op = FREE;
  r->in.op = FREE;
  if( l->tn.lval ){
   tfree( r->in.right );
   return( r->in.left );
   }
  else {
   tfree( r->in.left );
   return( r->in.right );
   }
  }

 else if( (o==ANDAND || o==OROR) && (l->in.op==ICON||r->in.op==ICON) ) goto ccwarn;

 else if( opty == BITYPE && l->in.op == ICON && r->in.op == ICON ){

  switch( o ){

  case ULT:
  case UGT:
  case ULE:
  case UGE:
  case LT:
  case GT:
  case LE:
  case GE:
  case EQ:
  case NE:
  case ANDAND:
  case OROR:
  case CBRANCH:

  ccwarn:
   if( hflag ) 
    werror( 8 );  /* const in conditional context */

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
   if( conval( l, o, r ) ) {
    r->in.op = FREE;
    return(l);
    }
   break;
   }
  }

 else if( opty == BITYPE && (l->in.op==FCON||l->in.op==ICON) &&
  (r->in.op==FCON||r->in.op==ICON) ){
  if (o == PLUS || o == MINUS || o == MUL || o == DIV) {
   if( l->in.op == ICON ){
    l->fpn.dval = l->tn.lval;
    }
   if( r->in.op == ICON ){
    r->fpn.dval = r->tn.lval;
    }
   l->in.op = FCON;
   l->in.type = l->fn.csiz = DOUBLE;
   r->in.op = FREE;
   switch(o){
   case PLUS:
    l->fpn.dval += r->fpn.dval;
    return(l);
   case MINUS:
    l->fpn.dval -= r->fpn.dval;
    return(l);
   case MUL:
    l->fpn.dval *= r->fpn.dval;
    return(l);
   case DIV:
    if( r->fpn.dval == 0 ) 
     uerror( 93 );  /* division by 0. */
    else l->fpn.dval /= r->fpn.dval;
    return(l);
    }
   }
  }

 /* its real; we must make a new node */

 p = block( o, l, r, INT, 0, INT );

 actions = opact(p);

 if( actions&LVAL ){ /* check left descendent */
  if( notlval(p->in.left) ) {
   uerror( 94 );  /* illegal lhs of assignment operator */
   }
  }

 if( actions & NCVTR ){
  p->in.left = pconvert( p->in.left );
  }
 else if( !(actions & NCVT ) ){
  if (opty == BITYPE)
   p->in.right = pconvert( p->in.right );
  if (opty == UTYPE || opty == BITYPE)
   p->in.left = pconvert( p->in.left );
  }

 if( (actions&PUN) && (o!=CAST||cflag) ){
  chkpun(p);
  }

 if( actions & (TYPL|TYPR) ){

  q = (actions&TYPL) ? p->in.left : p->in.right;

  p->in.type = q->in.type;
  p->fn.cdim = q->fn.cdim;
  p->fn.csiz = q->fn.csiz;
  }

 if( actions & CVTL ) p = convert( p, CVTL );
 if( actions & CVTR ) p = convert( p, CVTR );
 if( actions & TYMATCH ) p = tymatch(p);
 if( actions & PTMATCH ) p = ptmatch(p);

 if( actions & OTHER ){
  l = p->in.left;
  r = p->in.right;

  switch(o){

  case NAME:
   sp = &stab[idname];
   if( sp->stype == UNDEF ){
    uerror( 92,sp->sname );  /* %s undefined */
    /* make p look reasonable */
    p->in.type = p->fn.cdim = p->fn.csiz = INT;
    p->tn.rval = idname;
    p->tn.lval = 0;
    defid( p, SNULL );
    break;
    }
   p->in.type = sp->stype;
   p->fn.cdim = sp->dimoff;
   p->fn.csiz = sp->sizoff;
   p->tn.lval = 0;
   p->tn.rval = idname;
   /* special case: MOETY is really an ICON... */
   if( p->in.type == MOETY ){
    p->tn.rval = NONAME;
    p->tn.lval = sp->offset;
    p->fn.cdim = 0;
    p->in.type = ENUMTY;
    p->in.op = ICON;
    }
   break;

  case ICON:
   p->in.type = INT;
   p->fn.cdim = 0;
   p->fn.csiz = INT;
   break;

  case STRING:
   p->in.op = NAME;
   p->in.type = CHAR+ARY;
   p->tn.lval = 0;
   p->tn.rval = NOLAB;
   p->fn.cdim = curdim;
   p->fn.csiz = CHAR;
   break;

  case FCON:
   p->tn.lval = 0;
   p->tn.rval = 0;
   p->in.type = DOUBLE;
   p->fn.cdim = 0;
   p->fn.csiz = DOUBLE;
   break;

  case STREF:
   /* p->x turned into *(p+offset) */
   /* rhs must be a name; check correctness */

   i = r->tn.rval;
   if( i<0 || ((sp= &stab[i])->sclass != MOS 
      && sp->sclass != MOU && !(sp->sclass&FIELD)) ){
    uerror( 95 );  /* member of struct or union required */
    }else
   /* if this name is non-unique, find right one */
   if( stab[i].sflags & SNONUNIQ &&
    (l->in.type==PTR+STRTY || l->in.type == PTR+UNIONTY) &&
    (l->fn.csiz +1) >= 0 ){
    /* nonunique name && structure defined */
    char * memnam, * tabnam;
    int j;
    int memi;
    j=dimtab[l->fn.csiz+1];
    for( ; (memi=dimtab[j]) >= 0; ++j ){
     tabnam = stab[memi].sname;
     memnam = stab[i].sname;
# ifndef BUG1
     if( ddebug>1 )
      printf("member %s==%s?\n", memnam, tabnam);
# endif
     if( stab[memi].sflags & SNONUNIQ ){
      if (memnam != tabnam)
       continue;
      r->tn.rval = i = memi;
      break;
      }
     }
    if( memi < 0 )
     uerror( 96, stab[i].sname); /* illegal member use: %s */
    }
   else {
    register j, align;
    if( l->in.type != PTR+STRTY && l->in.type != PTR+UNIONTY ){
     if( stab[i].sflags & SNONUNIQ )
/* nonunique name demands struct/union or struct/union ptr */ uerror( 97 ); 
     else {
      align=talign(BTYPE(l->in.type),l->fn.csiz);
      /* align better be an even multiple of ALSTRUCT */
      if( align % ALSTRUCT == 0 )
/* struct/union or struct/union ptr required */   werror( 9 );  
      else
       uerror( 126 ); /* bad structure offset */
     }
    }
    else if( (j=l->fn.csiz+1)<0 ) 
     cerror( 238 );  /* undefined struct or union */
    else if( !chkstr( i, dimtab[j], DECREF(l->in.type) ) ){
     werror( 10, stab[i].sname ); /* illegal member use: %s */ 
     }
    }

   p = stref( p );
   break;

  case UNARY MUL:
   if( l->in.op == UNARY AND ){
    p->in.op = l->in.op = FREE;
    p = l->in.left;
    }
   if( !ISPTR(l->in.type))
    uerror( 98 );  /* illegal indirection */
   p->in.type = DECREF(l->in.type);
   p->fn.cdim = l->fn.cdim;
   p->fn.csiz = l->fn.csiz;
   break;

  case UNARY AND:
   switch( l->in.op ){

   case UNARY MUL:
    p->in.op = l->in.op = FREE;
    p = l->in.left;
   case NAME:
    p->in.type = INCREF( l->in.type );
    p->fn.cdim = l->fn.cdim;
    p->fn.csiz = l->fn.csiz;
    break;

   case COMOP:
    lr = buildtree( UNARY AND, l->in.right, NIL );
    p->in.op = l->in.op = FREE;
    p = buildtree( COMOP, l->in.left, lr );
    break;

   case QUEST:
    lr = buildtree( UNARY AND, l->in.right->in.right, NIL );
    ll = buildtree( UNARY AND, l->in.right->in.left, NIL );
    p->in.op = l->in.op = l->in.right->in.op = FREE;
    p = buildtree( QUEST, l->in.left, buildtree( COLON, ll, lr ) );
    break;

   default:
    uerror( 99 );  /* unacceptable operand of & */
    break;
    }
   break;

  case LS:
  case RS:
  case ASG LS:
  case ASG RS:
   if(tsize(p->in.right->in.type, p->in.right->fn.cdim, p->in.right->fn.csiz) > SZINT)
    p->in.right = makety(p->in.right, INT, 0, INT );
   break;

  case RETURN:
  case ASSIGN:
  case CAST:
   /* structure assignment */
   /* take the addresses of the two sides; then make an
   /* operator using STASG and
   /* the addresses of left and right */

   {
    register TWORD t;
    register d, s;

    if( l->fn.csiz != r->fn.csiz ) 
       uerror( 100 ); /* assignment of different structures */

    r = buildtree( UNARY AND, r, NIL );
    t = r->in.type;
    d = r->fn.cdim;
    s = r->fn.csiz;

    l = block( STASG, l, r, t, d, s );

    if( o == RETURN ){
     p->in.op = FREE;
     p = l;
     break;
     }

#ifndef BUG1
    if( bdebug > 1 ) printf( "in buildtree:\n" );
#endif
    l = clocal( l );      /* SS. 27.feb.87 added */
    p->in.op = UNARY MUL;
    p->in.left = l;
    p->in.right = NIL;
    break;
    }
  case COLON:
   /* structure colon */

   if( l->fn.csiz != r->fn.csiz ) 
       uerror( 101 );  /* type clash in conditional */
   break;

  case CALL:
   p->in.right = r = strargs( p->in.right );
  case UNARY CALL:
   if( !ISPTR(l->in.type)) 
    uerror( 102 );  /* illegal function */
   p->in.type = DECREF(l->in.type);
   if( !ISFTN(p->in.type))
     uerror( 102 );  /* illegal function */
   p->in.type = DECREF( p->in.type );
   p->fn.cdim = l->fn.cdim;
   p->fn.csiz = l->fn.csiz;
   if( l->in.op == UNARY AND && l->in.left->in.op == NAME &&
    l->in.left->tn.rval >= 0 && l->in.left->tn.rval != NONAME &&
    ( (i=stab[l->in.left->tn.rval].sclass) == FORTRAN || i==UFORTRAN ) ){
    p->in.op += (FORTCALL-CALL);
    }
   if( p->in.type == STRTY || p->in.type == UNIONTY ){
    /* function returning structure */
    /*  make function really return ptr to str., with * */

    p->in.op += STCALL-CALL;
    p->in.type = INCREF( p->in.type );
    p = buildtree( UNARY MUL, p, NIL );

    }
   break;

  default:
   cerror( 239, o );  /* other code %d */
   }

  }

 if( actions & CVTO ) p = oconvert(p);
#ifndef BUG1
 if( bdebug > 1 ) printf( "end of buildtree:\n" );
#endif
 p = clocal(p);

# ifndef BUG1
 if( bdebug ) fwalk( p, eprint, 0 );
# endif

 return(p);

 }


paintcast(p, t)  /* host-port: was static */    /* can we paint type t onto ICON *p? */
register NODE *p;
register TWORD t;
{
 register int itype = p->tn.type;

 /* for now just worry about integral->integral conversions */

 if ((ISUNSIGNED(itype) || UNSIGNABLE(itype)) &&
     (ISUNSIGNED(t) || UNSIGNABLE(t))){
  p->in.type = t;
  return (1);
 }
 else
  return (0);
}


NODE *
strargs( p ) register NODE *p;  { /* rewrite structure flavored arguments */

 if( p->in.op == CM ){
  p->in.left = strargs( p->in.left );
  p->in.right = strargs( p->in.right );
  return( p );
  }

 if( p->in.type == STRTY || p->in.type == UNIONTY ){
  p = block( STARG, p, NIL, p->in.type, p->fn.cdim, p->fn.csiz );
  p->in.left = buildtree( UNARY AND, p->in.left, NIL );
  p = clocal(p);
  }
 return( p );
 }

chkstr( i, j, type ) TWORD type; {
 /* is the MOS or MOU at stab[i] OK for strict reference by a ptr */
 /* i has been checked to contain a MOS or MOU */
 /* j is the index in dimtab of the members... */
 int k, kk;

 extern int ddebug;

# ifndef BUG1
 if( ddebug > 1 ) printf( "chkstr( %s(%d), %d )\n", stab[i].sname, i,j );
# endif
 if( (k = j) < 0 ) 
  uerror( 103 );  /* undefined structure or union */
 else {
  for( ; (kk = dimtab[k] ) >= 0; ++k ){
   if( kk >= SYMTSZ ){
    cerror( 240 );  /* gummy structure */
    return(1);
    }
   if( kk == i ) return( 1 );
   switch( stab[kk].stype ){

   case STRTY:
   case UNIONTY:
    if( type == STRTY ) continue;  /* no recursive looking for strs */
    if( hflag && chkstr( i, dimtab[stab[kk].sizoff+1], stab[kk].stype ) ){
     if( stab[kk].sname[0] == '$' ) return(0);  /* $FAKE */
/* illegal member use: perhaps %s.%s? */ werror( 11,stab[kk].sname, stab[i].sname );
     return(1);
     }
    }
   }
  }
# ifndef BUG1
 if( ddebug > 1 ) printf( "RET 0 <- chkstr( %s(%d), %d )\n", stab[i].sname, i,j );
# endif
 return( 0 );
 }

conval( p, o, q ) register NODE *p, *q; {
 /* apply the op o to the lval part of p; if binary, rhs is val */
 int i, lu;
 CONSZ val;

 val = q->tn.lval;
 lu = ISUNSIGNED(p->in.type); /* SS. 4.apr.87 for Kronos shifts */
 if( (lu || ISUNSIGNED(q->in.type)) && (o==LE||o==LT||o==GE||o==GT))
  o += (UGE-GE);

 if( p->tn.rval != NONAME && q->tn.rval != NONAME ) return(0);
 if( q->tn.rval != NONAME && o!=PLUS ) return(0);
 if( p->tn.rval != NONAME && o!=PLUS && o!=MINUS ) return(0);

 switch( o ){

 case PLUS:
  p->tn.lval += val;
  if( p->tn.rval == NONAME ){
   p->tn.rval = q->tn.rval;
   p->in.type = q->in.type;
   }
  break;
 case MINUS:
  p->tn.lval -= val;
  break;
 case MUL:
  p->tn.lval *= val;
  break;
 case DIV:
  if( val == 0 ) 
   uerror( 104 );  /* division by 0 */
  else p->tn.lval /= val;
  break;
 case MOD:
  if( val == 0 )
    uerror( 104 );  /* division by 0 */
  else p->tn.lval %= val;
  break;
 case AND:
  p->tn.lval &= val;
  break;
 case OR:
  p->tn.lval |= val;
  break;
 case ER:
  p->tn.lval ^=  val;
  break;
 case LS:
  i = val;
/* SS. 4.apr.87 perform separately unsigned and signed const shifts for Kronos*/
  p->tn.lval = lu ? (unsigned)p->tn.lval << i : p->tn.lval << i;
/*  p->tn.lval = p->tn.lval << i; */ 
  break;
 case RS:
  i = val;
/* SS. 4.apr.87 perform separately unsigned and signed const shifts for Kronos*/
  p->tn.lval = lu ? (unsigned)p->tn.lval >> i : p->tn.lval >> i;
/*  p->tn.lval = p->tn.lval >> i; */
  break;

 case UNARY MINUS:
  p->tn.lval = - p->tn.lval;
  break;
 case COMPL:
  p->tn.lval = ~p->tn.lval;
  break;
 case NOT:
  p->tn.lval = !p->tn.lval;
  break;
 case LT:
  p->tn.lval = p->tn.lval < val;
  break;
 case LE:
  p->tn.lval = p->tn.lval <= val;
  break;
 case GT:
  p->tn.lval = p->tn.lval > val;
  break;
 case GE:
  p->tn.lval = p->tn.lval >= val;
  break;
 case ULT:
  p->tn.lval = (p->tn.lval-val)<0;
  break;
 case ULE:
  p->tn.lval = (p->tn.lval-val)<=0;
  break;
 case UGE:
  p->tn.lval = (p->tn.lval-val)>=0;
  break;
 case UGT:
  p->tn.lval = (p->tn.lval-val)>0;
  break;
 case EQ:
  p->tn.lval = p->tn.lval == val;
  break;
 case NE:
  p->tn.lval = p->tn.lval != val;
  break;
 default:
  return(0);
  }
 return(1);
 }

chkpun(p) register NODE *p; {

 /* checks p for the existance of a pun */

 /* this is called when the op of p is ASSIGN, RETURN, CAST, COLON, or relational */

 /* one case is when enumerations are used: this applies only to lint */
 /* in the other case, one operand is a pointer, the other integer type */
 /* we check that this integer is in fact a constant zero... */

 /* in the case of ASSIGN, any assignment of pointer to integer is illegal */
 /* this falls out, because the LHS is never 0 */

 register NODE *q;
 register TWORD t1, t2;
 register d1, d2;

 t1 = p->in.left->in.type;
 t2 = p->in.right->in.type;

#ifndef BUG1
 if( bdebug > 1 ){
  register lo = p->in.left->in.op;
  register ro = p->in.right->in.op;  
  printf("chkpun(%o): lo=%s, t1=0%o, op:%s, ro=%s, t2=0%o\n",
    p, opst[lo], t1, opst[p->in.op], opst[ro], t2);
 }
#endif

 if( t1==ENUMTY || t2==ENUMTY ) { /* check for enumerations */
  if( logop( p->in.op ) && p->in.op != EQ && p->in.op != NE ) {
   uerror( 105 );  /* illegal comparison of enums */
   return;
   }
  if( t1==ENUMTY && t2==ENUMTY && p->in.left->fn.csiz==p->in.right->fn.csiz ) return;
  werror( 12, opst[p->in.op] ); /* enum type clash, operator %s */
  return;
  }

 if( ISPTR(t1) || ISARY(t1) ) q = p->in.right;
 else q = p->in.left;

 if( !ISPTR(q->in.type) && !ISARY(q->in.type) ){
 /* SS. 13.10.86  the following is commented out by Mari, but why? */
  if( q->in.op != ICON || q->tn.lval != 0 ){
   werror( 13, opst[p->in.op] ); /* illegal combination of ptr and int, op %s */
   }
  }
 else {
  d1 = p->in.left->fn.cdim;
  d2 = p->in.right->fn.cdim;
  for( ;; ){
   if( t1 == t2 ) {;
    if( p->in.left->fn.csiz != p->in.right->fn.csiz ) {
     werror( 14 ); /* illegal struct pointer combination */
     }
    return;
    }
   if( ISARY(t1) || ISPTR(t1) ){
    if( !ISARY(t2) && !ISPTR(t2) ) break;
    if( ISARY(t1) && ISARY(t2) && dimtab[d1] != dimtab[d2] ){
     werror( 15 ); /* illegal array size combination */
     return;
     }
    if( ISARY(t1) ) ++d1;
    if( ISARY(t2) ) ++d2;
    }
   else break;
   t1 = DECREF(t1);
   t2 = DECREF(t2);
   } /* for .. */
  werror( 16 );  /* illegal pointer combination */
  }

 }

NODE *
stref( p ) register NODE *p; {

 TWORD t;
 int d, s, dsc, align;
 OFFSZ off;
 register struct symtab *q;

 /* make p->x */
 /* this is also used to reference automatic variables */


#ifndef BUG1
 if( bdebug > 1 ) printf( "stref( %o )\n", p );
#endif

 q = &stab[p->in.right->tn.rval];
 p->in.right->in.op = FREE;
 p->in.op = FREE;
 p = pconvert( p->in.left );

 /* make p look like ptr to x */

 if( !ISPTR(p->in.type)){
  p->in.type = PTR+UNIONTY;
  }

 t = INCREF( q->stype );
 d = q->dimoff;
 s = q->sizoff;

 p = makety( p, t, d, s );

 /* compute the offset to be added */

 off = q->offset;
 dsc = q->sclass;

 if( dsc & FIELD ) {  /* normalize offset */
  align = ALINT;
  s = INT;
  off = (off/align)*align;
  }
/* MK 23.04.86 p = clocal( block( PLUS, p, offcon( off, t, d, s ), t, d, s ) );*/
      
 if( off != 0 ){  /* SS. 27.feb.87 the old version restored */
#ifndef BUG1
  if( bdebug > 1 ) printf( "in stref():\n" );
#endif
  p = clocal( block( PLUS, p, offcon( off, t, d, s ), t, d, s ) );
 }
 p = buildtree( UNARY MUL, p, NIL );

 /* if field, build field info */

 if( dsc & FIELD ){
  p = block( FLD, p, NIL, q->stype, 0, q->sizoff );
 /* SS. 8.apr.87 PKFIELD performs <<, I add the unsigned cast */
  p->tn.rval = PKFIELD( (unsigned)dsc&FLDSIZ, q->offset%align );
  }

#ifndef BUG1
 if( bdebug > 1 ) printf( "end of stref():\n" );
#endif
 return( clocal(p) );
 }

notlval(p) register NODE *p; {

 /* return 0 if p an lvalue, 1 otherwise */

 again:

 switch( p->in.op ){

 case FLD:
  p = p->in.left;
  goto again;

 case UNARY MUL:
  /* fix the &(a=b) bug, given that a and b are structures */
  if( p->in.left->in.op == STASG ) return( 1 );

  /* and the f().a bug, given that f returns a structure */
  /* SS. 27.feb.87 this is not a bug! */
  /* if( p->in.left->in.op == UNARY STCALL ||
      p->in.left->in.op == STCALL ) return( 1 ); */
 case NAME:
 case OREG:
  if( ISARY(p->in.type) || ISFTN(p->in.type) ) return(1);
 case REG:
  return(0);

 default:
  return(1);

  }

 }

NODE *
bcon( i ){ /* make a constant node with value i */
 register NODE *p;

 p = block( ICON, NIL, NIL, INT, 0, INT );
 p->tn.lval = i;
 p->tn.rval = NONAME;
 return( clocal(p) );
 }

NODE *
bpsize(p) register NODE *p; {
 return( offcon( psize(p), p->in.type, p->fn.cdim, p->fn.csiz ) );
 }

OFFSZ
psize( p ) NODE *p; {
 /* p is a node of type pointer; psize returns the
    size of the thing pointed to */

 if( !ISPTR(p->in.type) ){
  uerror( 106 );  /* pointer required */
  return( SZINT );
  }
 /* note: no pointers to fields */
 return( tsize( DECREF(p->in.type), p->fn.cdim, p->fn.csiz ) );
 }

NODE *
convert( p, f )  register NODE *p; {
 /*  convert an operand of p
     f is either CVTL or CVTR
     operand has type int, and is converted by the size of the other side
     */

 register NODE *q, *r;


#ifndef BUG1
 if( tdebug > 1 ) printf( "convert( %o, %o )\n", p, f );
#endif

 q = (f==CVTL)?p->in.left:p->in.right;

 r = block( PMCONV,
  q, bpsize(f==CVTL?p->in.right:p->in.left), INT, 0, INT );
#ifndef BUG1
 if( bdebug > 1 ) printf( "end of convert():\n" );
#endif
 r = clocal(r);
 if( f == CVTL )
  p->in.left = r;
 else
  p->in.right = r;
 return(p);

 }

econvert( p ) register NODE *p; {

 /* change enums to ints, or appropriate types */

 /* register TWORD ty;  */

 p->fn.csiz = INT;
 MODTYPE(p->in.type,INT);

/* SS. 06.mar.87 all enums are integer types 
 if( (ty=BTYPE(p->in.type)) == ENUMTY || ty == MOETY ) {
  if( dimtab[ p->fn.csiz ] == SZCHAR ) ty = CHAR;
  else if( dimtab[ p->fn.csiz ] == SZINT ) ty = INT;
  else if( dimtab[ p->fn.csiz ] == SZSHORT ) ty = SHORT;
  else ty = LONG;
  ty = ctype( ty );
  p->fn.csiz = ty;
  MODTYPE(p->in.type,ty);
  if( p->in.op == ICON && ty != LONG )
   p->in.type = p->fn.csiz = INT;
  }
*/
 }


NODE *
pconvert( p ) register NODE *p; {
 /* if p should be changed into a pointer, do so */

#ifndef BUG1
 if( tdebug > 1 ) printf( "pconvert( %o )\n", p );
#endif

 if( ISARY( p->in.type) ){
  p->in.type = DECREF( p->in.type );
  ++p->fn.cdim;
  return( buildtree( UNARY AND, p, NIL ) );
  }
 if( ISFTN( p->in.type) )
  return( buildtree( UNARY AND, p, NIL ) );

 return( p );
 }

NODE *
oconvert(p) register NODE *p; {
 /* convert the result itself: used for pointer and unsigned */

#ifndef BUG1
 if( tdebug > 1 ) printf( "oconvert( %o )\n", p );
#endif

 switch(p->in.op) {

 case LE:
 case LT:
 case GE:
 case GT:
  if( ISUNSIGNED(p->in.left->in.type) || ISUNSIGNED(p->in.right->in.type) )  
   p->in.op += (ULE-LE);
 case EQ:
 case NE:
  return( p );

 case MINUS:
  return(  clocal( block( PVCONV,
   p, bpsize(p->in.left), INT, 0, INT ) ) );
  }

 cerror( 241, p->in.op );  /* illegal oconvert: %d */

 return(p);
 }

NODE *
ptmatch(p)  register NODE *p; {

 /* makes the operands of p agree; they are
    either pointers or integers, by this time */
 /* with MINUS, the sizes must be the same */
 /* with COLON, the types must be the same */

 TWORD t1, t2, t;
 int o, d2, d, s2, s;


#ifndef BUG1
 if( tdebug > 1 ) printf( "ptmatch( %o )\n", p );
#endif

 o = p->in.op;
 t = t1 = p->in.left->in.type;
 t2 = p->in.right->in.type;
 d = p->in.left->fn.cdim;
 d2 = p->in.right->fn.cdim;
 s = p->in.left->fn.csiz;
 s2 = p->in.right->fn.csiz;

 switch( o ){

 case ASSIGN:
 case RETURN:
 case CAST:
  {  break; }

 case MINUS:
  {  if( psize(p->in.left) != psize(p->in.right) ){
   uerror( 107 );  /* illegal pointer subtraction */
   }
     break;
     }
 case COLON:
  {  if( t1 != t2 ) 
   uerror( 108 );  /* illegal types in : */
     break;
     }
 default:  /* must work harder: relationals or comparisons */

  if( !ISPTR(t1) ){
   t = t2;
   d = d2;
   s = s2;
   break;
   }
  if( !ISPTR(t2) ){
   break;
   }

  /* both are pointers */
  if( talign(t2,s2) < talign(t,s) ){
   t = t2;
   s = s2;
   }
  break;
  }

 p->in.left = makety( p->in.left, t, d, s );
 p->in.right = makety( p->in.right, t, d, s );
 if( o!=MINUS && !logop(o) ){

  p->in.type = t;
  p->fn.cdim = d;
  p->fn.csiz = s;
  }

#ifndef BUG1
 if( bdebug > 1 ) printf( "end of ptmatch():\n" );
#endif
 return(clocal(p));
 }


NODE *
tymatch(p)  register NODE *p; {

 /* satisfy the types of various arithmetic binary ops */

 /* rules are:
  if assignment, op, type of LHS
  if any float or doubles, make double
  if any longs, make long
  otherwise, make int
  if either operand is unsigned, the result is...
 */

 register TWORD t1, t2, t, tu;
 register o, u;

 o = p->in.op;

 t1 = p->in.left->in.type;
 t2 = p->in.right->in.type;
 if( (t1==UNDEF || t2==UNDEF) && o!=CAST )
  uerror( 109 );  /* void type illegal in expression */

 u = 0;
 if( ISUNSIGNED(t1) ){
  u = 1;
  t1 = DEUNSIGN(t1);
  }
 if( ISUNSIGNED(t2) ){
  u = 1;
  t2 = DEUNSIGN(t2);
  }

 if( ( t1 == CHAR || t1 == SHORT ) && o!= RETURN ) t1 = INT;
 if( t2 == CHAR || t2 == SHORT ) t2 = INT;

 if( t1==DOUBLE || t1==FLOAT || t2==DOUBLE || t2==FLOAT ) t = DOUBLE;
 else if( t1==LONG || t2==LONG ) t = LONG;
 else t = INT;

 if( asgop(o) ){
  tu = p->in.left->in.type;
  t = t1;
  }
 else {
  tu = (u && UNSIGNABLE(t))?ENUNSIGN(t):t;
  }

 /* because expressions have values that are at least as wide
    as INT or UNSIGNED, the only conversions needed
    are those involving FLOAT/DOUBLE, and those
    from LONG to INT and ULONG to UNSIGNED */

 if( t != t1 ) p->in.left = makety( p->in.left, tu, 0, (int)tu );

 if( t != t2 || o==CAST ) p->in.right = makety( p->in.right, tu, 0, (int)tu );

 if( asgop(o) ){
  p->in.type = p->in.left->in.type;
  p->fn.cdim = p->in.left->fn.cdim;
  p->fn.csiz = p->in.left->fn.csiz;
  }
 else if( !logop(o) ){
  p->in.type = tu;
  p->fn.cdim = 0;
  p->fn.csiz = t;
  }

# ifndef BUG1
 if( tdebug ) printf( "tymatch(%o): %o %s %o => %o\n",p,t1,opst[o],t2,tu );
# endif

 return(p);
 }

NODE *
makety( p, t, d, s ) register NODE *p; TWORD t; {
 /* make p into type t by inserting a conversion */

#ifndef BUG1
 if( tdebug > 1 )
  printf( "makety( %o, 0%o, %d, %d )\n", p,t,d,s );
#endif

 if( p->in.type == ENUMTY && p->in.op == ICON ) econvert(p);
 if( t == p->in.type ){
  p->fn.cdim = d;
  p->fn.csiz = s;
  return( p );
  }

 if( t & TMASK ){
  /* non-simple type */
  return( block( PCONV, p, NIL, t, d, s ) );
  }

 if( p->in.op == ICON ){
  if( t==DOUBLE || t==FLOAT ){
   p->in.op = FCON;
   if( ISUNSIGNED(p->in.type) ){
    p->fpn.dval = /* (unsigned CONSZ) */ p->tn.lval;
    }
   else {
    p->fpn.dval = p->tn.lval;
    }

   p->in.type = p->fn.csiz = t;
#ifndef BUG1
   if( bdebug > 1 ) printf( "end of makety():\n" );
#endif
   return( clocal(p) );
   }
  }

 return( block( SCONV, p, NIL, t, d, s ) );

 }

NODE *
block( o, l, r, t, d, s ) register NODE *l, *r; TWORD t; {

 register NODE *p;

 p = talloc();
 p->in.op = o;
 p->in.left = l;
 p->in.right = r;
 p->in.type = t;
 p->fn.cdim = d;
 p->fn.csiz = s;
 return(p);
 }

icons(p) register NODE *p; {
 /* if p is an integer constant, return its value */
 int val;

 if( p->in.op != ICON ){
  uerror( 110 );  /* constant expected */
  val = 1;
  }
 else {
  val = p->tn.lval;
  if( val != p->tn.lval ) 
   uerror( 111);  /* constant too big for cross-compiler */
  }
 tfree( p );
 return(val);
 }

/*  the intent of this table is to examine the
 operators, and to check them for
 correctness.

 The table is searched for the op and the
 modified type (where this is one of the
 types INT (includes char and short), LONG,
 DOUBLE (includes FLOAT), and POINTER

 The default action is to make the node type integer

 The actions taken include:
  PUN   check for puns
  CVTL   convert the left operand
  CVTR   convert the right operand
  TYPL   the type is determined by the left operand
  TYPR   the type is determined by the right operand
  TYMATCH   force type of left and right to match, by inserting conversions
  PTMATCH   like TYMATCH, but for pointers
  LVAL   left operand must be lval
  CVTO   convert the op
  NCVT   do not convert the operands
  OTHER    handled by code
  NCVTR    convert the left operand, not the right...

 */

# define MINT 01  /* integer */
# define MDBI 02   /* integer or double */
# define MSTR 04  /* structure */
# define MPTR 010  /* pointer */
# define MPTI 020  /* pointer or integer */
# define MENU 040 /* enumeration variable or member */

opact( p )  NODE *p; {

 register mt12, mt1, mt2, o;

#ifndef BUG1
 if( tdebug > 2 ) printf( "opact( %o ) %s\n", p, opst[p->in.op] );
#endif

 mt12 = 0;

 switch( optype(o=p->in.op) ){

 case BITYPE:
  mt12=mt2 = moditype( p->in.right->in.type );
 case UTYPE:
  mt12 &= (mt1 = moditype( p->in.left->in.type ));

  }

 switch( o ){

 case NAME :
 case STRING :
 case ICON :
 case FCON :
 case CALL :
 case UNARY CALL:
 case UNARY MUL:
  {  return( OTHER ); }
 case UNARY MINUS:
  if( mt1 & MDBI ) return( TYPL );
  break;

 case COMPL:
  if( mt1 & MINT ) return( TYPL );
  break;

 case UNARY AND:
  {  return( NCVT+OTHER ); }

 case CBRANCH: /* SS. 27.feb.87 was only ret(0),which causes cerror()*/
 case NOT:
  if( !(mt1 & MSTR) )  return( 0 );
  else { /* SS. 27.feb.87 may be "break", but this msg is better*/
   uerror( 127 );  /* illegal use of structure */
   return(NCVT);
  }
 case INIT:
 case CM:
  return(0);
 
 case ANDAND:   /* SS. 27.feb.87 was only ret(0),which causes cerror()*/
 case OROR:
  if( (!(mt1 & MSTR)) & (!(mt2 & MSTR)) ) return(0);
  else { /* SS. 27.feb.87 may be "break", but this msg is better*/
   uerror( 127 );  /* illegal use of structure */
   return(NCVT);
  }

 case MUL:
 case DIV:
  if( mt12 & MDBI ) return( TYMATCH );
  break;

 case MOD:
 case AND:
 case OR:
 case ER:
  if( mt12 & MINT ) return( TYMATCH );
  break;

 case LS:
 case RS:
  if( mt12 & MINT ) return( TYMATCH+OTHER );
  break;

 case EQ:
 case NE:
 case LT:
 case LE:
 case GT:
 case GE:
  if( (mt1&MENU)||(mt2&MENU) ) return( PTMATCH+PUN+NCVT );
  if( mt12 & MDBI ) return( TYMATCH+CVTO );
  else if( mt12 & MPTR ) return( PTMATCH+PUN );
  else if( mt12 & MPTI ) return( PTMATCH+PUN );
  else break;

 case QUEST:
 case COMOP:
  if( mt2&MENU ) return( TYPR+NCVTR );
  return( TYPR );

 case STREF:
  return( NCVTR+OTHER );

 case FORCE:
  return( TYPL );

 case COLON:
  if( mt12 & MENU ) return( NCVT+PUN+PTMATCH );
  else if( mt12 & MDBI ) return( TYMATCH );
  else if( mt12 & MPTR ) return( TYPL+PTMATCH+PUN );
  else if( (mt1&MINT) && (mt2&MPTR) ) return( TYPR+PUN );
  else if( (mt1&MPTR) && (mt2&MINT) ) return( TYPL+PUN );
  else if( mt12 & MSTR ) return( NCVT+TYPL+OTHER );
  break;

 case RETURN:
  if( mt1 == 0 ) /* SS. 04.Jun.87 don't output error, because the
        "void func can't ret value" is already generated */
   return(0); 
   /* 04.Jun.87 added the 3 lines above */ 
 case ASSIGN:
  if( mt12 & MSTR ) return( LVAL+NCVT+TYPL+OTHER );
 case CAST:
  if(o==CAST && mt1==0)return(TYPL+TYMATCH);
  if( mt12 & MDBI ) return( TYPL+LVAL+TYMATCH );
  else if( (mt1&MENU)||(mt2&MENU) ) return( LVAL+NCVT+TYPL+PTMATCH+PUN );
  /* SS. 27.feb.87 give the error only if the rhs is call and the type is TVOID */
  else if( mt2 == 0 && callop(p->in.right->in.op) ) break;
  else if( mt1 & MPTR ) return( LVAL+PTMATCH+PUN );
  else if( mt12 & MPTI ) return( TYPL+LVAL+TYMATCH+PUN );
  break;

 case ASG RS:
 case ASG LS:
  if( mt12 & MINT ) return( TYPL+LVAL+OTHER );
  break;
 case ASG MUL:
 case ASG DIV:
  if( mt12 & MDBI ) return( LVAL+TYMATCH );
  break;

 case ASG MOD:
 case ASG AND:
 case ASG OR:
 case ASG ER:
  if( mt12 & MINT ) return( LVAL+TYMATCH );
  break;

 case ASG PLUS:
 case ASG MINUS:
 case INCR:
 case DECR:
  if( mt12 & MDBI ) return( TYMATCH+LVAL );
  else if( (mt1&MPTR) && (mt2&MINT) ) return( TYPL+LVAL+CVTR );
  break;

 case MINUS:
  if( mt12 & MPTR ) return( CVTO+PTMATCH+PUN );
  if( mt2 & MPTR ) break;
 case PLUS:
  if( mt12 & MDBI ) return( TYMATCH );
  else if( (mt1&MPTR) && (mt2&MINT) ) return( TYPL+CVTR );
  else if( (mt1&MINT) && (mt2&MPTR) ) return( TYPR+CVTL );

  }
 uerror( 112, opst[o] );  /* operands of %s have incompatible types */
 return( NCVT );
 }

moditype( ty ) TWORD ty; {

 switch( ty ){

 case TVOID:
 case UNDEF:
  return(0); /* type is void */
 case ENUMTY:
 case MOETY:
  return( MENU );

 case STRTY:
 case UNIONTY:
  return( MSTR );

 case CHAR:
 case SHORT:
 case UCHAR:
 case USHORT:
  return( MINT|MPTI|MDBI );
 case UNSIGNED:
 case ULONG:
 case INT:
 case LONG:
  return( MINT|MDBI|MPTI );
 case FLOAT:
 case DOUBLE:
  return( MDBI );
 default:
  return( MPTR|MPTI );

  }
 }

NODE *
doszof( p )  register NODE *p; {
 /* do sizeof p */
 int i;

 /* whatever is the meaning of this if it is a bitfield? */
 if ( ISCHAR(p->in.type) ) i = 1;
 else i = tsize( p->in.type, p->fn.cdim, p->fn.csiz )/(SZCHAR/4);

 tfree(p);
 if( i <= 0 ) 
  werror( 17 );  /* sizeof returns 0 */
 p = bcon( i );
 /* sizeof is a unsigned ICON, change the type here and look 
    for it when folding constants in conval() */
 p->in.type = UNSIGNED;
 return( p ); /* SS. 27.feb.87 it was only return(bcon(i)) */
 }

# ifndef BUG2
eprint( p, down, a, b ) register NODE *p; int *a, *b; {
 register ty;

 *a = *b = down+1;
 while( down > 1 ){
  printf( "\t" );
  down -= 2;
  }
 if( down ) printf( "    " );

 ty = optype( p->in.op );

 printf("%o) %s, ", p, opst[p->in.op] );
 if( ty == LTYPE ){
  if (p->in.op == FCON)
   printf("%lf, ", p->fpn.dval);
  else {
  printf( CONFMT, p->tn.lval );
  printf( ", %d, ", p->tn.rval );
  }
  }
 tprint( p->in.type );
 printf(", %d, %d", p->fn.cdim, p->fn.csiz);
 /* SS. 26.nov.86 print stalign field containing stab index */
 printf( ", sta=%d", p->in.stalign );
 printf("\n");
 }
# endif


ecomp( p ) register NODE *p; {
# ifndef BUG1
 if( edebug ) fwalk( p, eprint, 0 );
# endif
 if( !reached ){
  werror( 18 );  /* statement not reached */
  reached = 1;
  }
#ifndef BUG1
 if( edebug > 1 ) printf( "ecomp() -> optim()\n" );
#endif
 p = optim(p);
 ecode( p );
 tfree(p);
 }

p2tree(p) register NODE *p; {
 register ty;

# ifdef MYP2TREE
 MYP2TREE(p);  /* local action can be taken here; then return... */
# endif

 ty = optype(p->in.op);

 switch( p->in.op ){

 case FCON : /* dval should not be changed 30.05.86 */
  break;
 case NAME:
 case ICON:
  if( p->tn.rval == NONAME ) p->in.name = "";
  else if( p->tn.rval >= 0 ) /* copy name from exname */
/* MK 20.10.86 BACK to original static names  */

   p->in.name = tstr(stab[p->tn.rval].sname);
  else {
   char temp[32];
   sprintf( temp, LABFMT, -p->tn.rval );
   p->in.name = tstr(temp);
  }
  break;

 case STARG:
 case STASG:
 case STCALL:
 case UNARY STCALL:
  /* set up size parameters */
  p->stn.stsize = (tsize(STRTY,p->in.left->fn.cdim,p->in.left->fn.csiz)+SZCHAR-1)/SZCHAR;
  p->stn.stalign = talign(STRTY,p->in.left->fn.csiz)/SZCHAR;
  break;

/* SS. 09.Feb.87  there are no REG nodes in the tree after Pass1 */
/*
 case REG:
  rbusy( p->tn.rval );
*/
 default:
  p->in.name = "";
  }

 p->in.rall = NOPREF;

 if( ty != LTYPE ) p2tree( p->in.left );
 if( ty == BITYPE ) p2tree( p->in.right );
 }

