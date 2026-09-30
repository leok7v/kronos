/* this file contains code which is dependent on the target machine */

/* Revision log:
 *  14.Oct.86 MK  struct sw heapsw[SWITSZ] removed (not used)
 *                actions related to gdebug removed
 *  20.Oct.86 MK  STATIC names in globalname(), globsize in defnam()
 *  27.Oct.86 MK  calls to exname removed
 *  29.Oct.86 MK  lactloc,locctr(),aocode(),aobeg(),aoend(),ejobcode() removed
 *  16.nov.86 SS  char array type testing replaced with func call.
 *  01.Dec.86 MK  PROFILE added
 *  10.Dec.86 MK  arguments -> locals in bfcode
 *  02.Jan.87 MK  defnam(): memory allocation for global aggregates, globno
 *  26.Jan.87 MK  bfcode(): entr M%d, no jumps -> <-
 *  29.jan.87 SS  bfcode(): struct args implemented
 *  30.jan.87 SS  release string moved here
 *  30.Jan.87 MK  functions returning structures
 *  06.Feb.87 SS  where() moved to common
 *  20.Feb.87 MK  prints() for strings that don't contain formats;
 *    defalign() & zecode() removed
 *  10.Apr.87 SS  bug fixed in clocal(), see NAME label
 *  14.Apr.87 SS  introduced function number in bfcode()
 *  22.Apr.87 MK  outline call in genswitch(), defnam() & efcode()
 *  03.Sep.87 SS  rewritten code gen. for func returning structures
 *  18.Sep.87 MK  checks for no. of functions and variables; memory
 *    allocation for aggregates back to seq. "li;alloc;.."
 *  25.Sep.87 SS  defnam(): now separating extern "void" func from non-void func
 *  03.Dec.87 SS  show debug version in the release message
 *  12.Jan.87 SS  fixed bug in clocal()
 *  18.Jan.87 SS  fixed bug in clocal()
 *  28.Sep.88 SS  generate Modula-2 proc entry code if m2calls is true
 */

# include "mfile1.h"

char *release =
#ifndef BUG4
  "\tPass 2 -- Code Gen v0.9x     /28-Sep-88/";
#else
 "\tPass 2 -- Code Gen v0.9      /28-Sep-88/";
#endif

OFFSZ paramoff;   /* SS 28.sep.88 offset value after processing args */
extern int verbose, /* if set then output function names */
    m2calls, /* SS 28.sep.88 if set then use Modula-2 func protocol*/
    globsize,
    globno, /* in scan.c */
    fnr;  /* function number */
#ifdef PROFILE
extern int profnum;
#endif

/* MK 22.Apr.87 */
extern int asflag; /* Do we include C source lines in code ? */
extern void outline();

int proflg;  /* are we generating profiling code? */
int strftn;  /* is the current function one which returns a value */
int fdefflag;     /* are we within a function definition ? */

#ifndef BUG4
int xdebug;  /* general debug flag */
#endif


int labelno;

branch( n ){
 /* output a branch to label n */
 /* exception is an ordinary function branching to retlab: 
       then, return */
 if( n == retlab && !strftn )
/* tbl */ prints("\trtn\n");
/* tbl */ else printf("\tjump\tM%d\n", n);
}


deflab( n ){
 /* output something to define the current position as label n */
/* tbl */   printf( "M%d:\n", n );
}

int crslab = 10;

getlab(){
 /* return a number usable for a label */
 return( ++crslab );
 }

int reg_use = 11;

struct symtab str_ret_area;  /* SS. 03.sep.87 symtab entry for struct_ret_area*/
char str_area_name[15];       /* SS. 03.sep.87 buffer for struct_ret_area name*/

