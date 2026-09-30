# define NAME 2
# define STRING 3
# define ICON 4
# define FCON 5
# define PLUS 6
# define MINUS 8
# define MUL 11
# define AND 14
# define OR 17
# define ER 19
# define QUEST 21
# define COLON 22
# define ANDAND 23
# define OROR 24
# define ASOP 25
# define RELOP 26
# define EQUOP 27
# define DIVOP 28
# define SHIFTOP 29
# define INCOP 30
# define UNOP 31
# define STROP 32
# define TYPE 33
# define CLASS 34
# define STRUCT 35
# define RETURN 36
# define GOTO 37
# define IF 38
# define ELSE 39
# define SWITCH 40
# define BREAK 41
# define CONTINUE 42
# define WHILE 43
# define DO 44
# define FOR 45
# define DEFAULT 46
# define CASE 47
# define SIZEOF 48
# define ENUM 49
# define LP 50
# define RP 51
# define LC 52
# define RC 53
# define LB 54
# define RB 55
# define CM 56
# define SM 57
# define ASSIGN 58

# line 120 "cgram.y"
# include "mfile1.h"
#define yyclearin yychar = -1
#define yyerrok yyerrflag = 0
extern int yychar;
extern int yyerrflag;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
YYSTYPE yylval, yyval;
typedef int yytabelem;

# line 140 "cgram.y"
 /* SS. 27.feb.87 the following vars were brought from pcc v6.0 */
 int wloop_level = LL_BOT; /* specifies while loop code gen.,test at loop bottom */
 int floop_level = LL_BOT; /* specifies for loop code gen.,test at loop bottom */
 static int fake = 0;

 static char fakename[24]; /* room enough for pattern */
# define YYERRCODE 256

# line 910 "cgram.y"


/*
 *  03.Feb.87  MK  yyaccpt() & ftnend() brought here
 */

NODE *
mkty( t, d, s ) unsigned t; {
 return( block( TYPE, NIL, NIL, t, d, s ) );
 }

NODE *
bdty( op, p, v ) NODE *p; {
 register NODE *q;

 q = block( op, p, NIL, INT, 0, INT );

 switch( op ){

 case UNARY MUL:
 case UNARY CALL:
  break;

 case LB:
  q->in.right = bcon(v);
  break;

 case NAME:
  q->tn.rval = v;
  break;

 default:
  cerror( 247 );  /* bad bdty */
  }

 return( q );
 }

dstash( n ){ /* put n into the dimension table */
 if( curdim >= DIMTABSZ-1 ){
  cerror( 248 );  /* dimension table overflow */
  }
 dimtab[ curdim++ ] = n;
 }

savebc() {
 if( psavbc > & asavbc[BCSZ-4 ] ){
  cerror( 249 );  /* whiles, fors, etc. too deeply nested */
  }
 *psavbc++ = brklab;
 *psavbc++ = contlab;
 *psavbc++ = flostat;
 *psavbc++ = swx;
 flostat = 0;
 }

resetbc(mask){

 swx = *--psavbc;
 flostat = *--psavbc | (flostat&mask);
 contlab = *--psavbc;
 brklab = *--psavbc;

 }

addcase(p) NODE *p; { /* add case to switch */

 p = optim( p );  /* change enum to ints */
 if( p->in.op != ICON ){
  uerror( 113 );  /* non-constant case expression */
  return;
  }
 if( swp == swtab ){
  uerror( 114 );  /* case not in switch */
  return;
  }
 if( swp >= &swtab[SWITSZ] ){
  cerror( 250 );  /* switch table overflow */
  }
 swp->sval = p->tn.lval;
 deflab( swp->slab = getlab() );
 ++swp;
 tfree(p);
 }


adddef(){ /* add default case to switch */
 if( swtab[swx].slab >= 0 ){
  uerror( 115 );  /* duplicate default in switch */
  return;
  }
 if( swp == swtab ){
  uerror( 116 );  /* default not inside switch */
  return;
  }
 deflab( swtab[swx].slab = getlab() );
 }


swstart(){
 /* begin a switch block */
 if( swp >= &swtab[SWITSZ] ){
  cerror( 250 );  /* switch table overflow */
  }
 swx = swp - swtab;
 swp->slab = -1;
 ++swp;
 }

swend(){ /* end a switch block */

 register struct sw *swbeg, *p, *q, *r, *r1;
 CONSZ temp;
 int tempi;

 swbeg = &swtab[swx+1];

 /* sort */

 r1 = swbeg;
 r = swp-1;

 while( swbeg < r ){
  /* bubble largest to end */
  for( q=swbeg; q<r; ++q ){
   if( q->sval > (q+1)->sval ){
    /* swap */
    r1 = q+1;
    temp = q->sval;
    q->sval = r1->sval;
    r1->sval = temp;
    tempi = q->slab;
    q->slab = r1->slab;
    r1->slab = tempi;
    }
   }
  r = r1;
  r1 = swbeg;
  }

 /* it is now sorted */

 for( p = swbeg+1; p<swp; ++p ){
  if( p->sval == (p-1)->sval ){
   uerror( 117, tempi=p->sval ); /* duplicate case in switch, %d */
   return;
   }
  }

 genswitch( swbeg-1, swp-swbeg );
 swp = swbeg-1;
 }


ftnend(){ /* end of function */
 if( retlab != NOLAB ){ /* inside a real function */
  efcode();
  }
 checkst(0);
 retstat = 0;
 tcheck();
 curclass = SNULL;
 brklab = contlab = retlab = NOLAB;
 flostat = 0;
 if( nerrors == 0 ){
  if( psavbc != & asavbc[0] ) 
   cerror( 216 );  /* bcsave error */ 
  if( paramno != 0 ) 
   cerror( 217 );  /* parameter reset error */ 
  if( swx != 0 ) 
   cerror( 218 );  /* switch error */ 
  }
 psavbc = &asavbc[0];
 paramno = 0;
 autooff = AUTOINIT;
 regvar = MAXRVAR;
 reached = 1;
 swx = 0;
 swp = swtab;
 }

yyaccpt() {
 ftnend ();
}

yytabelem yyexca[];
yytabelem yyexca[] = { -1, 1, 0, -1, 2, 21, 11, 21, 50, 21, 57, 21, -2, 0, -1, 19, 56, 80, 57, 80, -2, 7, -1, 24, 58, 77, -2, 79, -1, 26, 58, 78, -2, 83, -1, 32, 52, 44, -2, 42, -1, 34, 52, 36, -2, 34, -1, 56, 53, 48, 57, 48, -2, 0 };

# define YYNPROD 180
# define YYLAST 1168

