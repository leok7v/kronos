/* Revision log:
 * 20.Oct.86 MK globsize after EQU
 * 27.Oct.86 MK removed: calls to exname,LCOMM stuff in nidcl(),strflg,putbyte()
 * 29.Oct.86 MK call to cendarg removed in dclargs(), to noinit in nidcl()
 *         ilocctr, minrvar & calls to locctr removed everywhere
 *         calls to aobeg(), aocode(), aoend() removed in clearst()
 * 03.Nov.86 MK CHAR -> INT in fixtype()
 * 16.nov.86 SS char array type testing replaced with func call.
 * 25.Nov.86 MK changes: instk() and doinit()
 * 21.dec.86 SS added test for zero offset in doinit()
 * 29.dec.86 SS removed unneeded "inb"|"ine" and added to beginit(),endinit()
 * 03.Jan.87 MK unknown array size initialization (instk, endinit)
 * 10.Jan.87 MK dimtab,stab,scnames, ccnames, strucoff, inoff from xdefs
 * 03.Feb.87 MK paramstk , paramno, argoff (?) brought here from xdefs
 *         ftnend(), yyaccpt()  --> cgram
 * 04.Feb.87 MK offsz, curftn, ftnno, ddebug -> end
 * 10.Feb.87 MK dat labels on same line (getstr)
 * 19.Feb.87 MK no init with 0 (doinit); doinit partly rewritten
 * 20.Feb.87 MK calls to defalign removed; prints
 * 26.Feb.87 SS code improved in defid(), mknonuniq() and lookup()
 *           SS movestab() replaced with inline struct assignment
 *           SS array size checking added to tsize()
 * 05.Mar.87 SS if not in excII environment then check for modula keyword
 * 13.Mar.87 SS yyerror() removed, yyerror() in cgram.c replaced with uerror()
 * 23.Apr.87 MK call to outline() in beginit() & endinit()
 * 18.Sep.87 MK memory allocation for aggregates back to alloc
 * 22.Sep.87 SS fixed bug in fixtype(), now convert type FTN+CHAR also
 * 25.Sep.87 SS removed qflag related stuff
 * 13.Oct.87 SS fixed bug in fixtype(), don't convert types with sclass TYPEDEF
 *  (reported by C hackers from Novosibirsk)
 */

# include "mfile1.h"


extern int ischarary();   /* SS. 16.nov.86 from local.c */
extern int globsize;

extern int asflag;  /* MK  23.Apr.87 from scan.c  */
extern void  outline();

int strucoff; /* the next structure offset position */
extern int curftn,  /* "current" function */  /* HERE */
 ftnno;   /* "current" function number */   /* HERE */
int instruct, /* "in structure" flag */
 blevel,  /* block level: 0 for extern, 1 for ftn args,
    >= 2 inside function */
 curdim;  /* current offset into the dimension table */

struct instk {
 int in_sz;   /* size of array element */
 int in_x;    /* current index for structure member in structure initializations */
 int in_n;    /* number of initializations seen */
 int in_s;    /* sizoff */
 int in_d;    /* dimoff */
 TWORD in_t;    /* type */
 int in_id;   /* stab index */
 int in_fl;   /* flag which says if this level is controlled by {} */
 OFFSZ in_off;  /* offset of the beginning of this level */
 }
instack[10],
*pstk;

struct symtab stab [SYMTSZ + 1]; /* one extra slot for scratch */
int dimtab [DIMTABSZ];

int paramno; /* the number of function parameters */
int paramstk [PARAMSZ];  /* host-port: was static, extern in mfile1.h */

struct symtab *relook();

extern int cdebug;  /* SS. 02.07.86 controls comments, default set 0 in scan.c*/
extern int globsize;
#ifndef BUG1
int ddebug; /* HERE */
int idebug;
#endif
int autooff; /* the next unused automatic offset */

struct symtab * mknonuniq();