efcode(){  /* code for the end of a function */

/* MK 22.Apr.87 */
 if (asflag)
  outline();
 if( strftn ){  /* copy output to caller (address of structure is L6,
     address of value to be copied is on E-stack) */

  register struct symtab *p;
  register TWORD t;
  int sz;

  p = &stab[curftn];
  t = DECREF(p->stype);

  deflab( retlab );

  sz = tsize(t, p->dimoff, p->sizoff) / SZCHAR;

 /* MK 30.Jan.86  L6 is address of structure (L4, L5 are swap area) */

  reached = 1;
  strftn = 0;

#ifdef OLDSTRFUN
/* tbl */ prints("\tllw\t6\n\tswap\n\tli\t");
/* tbl */ printf("%d\n\tmove\n", sz);
/* tbl */ prints("\tllw\t6\n\trtn\n");
#endif
  printf("\tlw\t%s\n\tswap\n\tli\t%d\n\tmove\n",str_area_name,sz);
  printf("\tlw\t%s\n\trtn\n", str_area_name );
 }

 else if (reached) {

/* if function is not void, but didn't explicitly return a value, do it here */

  if(BTYPE(stab[curftn].stype) != UNDEF)
/* tbl */        prints("\tli\t0\n");
  branch( retlab );
 }

 reg_use = 11;
 p2bend();
 fdefflag = 0;
/* tbl */ D_END; globalname( &stab[curftn]);
}


int ftlab1;

bfcode( a, n ) int a[]; {

 /* code for the beginning of a function; a is an array of
  indices in stab for the arguments; n is the number */
 register i;
 register temp;
 register struct symtab *p, *qq;

 p = &stab[curftn];
/* MK. 18.Sep.87 */
 if (++fnr > 255)
  cerror (297);  /* Too many functions */

 if( verbose ){   /* SS. 30.mar.87 output function name */
  fprintf( stderr, "%3d: %s,\t[%d]\n", fnr, p->sname, lineno );
#ifdef BUFSTDERR
  fflush(stderr);
#endif
 }
 if( cdebug )
/* tbl */  printf(";\n;\t%s\n;\n", p->sname);

 defnam( p );
 temp = DECREF(p->stype);
 strftn = (temp==STRTY) || (temp==UNIONTY);
 retlab = getlab();

 /* routine prolog */
/* SS. 03.sep.87 code for allocating struct ret area */
 if( strftn ){
  str_ret_area = *p;
  qq = &str_ret_area;
  qq->stype = temp;
  qq->sclass = STATIC; /* declare as static variable */
  sprintf( str_area_name, "__STR_FTN_#%d", fnr );
  qq->sname = str_area_name;
  defnam( qq );
 }

 ftlab1 = getlab();

 if ( m2calls && n > 1 ){ /* SS 28.sep.88 # args is > 1 */
/* tbl */ printf("\tli\t0\n");   /* SS 28.sep.88 reserve room for LW4  */
/* tbl */ printf("\tstore\n");   /* entr 1 replaced with li0 */
 }

/* tbl */ printf("\tentr\tM%d\n", ftlab1); /* reserve space for locals */

 if ( m2calls && n == 1 )
/* tbl */ printf("\tslw\t5\n");  /* SS 28.sep.88 store the only argument */ 

#ifdef PROFILE
 if(proflg){
  profnum += 1;  
/* tbl */ prints("\tlw\tprofile\n");
/* tbl */ printf("\tlsa\t%d\n", profnum);
/* tbl */ prints("\tinc1\n");
 }
#endif
#ifdef OLDSTRFUN
 if (strftn) autooff += 32;  /* one local for structure ret.addr. */
#endif

 for( i=0; i<n; ++i ){
  p = &stab[a[i]];
  autooff += 32;
  p->offset = - autooff;

  if( p->stype == STRTY || p->stype == UNIONTY){
/* tbl */  printf( "\tllw\t%d", autooff/32 );
   if( cdebug )
/* tbl */   printf( "\t%%&%s\n", stab[a[i]].sname );
   else
/* tbl */   putchar( '\n' );
/* tbl */  printf( "\tli\t%d\n", dimtab[p->sizoff]/32 );
/* tbl */  printf( "\tpcop\t%d\n", autooff/32 );
  }
 }

 /* SS. 28.sep.88 save offset value after processing args. Used later 
   in the eobl2() to compute the correct number for "entr" instr. (we
   must know the number of locals in the function if m2call is active) */
 paramoff = autooff;

 fdefflag = 1;

} /* bfcode */