yytabelem yyact[];
yytabelem yyact[] = { 230, 228, 282, 18, 248, 88, 86, 87, 27, 92, 79, 46, 97, 77, 27, 195, 78, 21, 129, 10, 9, 14, 27, 21, 76, 128, 6, 99, 95, 231, 144, 21, 81, 80, 50, 16, 91, 58, 240, 241, 245, 301, 247, 238, 239, 234, 246, 236, 250, 249, 82, 74, 83, 270, 107, 229, 22, 269, 55, 242, 45, 300, 22, 161, 10, 9, 14, 109, 292, 226, 22, 285, 93, 20, 36, 256, 130, 17, 19, 265, 16, 27, 27, 107, 145, 149, 153, 204, 24, 105, 21, 21, 102, 264, 41, 43, 36, 35, 162, 90, 188, 95, 137, 138, 139, 140, 141, 142, 153, 65, 40, 42, 98, 163, 164, 165, 167, 169, 171, 173, 175, 176, 178, 180, 182, 183, 184, 185, 186, 22, 22, 277, 130, 101, 100, 190, 157, 253, 199, 111, 8, 187, 160, 198, 145, 94, 69, 255, 28, 152, 220, 70, 38, 68, 38, 154, 39, 189, 39, 73, 108, 133, 72, 135, 48, 67, 64, 205, 49, 206, 52, 207, 56, 208, 192, 209, 220, 252, 210, 214, 211, 136, 212, 48, 48, 134, 202, 49, 49, 130, 197, 156, 151, 33, 94, 10, 9, 14, 31, 224, 304, 289, 216, 218, 274, 155, 227, 215, 27, 158, 225, 16, 222, 223, 251, 197, 193, 21, 219, 71, 10, 63, 14, 291, 146, 278, 273, 272, 262, 196, 221, 56, 150, 260, 258, 259, 16, 261, 150, 263, 51, 267, 4, 219, 281, 9, 271, 279, 53, 30, 275, 276, 118, 268, 196, 115, 22, 116, 243, 191, 118, 280, 96, 283, 201, 47, 59, 110, 26, 114, 34, 32, 66, 286, 287, 101, 26, 114, 117, 248, 88, 86, 87, 244, 146, 79, 302, 106, 77, 94, 295, 78, 296, 283, 115, 297, 116, 298, 104, 118, 7, 131, 25, 283, 60, 303, 305, 81, 80, 29, 44, 200, 89, 240, 241, 245, 114, 247, 238, 239, 234, 246, 236, 250, 249, 82, 54, 83, 57, 107, 254, 159, 203, 103, 242, 96, 26, 115, 62, 116, 61, 115, 118, 116, 37, 120, 118, 3, 121, 120, 122, 2, 125, 148, 123, 124, 126, 112, 119, 114, 117, 112, 119, 114, 117, 84, 11, 12, 5, 23, 13, 15, 237, 115, 235, 116, 232, 233, 118, 1, 0, 120, 299, 0, 121, 0, 122, 113, 125, 127, 123, 124, 126, 112, 119, 114, 117, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 115, 0, 116, 0, 0, 118, 0, 0, 120, 294, 0, 121, 0, 122, 113, 125, 127, 123, 124, 126, 112, 119, 114, 117, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 115, 0, 116, 0, 0, 118, 0, 0, 120, 293, 0, 121, 0, 122, 113, 125, 127, 123, 124, 126, 112, 119, 114, 117, 0, 0, 0, 85, 88, 86, 87, 0, 0, 79, 0, 0, 77, 0, 0, 78, 0, 0, 0, 0, 0, 290, 115, 0, 116, 0, 113, 118, 127, 0, 120, 81, 80, 121, 0, 122, 0, 125, 288, 123, 124, 126, 112, 119, 114, 117, 0, 0, 115, 82, 116, 83, 0, 118, 0, 0, 120, 0, 266, 121, 0, 122, 0, 125, 243, 123, 124, 126, 112, 119, 114, 117, 113, 115, 127, 116, 0, 0, 118, 0, 0, 120, 0, 0, 121, 0, 122, 0, 125, 0, 123, 124, 126, 112, 119, 114, 117, 0, 113, 284, 127, 0, 0, 0, 0, 0, 0, 115, 0, 116, 0, 0, 118, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 113, 257, 127, 115, 112, 116, 114, 117, 118, 0, 0, 120, 0, 0, 121, 0, 122, 0, 125, 0, 123, 124, 126, 112, 119, 114, 117, 115, 0, 116, 0, 0, 118, 0, 0, 120, 0, 0, 121, 0, 122, 0, 125, 213, 123, 124, 126, 112, 119, 114, 117, 0, 217, 113, 0, 127, 0, 0, 0, 0, 0, 0, 0, 115, 0, 116, 0, 115, 118, 116, 0, 120, 118, 0, 121, 0, 122, 113, 125, 127, 123, 124, 126, 112, 119, 114, 117, 112, 119, 114, 117, 0, 0, 0, 0, 0, 0, 0, 0, 115, 0, 116, 0, 0, 118, 0, 0, 120, 194, 0, 121, 0, 122, 113, 125, 127, 123, 124, 126, 112, 119, 114, 117, 115, 0, 116, 0, 0, 118, 0, 0, 120, 0, 0, 121, 0, 122, 0, 125, 0, 123, 124, 126, 112, 119, 114, 117, 0, 0, 113, 0, 127, 85, 88, 86, 87, 0, 0, 79, 0, 0, 77, 0, 0, 78, 85, 88, 86, 87, 0, 0, 79, 0, 0, 77, 127, 0, 78, 0, 0, 81, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 80, 0, 0, 0, 82, 0, 83, 0, 0, 85, 88, 86, 87, 0, 181, 79, 0, 82, 77, 83, 0, 78, 85, 88, 86, 87, 0, 179, 79, 0, 0, 77, 0, 0, 78, 0, 0, 81, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 80, 0, 0, 0, 82, 0, 83, 0, 0, 85, 88, 86, 87, 0, 177, 79, 0, 82, 77, 83, 0, 78, 85, 88, 86, 87, 0, 174, 79, 0, 0, 77, 0, 0, 78, 0, 0, 81, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 80, 0, 0, 0, 82, 0, 83, 0, 0, 85, 88, 86, 87, 0, 172, 79, 0, 82, 77, 83, 0, 78, 85, 88, 86, 87, 0, 170, 79, 0, 0, 77, 0, 0, 78, 0, 0, 81, 80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 81, 80, 0, 0, 0, 82, 0, 83, 0, 0, 85, 88, 86, 87, 0, 168, 79, 0, 82, 77, 83, 0, 78, 0, 85, 88, 86, 87, 166, 0, 79, 0, 0, 77, 0, 0, 78, 0, 81, 80, 0, 0, 0, 0, 0, 0, 115, 0, 116, 0, 0, 118, 81, 80, 120, 10, 82, 14, 83, 122, 132, 0, 85, 88, 86, 87, 112, 119, 114, 117, 82, 16, 83, 85, 88, 86, 87, 0, 0, 79, 0, 0, 77, 0, 0, 78, 85, 88, 86, 87, 81, 80, 79, 0, 0, 77, 0, 0, 78, 0, 0, 81, 80, 0, 0, 0, 0, 0, 82, 0, 83, 0, 0, 0, 81, 80, 0, 0, 0, 82, 0, 83, 0, 75, 85, 88, 86, 87, 0, 0, 79, 0, 82, 77, 83, 147, 78, 85, 88, 86, 87, 0, 0, 79, 0, 0, 77, 0, 0, 78, 0, 0, 81, 80, 0, 0, 115, 0, 116, 0, 0, 118, 0, 0, 120, 81, 80, 121, 0, 122, 82, 125, 83, 123, 124, 0, 112, 119, 114, 117, 0, 0, 115, 82, 116, 143, 0, 118, 0, 0, 120, 0, 0, 121, 0, 122, 0, 0, 0, 123, 0, 0, 112, 119, 114, 117, 115, 0, 116, 0, 0, 118, 0, 0, 120, 0, 0, 121, 0, 122, 0, 0, 0, 0, 0, 0, 112, 119, 114, 117 };

yytabelem yypact[];
yytabelem yypact[] = { -1000, -14, -1000, -1000, -1000, 20, -1000, 187, 211, -1000, 216, -1000, -1000, 146, 269, 141, 268, -1000, 40, 104, -1000, 206, 206, 9, 134, -24, -1000, 190, -1000, 187, 215, 187, -1000, 264, -1000, -1000, -1000, -1000, 170, 111, 134, 104, 114, 102, 95, -1000, -1000, -1000, 168, 107, 1009, -1000, -1000, -1000, 42, -1000, 79, 56, -1000, -31, 80, -1000, 31, -1000, -1000, 105, 1060, -1000, -1000, -1000, 265, -1000, -1000, 84, 709, 946, 131, 1060, 1060, 1060, 1060, 1060, 1073, 960, 1022, 188, -1000, -1000, -1000, 139, 187, 52, -1000, 104, 133, -1000, -1000, 138, 264, -1000, -1000, 104, -1000, -1000, -1000, 6, 41, -1000, -1000, 709, -1000, -1000, 1060, 1060, 908, 895, 857, 844, 806, 1060, 793, 755, 742, 1060, 1060, 1060, 1060, 1060, 44, -1000, 709, 946, -1000, -1000, 1060, 257, -1000, 131, 131, 131, 131, 131, 131, 960, 165, 649, 204, -1000, 87, 709, -1000, -1000, -1000, -1000, -1000, -1000, -1000, -1000, -1000, 162, 30, -1000, -1000, 249, 709, -1000, 1060, 241, 1060, 241, 1060, 288, 1060, -1000, 1060, 570, 653, 1060, 978, 1060, 335, 1060, 1138, 1114, 613, 709, 709, 126, 946, 44, 589, -1000, 152, 1060, -1000, 122, 179, 204, 1060, -1000, 79, -1000, 12, 2, -1000, 709, 709, 709, 709, 709, 709, 709, 709, 1060, -1000, -1000, 124, -1000, 998, 131, 82, -1000, 96, 122, 709, -1000, -1000, 18, -1000, -1000, 536, -1000, 277, 277, 183, 277, 178, 277, 36, 22, 470, 251, -1000, 0, 277, 177, -1000, 176, 182, 1060, 229, 1088, -1000, -1000, 76, 175, -1000, -1000, 208, -1000, 1060, 201, 1060, -1000, -1000, -1000, -1000, 511, 14, -1000, -1000, -1000, 1060, 1060, -1000, 485, -1000, -1000, 150, -1000, 439, 173, 11, 685, -1000, -1000, 403, 367, -1000, -1000, -1000, 1060, 1060, -1000, -1000, 277, 331, 4, -1000, -16, -1000, -1000, 1060, 149, 277, -1000 };

yytabelem yypgo[];
yytabelem yypgo[] = { 0, 379, 109, 377, 376, 374, 372, 371, 370, 369, 0, 2, 24, 26, 368, 140, 367, 366, 30, 15, 365, 9, 72, 88, 353, 351, 347, 3, 344, 340, 338, 29, 333, 332, 1, 331, 36, 300, 328, 12, 37, 326, 312, 58, 311, 310, 73, 304, 302, 25, 18, 301, 298, 290, 286, 283, 272 };