defid( q, class )  NODE *q; {
 register struct symtab *p;
 int idp;
 TWORD type;
 TWORD stp;
 int scl;
 int dsym, ddef;
 int slev, temp;

 if( q == NIL ) return;  /* an error was detected */

 if( q < node || q >= &node[TREESZ] ) 
  cerror( 213 );   /* defid call */

 idp = q->tn.rval;

 if( idp < 0 ) 
  cerror( 214 );  /* tyreduce */
 p = &stab[idp];

# ifndef BUG1
 if( ddebug ){
  printf( "defid( %s (%d), ", p->sname, idp );
  tprint( q->in.type );
  printf( "(0%o), %s, (%d,%d) ), level %d\n", 
   q->in.type, scnames(class), q->fn.cdim, q->fn.csiz, blevel );
  }
# endif

 fixtype( q, class );

 type = q->in.type;
 class = fixclass( class, type );

 stp = p->stype;
 slev = p->slevel;

# ifndef BUG1
 if( ddebug ){
  printf( " modified to " );
  tprint( type );
  printf( ", %s\n", scnames(class) );
  printf( " previous def'n: " );
  tprint( stp );
  printf( ", %s, (%d,%d) ), level %d\n", scnames(p->sclass), p->dimoff, p->sizoff, slev );
  }
# endif

 if( stp == FTN && p->sclass == SNULL )goto enter;
  /* name encountered as function, not yet defined */
 if( stp == UNDEF|| stp == FARG ){
  if( blevel==1 && stp!=FARG ) switch( class ){

  default:
   if(!(class&FIELD)) 
    uerror( 63, p->sname );  /* declared argument %s is missing */
  case MOS:
  case STNAME:
  case MOU:
  case UNAME:
  case MOE:
  case ENAME:
  case TYPEDEF:
   ;
   }
  goto enter;
  }

 if( type != stp ) goto mismatch;
 /* test (and possibly adjust) dimensions */
 dsym = p->dimoff;
 ddef = q->fn.cdim;
 for( temp=type; temp&TMASK; temp = DECREF(temp) ){
  if( ISARY(temp) ){
   if( dimtab[dsym] == 0 ) dimtab[dsym] = dimtab[ddef];
   else if( dimtab[ddef]!=0 && dimtab[dsym] != dimtab[ddef] ){
    goto mismatch;
    }
   ++dsym;
   ++ddef;
   }
  }

 /* check that redeclarations are to the same structure */
 if( (temp==STRTY || temp==UNIONTY || temp==ENUMTY)
   && p->sizoff != q->fn.csiz
   && class!=STNAME && class!=UNAME && class!=ENAME ){
  goto mismatch;
  }

 scl = ( p->sclass );

# ifndef BUG1
 if( ddebug ){
  printf( " previous class: %s\n", scnames(scl) );
  }
# endif

 if( class&FIELD ){
  /* redefinition */
  if( !falloc( p, class&FLDSIZ, 1, NIL ) ) {
   /* successful allocation */
   psave( idp );
   return;
   }
  /* blew it: resume at end of switch... */
  }

 else switch( class ){

 case EXTERN:
  if (scl == STATIC || scl == USTATIC) {
   if( slev==0 ) return;
   break;
   }
  if (scl == EXTDEF || scl == EXTERN || scl == FORTRAN ||
      scl == UFORTRAN)
   return;
  break;

 case STATIC:
  if( scl==USTATIC || (scl==EXTERN && blevel==0) ){
   p->sclass = STATIC;
   if( ISFTN(type) ) curftn = idp;
   return;
   }
  break;

 case USTATIC:
  if( scl==STATIC || scl==USTATIC ) return;
  break;

 case LABEL:
  if( scl == ULABEL ){
   p->sclass = LABEL;
   deflab( p->offset );
   return;
   }
  break;

 case TYPEDEF:
  if( scl == class ) return;
  break;

 case UFORTRAN:
  if( scl == UFORTRAN || scl == FORTRAN ) return;
  break;

 case FORTRAN:
  if( scl == UFORTRAN ){
   p->sclass = FORTRAN;
   if( ISFTN(type) ) curftn = idp;
   return;
   }
  break;

 case MOU:
 case MOS:
  if( scl == class ) {
   if( oalloc( p, &strucoff ) ) break;
   if( class == MOU ) strucoff = 0;
   psave( idp );
   return;
   }
  break;

 case MOE:
  if( scl == class ){
   if( p->offset!= strucoff++ ) break;
   psave( idp );
   }
  break;

 case EXTDEF:
  if( scl == EXTERN ) {
   p->sclass = EXTDEF;
   if( ISFTN(type) ) curftn = idp;
   return;
   }
  break;

 case STNAME:
 case UNAME:
 case ENAME:
  if( scl != class ) break;
  if( dimtab[p->sizoff] == 0 ) return;  /* previous entry just a mention */
  break;

 case ULABEL:
  if( scl == LABEL || scl == ULABEL ) return;
/* case PARAM:
 case AUTO:
 case REGISTER:
  ; */    /* mismatch.. */

  }

 mismatch:
 /* allow nonunique structure/union member names */

 if( class==MOU || class==MOS || class & FIELD ){/* make a new entry */
  int * memp;
  p->sflags |= SNONUNIQ;  /* old entry is nonunique */
  /* determine if name has occurred in this structure/union */
  for( memp = &paramstk[paramno-1];
   /* while */ *memp>=0 && stab[*memp].sclass != STNAME && stab[*memp].sclass != UNAME;
   /* iterate */ --memp){    /* SS. 26.feb.87 */
   if( stab[*memp].sflags & SNONUNIQ ){
    if (p->sname != stab[*memp].sname) continue;
    uerror( 64,p->sname ); /* redeclaration of: %s*/
    break;
   }
  }
  p = mknonuniq( &idp ); /* update p and idp to new entry */
  goto enter;
  }
 if( blevel > slev && class != EXTERN && class != FORTRAN &&
  class != UFORTRAN && !( class == LABEL && slev >= 2 ) ){
  q->tn.rval = idp = hide( p );
  p = &stab[idp];
  goto enter;
  }
 uerror( 64,p->sname );   /* redeclaration of: %s */
 if( class==EXTDEF && ISFTN(type) ) curftn = idp;
 return;

 enter:  /* make a new entry */

# ifndef BUG1
 if( ddebug ) printf( "  new entry made\n" );
# endif
 if( type == UNDEF )
  uerror( 65,p->sname );   /* void type for %s */
 p->stype = type;
 p->sclass = class;
 p->slevel = blevel;
 p->offset = NOOFFSET;
 p->suse = lineno;
 if( class == STNAME || class == UNAME || class == ENAME ) {
  p->sizoff = curdim;
  dstash( 0 );  /* size */
  dstash( -1 ); /* index to members of str or union */
  dstash( ALSTRUCT );  /* alignment */
  dstash( idp );
  }
 else {
  int tyyp = BTYPE(type);
  if (tyyp == STRTY || tyyp == UNIONTY || tyyp == ENUMTY)
   p->sizoff = q->fn.csiz;
  else
   p->sizoff = tyyp;
  }

 /* copy dimensions */

 p->dimoff = q->fn.cdim;

 /* allocate offsets */
 if( class&FIELD ){
  falloc( p, class&FLDSIZ, 0, NIL );  /* new entry */
  psave( idp );
  }
 else switch( class ){

 case AUTO:
/* M.K. */
  {
   OFFSZ ltemp;
   int tsz;
   ltemp = autooff + 32;
   oalloc (p, &autooff);
   autooff = ltemp;
   p->offset = -autooff;
/* MK 13.Nov.86 */
   tsz = tsize(p->stype,p->dimoff,p->sizoff) ;
   SETOFF (tsz, ALINT);
   tsz /= 32;
/* SS. 03.07.86  added array check too!  removed check "tsz>1" (NB! doubles) */

   if( tsz > 0 && ISAGGREGATE(p->stype) ){
/* tbl */                       printf("\tli\t%d\n\talloc\n", tsz);
    if( ischarary(p->stype) )
/* tbl */    prints("\tshl2\n");
/* tbl */                       printf( "\tslw\t%d", autooff/32 );
/* tbl */   if( cdebug ) printf( "\t%%%s",p->sname );
/* tbl */   putchar ('\n');
   }
   break;
   }
 case STATIC:
 case EXTDEF:
  p->offset = getlab();
  if( ISFTN(type) ) curftn = idp;
  break;
 case ULABEL:
 case LABEL:
  p->offset = getlab();
  p->slevel = 2;
  if( class == LABEL )
   deflab( p->offset );
  break;

 case EXTERN:
 case UFORTRAN:
 case FORTRAN:
  p->offset = getlab();
  p->slevel = 0;
  break;
 case MOU:
 case MOS:
  oalloc( p, &strucoff );
  if( class == MOU ) strucoff = 0;
  psave( idp );
  break;

 case MOE:
  p->offset = strucoff++;
  psave( idp );
  break;
 case REGISTER:
  p->offset = regvar--;
  if( blevel == 1 ) p->sflags |= SSET;
  break;
  }

 /* user-supplied routine to fix up new definitions */

 FIXDEF(p);

# ifndef BUG1
 if( ddebug ) printf( "  dimoff, sizoff, offset: %d, %d, %d\n", p->dimoff, p->sizoff, p->offset );
# endif

 }

psave( i ){
 if( paramno >= PARAMSZ ){
  cerror( 215 );  /* parameter stack overflow */
  }
 paramstk[ paramno++ ] = i;
 }

int argoff;  /* host-port: was static, extern in mfile1.h */

dclargs(){
 register i, j;
 register struct symtab *p;
 register NODE *q;
 argoff = ARGINIT;
# ifndef BUG1
 if( ddebug > 2) printf("dclargs()\n");
# endif
 for( i=0; i<paramno; ++i ){
  if( (j = paramstk[i]) < 0 ) continue;
  p = &stab[j];
# ifndef BUG1
  if( ddebug > 2 ){
   printf("\t%s (%d) ",p->sname, j);
   tprint(p->stype);
   printf("\n");
   }
# endif
  if( p->stype == FARG ) {
   q = block(FREE,NIL,NIL,INT,0,INT);
   q->tn.rval = j;
   defid( q, PARAM );
   }
  FIXARG(p); /* local arg hook, eg. for sym. debugger */
  oalloc( p, &argoff );  /* always set aside space, even for register arguments */
  }
 autooff = AUTOINIT;
 ftnno = getlab();
 bfcode( paramstk, paramno );
 paramno = 0;
 }

NODE *
rstruct( idn, soru ){ /* reference to a structure or union, with no definition */
 register struct symtab *p;
 register NODE *q;

#ifndef BUG1
 if( ddebug > 2 ) printf( "rstruct( %d, %d )\n", idn,soru );
#endif
 p = &stab[idn];
 switch( p->stype ){

 case UNDEF:
 def:
  q = block( FREE, NIL, NIL, 0, 0, 0 );
  q->tn.rval = idn;
  q->in.type = (soru&INSTRUCT) ? STRTY : ( (soru&INUNION) ? UNIONTY : ENUMTY );
  defid( q, (soru&INSTRUCT) ? STNAME : ( (soru&INUNION) ? UNAME : ENAME ) );
  break;

 case STRTY:
  if( soru & INSTRUCT ) break;
  goto def;

 case UNIONTY:
  if( soru & INUNION ) break;
  goto def;

 case ENUMTY:
  if( !(soru&(INUNION|INSTRUCT)) ) break;
  goto def;

  }
 stwart = instruct;
 return( mkty( p->stype, 0, p->sizoff ) );
 }

moedef( idn ){
 register NODE *q;

 q = block( FREE, NIL, NIL, MOETY, 0, 0 );
 q->tn.rval = idn;
 if( idn>=0 ) defid( q, MOE );
 }