bccode () { 
 /* called just before the first executable statement */
 /* by now, the automatics and register variables are allocated */

 SETOFF( autooff, SZINT );
 /* set aside store area offset */
 p2bbeg( autooff, regvar );
 reg_use = (reg_use > regvar ? regvar : reg_use);
 }


defnam( p ) register struct symtab *p; {
 /* define the current location as the name p->sname */

OFFSZ off;

/* MK 22.Apr.87 */
 if (asflag) outline();
 if( p->sclass == EXTDEF ){
/* tbl */        if (ISFTN( p->stype)){ /* SS.25.Sep.87 added test for void func */
   if( DECREF(p->stype)!=UNDEF ) D_FUN;
   else D_VOIDFUN;
  }
  else D_VAR;
 }
 if( p->sclass == STATIC) {
/* tbl */        if (ISFTN( p->stype)) D_STATFUN; else D_STATVAR;
 }
/* tbl */ globalname(p);
 if(ISFTN( p->stype)) return;
/* MK. 18.Sep.87 */
 if (++globno > 253)
  cerror (298);  /* Too many global variables */
 off = tsize( p->stype, p->dimoff, p->sizoff);
 SETOFF (off, ALINT);
 off  /= 32;
/* SS. 03.07.  if (off > 1 || p->stype == UNIONTY || p->stype == STRTY) { */

 if ( off && ISAGGREGATE(p->stype) ){
 /* allocate space for global aggregates */
/* tbl */        D_BEGALLOC;

/*  MK. 18.Sep.87  alloc for each aggregate separately
   tbl    if(globsize < 256)
   printf("\tlla\t%d\n", globsize);
   tbl    else printf("\tlla\t255\n\tli\t%d\n\tadd\n", globsize-255);
*/

/* tbl */ printf("\tli\t%d\n\talloc\n", off);
  /* NB! SS. 16.nov.86 char array type checking changed. */

  if( ischarary(p->stype) )
/* tbl */  prints("\tshl2\n");
/* tbl */        prints("\tsw\t"); globalname(p);
/* tbl */        D_ENDALLOC;
  globsize += off;
 }
}


bycode( t, i ){
static int lastpos = 0;
static int lastoctal = 0;

 /* put byte i+1 in a string */

 if ( t < 0 ){
/* tbl */       if ( i != 0 ) prints( "\"\n" );
/* MK */ lastoctal = lastpos = 0;
 } else {
/* M.K. */
/* tbl */  if ( !i ) {
   D_DATA; prints("\"");
  }
  if (!lastoctal && lastpos >= 074) { 
   /* end of line; continue on next line */
/* tbl */               prints("\\\n");
   lastoctal = lastpos = 0;
  }
  if ( t == '\\' || t == '"'){
   lastpos += 2;
   lastoctal = 0;
/* tbl */               printf("\\%c", t);
  }
  else if ( t < 040 || t >= 0177) {
   /* 040 = 32 -- space, 0177 = 127 -- DEL */
   lastpos += 4;
   lastoctal ++;
/* tbl */               printf("\\%o",t);
  }
  else if (lastoctal && '0' <=t && t <= '9') {
   lastpos = lastoctal = 0;
/* tbl */               printf("\\\n%c", t );
  }
  else {
   lastpos += 1;
/* tbl */  putchar(t);
  }
 }
}


fldal( t ) unsigned t; { 
 /* return the alignment of field of type t */
 uerror( 78 );  /* illegal field type */
 return( ALINT );
 }