yytabelem yyr1[];
yytabelem yyr1[] = { 0, 1, 1, 25, 25, 26, 26, 28, 26, 29, 30, 30, 33, 33, 35, 35, 35, 32, 32, 32, 14, 14, 13, 13, 13, 13, 13, 37, 15, 15, 15, 15, 15, 16, 16, 7, 7, 38, 38, 40, 40, 17, 17, 8, 8, 41, 41, 43, 43, 36, 44, 36, 21, 21, 21, 21, 21, 23, 23, 23, 23, 23, 23, 22, 22, 22, 22, 22, 22, 22, 9, 45, 45, 45, 27, 47, 27, 48, 48, 46, 46, 46, 46, 46, 49, 49, 50, 50, 39, 39, 42, 42, 51, 31, 52, 34, 34, 34, 34, 53, 34, 34, 54, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 34, 55, 55, 55, 5, 4, 3, 6, 56, 2, 11, 11, 24, 24, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 18, 19, 19, 19, 19, 19, 19, 19, 20, 20 };

yytabelem yyr2[];
yytabelem yyr2[] = { 0, 4, 1, 3, 3, 5, 7, 1, 9, 4, 4, 1, 4, 1, 7, 9, 0, 7, 5, 5, 2, 1, 5, 4, 3, 3, 7, 3, 2, 5, 7, 2, 2, 11, 5, 3, 5, 2, 6, 3, 7, 11, 5, 3, 5, 2, 6, 5, 3, 3, 1, 9, 2, 2, 7, 5, 3, 5, 7, 7, 9, 3, 7, 5, 7, 7, 9, 7, 7, 5, 5, 3, 7, 2, 2, 1, 8, 3, 2, 3, 3, 7, 13, 2, 2, 6, 3, 9, 0, 2, 0, 2, 3, 9, 3, 5, 2, 5, 5, 1, 13, 15, 1, 21, 5, 5, 5, 5, 7, 7, 2, 4, 4, 4, 5, 7, 5, 3, 9, 7, 9, 1, 5, 2, 1, 2, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 9, 9, 9, 9, 9, 9, 9, 9, 11, 7, 7, 2, 5, 5, 5, 5, 5, 5, 5, 9, 9, 9, 5, 7, 7, 3, 3, 3, 3, 7, 5, 1, 5, 11, 5, 7, 9, 7, 5, 4 };

yytabelem yychk[];
yytabelem yychk[] = { -1000, -1, -25, -26, 256, -14, -13, -37, -15, 34, 33, -17, -16, -8, 35, -7, 49, 57, -27, -22, -46, 11, 50, -9, -23, -48, 256, 2, -15, -37, 33, 52, 2, 52, 2, 57, 56, -28, 50, 54, -23, -22, -23, -22, -45, 51, 2, 256, 50, 54, 58, 50, -15, 33, -41, -43, -15, -38, -40, 2, -47, -29, -30, 51, 55, -2, -56, 51, 51, 51, 56, 51, 55, -2, -10, 52, -12, 11, 14, 8, 31, 30, 48, 50, -20, 2, 4, 5, 3, -42, 57, -36, -21, -22, -23, 22, 256, -39, 56, 58, -46, -22, -31, -32, -52, -13, 256, 52, 55, -10, 2, 55, 26, 56, 28, 6, 8, 29, 11, 27, 14, 17, 19, 23, 24, 21, 25, 58, -49, -50, -10, -51, 52, 30, 54, 32, 50, -12, -12, -12, -12, -12, -12, 50, -18, -10, -15, 51, -24, -10, 50, 53, -43, 56, 22, -2, 53, -40, -2, -35, -36, 57, 57, -10, -10, -10, 58, -10, 58, -10, 58, -10, 58, -10, 58, -10, -10, 58, -10, 58, -10, 58, -10, -10, -10, -10, -10, -39, 56, -49, -10, 2, -18, 51, 51, -19, 50, 11, 56, 51, -44, -2, -13, -33, 57, -10, -10, -10, -10, -10, -10, -10, -10, 22, 53, -50, -39, 55, 51, -12, 54, 51, -19, -19, -10, -21, 57, -27, -34, 53, -10, -31, -4, -3, 43, -5, 45, -6, 41, 42, 36, 37, 57, 256, -55, 38, 44, 40, 2, 47, 46, -10, 53, 55, -2, 51, 57, 57, -34, -34, 50, -34, 50, -34, 57, 57, 57, -10, 2, 57, 53, -34, 50, 50, 22, -10, 22, 55, 50, 39, -10, 43, -11, -10, 57, 57, -10, -10, 22, 51, 51, 50, 57, 51, 51, -53, -10, -11, -34, 51, 57, 57, -54, -11, 51, -34 };

yytabelem yydef[];
yytabelem yydef[] = { 2, -2, 1, 3, 4, 0, 20, 24, 25, 27, 28, 31, 32, 0, 43, 0, 35, 5, 0, -2, 74, 0, 0, 0, -2, 0, -2, 61, 22, 23, 29, 0, -2, 0, -2, 6, 75, 11, 0, 121, 57, 63, 0, 0, 0, 69, 71, 73, 0, 121, 0, 70, 26, 30, 90, 45, -2, 88, 37, 39, 0, 8, 0, 64, 65, 0, 0, 62, 67, 68, 0, 58, 59, 0, 81, 0, 151, 0, 0, 0, 0, 0, 0, 0, 0, 165, 166, 167, 168, 0, 91, 47, 49, 52, 53, 121, 56, 0, 89, 121, 76, 80, 9, 10, 16, 0, 0, 94, 66, 122, 72, 60, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 88, 84, 86, 0, 92, 152, 0, 0, 179, 153, 154, 155, 156, 157, 158, 0, 0, 0, 171, 162, 0, 125, 178, 41, 46, 50, 121, 55, 33, 38, 40, 13, 0, 18, 19, 127, 128, 129, 0, 130, 0, 131, 0, 132, 0, 133, 0, 134, 135, 0, 136, 0, 137, 0, 138, 139, 0, 149, 150, 0, 89, 88, 0, 164, 0, 0, 169, 170, 171, 171, 0, 163, 0, 54, 0, 0, 17, 141, 142, 143, 144, 140, 145, 146, 147, 0, 82, 85, 0, 161, 160, 159, 121, 172, 0, 174, 126, 51, 14, 0, 12, 93, 0, 96, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 110, 0, 0, 0, 117, 0, 165, 0, 0, 148, 87, 175, 0, 177, 15, 95, 97, 98, 0, 0, 124, 104, 105, 106, 107, 0, 0, 111, 112, 113, 0, 0, 114, 0, 116, 176, 0, 119, 0, 0, 0, 123, 108, 109, 0, 0, 115, 173, 99, 0, 124, 118, 120, 0, 0, 0, 100, 0, 102, 101, 124, 0, 0, 103 };

typedef struct { char *t_name; int t_val; } yytoktype;
#ifndef YYDEBUG
# define YYDEBUG 0 /* don't allow debugging */
#endif

#if YYDEBUG

yytoktype yytoks[] =
{
 "NAME", 2,
 "STRING", 3,
 "ICON", 4,
 "FCON", 5,
 "PLUS", 6,
 "MINUS", 8,
 "MUL",  11,
 "AND",  14,
 "OR", 17,
 "ER", 19,
 "QUEST", 21,
 "COLON", 22,
 "ANDAND", 23,
 "OROR", 24,
 "ASOP", 25,
 "RELOP", 26,
 "EQUOP", 27,
 "DIVOP", 28,
 "SHIFTOP", 29,
 "INCOP", 30,
 "UNOP", 31,
 "STROP", 32,
 "TYPE", 33,
 "CLASS", 34,
 "STRUCT", 35,
 "RETURN", 36,
 "GOTO", 37,
 "IF", 38,
 "ELSE", 39,
 "SWITCH", 40,
 "BREAK", 41,
 "CONTINUE", 42,
 "WHILE", 43,
 "DO", 44,
 "FOR",  45,
 "DEFAULT", 46,
 "CASE", 47,
 "SIZEOF", 48,
 "ENUM", 49,
 "LP", 50,
 "RP", 51,
 "LC", 52,
 "RC", 53,
 "LB", 54,
 "RB", 55,
 "CM", 56,
 "SM", 57,
 "ASSIGN", 58,
 "-unknown-", -1 /* ends search */
};