bstruct( idn, soru ){ /* begining of structure or union declaration */
 register NODE *q;

#ifndef BUG1
 if( ddebug > 2 ) printf( "bstruct( %d, %d )\n", idn,soru );
#endif
 psave( instruct );
 psave( curclass );
 psave( strucoff );
 strucoff = 0;
 instruct = soru;
 q = block( FREE, NIL, NIL, 0, 0, 0 );
 q->tn.rval = idn;
 if( instruct==INSTRUCT ){
  curclass = MOS;
  q->in.type = STRTY;
  if( idn >= 0 ) defid( q, STNAME );
  }
 else if( instruct == INUNION ) {
  curclass = MOU;
  q->in.type = UNIONTY;
  if( idn >= 0 ) defid( q, UNAME );
  }
 else { /* enum */
  curclass = MOE;
  q->in.type = ENUMTY;
  if( idn >= 0 ) defid( q, ENAME );
  }
 psave( idn = q->tn.rval );
 /* the "real" definition is where the members are seen */
 if ( idn >= 0 ) stab[idn].suse = lineno;
 return( paramno-4 );
 }

NODE *
dclstruct( oparam ){
 register struct symtab *p;
 register i, al, sa, j, sz, szindex;
 register TWORD temp;
 register high, low;

 /* paramstack contains:
  paramstack[ oparam ] = previous instruct
  paramstack[ oparam+1 ] = previous class
  paramstk[ oparam+2 ] = previous strucoff
  paramstk[ oparam+3 ] = structure name

  paramstk[ oparam+4, ... ]  = member stab indices
  */

 if( (i=paramstk[oparam+3]) < 0 ){
  szindex = curdim;
  dstash( 0 );  /* size */
  dstash( -1 );  /* index to member names */
  dstash( ALSTRUCT );  /* alignment */
  dstash( -lineno ); /* name of structure */
  }
 else {
  szindex = stab[i].sizoff;
  }

# ifndef BUG1
 if( ddebug ){
  printf( "dclstruct( %s ), szindex = %d\n", (i>=0)? stab[i].sname : "??", szindex );
  }
# endif
 temp = (instruct&INSTRUCT)?STRTY:((instruct&INUNION)?UNIONTY:ENUMTY);
 stwart = instruct = paramstk[ oparam ];
 curclass = paramstk[ oparam+1 ];
 dimtab[ szindex+1 ] = curdim;
 al = ALSTRUCT;

 high = low = 0;

 for( i = oparam+4;  i< paramno; ++i ){
  dstash( j=paramstk[i] );
  if( j<0 || j>= SYMTSZ ) 
   cerror( 219 );   /* gummy structure member */
  p = &stab[j];
  if( temp == ENUMTY ){
   if( p->offset < low ) low = p->offset;
   if( p->offset > high ) high = p->offset;
   p->sizoff = szindex;
   continue;
   }
  sa = talign( p->stype, p->sizoff );
  if( p->sclass & FIELD ){
   sz = p->sclass&FLDSIZ;
   }
  else {
   sz = tsize( p->stype, p->dimoff, p->sizoff );
   }
  if( sz == 0 ){
   werror( 3, p->sname ); /* illegal zero sized structure member: %s */  
   }
  if( sz > strucoff ) strucoff = sz;  /* for use with unions */
  SETOFF( al, sa );
  /* set al, the alignment, to the lcm of the alignments of the members */
  }
 dstash( -1 );  /* endmarker */
 SETOFF( strucoff, al );

 if( temp == ENUMTY ){
  register TWORD ty;

# ifdef ENUMSIZE
  ty = ENUMSIZE(high,low);
# else
  if( (char)high == high && (char)low == low ) ty = ctype( CHAR );
  else if( (short)high == high && (short)low == low ) ty = ctype( SHORT );
  else ty = ctype(INT);
#endif
  strucoff = tsize( ty, 0, (int)ty );
  dimtab[ szindex+2 ] = al = talign( ty, (int)ty );
  }

 if( strucoff == 0 ) 
  uerror( 66 );  /* zero sized structure */
 dimtab[ szindex ] = strucoff;
 dimtab[ szindex+2 ] = al;
 dimtab[ szindex+3 ] = paramstk[ oparam+3 ];  /* name index */

 FIXSTRUCT( szindex, oparam ); /* local hook, eg. for sym debugger */
# ifndef BUG1
 if( ddebug>1 ){
  printf( "\tdimtab[%d,%d,%d] = %d,%d,%d\n", szindex,szindex+1,szindex+2,
    dimtab[szindex],dimtab[szindex+1],dimtab[szindex+2] );
  for( i = dimtab[szindex+1]; dimtab[i] >= 0; ++i ){

   printf( "\tmember %s(%d)\n", stab[dimtab[i]].sname, dimtab[i] );
   }
  }
# endif

 strucoff = paramstk[ oparam+2 ];
 paramno = oparam;

 return( mkty( temp, 0, szindex ) );
 }


ftnarg( idn ){
 switch( stab[idn].stype ){

 case UNDEF:
  /* this parameter, entered at scan */
  break;
 case FARG:
  uerror( 67, stab[idn].sname );  /* redeclaration of formal parameter, %s */
  /* fall thru */
 case FTN:
  /* the name of this function matches parm */
  /* fall thru */
 default:
  idn = hide( &stab[idn]);
  break;
 case TNULL:
  /* unused entry, fill it */
  ;
  }
 stab[idn].stype = FARG;
 stab[idn].sclass = PARAM;
 psave( idn );
 }

talign( ty, s) register unsigned ty; register s; {
 /* compute the alignment of an object with type ty, sizeoff index s */

 register i, tyyp;
 if( s<0 && ty!=INT && ty!=CHAR && ty!=SHORT && ty!=UNSIGNED && ty!=UCHAR && ty!=USHORT 
#ifdef LONGFIELDS
  && ty!=LONG && ty!=ULONG
#endif
     ){
  return( fldal( ty ) );
  }

 for( i=0; i<=(SZINT-BTSHIFT-1); i+=TSHIFT ){
  tyyp = (ty>>i)&TMASK;
  if (tyyp == FTN)
   cerror( 220 );  /* compiler takes alignment of func */
  if (tyyp == PTR)
   return( ALPOINT );
/* MK continue and break here are equivalent: just continue FOR loop 
  case ARY:
   continue;
  case 0:
   break;
   }
*/
  }

 tyyp = BTYPE(ty);

 if (tyyp == UNIONTY || tyyp == ENUMTY || tyyp == STRTY)
  return( (unsigned int) dimtab[ s+2 ] );
 else return (ALINT);
/* MK  all alignments on Kronos on word boundary 
 case CHAR:
 case UCHAR:
  return( ALCHAR );
 case FLOAT:
  return( ALFLOAT );
 case DOUBLE:
  return( ALDOUBLE );
 case LONG:
 case ULONG:
  return( ALLONG );
 case SHORT:
 case USHORT:
  return( ALSHORT );
 default:
  return( ALINT );
  }
*/
 }

OFFSZ
tsize( ty, d, s )  TWORD ty; {
 /* compute the size associated with type ty,
     dimoff d, and sizoff s */
 /* BETTER NOT BE CALLED WHEN t, d, and s REFER TO A BIT FIELD... */

 int i, tyyp;
 OFFSZ mult,tmp;
 int size, ar = 0;

 mult = 1;

 for( i=0; i<=(SZINT-BTSHIFT-1); i+=TSHIFT ){
  tyyp = (ty>>i)&TMASK;
  if(tyyp == FTN)
   cerror( 221 );  /* compiler takes size of function */
  if(tyyp == PTR)
   if( (tmp=SZPOINT * mult) >= offsz )  /* SS. 26.feb.87 */
    cerror( 295 );  /* array too large */
   else
    return( tmp );
  if(tyyp == ARY){
   ar = 1;
   mult *= (unsigned int) dimtab[ d++ ];
  }
/* MK continue FOR loop, whatever it was
   continue;
  case 0:
   break;

   }
*/
  }

 if( dimtab[s]==0 ) {
  uerror( 68 );   /* unknown size */
  return( SZINT );
  }
/* SS. 16.nov. manifest contains now macro for char type testing */
 size = (ar && ISCHAR(s) ) ? 8 : dimtab[s];
 if( (tmp=(unsigned int) size * mult) >= offsz ) /* SS. 26.feb.87 */
  cerror( 295 ); /* array too large */
 else
  return( tmp );
/* MK 13.Nov.86  was :
 return( (unsigned int) dimtab[ s ] * mult );
*/
 }

OFFSZ inoff; /* offset of external element being initialized */