fldty( p ) struct symtab *p; { 
 /* fix up type of field p */
 }


main( argc, argv ) char *argv[]; {

#ifdef BUFSTDERR
 char errbuf[BUFSIZ];
 setbuf(stderr, errbuf);
#endif

 return (mainp1( argc, argv ));
}


genswitch(p,n) register struct sw *p;{
 /* p points to an array of structures, each consisting
  of a constant value and a label.
  The first is >=0 if there is a default label;
  its value is the label number
  The entries p[1] to p[n] are the nontrivial cases
  */
 register i;
 int nextlab;

 /* simple switch code */
#ifndef BUG4
 if( xdebug )  prints("%%\tstarting switch code:\n");
#endif
/* MK 23.Apr.87 */
 if (asflag)
  outline();
 for( i=1; i<=n; ++i ){
  /* already on top of E-stack */

/* tbl */        printf("\tcopt\n\tli\t%d\n\tequ\n", p[i].sval);
/* tbl */        printf("\tjcnd\tM%d\n", (nextlab = getlab()));
/* tbl */ printf("\tdrop\n\tjump\tM%d\n", p[i].slab);
  deflab(nextlab);
 }

/* tbl */ prints("\tdrop\n");
 if( p->slab>=0 ) branch( p->slab );
 }


/* MK function to print global names and name labels, 27.04.86 */

globalname(p) register struct symtab *p; 
{
/* MK 20.10.86 STATIC names back to original */
    if (p->sclass == EXTDEF || p->sclass == STATIC)
/* tbl */ printf("%s\n", p->sname);

}


void prints (s) char *s; {
int c;

 while (c = *s++)
  putchar (c);
}


/* local.c */

/* Revision log:
 * 14.Oct.86 MK CHANGES for Kronos version
 *              cast() -> VAX
 *              gdebug functions, cisreg(), exname() & tlen() removed
 *         cendarg(), noinit() & andable() removed
 * 15.Nov.86 SS PCONV processing rewritten
 *         added func ischarary()
 * 01.Dec.86 MK PROFILE
 * 03.Feb.87 MK isitfloat() removed; xdebug brought here
 * 04.Feb.87 MK optim.c brought here
 * 10.Feb.87 SS pass unsigned by ctype() and convert ulong->unsigned
 * 25.Feb.87 SS SCONV processing rewritten
 * 14.Oct.87 SS fixed two bugs in clocal(), which reported by C hackers from Novosibirsk
 */