char * yyreds[] =
{
 "-no such reduction-",
 "ext_def_list : ext_def_list external_def",
 "ext_def_list : /* empty */",
 "external_def : data_def",
 "external_def : error",
 "data_def : oattributes SM",
 "data_def : oattributes init_dcl_list SM",
 "data_def : oattributes fdeclarator",
 "data_def : oattributes fdeclarator function_body",
 "function_body : arg_dcl_list compoundstmt",
 "arg_dcl_list : arg_dcl_list declaration",
 "arg_dcl_list : /* empty */",
 "stmt_list : stmt_list statement",
 "stmt_list : /* empty */",
 "dcl_stat_list : dcl_stat_list attributes SM",
 "dcl_stat_list : dcl_stat_list attributes init_dcl_list SM",
 "dcl_stat_list : /* empty */",
 "declaration : attributes declarator_list SM",
 "declaration : attributes SM",
 "declaration : error SM",
 "oattributes : attributes",
 "oattributes : /* empty */",
 "attributes : class type",
 "attributes : type class",
 "attributes : class",
 "attributes : type",
 "attributes : type class type",
 "class : CLASS",
 "type : TYPE",
 "type : TYPE TYPE",
 "type : TYPE TYPE TYPE",
 "type : struct_dcl",
 "type : enum_dcl",
 "enum_dcl : enum_head LC moe_list optcomma RC",
 "enum_dcl : ENUM NAME",
 "enum_head : ENUM",
 "enum_head : ENUM NAME",
 "moe_list : moe",
 "moe_list : moe_list CM moe",
 "moe : NAME",
 "moe : NAME ASSIGN con_e",
 "struct_dcl : str_head LC type_dcl_list optsemi RC",
 "struct_dcl : STRUCT NAME",
 "str_head : STRUCT",
 "str_head : STRUCT NAME",
 "type_dcl_list : type_declaration",
 "type_dcl_list : type_dcl_list SM type_declaration",
 "type_declaration : type declarator_list",
 "type_declaration : type",
 "declarator_list : declarator",
 "declarator_list : declarator_list CM",
 "declarator_list : declarator_list CM declarator",
 "declarator : fdeclarator",
 "declarator : nfdeclarator",
 "declarator : nfdeclarator COLON con_e",
 "declarator : COLON con_e",
 "declarator : error",
 "nfdeclarator : MUL nfdeclarator",
 "nfdeclarator : nfdeclarator LP RP",
 "nfdeclarator : nfdeclarator LB RB",
 "nfdeclarator : nfdeclarator LB con_e RB",
 "nfdeclarator : NAME",
 "nfdeclarator : LP nfdeclarator RP",
 "fdeclarator : MUL fdeclarator",
 "fdeclarator : fdeclarator LP RP",
 "fdeclarator : fdeclarator LB RB",
 "fdeclarator : fdeclarator LB con_e RB",
 "fdeclarator : LP fdeclarator RP",
 "fdeclarator : name_lp name_list RP",
 "fdeclarator : name_lp RP",
 "name_lp : NAME LP",
 "name_list : NAME",
 "name_list : name_list CM NAME",
 "name_list : error",
 "init_dcl_list : init_declarator",
 "init_dcl_list : init_dcl_list CM",
 "init_dcl_list : init_dcl_list CM init_declarator",
 "xnfdeclarator : nfdeclarator",
 "xnfdeclarator : error",
 "init_declarator : nfdeclarator",
 "init_declarator : fdeclarator",
 "init_declarator : xnfdeclarator ASSIGN e",
 "init_declarator : xnfdeclarator ASSIGN LC init_list optcomma RC",
 "init_declarator : error",
 "init_list : initializer",
 "init_list : init_list CM initializer",
 "initializer : e",
 "initializer : ibrace init_list optcomma RC",
 "optcomma : /* empty */",
 "optcomma : CM",
 "optsemi : /* empty */",
 "optsemi : SM",
 "ibrace : LC",
 "compoundstmt : begin dcl_stat_list stmt_list RC",
 "begin : LC",
 "statement : e SM",
 "statement : compoundstmt",
 "statement : ifprefix statement",
 "statement : ifelprefix statement",
 "statement : WHILE LP e RP",
 "statement : WHILE LP e RP statement",
 "statement : doprefix statement WHILE LP e RP SM",
 "statement : FOR LP .e SM .e SM",
 "statement : FOR LP .e SM .e SM .e RP statement",
 "statement : switchpart statement",
 "statement : BREAK SM",
 "statement : CONTINUE SM",
 "statement : RETURN SM",
 "statement : RETURN e SM",
 "statement : GOTO NAME SM",
 "statement : SM",
 "statement : error SM",
 "statement : error RC",
 "statement : label statement",
 "label : NAME COLON",
 "label : CASE e COLON",
 "label : DEFAULT COLON",
 "doprefix : DO",
 "ifprefix : IF LP e RP",
 "ifelprefix : ifprefix statement ELSE",
 "switchpart : SWITCH LP e RP",
 "con_e : /* empty */",
 "con_e : e",
 ".e : e",
 ".e : /* empty */",
 "elist : e",
 "elist : elist CM e",
 "e : e RELOP e",
 "e : e CM e",
 "e : e DIVOP e",
 "e : e PLUS e",
 "e : e MINUS e",
 "e : e SHIFTOP e",
 "e : e MUL e",
 "e : e EQUOP e",
 "e : e AND e",
 "e : e OR e",
 "e : e ER e",
 "e : e ANDAND e",
 "e : e OROR e",
 "e : e MUL ASSIGN e",
 "e : e DIVOP ASSIGN e",
 "e : e PLUS ASSIGN e",
 "e : e MINUS ASSIGN e",
 "e : e SHIFTOP ASSIGN e",
 "e : e AND ASSIGN e",
 "e : e OR ASSIGN e",
 "e : e ER ASSIGN e",
 "e : e QUEST e COLON e",
 "e : e ASOP e",
 "e : e ASSIGN e",
 "e : term",
 "term : term INCOP",
 "term : MUL term",
 "term : AND term",
 "term : MINUS term",
 "term : UNOP term",
 "term : INCOP term",
 "term : SIZEOF term",
 "term : LP cast_type RP term",
 "term : SIZEOF LP cast_type RP",
 "term : term LB e RB",
 "term : funct_idn RP",
 "term : funct_idn elist RP",
 "term : term STROP NAME",
 "term : NAME",
 "term : ICON",
 "term : FCON",
 "term : STRING",
 "term : LP e RP",
 "cast_type : type null_decl",
 "null_decl : /* empty */",
 "null_decl : LP RP",
 "null_decl : LP null_decl RP LP RP",
 "null_decl : MUL null_decl",
 "null_decl : null_decl LB RB",
 "null_decl : null_decl LB con_e RB",
 "null_decl : LP null_decl RP",
 "funct_idn : NAME LP",
 "funct_idn : term LP",
};
#endif /* YYDEBUG */
/* @(#)yaccpar 1.9 */

/*
** Skeleton parser driver for yacc output
*/

/*
** yacc user known macros and defines
*/
#define YYERROR   goto yyerrlab
#define YYACCEPT return(0)
#define YYABORT   return(1)
#define YYBACKUP( newtoken, newvalue )\
{\
 if ( yychar >= 0 || ( yyr2[ yytmp ] >> 1 ) != 1 )\
 {\
  uerror( 128 );\
  goto yyerrlab;\
 }\
 yychar = newtoken;\
 yystate = *yyps;\
 yylval = newvalue;\
 goto yynewstate;\
}
#define YYRECOVERING() (!!yyerrflag)
#ifndef YYDEBUG
# define YYDEBUG 1 /* make debugging available */
#endif

/*
** user known globals
*/
int yydebug;   /* set to 1 to get debugging */

/*
** driver internal defines
*/
#define YYFLAG   (-1000)

/*
** global variables used by the parser
*/
YYSTYPE yyv[ YYMAXDEPTH ]; /* value stack */
int yys[ YYMAXDEPTH ];   /* state stack */

YYSTYPE *yypv;    /* top of value stack */
int *yyps;   /* top of state stack */

int yystate;   /* current state */
int yytmp;   /* extra var (lasts between blocks) */

int yynerrs;   /* number of errors */
int yyerrflag;    /* error recovery flag */
int yychar;   /* current input token number */