inforce( n ) OFFSZ n; {  /* force inoff to have the value n */
 /* inoff is updated to have the value n */
 OFFSZ wb;
 register rest;
 /* rest is used to do a lot of conversion to ints... */

#ifndef BUG1
 if (idebug)
  printf("<<< inforce >>> n = %d inoff = %d\n", n, inoff);
#endif
 if( inoff == n ) return;
 if( inoff > n ) {
  cerror( 222 );  /* initialization alignment error */
  }

 wb = inoff;
 SETOFF( wb, SZINT );

 /* wb now has the next higher word boundary */

 if( wb >= n ){ /* in the same word */
  rest = n - inoff;
  vfdzero( rest );
  return;
  }

 /* otherwise, extend inoff to be word aligned */

 rest = wb - inoff;
 vfdzero( rest );

 /* now, skip full words until near to n */

 rest = (n-inoff)/SZINT;
/* zecode( rest ); */
 if (rest > 0) inoff += rest * SZINT;

 /* now, the remainder of the last word */

 rest = n-inoff;
 vfdzero( rest );
 if( inoff != n ) 
  cerror( 223 );  /* inoff error */

 }

vfdalign( n ){ /* make inoff have the offset the next alignment of n */
 OFFSZ m;

 m = inoff;
 SETOFF( m, n );
 inforce( m );
 }


int ibseen = 0;  /* the number of } constructions which have been filled */

int iclass;  /* storage class of thing being initialized */


beginit(curid){
 /* beginning of initilization; set location ctr and set type */
 register struct symtab *p;

# ifndef BUG1
 if( idebug >= 3 ) printf( "beginit(), curid = %d\n", curid );

 if (idebug) printf("name is %s\n",stab[curid].sname);
#endif
 p = &stab[curid];

 iclass = p->sclass;
 if( curclass == EXTERN || curclass == FORTRAN ) iclass = EXTERN;
 if (iclass == UNAME || iclass == EXTERN) return;
 if (iclass == EXTDEF || iclass == STATIC) 
  defnam( p );

 inoff = 0;
 ibseen = 0;

 pstk = 0;

 if( iclass != AUTO && iclass != REGISTER ) {
/* MK 23.Apr.87 */
  if (asflag)
   outline();
/* tbl */  D_BEGINIT;/* SS. 29.dec.86 added to start init. for aggregates*/
  }

 instk( curid, p->stype, p->dimoff, p->sizoff, inoff );

 }


instk( id, t, d, s, off ) OFFSZ off; TWORD t; {
 /* make a new entry on the parameter stack to initialize id */

 register struct symtab *p;

 for(;;){
# ifndef BUG1
  if( idebug ) printf( "instk((%d, %o,%d,%d, %d)\n", id, t, d, s, off );
  if (idebug) printf("inoff=%d\n", inoff);
# endif

  /* save information on the stack */

  if( !pstk ) pstk = instack;
  else ++pstk;
#ifndef BUG1
  if (idebug) printf("pstk=%o\n", pstk);
#endif

  pstk->in_fl = 0; /* { flag */
  pstk->in_id =  id ;
  pstk->in_t =  t ;
  pstk->in_d =  d ;
  pstk->in_s =  s ;
  pstk->in_n = 0;  /* number seen */
  pstk->in_x =  t==STRTY ?dimtab[s+1] : 0 ;
  pstk->in_off =  off;   /* offset at the beginning of this element */
  /* if t is an array, DECREF(t) can't be a field */
  /* INS_sz has size of array elements, and -size for fields */
  if( ISARY(t) ){
/* MK 28.04.86 allocate space for arrays of undefined size using the 
   .equ construction; save label number in in_x field (???)  */
   if (pstk == instack && dimtab[d] == 0) {
    int lab;
    pstk->in_x = lab = getlab();
                  D_ENDINIT; /* SS. 18.sep.87   */
                  D_BEGALLOC; /* SS. 18.sep.87   */
/* MK 03.Jan.86 */
/* MK. 18.Sep.87 separate allocs for aggregates
         if(globsize < 256)
   tbl        printf ("\tlla\t%d\n", globsize);
   tbl       else printf ("\tlla\t255\n\tli\t%d\n\tadd\n", globsize-255);
*/

/* tbl */                        printf("\tli\tM%d\n\talloc\n", lab); 

/* NB!  SS. 16.nov.86 char array type checking changed */

    if( ischarary(t) )
/* tbl */    prints("\tshl2\n");
/* tbl */                       prints("\tsw\t");
/* tbl */                       globalname(&stab[id]);
                  D_ENDALLOC; /* SS. 18.sep.87   */
    D_BEGINIT;
   }
   pstk->in_sz = (t == ARY + CHAR || t == ARY + UCHAR)? 8: 
     tsize( DECREF(t), d+1, s );
   }
  else if( stab[id].sclass & FIELD ){
   pstk->in_sz = - ( stab[id].sclass & FLDSIZ );
   }
  else {
   pstk->in_sz = 0;
   }

  if( (iclass==AUTO || iclass == REGISTER ) &&
   (ISARY(t) || t==STRTY) ) 
   uerror( 69 ); /* no automatic aggregate initialization*/

/* MK 22.04.86 */
#ifndef BUG1
  if (idebug)
 printf("%d %d %d %d %d %d %d\n",pstk->in_id, pstk->in_t, pstk->in_d,
  pstk->in_s,pstk->in_x,pstk->in_off,pstk->in_sz);
  if (idebug) printf("inoff=%d\n", inoff);
#endif
  /* now, if this is not a scalar, put on another element */

  if( ISARY(t) ){
   t = DECREF(t);
   ++d;
   continue;
   }
  else if( t == STRTY ){
   id = dimtab[pstk->in_x];
   p = &stab[id];
   if( p->sclass != MOS && !(p->sclass&FIELD) ) 
    cerror( 224 );  /* insane structure member list */
   t = p->stype;
   d = p->dimoff;
   s = p->sizoff;
   off += p->offset;
   continue;
   }
  else return;
  }
} /* instk */


NODE *
getstr(){ /* decide if the string is external or an initializer, and get the contents accordingly */

/* register cond; */
 register l;
 register NODE *p;

/* NB! */ /* SS. 11.07.86  following correction produces illegal code! */
   /* SS. 10.07.86  added the following statement and */ 
   /* calls to ilbrace() and irbrace()                */
/*
  cond = ( (iclass==EXTDEF || iclass==STATIC)
    && (pstk->in_t==CHAR || pstk->in_t==UCHAR)
    && pstk != instack && ISARY( pstk[-1].in_t ) );
*/

      /* make a label, and get the contents and stash them away */
  if( iclass != SNULL ){ /* initializing */
   /* fill out previous word, to permit pointer */
   vfdalign( ALPOINT );
  }
/* MK 10.02.87 deflab( l = getlab() ); */
  l = getlab();
/* tbl */ printf("M%d", l);
/* NB!   if( cond ) ilbrace(); ** simulate { */
  lxstr(0);   /* get the contents */
/* NB!   if( cond ) irbrace(); ** simulate } */
  p = buildtree( STRING, NIL, NIL );
  p->tn.rval = -l;
  return(p);
 }