NODE *
clocal(p) NODE *p; {

 /* this is called to do local transformations on an expression tree
    preparitory to its being written out in intermediate code.

    the major essential job is rewriting the automatic variables and
    arguments in terms of REG and OREG nodes.

    conversion ops which are not necessary are also clobbered here
    in addition, any special features (such as rewriting exclusive or)
    are easily handled here as well.
 */

 register struct symtab *q;
 register NODE *r;
 register o;
 register m, ml;
#ifndef BUG4
 if (xdebug) {
  printf("CLOCAL() with:\n");
  fwalk (p, eprint, 0);
 }
#endif
 switch( o = p->in.op ){

 case NAME:
  /*SS.10.apr.87 added test for NONAME to avoid addressing error*/
  if( p->tn.rval < 0 || p->tn.rval == NONAME) { /* already processed; ignore... */
   /* return (p); */
   break;
  }
  q = &stab[p->tn.rval];
  switch( q->sclass ){

  case AUTO:
  case PARAM:
   /* fake up a structure reference */
   r = block( REG, NIL, NIL, PTR+STRTY, 0, 0 );
  /* MK. 30.05.86 stab index for locals -> tn.stalign */
   r->tn.stalign = p->tn.rval;
   r->tn.lval = 0;
  /* SS. 11.12.86  use STKREG as base register for arguments */
   r->tn.rval = STKREG;
 /*   r->tn.rval = (q->sclass==AUTO ? STKREG : ARGREG);  */
   p = stref( block( STREF, r, p, 0, 0, 0 ) );
   break;

  case ULABEL:
  case LABEL:
  case STATIC:
   if( q->slevel == 0 ) break;
   p->tn.lval = 0;
  /* MK 15.05.86 let statics have their stab indices ... */
   if (q->sclass != STATIC) p->tn.rval = -q->offset;
   break;

  case REGISTER:
   p->in.op = REG;
   p->tn.lval = 0;
   p->tn.rval = q->offset;
   break;

   } /* sclass switch */
  break; /* name node */

 case PCONV: {
  /* do pointer conversions  */
  NODE *l = p->in.left;
  m = p->in.type;
  ml = l->in.type;

  /* first of all deal with special cases, when we don't need PCONV */
  /* SS. 28.nov.86  NB! only for char ptr conversions */
  /* SS. 14.Oct.87 fixed bug which reported by C hackers from
  Novosibirsk: in case of "(char **) c",where c is array of char,
  the PCONV node was removed. To fix the bug we must test that the
  p->in.type is not PTR PTR char. Added "&& !ISPTR(..."  */
  if( ISCHAR(BTYPE(m)) && !ISPTR(DECREF(m)) ){ 
   if( (isfpreg(l) && ischarary(stab[l->tn.stalign].stype))
         || (l->in.op==MINUS && isfpreg(l->in.left)
      && ischarary(stab[l->in.left->tn.stalign].stype)) )
   goto inherit; /* local char array,adr is already multiplied */

   if( l->in.op==ICON && l->tn.rval != NONAME  
        && l->tn.rval>0 && ischarary(stab[l->tn.rval].stype) )
   goto inherit; /* global char array,adr is already multiplied */
  } /* btype was char */

 /* SS. 14.Oct.87 fixed bug reported by C hackers from Novosibirsk:
  don't inherit type in cases like that "(int *) (char *)0" 
 because this produce code "li0; shr2". Added the whole if(..){..} */
 if( !ISPTR(ml) && l->in.op==ICON && l->tn.rval==NONAME && l->tn.lval==0 ){
#ifndef BUG3
  if( xdebug > 1 )
   printf( "%s removed 0 CAST: m= %o, ml= %o\n", opst[p->in.op],m, ml );
#endif   
  p->in.op = FREE;
  return( p->in.left );
 }
   
  /* and now check all remaining situations */

  if( (ISCHAR(DECREF(m)) || ISCHAR(DECREF(ml)))
   || (ischarary(DECREF(m)) || ischarary(DECREF(ml))) )
    break;

  /* pointers,except char *, all have the same representation; 
     the type is inherited */

 inherit:
#ifndef BUG3
  if( xdebug > 1 )
   printf( "%s removed: m= %o, ml= %o\n", opst[p->in.op],m, ml );
#endif   
 /* SS. 25.feb.87 if we have (void) func(), then we can't inherit type */
  if( !(m == UNDEF && callop(p->in.left->in.op)) ){ 
   p->in.left->in.type = p->in.type;
   p->in.left->fn.cdim = p->fn.cdim;
   p->in.left->fn.csiz = p->fn.csiz;
  }
  p->in.op = FREE;
#ifndef BUG4
  if (xdebug) {
   printf("returning from CLOCAL with :\n");
   fwalk (p->in.left, eprint, 0);
  }
#endif
  return( p->in.left );
  }

 case SCONV:

/* MK & SS 04.Mar.87  extern double fun(); {(void) fun();} */
 if(p->in.type != UNDEF) {
  m = (p->in.type == FLOAT || p->in.type == DOUBLE );
  ml = (p->in.left->in.type == FLOAT || p->in.left->in.type == DOUBLE );
  if( m != ml ) break;
 }


  /* now, look for conversions downwards */

  m = p->in.type;
  ml = p->in.left->in.type;
  if( ISCHAR(m) && p->in.left->in.op==ICON ){ /* simulate the conversion here */
   p->in.left->tn.lval &= 0XFF;
  }
  else {
  /* meaningful ones are conversion of int to char and char to int */
  /* because if p->in.left->in.op is U* we must know to load with "lxb" instr. */
  /* SS 12-Jan-88 added 'ISPTR()' test to fix the bug when we
  have PCONV PTR char -> SCONV int; if we inherit the node then
  we got the cerror(compiler loop),because we havn't template 
  for PTR int -> PCONV int { int k,i; i=(char *)&k; }
  */
  /* SS 18-Jan-88 fixed bug {(void)memcpy();drop missing} */
   if(p->in.type != UNDEF) {
    if( ISCHAR(m) || ISCHAR(ml) || ISPTR(ml) ) break;
    if(p->in.left->in.op == FLD && ISUNSIGNED(ml) && !ISUNSIGNED(m) ) break;
   }
  }

  /* clobber conversion */
  /* checking tlen() is not meaningful, all types are same length*/
  goto inherit;

 case PVCONV:
 case PMCONV:
  if( p->in.right->in.op != ICON ) 
   cerror( 245, 0 );  /* bad conversion */
  p->in.op = FREE;
  p = ( buildtree( o==PMCONV?MUL:DIV, p->in.left, p->in.right ) );
  break;

  /* convert >> to << with negative shift count */
  /* only if type of left operand is not unsigned */
  /* 12.05.86 no need to do this on Kronos (SS).  */


 case FLD:
  /* make sure that the second pass does not make the
     descendant of a FLD operator into a doubly indexed OREG */

  if( p->in.left->in.op==UNARY MUL && (r=p->in.left->in.left)->in.op==PCONV)
         if( r->in.left->in.op==PLUS || r->in.left->in.op==MINUS )
    if( ISPTR(r->in.type) )
     p->in.left->in.type = ISUNSIGNED(p->in.left->in.type)?UCHAR:CHAR; 
  break;
 }

#ifndef BUG4
 if (xdebug) {
  printf("ret from CLOCAL with :\n");
  fwalk (p, eprint, 0);
 }
#endif

 return(p);
 }