/*
** yyparse - return 0 if worked, 1 if syntax error not recovered from
*/
int
yyparse()
{
 register YYSTYPE *yypvt; /* top of value stack for $vars */

 /*
 ** Initialize externals - yyparse may be called more than once
 */
 yypv = &yyv[-1];
 yyps = &yys[-1];
 yystate = 0;
 yytmp = 0;
 yynerrs = 0;
 yyerrflag = 0;
 yychar = -1;

 goto yystack;
 {
  register YYSTYPE *yy_pv; /* top of value stack */
  register int *yy_ps;   /* top of state stack */
  register int yy_state;  /* current state */
  register int  yy_n;  /* internal state number info */

  /*
  ** get globals into registers.
  ** branch to here only if YYBACKUP was called.
  */
 yynewstate:
  yy_pv = yypv;
  yy_ps = yyps;
  yy_state = yystate;
  goto yy_newstate;

  /*
  ** get globals into registers.
  ** either we just started, or we just finished a reduction
  */
 yystack:
  yy_pv = yypv;
  yy_ps = yyps;
  yy_state = yystate;

  /*
  ** top of for (;;) loop while no reductions done
  */
 yy_stack:
  /*
  ** put a state and value onto the stacks
  */
#if YYDEBUG
  /*
  ** if debugging, look up token value in list of value vs.
  ** name pairs.  0 and negative (-1) are special values.
  ** Note: linear search is used since time is not a real
  ** consideration while debugging.
  */
  if ( yydebug )
  {
   register int yy_i;

   printf( "State %d, token ", yy_state );
   if ( yychar == 0 )
    printf( "end-of-file\n" );
   else if ( yychar < 0 )
    printf( "-none-\n" );
   else
   {
    for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
     yy_i++ )
    {
     if ( yytoks[yy_i].t_val == yychar )
      break;
    }
    printf( "%s\n", yytoks[yy_i].t_name );
   }
  }
#endif /* YYDEBUG */
  if ( ++yy_ps >= &yys[ YYMAXDEPTH ] ) /* room on stack? */
  {
   uerror( 129 );
   YYABORT;
  }
  *yy_ps = yy_state;
  *++yy_pv = yyval;

  /*
  ** we have a new state - find out what to do
  */
 yy_newstate:
  if ( ( yy_n = yypact[ yy_state ] ) <= YYFLAG )
   goto yydefault;  /* simple state */
#if YYDEBUG
  /*
  ** if debugging, need to mark whether new token grabbed
  */
  yytmp = yychar < 0;
#endif
  if ( ( yychar < 0 ) && ( ( yychar = yylex() ) < 0 ) )
   yychar = 0;   /* reached EOF */
#if YYDEBUG
  if ( yydebug && yytmp )
  {
   register int yy_i;

   printf( "Received token " );
   if ( yychar == 0 )
    printf( "end-of-file\n" );
   else if ( yychar < 0 )
    printf( "-none-\n" );
   else
   {
    for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
     yy_i++ )
    {
     if ( yytoks[yy_i].t_val == yychar )
      break;
    }
    printf( "%s\n", yytoks[yy_i].t_name );
   }
  }
#endif /* YYDEBUG */
  if ( ( ( yy_n += yychar ) < 0 ) || ( yy_n >= YYLAST ) )
   goto yydefault;
  if ( yychk[ yy_n = yyact[ yy_n ] ] == yychar ) /*valid shift*/
  {
   yychar = -1;
   yyval = yylval;
   yy_state = yy_n;
   if ( yyerrflag > 0 )
    yyerrflag--;
   goto yy_stack;
  }

 yydefault:
  if ( ( yy_n = yydef[ yy_state ] ) == -2 )
  {
#if YYDEBUG
   yytmp = yychar < 0;
#endif
   if ( ( yychar < 0 ) && ( ( yychar = yylex() ) < 0 ) )
    yychar = 0;   /* reached EOF */
#if YYDEBUG
   if ( yydebug && yytmp )
   {
    register int yy_i;

    printf( "Received token " );
    if ( yychar == 0 )
     printf( "end-of-file\n" );
    else if ( yychar < 0 )
     printf( "-none-\n" );
    else
    {
     for ( yy_i = 0;
      yytoks[yy_i].t_val >= 0;
      yy_i++ )
     {
      if ( yytoks[yy_i].t_val
       == yychar )
      {
       break;
      }
     }
     printf( "%s\n", yytoks[yy_i].t_name );
    }
   }
#endif /* YYDEBUG */
   /*
   ** look through exception table
   */
   {
    register int *yyxi = yyexca;

    while ( ( *yyxi != -1 ) ||
     ( yyxi[1] != yy_state ) )
    {
     yyxi += 2;
    }
    while ( ( *(yyxi += 2) >= 0 ) &&
     ( *yyxi != yychar ) )
     ;
    if ( ( yy_n = yyxi[1] ) < 0 )
     YYACCEPT;
   }
  }

  /*
  ** check for syntax error
  */
  if ( yy_n == 0 ) /* have an error */
  {
   /* no worry about speed here! */
   switch ( yyerrflag )
   {
   case 0:  /* new error */
    uerror( 130 ); /* "syntax error" */
    goto skip_init;
   yyerrlab:
    /*
    ** get globals into registers.
    ** we have a user generated syntax type error
    */
    yy_pv = yypv;
    yy_ps = yyps;
    yy_state = yystate;
    yynerrs++;
   skip_init:
   case 1:
   case 2:  /* incompletely recovered error */
     /* try again... */
    yyerrflag = 3;
    /*
    ** find state where "error" is a legal
    ** shift action
    */
    while ( yy_ps >= yys )
    {
     yy_n = yypact[ *yy_ps ] + YYERRCODE;
     if ( yy_n >= 0 && yy_n < YYLAST &&
      yychk[yyact[yy_n]] == YYERRCODE)      {
      /*
      ** simulate shift of "error"
      */
      yy_state = yyact[ yy_n ];
      goto yy_stack;
     }
     /*
     ** current state has no shift on
     ** "error", pop stack
     */
#if YYDEBUG
# define _POP_ "Error recovery pops state %d, uncovers state %d\n"
     if ( yydebug )
      printf( _POP_, *yy_ps,
       yy_ps[-1] );
# undef _POP_
#endif
     yy_ps--;
     yy_pv--;
    }
    /*
    ** there is no state on stack with "error" as
    ** a valid shift.  give up.
    */
    YYABORT;
   case 3:  /* no shift yet; eat a token */
#if YYDEBUG
    /*
    ** if debugging, look up token in list of
    ** pairs.  0 and negative shouldn't occur,
    ** but since timing doesn't matter when
    ** debugging, it doesn't hurt to leave the
    ** tests here.
    */
    if ( yydebug )
    {
     register int yy_i;

     printf( "Error recovery discards " );
     if ( yychar == 0 )
      printf( "token end-of-file\n" );
     else if ( yychar < 0 )
      printf( "token -none-\n" );
     else
     {
      for ( yy_i = 0;
       yytoks[yy_i].t_val >= 0;
       yy_i++ )
      {
       if ( yytoks[yy_i].t_val
         == yychar )
       {
         break;
       }
      }
      printf( "token %s\n",
       yytoks[yy_i].t_name );
     }
    }
#endif /* YYDEBUG */
    if ( yychar == 0 ) /* reached EOF. quit */
     YYABORT;
    yychar = -1;
    goto yy_newstate;
   }
  }/* end if ( yy_n == 0 ) */
  /*
  ** reduction by production yy_n
  ** put stack tops, etc. so things right after switch
  */
#if YYDEBUG
  /*
  ** if debugging, print the string that is the user's
  ** specification of the reduction which is just about
  ** to be done.
  */
  if ( yydebug )
   printf( "Reduce by (%d) \"%s\"\n",
    yy_n, yyreds[ yy_n ] );