endinit(){
 register TWORD t;
 register d, s, n, d1;

# ifndef BUG1
 if( idebug ) printf( "endinit(), inoff = %d\n", inoff );
# endif
 if (iclass == EXTERN || iclass == AUTO || iclass== REGISTER)
  return;

 if (asflag) /* MK 23.Apr.87 */
  outline();
 D_ENDINIT;   

 pstk = instack;
 t = pstk->in_t;
 d = pstk->in_d;
 s = pstk->in_s;
 n = pstk->in_n;

#ifndef BUG1
 if (idebug)
  printf("t= %d, d= %d, s= %d, n= %d\n", t, d, s, n);
#endif

 if( ISARY(t) ){
  d1 = dimtab[d];
#ifndef BUG1
  if (idebug) {
   printf("going to vfdalign,pstk->in_sz= %d\n", pstk->in_sz);
   printf("d1 is %d\n", d1);
   }
#endif

/* MK 1.Dec.86 this caused 'too many initializers' error for, e.g.,
 char a[3]="ab"; inoff got too big  */

  vfdalign( pstk->in_sz ); 
   /* fill out part of the last element, if needed */
  n = inoff/pstk->in_sz; /* real no. of initializers */

#ifndef BUG1
  if (idebug) printf("new n is %d\n", n);
#endif

/* now the size of array is known from initialization */
  if (d1 == 0) {
   int size;
/* MK 21.09.87 Back to old version */
/* tbl */  printf("M%d", pstk->in_x);
/* tbl */  D_EQU;
   size = n * pstk->in_sz;
   SETOFF (size, ALINT);
/* tbl */  printf("%d\n", (size / SZCHAR));
   globsize += (size / SZCHAR);
   }
  if( d1 >= n )
   /* once again, t is an array, so no fields */
   n = d1;
  if( d1!=0 && d1!=n ) 
   uerror( 70 );  /* too many initializers */
  if( n==0 ) werror( 4 );   /* empty array declaration */
  dimtab[d] = n;
  }

 else if( n > 1 ) uerror( 71 );  /* bad scalar initialization */

 paramno = 0;
 vfdalign( AL_INIT );
 inoff = 0;
 iclass = SNULL;

 }

/* 
 * D O I N I T
 */

doinit( p ) register NODE *p; {

 /* take care of generating a value for the initializer p */
 /* inoff has the current offset (last bit written)
  in the current word being generated */

 NODE  * np; /* SS. 19.nov.86 see NB! below */
 NODE  * left; /* MK. 19.Feb.87 to simplify access */
 register sz, d, s;
 register TWORD t;
 int flag = 0;
 int aggr;  /* MK. 19.Feb.87 pstk != instack */
 struct  instk * pk;   /* MK 19.Feb.87 pstk - 1 much used */
 extern int eprint();

 /* note: size of an individual initializer is assumed to fit 
    into an integer */

#ifndef BUG1
 if (idebug) 
  printf("doinit() iclass=%d inoff=%d\n",iclass, inoff);
#endif

 if( iclass < 0 ) goto leave;

 if( iclass == EXTERN || iclass == UNAME ){
  uerror( 72 );  /* cannot initialize extern or union */
  iclass = -1;
  goto leave;
  }

 if( iclass == AUTO || iclass == REGISTER ){
  /* do the initialization and get out, without regard 
      for filing out the variable with zeros, etc. */
  bccode();
  idname = pstk->in_id;
  p = buildtree( ASSIGN, buildtree( NAME, NIL, NIL ), p );
  ecomp(p);
  return;
  }

 if( ibseen ){
  uerror( 73 );  /* } expected */
  goto leave;
  }

# ifndef BUG1
 if( idebug )  {
  printf( "doinit(%o) Globals\n", p );
  printf("pstk=%o in_t=%d in_off=%d id=%d\n", pstk,pstk->in_t,
   pstk->in_off, pstk->in_id);
  }
#endif

 /* Get some constants used in the future */

 t = pstk->in_t;  /* type required */
 d = pstk->in_d;
 s = pstk->in_s;
 if( pstk->in_sz < 0 )  /* bit field */
  sz = -pstk->in_sz;
 else    sz = tsize( t, d, s );
 
 aggr = pstk != instack; 
 pk = pstk - 1;

 inforce( pstk->in_off );

 /* SS. 19.nov.86 the following 5 lines attempt to replace the next one
    the reason was to avoid the warning msg from chkpun(), when 
    initializing  char ary */
 
 np = block( NAME, NIL,NIL, t, d, s );
 if (p->in.op==NAME && p->tn.rval < 0 && p->in.type==CHAR + ARY
 && aggr && t == CHAR && pk->in_t==CHAR + ARY)
  np->in.type = PTR + CHAR; 
 p = buildtree( ASSIGN, np, p );

/* p = buildtree( ASSIGN, block( NAME, NIL,NIL, t, d, s ), p );  */

 p->in.left->in.op = FREE;
 p->in.left = p->in.right;
 p->in.right = NIL;

#ifndef BUG1
 if( idebug ){
  printf( "before optim():\n" );
  fwalk( p->in.left, eprint, 0 );
  }
#endif

 p->in.left = optim( p->in.left );
 if( p->in.left->in.op == UNARY AND ){
  p->in.left->in.op = FREE;
  p->in.left = p->in.left->in.left;
  }

 p->in.op = FORCE;
 left = p->in.left; /* MK 19.Feb.87 to simplify addressing */

#ifndef BUG1
 if( idebug ){
  printf( "after making FORCE:\n" );
  fwalk( p, eprint, 0 );
 }
#endif

 if( sz < SZINT ){ /* special case: bit fields, etc. */
  if( left->in.op != ICON ) 
   uerror( 74 );  /* illegal initialization */
  else incode( left, sz );
 }
 else if (left->in.op == ICON && left->tn.lval == 0 && 
      left->tn.rval == NONAME) { 
  /* special case : initialization with 0; do nothing */
  if (t == CHAR && pk->in_t == CHAR+ARY)
   inoff += 8;
  else inoff += 32;
 } 
 else {

/* M A I N   part of initialization */

 struct symtab * pp = & stab[instack[0].in_id];
 OFFSZ ofset;

 if (aggr) {

   /*  1.  Char array initialization as separate bytes (no string) with
    SXB; is done apart of everything else */

     if (t == CHAR && pk->in_t == CHAR+ARY && 
    !(left->in.op == ICON && left->tn.rval <0)){
/* tbl */ prints("\tli\t0\n\tlw\t");
  globalname(pp);
  if (!ischarary(pp->stype)) 
/* tbl */  prints("\tshl2\n");
  if( ofset = inoff/8 ){
   if(ofset < 256)
/* tbl */   printf("\tlsa\t%d\n", ofset);
   else
/* tbl */   printf("\tli\t%d\n\tadd\n", ofset);
  }
  cinit(optim(p), 8);
/* tbl */ prints("\tsxb\n");
  gotscal();
  tfree(p);
  return;
     } 

   /* All other aggregates : form address on E-stack */

/* tbl */ prints("\tlw\t");
      globalname(pp);
   if ((ofset = inoff/32) > 255) {
/* tbl */        printf("\tli\t%d\n\tadd\n", ofset);
  ofset = 0;
  /* MK 19.Feb.87 ofset != 0 means that ssw sth must be generated */
    }
 }

   /* 2.  Initialization with STRING - special case */

 if (left->in.op == ICON && left->tn.rval < 0) {

     switch (t){

   /*    2.a   char * sth = " ... "   */

  case PTR+CHAR:
/* tbl */               printf("\tlsta\tM%d\n",-left->tn.rval);
/* tbl */  prints("\tshl2\n");
   if (!aggr) {
/* tbl */   prints("\tsw\t");
    globalname(pp);
   }
   else 
/* tbl */   printf( "\tssw\t%d\n", ofset );
   inoff += SZPOINT;
   break;
 
   /*    2.b   error  */
  
  default: 
   uerror( 75 );  /* illegal initialization with string */
   if (aggr) 
/* tbl */   prints("\tdrop\n"); /* drop address */
   break;

   /*    2.c   char sth [] = " ... "    */

  case CHAR:
      if (!aggr) 
   uerror( 75 );  /* illegal initialization with string */
      else {
   int dd, size;

   ilbrace();
   flag = 1;
   size = dimtab[curdim] * 8;
   SETOFF (size, ALINT);
   if (ischarary(pp->stype))
/* tbl */   prints("\tshr2\n");
   if (ofset)
/* tbl */   printf( "\tlsa\t%d\n", ofset );
/* tbl */               printf("\tlsta\tM%d\n",-left->tn.rval);
   if (size == 32) 
/* tbl */   prints("\tlsw\t0\n\tssw\t0\n");
   else 
/* tbl */                printf("\tli\t%d\n\tmove\n",size/32);
   if ((dd = dimtab[pk->in_d])==0)
    dd = dimtab[curdim];
   else if (dd < dimtab[curdim])
    uerror( 76 );  /* String too long */
   size = dd * 8;
/*   SETOFF (size, ALINT); */
/* MK 09.01.87 */
   inoff += size; 
   pk->in_n = dd;
   irbrace();
   }
      break;
  }
     } /* 2. initialization with STRING */

   /* 3.  All other things are passed to Pass 2 */

 else {
  cinit( optim(p), sz );
  if (aggr) 
/* tbl */  printf("\tssw\t%d\n", ofset);
  else {
/* tbl */  prints("\tsw\t");
   globalname(pp);
  }
 }
  }

 if (!flag) gotscal();

 leave:
 tfree(p);

} /* doinit */