NODE *
offcon( off, t, d, s ) OFFSZ off; TWORD t; {

 /* return a node, for structure references, which is suitable for
    being added to a pointer of type t, in order to be off bits offset
    into a structure */

 /* t, d, and s are the type, dimension offset, and sizeoffset */
 /* in general they  are necessary for offcon, but not on H'well */

 register NODE *p;
 p = bcon(0);

 /* SS. 15.nov.86  offset from char array must be in bytes  */
 p->tn.lval = off/( ischarary(DECREF(t)) ? 8 : SZCHAR);

/* p->tn.lval = off/SZCHAR;   */

#ifndef BUG2
 if( xdebug > 1 )
  printf( "offcon( %d, 0%o, %d, %d ) lval -> %d\n", off,t,d,s,p->tn.lval);
#endif
 return(p);
}


static inwd /* current bit offsed in word */;
static word /* word being built from fields */;

incode( p, sz ) register NODE *p; {

 /* generate initialization code for assigning a const c to a field of width sz */
 /* we assume that the proper alignment has been obtained */
 /* inoff is updated to have the proper final value */
 /* we also assume sz  < SZINT */

 if((sz+inwd) > SZINT) 
  cerror( 246 );  /* incode: field > int */
 word |= ((unsigned)(p->tn.lval<<(32-sz))) >> (32-sz-inwd);
 inwd += sz;
 inoff += sz;
 if(inoff%SZINT == 0) {
/*  printf("%%  INCODE\n");
  printf( "%% .long 0x%x\n", word);
*/
  word = inwd = 0;
  }
 }