#endif
  yytmp = yy_n;    /* value to switch over */
  yypvt = yy_pv;   /* $vars top of value stack */
  /*
  ** Look in goto table for next state
  ** Sorry about using yy_state here as temporary
  ** register variable, but why not, if it works...
  ** If yyr2[ yy_n ] doesn't have the low order bit
  ** set, then there is no action to be done for
  ** this reduction.  So, no saving & unsaving of
  ** registers done.  The only difference between the
  ** code just after the if and the body of the if is
  ** the goto yy_stack in the body.  This way the test
  ** can be made before the choice of what to do is needed.
  */
  {
   /* length of production doubled with extra bit */
   register int yy_len = yyr2[ yy_n ];

   if ( !( yy_len & 01 ) )
   {
    yy_len >>= 1;
    yyval = ( yy_pv -= yy_len )[1]; /* $$ = $1 */
    yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
     *( yy_ps -= yy_len ) + 1;
    if ( yy_state >= YYLAST ||
     yychk[ yy_state =
     yyact[ yy_state ] ] != -yy_n )
    {
     yy_state = yyact[ yypgo[ yy_n ] ];
    }
    goto yy_stack;
   }
   yy_len >>= 1;
   yyval = ( yy_pv -= yy_len )[1]; /* $$ = $1 */
   yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
    *( yy_ps -= yy_len ) + 1;
   if ( yy_state >= YYLAST ||
    yychk[ yy_state = yyact[ yy_state ] ] != -yy_n )
   {
    yy_state = yyact[ yypgo[ yy_n ] ];
   }
  }
     /* save until reenter driver code */
  yystate = yy_state;
  yyps = yy_ps;
  yypv = yy_pv;
 }
 /*
 ** code supplied by user is placed in this switch
 */
 switch( yytmp )
 {
  
case 2:
# line 150 "cgram.y"
ftnend(); break;
case 3:
# line 153 "cgram.y"
{ curclass = SNULL;  blevel = 0; } break;
case 4:
# line 155 "cgram.y"
{ curclass = SNULL;  blevel = 0; } break;
case 5:
# line 159 "cgram.y"
{  yypvt[-1].nodep->in.op = FREE; } break;
case 6:
# line 161 "cgram.y"
{  yypvt[-2].nodep->in.op = FREE; } break;
case 7:
# line 162 "cgram.y"
{
    defid( tymerge(yypvt[-1].nodep,yypvt[-0].nodep), curclass==STATIC?STATIC:EXTDEF );
    } break;
case 8:
# line 165 "cgram.y"
{  
       if( blevel ) 
    cerror( 251 );  /* function level error */
       if( reached ) retstat |= NRETVAL; 
       yypvt[-3].nodep->in.op = FREE;
       ftnend();
       } break;
case 11:
# line 177 "cgram.y"
{  blevel = 1; } break;
case 13:
# line 182 "cgram.y"
{  bccode();
       } break;
case 14:
# line 189 "cgram.y"
{  yypvt[-1].nodep->in.op = FREE; } break;
case 15:
# line 191 "cgram.y"
{  yypvt[-2].nodep->in.op = FREE; } break;
case 17:
# line 195 "cgram.y"
{ curclass = SNULL;  yypvt[-2].nodep->in.op = FREE; } break;
case 18:
# line 197 "cgram.y"
{ curclass = SNULL;  yypvt[-1].nodep->in.op = FREE; } break;
case 19:
# line 199 "cgram.y"
{  curclass = SNULL; } break;
case 21:
# line 203 "cgram.y"
{  yyval.nodep = mkty(INT,0,INT);  curclass = SNULL; } break;
case 22:
# line 206 "cgram.y"
{  yyval.nodep = yypvt[-0].nodep; } break;
case 24:
# line 209 "cgram.y"
{  yyval.nodep = mkty(INT,0,INT); } break;
case 25:
# line 211 "cgram.y"
{ curclass = SNULL ; } break;
case 26:
# line 213 "cgram.y"
{  yypvt[-2].nodep->in.type = types( yypvt[-2].nodep->in.type, yypvt[-0].nodep->in.type, UNDEF );
       yypvt[-0].nodep->in.op = FREE;
       } break;
case 27:
# line 220 "cgram.y"
{  curclass = yypvt[-0].intval; } break;
case 29:
# line 225 "cgram.y"
{  yypvt[-1].nodep->in.type = types( yypvt[-1].nodep->in.type, yypvt[-0].nodep->in.type, UNDEF );
       yypvt[-0].nodep->in.op = FREE;
       } break;
case 30:
# line 229 "cgram.y"
{  yypvt[-2].nodep->in.type = types( yypvt[-2].nodep->in.type, yypvt[-1].nodep->in.type, yypvt[-0].nodep->in.type );
       yypvt[-1].nodep->in.op = yypvt[-0].nodep->in.op = FREE;
       } break;
case 33:
# line 237 "cgram.y"
{ yyval.nodep = dclstruct(yypvt[-4].intval); } break;
case 34:
# line 239 "cgram.y"
{  yyval.nodep = rstruct(yypvt[-0].intval,0);  stwart = instruct; } break;
case 35:
# line 243 "cgram.y"
{  yyval.intval = bstruct(-1,0); stwart = SEENAME; } break;
case 36:
# line 245 "cgram.y"
{  yyval.intval = bstruct(yypvt[-0].intval,0); stwart = SEENAME; } break;
case 39:
# line 253 "cgram.y"
{  moedef( yypvt[-0].intval ); } break;
case 40:
# line 255 "cgram.y"
{  strucoff = yypvt[-0].intval;  moedef( yypvt[-2].intval ); } break;
case 41:
# line 259 "cgram.y"
{ yyval.nodep = dclstruct(yypvt[-4].intval);  } break;
case 42:
# line 261 "cgram.y"
{  yyval.nodep = rstruct(yypvt[-0].intval,yypvt[-1].intval); } break;
case 43:
# line 265 "cgram.y"
{  yyval.intval = bstruct(-1,yypvt[-0].intval);  stwart=0; } break;
case 44:
# line 267 "cgram.y"
{  yyval.intval = bstruct(yypvt[-0].intval,yypvt[-1].intval);  stwart=0;  } break;
case 47:
# line 275 "cgram.y"
{ curclass = SNULL;  stwart=0; yypvt[-1].nodep->in.op = FREE; } break;
case 48:
# line 277 "cgram.y"
{  if( curclass != MOU ){
    curclass = SNULL;
    }
       else {
    sprintf( fakename, "$%dFAKE", fake++ );
    /* No need to hash this, we won't look it up */
    defid( tymerge(yypvt[-0].nodep, bdty(NAME,NIL,lookup( fakename, SMOS ))), curclass );
    werror( 19 ); /* struct typed union member must be named */
    }
       stwart = 0;
       yypvt[-0].nodep->in.op = FREE;
       } break;
case 49:
# line 293 "cgram.y"
{ defid( tymerge(yypvt[-1].nodep,yypvt[-0].nodep), curclass);  stwart = instruct; } break;
case 50:
# line 294 "cgram.y"
{yyval.nodep=yypvt[-2].nodep;} break;
case 51:
# line 295 "cgram.y"
{ defid( tymerge(yypvt[-4].nodep,yypvt[-0].nodep), curclass);  stwart = instruct; } break;
case 54:
# line 301 "cgram.y"
{  if( !(instruct&INSTRUCT) ) 
    uerror( 118 );  /* field outside of structure */
       if( yypvt[-0].intval<0 || yypvt[-0].intval >= FIELD ){
    uerror( 119 );  /* illegal field size */
    yypvt[-0].intval = 1;
    }
       defid( tymerge(yypvt[-3].nodep,yypvt[-2].nodep), FIELD|yypvt[-0].intval );
       yyval.nodep = NIL;
       } break;
case 55:
# line 312 "cgram.y"
{  if( !(instruct&INSTRUCT) ) 
    uerror( 118 );  /* field outside of structure */
       falloc( stab, yypvt[-0].intval, -1, yypvt[-2].nodep );  /* alignment or hole */
       yyval.nodep = NIL;
       } break;
case 56:
# line 318 "cgram.y"
{  yyval.nodep = NIL; } break;
case 57:
# line 323 "cgram.y"
{  umul:
    yyval.nodep = bdty( UNARY MUL, yypvt[-0].nodep, 0 ); } break;
case 58:
# line 326 "cgram.y"
{  uftn:
    yyval.nodep = bdty( UNARY CALL, yypvt[-2].nodep, 0 );  } break;
case 59:
# line 329 "cgram.y"
{  uary:
    yyval.nodep = bdty( LB, yypvt[-2].nodep, 0 );  } break;
case 60:
# line 332 "cgram.y"
{  bary:
    if( (int)yypvt[-1].intval <= 0 ) 
     werror( 20 ); /* zero or negative subscript */
    yyval.nodep = bdty( LB, yypvt[-3].nodep, yypvt[-1].intval );  } break;
case 61:
# line 337 "cgram.y"
{  yyval.nodep = bdty( NAME, NIL, yypvt[-0].intval );  } break;
case 62:
# line 339 "cgram.y"
{ yyval.nodep=yypvt[-1].nodep; } break;
case 63:
# line 342 "cgram.y"
{  goto umul; } break;
case 64:
# line 344 "cgram.y"
{  goto uftn; } break;
case 65:
# line 346 "cgram.y"
{  goto uary; } break;
case 66:
# line 348 "cgram.y"
{  goto bary; } break;
case 67:
# line 350 "cgram.y"
{ yyval.nodep = yypvt[-1].nodep; } break;
case 68:
# line 352 "cgram.y"
{
    if( blevel!=0 ) 
       uerror( 120 );  /* func declaration in bad context */
    yyval.nodep = bdty( UNARY CALL, bdty(NAME,NIL,yypvt[-2].intval), 0 );
    stwart = 0;
    } break;
case 69:
# line 359 "cgram.y"
{
    yyval.nodep = bdty( UNARY CALL, bdty(NAME,NIL,yypvt[-1].intval), 0 );
    stwart = 0;
    } break;
case 70:
# line 366 "cgram.y"
{
    /* turn off typedefs for argument names */
    stwart = SEENAME;
    if( stab[yypvt[-1].intval].sclass == SNULL )
        stab[yypvt[-1].intval].stype = FTN;
    } break;
case 71:
# line 375 "cgram.y"
{ ftnarg( yypvt[-0].intval );  stwart = SEENAME; } break;
case 72:
# line 377 "cgram.y"
{ ftnarg( yypvt[-0].intval );  stwart = SEENAME; } break;
case 75:
# line 383 "cgram.y"
{yyval.nodep=yypvt[-2].nodep;} break;
case 77:
# line 387 "cgram.y"
{  defid( yypvt[-0].nodep = tymerge(yypvt[-1].nodep,yypvt[-0].nodep), curclass);
       beginit(yypvt[-0].nodep->tn.rval);
       } break;
case 79:
# line 394 "cgram.y"
{  nidcl( tymerge(yypvt[-1].nodep,yypvt[-0].nodep) ); } break;
case 80:
# line 396 "cgram.y"
{  defid( tymerge(yypvt[-1].nodep,yypvt[-0].nodep), uclass(curclass) );
   } break;
case 81:
# line 401 "cgram.y"
{  doinit( yypvt[-0].nodep );
       endinit(); } break;
case 82:
# line 404 "cgram.y"
{  endinit(); } break;
case 86:
# line 414 "cgram.y"
{  doinit( yypvt[-0].nodep ); } break;
case 87:
# line 416 "cgram.y"
{  irbrace(); } break;
case 92:
# line 430 "cgram.y"
{  ilbrace(); } break;
case 93:
# line 438 "cgram.y"
{  --blevel;
       /*CXREF becode(); */
       if( blevel == 1 ) blevel = 0;
       clearst( blevel );
  /*     cleardim();  */
       checkst( blevel );
       autooff = *--psavbc;
       regvar = *--psavbc;
       } break;
case 94:
# line 450 "cgram.y"
{  /*  setdim(); */
       if( blevel == 1 ) dclargs();
       /*CXREF else if (blevel > 1) bbcode(); */
       ++blevel;
       if( psavbc > &asavbc[BCSZ-2] ) 
    cerror( 252 );  /* "nesting too deep" */
       *psavbc++ = regvar;
       *psavbc++ = autooff;
       } break;
case 95:
# line 462 "cgram.y"
{ ecomp( yypvt[-1].nodep ); } break;
case 97:
# line 465 "cgram.y"
{ deflab(yypvt[-1].intval);
      reached = 1;
      } break;
case 98:
# line 469 "cgram.y"
{  if( yypvt[-1].intval != NOLAB ){
    deflab( yypvt[-1].intval );
    reached = 1;
    }
       } break;
case 99:
# line 475 "cgram.y"
{
    savebc();
    if (!reached)
     werror( 22 );  /* "loop not entered at top" */
    reached = 1;
    brklab = getlab();
    contlab = getlab();
    if( wloop_level == LL_BOT ){
     if (yypvt[-1].nodep->in.op==ICON && yypvt[-1].nodep->tn.lval != 0)
     {
      flostat = FLOOP;
      tfree(yypvt[-1].nodep);
      deflab(contlab);
     }
     else
     {
      branch(contlab);
      deflab(yyval.intval = getlab());
     }
    }
    else
     cerror( 296 ); /* bad for loop code gen value */
   } break;
case 100:
# line 499 "cgram.y"
{
   if( wloop_level == LL_BOT ){ /* test at loop bottom */
    if (flostat & FLOOP)
     branch(contlab);
    else
    {
     reached = 1;
     deflab(contlab);
     ecomp(buildtree(CBRANCH,
      buildtree(NOT, yypvt[-3].nodep, NIL),
      bcon(yypvt[-1].intval)));
    }
   }
   else
    cerror( 296 ); /* bad while loop code gen value */ 
   reached= ((flostat & FBRK) || !(flostat & FLOOP)) ? 1:0;
   deflab(brklab);
   resetbc(0);
  } break;
case 101:
# line 519 "cgram.y"
{  deflab( contlab );
       if( flostat & FCONT ) reached = 1;
       ecomp( buildtree( CBRANCH, buildtree( NOT, yypvt[-2].nodep, NIL ), bcon( yypvt[-6].intval ) ) );
       deflab( brklab );
       reached = 1;
       resetbc(0);
       } break;
case 102:
# line 527 "cgram.y"
{
    if (yypvt[-3].nodep)
     ecomp(yypvt[-3].nodep);
    else if (!reached)
     werror( 22 ); /* loop not entered at top*/
    savebc();
    contlab = getlab();
    brklab = getlab();
    reached = 1;
    if ( floop_level == LL_BOT ){ /* test at loop bottom */
     if (!yypvt[-1].nodep)
      flostat |= FLOOP;
     else if (yypvt[-1].nodep->in.op == ICON && yypvt[-1].nodep->tn.lval != 0)
     {
      flostat |= FLOOP;
      tfree(yypvt[-1].nodep);
      yypvt[-1].nodep = (NODE *)0;
     }
     else
      branch(yypvt[-5].intval = getlab());
     deflab(yyval.intval = getlab());
    } 
    else
     cerror( 296 ); /* "bad for loop code gen. value" */ 
   } break;
case 103:
# line 553 "cgram.y"
{
   if (flostat & FCONT)
   {
    deflab(contlab);
    reached = 1;
   }
   if (yypvt[-2].nodep)
    ecomp(yypvt[-2].nodep);
   if(floop_level == LL_BOT){ /* test at loop bottom */
    if (yypvt[-5].nodep)
     deflab(yypvt[-9].intval);
    /*FALLTHROUGH*/
   /* case LL_DUP: /* dup. test at top & bottom */
    if (yypvt[-5].nodep)
    {
     ecomp(buildtree(CBRANCH,
      buildtree(NOT, yypvt[-5].nodep, NIL),
      bcon(yypvt[-3].intval)));
    }
    else
     branch(yypvt[-3].intval);
   }
   else
    cerror( 296 ); /* bad for loop code gen. value*/

   deflab(brklab);
   reached =((flostat & FBRK) || !(flostat & FLOOP))? 1:0;
   resetbc(0);
  } break;
case 104:
# line 586 "cgram.y"
{  if( reached ) branch( brklab );
       deflab( yypvt[-1].intval );
      swend();
       deflab(brklab);
       if( (flostat&FBRK) || !(flostat&FDEF) ) reached = 1;
       resetbc(FCONT);
       } break;
case 105:
# line 594 "cgram.y"
{  if( brklab == NOLAB ) 
    uerror( 121 );  /* illegal break */
       else if(reached) branch( brklab );
       flostat |= FBRK;
       if( brkflag ) goto rch;
       reached = 0;
       } break;
case 106:
# line 602 "cgram.y"
{  if( contlab == NOLAB ) 
    uerror( 122 );  /* illegal continue */
       else branch( contlab );
       flostat |= FCONT;
       goto rch;
       } break;
case 107:
# line 609 "cgram.y"
{  retstat |= NRETVAL;
/* MK 08.sep.86 function must sometimes return a value */
       if (BTYPE(stab[curftn].stype) != UNDEF)
/* tbl */            printf( "\tli\t0\n" );
       branch( retlab );
   rch:
       if( !reached ) 
    werror( 18 );  /* statement not reached */
       reached = 0;
       } break;
case 108:
# line 620 "cgram.y"
{  register NODE *temp;
       idname = curftn;
       temp = buildtree( NAME, NIL, NIL );
       if(temp->in.type == TVOID)
    uerror( 123, stab[idname].sname); /* void func %s cannot return value */
       temp->in.type = DECREF( temp->in.type );
       temp = buildtree( RETURN, temp, yypvt[-1].nodep );
       /* now, we have the type of the RHS correct */
       temp->in.left->in.op = FREE;
       temp->in.op = FREE;
       ecomp( buildtree( FORCE, temp->in.right, NIL ) );
       retstat |= RETVAL;
       branch( retlab );
       reached = 0;
       } break;
case 109:
# line 636 "cgram.y"
{  register NODE *q;
       q = block( FREE, NIL, NIL, INT|ARY, 0, INT );
       q->tn.rval = idname = yypvt[-1].intval;
       defid( q, ULABEL );
       stab[idname].suse = -lineno;
       branch( stab[idname].offset );
       goto rch;
       } break;
case 114:
# line 650 "cgram.y"
{  register NODE *q;
       q = block( FREE, NIL, NIL, INT|ARY, 0, LABEL );
       q->tn.rval = yypvt[-1].intval;
       defid( q, LABEL );
       reached = 1;
       } break;
case 115:
# line 657 "cgram.y"
{  addcase(yypvt[-1].nodep);
       reached = 1;
       } break;
case 116:
# line 661 "cgram.y"
{  reached = 1;
       adddef();
       flostat |= FDEF;
       } break;
case 117:
# line 667 "cgram.y"
{  savebc();
       if( !reached ) 
    werror( 22 );  /* loop not entered at top */
       brklab = getlab();
       contlab = getlab();
       deflab( yyval.intval = getlab() );
       reached = 1;
       } break;
case 118:
# line 677 "cgram.y"
{  ecomp( buildtree( CBRANCH, yypvt[-1].nodep, bcon( yyval.intval=getlab()) ) ) ;
       reached = 1;
       } break;
case 119:
# line 682 "cgram.y"
{  if( reached ) branch( yyval.intval = getlab() );
       else yyval.intval = NOLAB;
       deflab( yypvt[-2].intval );
       reached = 1;
       } break;
case 120:
# line 692 "cgram.y"
{  savebc();
       brklab = getlab();
       ecomp( buildtree( FORCE, yypvt[-1].nodep, NIL ) );
       branch( yyval.intval = getlab() );
       swstart();
       reached = 0;
       } break;
case 121:
# line 703 "cgram.y"
{ yyval.intval=instruct; stwart=instruct=0; } break;
case 122:
# line 705 "cgram.y"
{  yyval.intval = icons( yypvt[-0].nodep );  instruct=yypvt[-1].intval; } break;
case 124:
# line 709 "cgram.y"
{ yyval.nodep=0; } break;
case 126:
# line 714 "cgram.y"
{  goto bop; } break;
case 127:
# line 718 "cgram.y"
{
   preconf:
       if( yychar==RELOP||yychar==EQUOP||yychar==AND||yychar==OR||yychar==ER ){
       precplaint:
    if( hflag ) 
       werror( 23 );  /* precedence confusion possible: parenthesize! */
    }
   bop:
       yyval.nodep = buildtree( yypvt[-1].intval, yypvt[-2].nodep, yypvt[-0].nodep );
       } break;
case 128:
# line 729 "cgram.y"
{  yypvt[-1].intval = COMOP;
       goto bop;
       } break;
case 129:
# line 733 "cgram.y"
{  goto bop; } break;
case 130:
# line 735 "cgram.y"
{  if(yychar==SHIFTOP) goto precplaint; else goto bop; } break;
case 131:
# line 737 "cgram.y"
{  if(yychar==SHIFTOP ) goto precplaint; else goto bop; } break;
case 132:
# line 739 "cgram.y"
{  if(yychar==PLUS||yychar==MINUS) goto precplaint; else goto bop; } break;
case 133:
# line 741 "cgram.y"
{  goto bop; } break;
case 134:
# line 743 "cgram.y"
{  goto preconf; } break;
case 135:
# line 745 "cgram.y"
{  if( yychar==RELOP||yychar==EQUOP ) goto preconf;  else goto bop; } break;
case 136:
# line 747 "cgram.y"
{  if(yychar==RELOP||yychar==EQUOP) goto preconf; else goto bop; } break;
case 137:
# line 749 "cgram.y"
{  if(yychar==RELOP||yychar==EQUOP) goto preconf; else goto bop; } break;
case 138:
# line 751 "cgram.y"
{  goto bop; } break;
case 139:
# line 753 "cgram.y"
{  goto bop; } break;
case 140:
# line 755 "cgram.y"
{  abop:
    yyval.nodep = buildtree( ASG yypvt[-2].intval, yypvt[-3].nodep, yypvt[-0].nodep );
    } break;
case 141:
# line 759 "cgram.y"
{  goto abop; } break;
case 142:
# line 761 "cgram.y"
{  goto abop; } break;
case 143:
# line 763 "cgram.y"
{  goto abop; } break;
case 144:
# line 765 "cgram.y"
{  goto abop; } break;
case 145:
# line 767 "cgram.y"
{  goto abop; } break;
case 146:
# line 769 "cgram.y"
{  goto abop; } break;
case 147:
# line 771 "cgram.y"
{  goto abop; } break;
case 148:
# line 773 "cgram.y"
{  yyval.nodep=buildtree(QUEST, yypvt[-4].nodep, buildtree( COLON, yypvt[-2].nodep, yypvt[-0].nodep ) );
       } break;
case 149:
# line 776 "cgram.y"
{  werror( 24 ); goto bop; } break;
case 150:
# line 778 "cgram.y"
{  goto bop; } break;
case 152:
# line 782 "cgram.y"
{  yyval.nodep = buildtree( yypvt[-0].intval, yypvt[-1].nodep, bcon(1) ); } break;
case 153:
# line 787 "cgram.y"
{ ubop:
       yyval.nodep = buildtree( UNARY yypvt[-1].intval, yypvt[-0].nodep, NIL );
       } break;
case 154:
# line 791 "cgram.y"
{  if( ISFTN(yypvt[-0].nodep->in.type) || ISARY(yypvt[-0].nodep->in.type) ){
    werror( 25 );  /* & before array or funct: ignored */
    yyval.nodep = yypvt[-0].nodep;
    }
       else goto ubop;
       } break;
case 155:
# line 798 "cgram.y"
{ goto ubop; } break;
case 156:
# line 800 "cgram.y"
{
       yyval.nodep = buildtree( yypvt[-1].intval, yypvt[-0].nodep, NIL );
       } break;
case 157:
# line 804 "cgram.y"
{  yyval.nodep = buildtree( yypvt[-1].intval==INCR ? ASG PLUS : ASG MINUS,
      yypvt[-0].nodep,
      bcon(1)  );
       } break;
case 158:
# line 809 "cgram.y"
{  yyval.nodep = doszof( yypvt[-0].nodep ); } break;
case 159:
# line 811 "cgram.y"
{  yyval.nodep = buildtree( CAST, yypvt[-2].nodep, yypvt[-0].nodep );

  /* carefully, now ...
  /* paintdown in buildtree returns the right child, thus
  /* we don't have to FREE nodes and return the right child here--
  /* we know this by the cast being tossed and having an ICON in
  /* hand upon the return from buildtree */

       if (yyval.nodep->in.op != ICON)
       {
        yyval.nodep->in.left->in.op = FREE;
        yyval.nodep->in.op = FREE;
        yyval.nodep = yyval.nodep->in.right;
       }
   } break;
case 160:
# line 827 "cgram.y"
{  yyval.nodep = doszof( yypvt[-1].nodep ); } break;
case 161:
# line 829 "cgram.y"
{  yyval.nodep = buildtree( UNARY MUL, buildtree( PLUS, yypvt[-3].nodep, yypvt[-1].nodep ), NIL ); } break;
case 162:
# line 831 "cgram.y"
{  yyval.nodep=buildtree(UNARY CALL,yypvt[-1].nodep,NIL); } break;
case 163:
# line 833 "cgram.y"
{  yyval.nodep=buildtree(CALL,yypvt[-2].nodep,yypvt[-1].nodep); } break;
case 164:
# line 835 "cgram.y"
{  if( yypvt[-1].intval == DOT ){
    if( notlval( yypvt[-2].nodep ) )
        uerror( 124 );  /* struct reference must be addressable */
    yypvt[-2].nodep = buildtree( UNARY AND, yypvt[-2].nodep, NIL );
    }
       idname = yypvt[-0].intval;
       yyval.nodep = buildtree( STREF, yypvt[-2].nodep, buildtree( NAME, NIL, NIL ) );
       } break;
case 165:
# line 844 "cgram.y"
{  idname = yypvt[-0].intval;
       /* recognize identifiers in initializations */
       if( blevel==0 && stab[idname].stype == UNDEF ) {
    register NODE *q;

    werror( 26, stab[idname].sname ); /* undeclared initializer name %s */
    q = block( FREE, NIL, NIL, INT, 0, INT );
    q->tn.rval = idname;
    defid( q, EXTERN );
    }
       yyval.nodep=buildtree(NAME,NIL,NIL);
       stab[yypvt[-0].intval].suse = -lineno;
   } break;
case 166:
# line 858 "cgram.y"
{  yyval.nodep=bcon(0);
       yyval.nodep->tn.lval = lastcon;
       yyval.nodep->tn.rval = NONAME;
       if( yypvt[-0].intval ) yyval.nodep->fn.csiz = yyval.nodep->in.type = ctype(LONG);
       } break;
case 167:
# line 864 "cgram.y"
{  yyval.nodep=buildtree(FCON,NIL,NIL);
       yyval.nodep->fpn.dval = dcon;
       } break;
case 168:
# line 868 "cgram.y"
{  yyval.nodep = getstr(); /* get string contents */ } break;
case 169:
# line 870 "cgram.y"
{ yyval.nodep=yypvt[-1].nodep; } break;
case 170:
# line 874 "cgram.y"
{
   yyval.nodep = tymerge( yypvt[-1].nodep, yypvt[-0].nodep );
   yyval.nodep->in.op = NAME;
   yypvt[-1].nodep->in.op = FREE;
   } break;
case 171:
# line 882 "cgram.y"
{ yyval.nodep = bdty( NAME, NIL, -1 ); } break;
case 172:
# line 884 "cgram.y"
{ yyval.nodep = bdty( UNARY CALL, bdty(NAME,NIL,-1),0); } break;
case 173:
# line 886 "cgram.y"
{  yyval.nodep = bdty( UNARY CALL, yypvt[-3].nodep, 0 ); } break;
case 174:
# line 888 "cgram.y"
{  goto umul; } break;
case 175:
# line 890 "cgram.y"
{  goto uary; } break;
case 176:
# line 892 "cgram.y"
{  goto bary;  } break;
case 177:
# line 894 "cgram.y"
{ yyval.nodep = yypvt[-1].nodep; } break;
case 178:
# line 898 "cgram.y"
{  if( stab[yypvt[-1].intval].stype == UNDEF ){
    register NODE *q;
    q = block( FREE, NIL, NIL, FTN|INT, 0, INT );
    q->tn.rval = yypvt[-1].intval;
    defid( q, EXTERN );
    }
       idname = yypvt[-1].intval;
       yyval.nodep=buildtree(NAME,NIL,NIL);
       stab[idname].suse = -lineno;
   } break;
 }
 goto yystack;   /* reset registers in driver code */
}