gotscal(){
 register t, ix;
 register n, id;
 struct symtab *p;
 OFFSZ temp;

#ifndef BUG1
 if (idebug) printf("gotscal \n");
#endif
 for( ; pstk > instack; ) {

  if( pstk->in_fl ) ++ibseen;

  --pstk;
  
  t = pstk->in_t;
#ifndef BUG1
  if (idebug) printf("type is %d\n", t);
#endif

  if( t == STRTY ){
   ix = ++pstk->in_x;
#ifndef BUG1
   if (idebug) printf("structure %x\n", ix);
#endif
   if( (id=dimtab[ix]) < 0 ) continue;

   /* otherwise, put next element on the stack */

   p = &stab[id];
   instk( id, p->stype, p->dimoff, p->sizoff, p->offset+pstk->in_off );
   return;
   }
  else if( ISARY(t) ){
   n = ++pstk->in_n;
#ifndef BUG1
   if (idebug) printf("array %d\n", n);
#endif
   if( n >= dimtab[pstk->in_d] && pstk > instack ) continue;

   /* put the new element onto the stack */

   temp = pstk->in_sz;
   instk( pstk->in_id, (TWORD)DECREF(pstk->in_t), pstk->in_d+1, pstk->in_s,
    pstk->in_off+n*temp );
   return;
   }

  }

 }

ilbrace(){ /* process an initializer's left brace */
 register t;
 struct instk *temp;

 temp = pstk;

 for( ; pstk > instack; --pstk ){

  t = pstk->in_t;
#ifndef BUG1
  if (idebug)
   printf("ILBRACE: t=%o %d\n", t, t);
#endif
  if( t!= STRTY && !ISARY(t) ) continue; /* not an aggregate */
  if( pstk->in_fl ){ /* already associated with a { */
   if( pstk->in_n ) 
    uerror( 77 );  /* illegal { */
   continue;
   }

  /* we have one ... */
  pstk->in_fl = 1;
#ifndef BUG1
  if (idebug)
   printf("ILBRACE: found at pstk=%o\n", pstk);
#endif
  break;
  }

 /* cannot find one */
 /* ignore such right braces */

 pstk = temp;
 }

irbrace(){
 /* called when a '}' is seen */

# ifndef BUG1
 if( idebug ) printf( "irbrace(): paramno = %d on entry, ibseen=%d\n", 
   paramno, ibseen );
# endif

 if( ibseen ) {
  --ibseen;
  return;
  }

 for( ; pstk > instack; --pstk ){
  if( !pstk->in_fl ) continue;

  /* we have one now */

  pstk->in_fl = 0;  /* cancel { */
  gotscal();  /* take it away... */
  return;
  }

 /* these right braces match ignored left braces: throw out */

 }

upoff( size, alignment, poff ) register alignment, *poff; {
 /* update the offset pointed to by poff; return the
 /* offset of a value of size `size', alignment `alignment',
 /* given that off is increasing */

 register long off;

 off = *poff;
 SETOFF( off, alignment );
 if( (offsz-off) <  size ){
  if( instruct!=INSTRUCT )
   cerror( 225 );  /* too many local variables */ 
  else 
   cerror( 226 );  /* Structure too large */ 
  }
 *poff = off+size;
 return( off );
 }

oalloc( p, poff ) register struct symtab *p; register *poff; {
 /* allocate p with offset *poff, and update *poff */
 register al, off, tsz;
 int noff;

 al = talign( p->stype, p->sizoff );
 noff = off = *poff;
 tsz = tsize( p->stype, p->dimoff, p->sizoff );
 /* MK 22.04.86  SS. 13.10.86  BACKAUTO undefined in macdefs */
#ifdef BACKAUTO
 if( p->sclass == AUTO ){
  if( (offsz-off) < tsz ) 
   cerror( 225 );  /* too many local variables */ 
  noff = off + tsz;
  SETOFF( noff, al );
  off = -noff;
  }
 else
#endif
  if( p->sclass == PARAM && ( tsz < SZINT ) ){
   off = upoff( SZINT, ALINT, &noff );
# ifndef RTOLBYTES
   off = noff - tsz;
#endif
   }
  else
  {
  off = upoff( tsz, al, &noff );
  }

 if( p->sclass != REGISTER ){ /* in case we are allocating stack space for register arguments */
  if( p->offset == NOOFFSET ) p->offset = off;
  else if( off != p->offset ) return(1);
  }

 *poff = noff;
 return(0);
 }

falloc( p, w, new, pty )  register struct symtab *p; NODE *pty; {
 /* allocate a field of width w */
 /* new is 0 if new entry, 1 if redefinition, -1 if alignment */

 register al,sz,type;

#ifndef BUG1
 if( ddebug > 2 ) printf( "falloc( w=%d, new=%d, pty=%o )\n", w,new,pty);
#endif
 type = (new<0)? pty->in.type : p->stype;

 /* this must be fixed to use the current type in alignments */
 switch( new<0?pty->in.type:p->stype ){

 case ENUMTY:
  {
   int s;
   s = new<0 ? pty->fn.csiz : p->sizoff;
   al = dimtab[s+2];
   sz = dimtab[s];
   break;
   }

 case CHAR:
 case UCHAR:
  al = ALCHAR;
  sz = SZCHAR;
  break;

 case SHORT:
 case USHORT:
  al = ALSHORT;
  sz = SZSHORT;
  break;

 case INT:
 case UNSIGNED:
  al = ALINT;
  sz = SZINT;
  break;
#ifdef LONGFIELDS

 case LONG:
 case ULONG:
  al = ALLONG;
  sz = SZLONG;
  break;
#endif

 default:
  if( new < 0 ) {
   uerror( 78 );  /* illegal field type */
   al = ALINT;
   }
  else {
   al = fldal( p->stype );
   sz =SZINT;
   }
  }

 if( w > sz ) {
  uerror( 79 );  /* field too big */ 
  w = sz;
  }

 if( w == 0 ){ /* align only */
  SETOFF( strucoff, al );
  if( new >= 0 ) 
   uerror( 80 );  /* zero size field */
  return(0);
  }

 if( strucoff%al + w > sz ) SETOFF( strucoff, al );
 if( new < 0 ) {
  if( (offsz-strucoff) < w )
   cerror( 227 );  /* structure too large (falloc) */ 
  strucoff += w;  /* we know it will fit */
  return(0);
  }

 /* establish the field */

 if( new == 1 ) { /* previous definition */
  if( p->offset != strucoff || p->sclass != (FIELD|w) ) return(1);
  }
 p->offset = strucoff;
 if( (offsz-strucoff) < w )
  cerror( 227 );  /* structure too large (falloc) */  
 strucoff += w;
 p->stype = type;
 fldty( p );
 return(0);
 }

nidcl( p ) NODE *p; { /* handle unitialized declarations */
 /* assumed to be not functions */
 register class;

 /* compute class */
 if( (class=curclass) == SNULL ){
  if( blevel > 1 ) class = AUTO;
  else if( blevel != 0 || instruct ) 
   cerror( 228 );  /* nidcl error */ 
  else  /* blevel = 0 */
   class = EXTDEF;
  }

 defid( p, class );

 if( class==EXTDEF || class==STATIC ){

  /* simulate initialization by 0 */

/* M.K.  no initialization, define name as is 

  beginit(p->tn.rval);
  endinit();
 */
  defnam (&stab[ p->tn.rval]);
  }
 }