cinit( p, sz ) NODE *p; {
 /* arrange for the initialization of p into a space of size sz */
 /* the proper alignment has been opbtained */
 /* inoff is updated to have the proper final value */

 ecode( p );
 inoff += sz;
 }


vfdzero( n ){ /* define n bits of zeros in a vfd */

 if( n <= 0 ) return;

 inwd += n;
 inoff += n;
 if( inoff%ALINT ==0 ) {
/*  printf("%%  VFDZERO n=%d\n", n);
  printf( "%% .long 0x%x\n", word );
*/
  word = inwd = 0;
  }
 }


ctype( type ){ /* map types which are not defined on the local machine */
/* MK 25.04.86 */
int btype ,new;

 btype = BTYPE(type);
 if (btype == UCHAR) new = CHAR;
 else if (btype == ULONG) new = UNSIGNED;  /* SS. 10-feb-87 included */
 else if (btype == DOUBLE) new = FLOAT;
 else if (btype == LONG || btype == SHORT || btype == USHORT) new = INT;
 else new = btype;
 MODTYPE(type, new);
 return( type );
 }


#ifndef BUG4
extern int bdebug;
#endif

ecode( p ) NODE *p; {

 /* walk the tree and write out the nodes.. */

 if( nerrors ) return;
 p2tree( p );
#ifndef BUG4
 /* 07.02.86 M.K. print the tree once more ... */
 if( bdebug ){
  printf (" Going to P2COMPILE :\n");
  fwalk (p, eprint, 0);
 }
#endif
 p2compile( p );
 }


ischarary( t ) TWORD t; {
/* returns true if type is char array */

 int ary = 0;

 while ( ISARY(t) ){
  t = DECREF(t);
  ary = 1;
 }
 return( ary && ISCHAR(t) );
} 

/* optim.c */

/* Revision log:
 * 27.Oct.86 MK oflag removed
 * 29.Oct.86 MK call to andable() removed
 * 17.dec.86 SS restored local optimizations related with constants
 * 21.dec.86 SS nncon() replaced with macro
 * 25.Feb.87 SS added LFCON macro
 */

# define SWAP(p,q) {sp=p; p=q; q=sp;}
# define RCON(p) (p->in.right->in.op==ICON)
# define RO(p) p->in.right->in.op
# define RV(p) p->in.right->tn.lval
# define LCON(p) (p->in.left->in.op==ICON)
# define LFCON(p) (p->in.left->in.op==FCON)
# define LO(p) p->in.left->in.op
# define LV(p) p->in.left->tn.lval
 /* is p a constant without a name */
# define nncon(p) (p->in.op==ICON && p->tn.rval==NONAME)


 /* mapping relationals when the sides are reversed */
short revrel[] ={ EQ, NE, GE, GT, LE, LT, UGE, UGT, ULE, ULT };