TWORD
types( t1, t2, t3 ) TWORD t1, t2, t3; {
 /* return a basic type from basic types t1, t2, and t3 */

 TWORD t[3], noun, adj, unsg;
 register i;

 t[0] = t1;
 t[1] = t2;
 t[2] = t3;

 unsg = INT;  /* INT or UNSIGNED */
 noun = UNDEF;  /* INT, CHAR, or FLOAT */
 adj = INT;  /* INT, LONG, or SHORT */

 for( i=0; i<3; ++i ){
  switch( t[i] ){

  default:
  bad:
   uerror( 81 );   /* illegal type combination */
   return( INT );

  case UNDEF:
   continue;

  case UNSIGNED:
   if( unsg != INT ) goto bad;
   unsg = UNSIGNED;
   continue;

  case LONG:
  case SHORT:
   if( adj != INT ) goto bad;
   adj = t[i];
   continue;

  case INT:
  case CHAR:
  case FLOAT:
   if( noun != UNDEF ) goto bad;
   noun = t[i];
   continue;
   }
  }

 /* now, construct final type */
 if( noun == UNDEF ) noun = INT;
 else if( noun == FLOAT ){
  if( unsg != INT || adj == SHORT ) goto bad;
  return( adj==LONG ? DOUBLE : FLOAT );
  }
 else if( noun == CHAR && adj != INT ) goto bad;

 /* now, noun is INT or CHAR */
 if( adj != INT ) noun = adj;
 if( unsg == UNSIGNED ) return( noun + (UNSIGNED-INT) );
 else return( noun );
 }

NODE *
tymerge( typ, idp ) NODE *typ, *idp; {
 /* merge type typ with identifier idp  */

 register unsigned t;
 register i;
 extern int eprint();

 if( typ->in.op != TYPE ) 
  cerror( 229 );  /* tymerge: arg 1 */
 if(idp == NIL ) return( NIL );

# ifndef BUG1
 if( ddebug > 2 ){
  printf( "tymerge( %o, %o, type=0%o )\n", typ,idp,typ->in.type); 
  fwalk( idp, eprint, 0 );
 }
# endif

 idp->in.type = typ->in.type;
 idp->fn.cdim = curdim;
 tyreduce( idp );
 idp->fn.csiz = typ->fn.csiz;

 for( t=typ->in.type, i=typ->fn.cdim; t&TMASK; t = DECREF(t) ){
  if( ISARY(t) ) dstash( dimtab[i++] );
  }

 /* now idp is a single node: fix up type */

 idp->in.type = ctype( idp->in.type );

 if( (t = BTYPE(idp->in.type)) != STRTY && t != UNIONTY && t != ENUMTY ){
  idp->fn.csiz = t;  /* in case ctype has rewritten things */
  }

 return( idp );
 }


tyreduce( p ) register NODE *p; {

 /* build a type, and stash away dimensions, from a parse tree of the declaration */
 /* the type is build top down, the dimensions bottom up */
 register o, temp;
 register unsigned t;

 o = p->in.op;

#ifndef BUG1
 if( ddebug > 2 ) printf( "tyreduce( %o, %s )\n", p, opst[o] );
#endif
 p->in.op = FREE;

 if( o == NAME ) return;

 t = INCREF( p->in.type );
 if( o == UNARY CALL ) t += (FTN-PTR);
 else if( o == LB ){
  t += (ARY-PTR);
  temp = p->in.right->tn.lval;
  p->in.right->in.op = FREE;
  if( ( temp == 0 ) & ( p->in.left->tn.op == LB ) )
   uerror( 82 );   /* Null dimension */
  }

 p->in.left->in.type = t;
 tyreduce( p->in.left );

 if( o == LB ) dstash( temp );

 p->tn.rval = p->in.left->tn.rval;
 p->in.type = p->in.left->in.type;

 }

fixtype( p, class ) register NODE *p; {
 register unsigned t, type;
 register mod1, mod2;
 /* fix up the types, and check for legality */

 if( (type = p->in.type) == UNDEF ) return;
/* SS. 13.Oct.87 fixed bug: we can't convert "typedef char SYMB",because the
   "SYMB c[10]" after that has type "array of int"  */
 if( class != TYPEDEF ){
/* SS. 22.Sep.87 fixed bug in converting type "FTN returning CHAR" 
    was only ischar macro testing  */
  if( ISCHAR(type) ) {
   p->in.type = INT;
   return;
  }
  else
   if( type==FTN + CHAR || type==FTN + UCHAR ) {
    p->in.type = FTN + INT;
    return;
   }
 }
 if( mod2 = (type&TMASK) ){
  t = DECREF(type);
  while( mod1=mod2, mod2 = (t&TMASK) ){
   if( mod1 == ARY && mod2 == FTN ){
    uerror( 83 ); /* array of functions is illegal*/
    type = 0;
    }
   else if( mod1 == FTN && ( mod2 == ARY || mod2 == FTN ) ){
    uerror( 84 ); /* function returns illegal type*/
    type = 0;
    }
   t = DECREF(t);
   }
  }

 /* detect function arguments, watching out for structure declarations */
 /* for example, beware of f(x) struct [ int a[10]; } *x; { ... } */
 /* the danger is that "a" will be converted to a pointer */

 if( class==SNULL && blevel==1 && !(instruct&(INSTRUCT|INUNION)) ) class = PARAM;
 if( class == PARAM || ( class==REGISTER && blevel==1 ) ){
  if( type == FLOAT ) type = DOUBLE;
  else if( ISARY(type) ){
   ++p->fn.cdim;
   type += (PTR-ARY);
   }
  else if( ISFTN(type) ){
   werror( 5 );  /* a function is declared as an argument*/
   type = INCREF(type);
   }

  }

 if( instruct && ISFTN(type) ){
  uerror( 85 );   /* function illegal in structure or union */
  type = INCREF(type);
  }
 p->in.type = type;
 }

uclass( class ) register class; {
 /* give undefined version of class */
 if( class == SNULL ) return( EXTERN );
 else if( class == STATIC ) return( USTATIC );
 else if( class == FORTRAN ) return( UFORTRAN );
 else return( class );
 }

fixclass( class, type ) TWORD type; {

 TWORD t;
 /* first, fix null class */

 if( class == SNULL ){
  if( instruct&INSTRUCT ) class = MOS;
  else if( instruct&INUNION ) class = MOU;
  else if( blevel == 0 ) class = EXTDEF;
  else if( blevel == 1 ) class = PARAM;
  else class = AUTO;

  }

 /* now, do general checking */

 if( ISFTN( type ) ){
  if (class == AUTO) class = EXTERN;
  if (!(class == EXTERN || class == EXTDEF || class == TYPEDEF
     || class == FORTRAN || class == UFORTRAN || class == STATIC
     || class == USTATIC))
   uerror( 86 );   /* function has illegal storage class */
  }

 if( class&FIELD ){
  if( !(instruct&INSTRUCT) ) 
   uerror( 87 );  /* illegal use of field */ 
  return( class );
  }

 switch( class ){

 case MOU:
  if( !(instruct&INUNION) ) 
   uerror( 88 );  /* illegal class */
  return( class );

 case MOS:
  if( !(instruct&INSTRUCT) ) 
   uerror( 88 );  /* illegal class */
  return( class );

 case MOE:
  if( instruct & (INSTRUCT|INUNION) ) 
   uerror( 88 );  /* illegal class */
  return( class );

 case REGISTER:
  if( blevel == 0 ) 
   uerror( 89 );  /* illegal register declaration */
/*
  else if( regvar >= MINRVAR && cisreg( type ) ) return( class );
*/
  if( blevel == 1 ) return( PARAM );
  else return( AUTO );

 case AUTO:
 case LABEL:
 case ULABEL:
  if( blevel < 2 ) 
   uerror( 88 );  /* illegal class */
  return( class );

 case PARAM:
  if( blevel != 1 ) 
   uerror( 88 );  /* illegal class */
  return( class );

 case UFORTRAN:
 case FORTRAN:
   /* a condition which can regulate the FORTRAN usage */
# ifndef excII
  /* SS. 05.mar.87 NOFORTRAN replaced with the following block */
  { extern int pflag; 
  if( pflag )  
   werror( 28 ); /* fortran/modula keyword nonportable */
  }
# endif
       /* SS. 09.sep.86  added ptr to ftn type checking */
  if( !ISFTN(type) && !( ISPTR(type) && (t=DECREF(type),ISFTN(t)) ) ) 
   uerror( 90 );  /* modula declaration must apply to function */
  else {
   if( ISPTR(type) )  type = DECREF(type);
   type = DECREF(type);
   if( ISFTN(type) ){    /* SS. 05.sep.86  removed array */
           /* and pointer type checking    */
    uerror( 91 );  /* modula func has wrong type */
    }
   }
 case STNAME:
 case UNAME:
 case ENAME:
 case EXTERN:
 case STATIC:
 case EXTDEF:
 case TYPEDEF:
 case USTATIC:
  return( class );

 default:
  cerror( 230, class );  /* illegal class: %d */ 
  /* NOTREACHED */

  }
 }