NODE *
optim(p) register NODE *p; {
/* local optimizations, most of which are probably machine independent */

 register o; 
 NODE *sp;
 int i;
 extern int ddebug;

 o = p->in.op;
#ifndef BUG1
 if( ddebug > 2 ) printf( "optim( %o, %s )\n", p, opst[o] );
#endif
 if( BTYPE(p->in.type)==ENUMTY || BTYPE(p->in.type)==MOETY ) econvert(p);
 if( optype(o) == LTYPE ) return(p);

 if( optype(o) == BITYPE ) p->in.right = optim(p->in.right);
 p->in.left = optim(p->in.left);

 /* collect constants */

 switch(o){

 case SCONV:
 case PCONV:
  return( clocal(p) );

/*  SS. 05.sep.86   don't convert pointer nodes to U* nodes */
#ifdef VAX
 case FORTCALL:
  p->in.right = fortarg( p->in.right );
  break;
#endif

 case UNARY AND:
  if( LO(p) != NAME ) 
   cerror( 253 );  /* & error */

  LO(p) = ICON;

  setuleft:
  /* paint over the type of the left hand side with the type of the top */
  p->in.left->in.type = p->in.type;
  p->in.left->fn.cdim = p->fn.cdim;
  p->in.left->fn.csiz = p->fn.csiz;
  p->in.op = FREE;
  return( p->in.left );

 case UNARY MUL:
  if( LO(p) != ICON ) break;
  LO(p) = NAME;
  goto setuleft;

 case MINUS:
  if( !nncon(p->in.right) ) break;
  RV(p) = -RV(p);
  o = p->in.op = PLUS;

 case MUL:
 case PLUS:
 case AND:
 case OR:
 case ER:
  /* commutative ops; for now, just collect constants */
  /* someday, do it right */
  if( nncon(p->in.left) || ((LCON(p) || LFCON(p)) && !RCON(p) ) ) 
   SWAP( p->in.left, p->in.right );
  /* make ops tower to the left, not the right */
  /* SS. 25.feb.87  NB! is it possible that the following 
     may cause problems if t1 is pointer and t3 is not ? */
  if( RO(p) == o ){
   NODE *t1, *t2, *t3;
   t1 = p->in.left;
   sp = p->in.right;
   t2 = sp->in.left;
   t3 = sp->in.right;
   /* now, put together again */
   p->in.left = sp;
   sp->in.left = t1;
   sp->in.right = t2;
   p->in.right = t3;
   }
#ifndef NOOPT
  /* SS. 21.dec.86 RCON(p) changed to nncon(p->in.right) to avoid
     negative subscript warning; isfpreg() check added to save the
     valid offset for Pass2 in case of address subtrees. */

  if(o==PLUS && LO(p)==MINUS && nncon(p->in.right)
    && RCON(p->in.left) && !isfpreg(p->in.left->in.left)
    && conval(p->in.right, MINUS, p->in.left->in.right)){
   zapleft:
   RO(p->in.left) = FREE;
   LO(p) = FREE;
   p->in.left = p->in.left->in.left;
  }
  /* SS. 17.dec.86 isapreg() check added to save the valid offset
     for Pass2 in case argument address subtrees. */

  if( RCON(p) && LO(p)==o && RCON(p->in.left) && !isapreg(p->in.left->in.left)
     && conval( p->in.right, o, p->in.left->in.right ) ){
   goto zapleft;
   }
  else
#endif
  if ( LCON(p) && RCON(p) && conval( p->in.left, o, p->in.right)){
   zapright:
   RO(p) = FREE;
   p->in.left = makety( p->in.left,p->in.type,p->fn.cdim,p->fn.csiz );
   p->in.op = FREE;
   return( clocal( p->in.left ) );
   }

  /* change muls to shifts */

  if( o==MUL && nncon(p->in.right) && (i=ispow2(RV(p)))>=0){
   if( !i ){ /* multiplication by 1 */
    goto zapright;
    }
   o = p->in.op = LS;
   p->in.right->in.type = p->in.right->fn.csiz = INT;
   RV(p) = i;
   }

  /* change +'s of negative consts back to - */
  if( o==PLUS && nncon(p->in.right) && RV(p)<0 ){
   RV(p) = -RV(p);
   o = p->in.op = MINUS;
   }
  break;

 case DIV:
  if( nncon( p->in.right ) && p->in.right->tn.lval == 1 ) goto zapright;
  break;

 case EQ:
 case NE:
 case LT:
 case LE:
 case GT:
 case GE:
 case ULT:
 case ULE:
 case UGT:
 case UGE:
  if( !LCON(p) && !LFCON(p) ) break;

  /* exchange operands */

  sp = p->in.left;
  p->in.left = p->in.right;
  p->in.right = sp;
  p->in.op = revrel[p->in.op - EQ ];
  break;

  }

 return(p);
} /* optim */


ispow2( c ) CONSZ c; {
register i;

 if( c <= 0 || (c&(c-1)) ) return(-1);
 for( i=0; c>1; ++i) c >>= 1;
 return(i);
 }