struct symtab *
mknonuniq(idindex) int *idindex; {/* locate a symbol table entry for */
 /* an occurrence of a nonunique structure member name or field */
 register i;
 register struct symtab * sp;

 sp = & stab[ i= *idindex ]; /* position search at old entry */
 while( sp->stype != TNULL ){ /* locate unused entry */
  if( ++i >= SYMTSZ ){/* wrap around symbol table */
   i = 0;
   sp = stab;
   }
  else ++sp;
  if( i == *idindex ) 
   cerror( 231 );  /* symbol table full (mknonuniq) */
  }
 sp->sflags = SNONUNIQ | SMOS; /* SS. 26.feb.87  removed unneeded var*/
 sp->sname = stab[*idindex].sname;
# ifndef BUG1
 if( ddebug )
  printf("\tnonunique entry for %s from %d to %d\n",
   sp->sname, *idindex, i );
# endif
 *idindex = i;
 return ( sp );
 }


lookup( name, s) register char *name; { 
 /* look up name: must agree with s w.r.t. STAG, SMOS and SHIDDEN */

 int i, ii;
 register struct symtab *sp;

 /* compute initial hash index */
# ifndef BUG1
 if( ddebug > 3 )
  printf( "lookup( %s, %d ), stwart=%d, instruct=%d\n", name, s, stwart, instruct );
# endif

 i = (int)((unsigned long)name % SYMTSZ);  /* host-port: (int)ptr truncates on 64-bit -> negative hash */
 sp = &stab[ii=i];

 for(;;){ /* look for name */

  if( sp->stype == TNULL ){ /* empty slot */
   sp->sflags = s;  /* set STAG, SMOS if needed, turn off all others */

   sp->sname = name;
   sp->stype = UNDEF;
   sp->sclass = SNULL;
   return( i );
   }
  if( (sp->sflags & (STAG|SMOS|SHIDDEN)) != s ) goto next;
  if( sp->sname == name )   /* SS 26.feb.87 removed unneede var*/
   return ( i );
 next:
  if( ++i >= SYMTSZ ){
   i = 0;
   sp = stab;
   }
  else ++sp;
  if( i == ii ) 
   cerror( 232 );  /* symbol table full (lookup) */
  }
 }

#ifndef checkst
/* if not debugging, make checkst a macro */
checkst(lev){
 register int s, i, j;
 register struct symtab *p, *q;

 for( i=0, p=stab; i<SYMTSZ; ++i, ++p ){
  if( p->stype == TNULL ) continue;
  j = lookup( p->sname, p->sflags&(SMOS|STAG) );
  if( j != i ){
   q = &stab[j];
   if( q->stype == UNDEF ||
       q->slevel <= p->slevel )
    cerror( 233, q->sname );  /* check error: %s */
   }
  else if( p->slevel > lev ) 
   cerror( 234, p->sname, lev ); /* %s check at level %d */ 
  }
 }
#endif

struct symtab *
relook(p) register struct symtab *p; {  /* look up p again, and see where it lies */

 register struct symtab *q;

 /* I'm not sure that this handles towers of several hidden definitions in all cases */
 q = &stab[lookup( p->sname, p->sflags&(STAG|SMOS|SHIDDEN) )];
 /* make relook always point to either p or an empty cell */
 if( q->stype == UNDEF ){
  q->stype = TNULL;
  return(q);
  }
 while( q != p ){
  if( q->stype == TNULL ) break;
  if( ++q >= &stab[SYMTSZ] ) q=stab;
  }
 return(q);
 }

clearst( lev ){ /* clear entries of internal scope  from the symbol table */
 register struct symtab *p, *q, *r;
 register int temp, rehash;

 temp = lineno;

 /* first, find an empty slot to prevent newly hashed entries from
    being slopped into... */

 for( q=stab; q< &stab[SYMTSZ]; ++q ){
  if( q->stype == TNULL )goto search;
  }

 cerror( 235 );  /* symbol table full (clearst) */

 search:
 p = q;

 for(;;){
  if( p->stype == TNULL ) {
   rehash = 0;
   goto next;
   }
  lineno = p->suse;
  if( lineno < 0 ) lineno = - lineno;
  if( p->slevel>lev ){ /* must clobber */
   if( p->stype == UNDEF || ( p->sclass == ULABEL && lev < 2 ) ){
    lineno = temp;
    uerror( 92, p->sname );  /* %s undefined */
    }
# ifndef BUG1
   if (ddebug) printf("removing %s from stab[ %d], flags %o level %d\n",
    p->sname,p-stab,p->sflags,p->slevel);
# endif
   if( p->sflags & SHIDES ) unhide(p);
   p->stype = TNULL;
   rehash = 1;
   goto next;
   }
  if( rehash ){
   if( (r=relook(p)) != p ){
    *r = *p;    /*  movestab( r, p ) */
    p->stype = TNULL;
    }
   }
  next:
  if( ++p >= &stab[SYMTSZ] ) p = stab;
  if( p == q ) break;
  }
 lineno = temp;
 }

/* SS. 26.feb.87  movestab( p, q ) register struct symtab *p, *q; 
   replaced with structure assignment: *p = *q;     */


hide( p ) register struct symtab *p; {
 register struct symtab *q;
 for( q=p+1; ; ++q ){
  if( q >= &stab[SYMTSZ] ) q = stab;
  if( q == p ) 
   cerror( 236 );  /* symbol table full (hide) */
  if( q->stype == TNULL ) break;
  }
 *q = *p;             /*  movestab( q, p ) */
 p->sflags |= SHIDDEN;
 q->sflags = (p->sflags&(SMOS|STAG)) | SHIDES;
 if( hflag ) 
  werror( 6, p->sname );  /* %s redefinition hides earlier one */
# ifndef BUG1
 if( ddebug ) printf( "  %d hidden in %d\n", p-stab, q-stab );
# endif
 return( idname = q-stab );
 }

unhide( p ) register struct symtab *p; {
 register struct symtab *q;
 register s;

 s = p->sflags & (SMOS|STAG);
 q = p;

 for(;;){

  if( q == stab ) q = &stab[SYMTSZ-1];
  else --q;

  if( q == p ) break;

  if( (q->sflags&(SMOS|STAG)) == s ){

   if (p->sname == q->sname) {
    q->sflags &= ~SHIDDEN;
# ifndef BUG1
    if( ddebug ) printf( "unhide uncovered %d from %d\n", q-stab,p-stab);
# endif
    return;
    }
   }

  }
 cerror( 237 );  /* unhide fails */
 }


# ifndef BUG1
static char *
ccnames[] = { /* names of storage classes */
 "SNULL",
 "AUTO",
 "EXTERN",
 "STATIC",
 "REGISTER",
 "EXTDEF",
 "LABEL",
 "ULABEL",
 "MOS",
 "PARAM",
 "STNAME",
 "MOU",
 "UNAME",
 "TYPEDEF",
 "FORTRAN",
 "ENAME",
 "MOE",
 "UFORTRAN",
 "USTATIC",
 };

char * scnames( c ) register c; {
 /* return the name for storage class c */
 static char buf[12];
 if( c&FIELD ){
  sprintf( buf, "FIELD[%d]", c&FLDSIZ );
  return( buf );
  }
 return( ccnames[c] );
 }
# endif

unsigned int offsz;
int curftn, ftnno;

