/* Revision log:
 * 14.Oct.86 MK gdebug related stuff removed
 * 20.Oct.86 MK globsize added for procedure #0 alloc's
 * 27.Oct.86 MK strflg, oflag removed
 * 29.Oct.86 MK part of lxtitle() & lastloc removed
 *         call to ejobcode removed in main()
 *         message about errors in main()
 * 01.Dec.86 MK PROFILE added
 * 07.Dec.86 MK "asm" introduced : asm (" ... ")
 * 08.Dec.86 MK gcos BCD const removed
 * 10.Dec.86 MK getchar() -> gtc()
 * 29.dec.86 SS "stk" directive introduced
 * 02.Jan.87 MK zero fill for data area: globlab, stklab, globno
 * 10.Jan.87 MK debugging flags -> BUG4
 * 30.jan.87 SS release moved to code.c
 * 03.Feb.87 MK asm_esc removed; ititle related stuff removed in lxtitle()
 *         ftitle , lineno, lastcon, dcon, pflag brought here
 * 04.Feb.87 MK for Kronos, part of variable definitions moved to end
 * 20.Feb.87 MK prints 
 * 26.Feb.87 SS hex and octal constant checking changed in yylex()
 *           SS code for lexical action A_EQ(=) replaced with code from v6.0
 * 08.Apr.87 SS unsigned casts added in lxstr() and yylex()
 * 15.Apr.87 SS GW2 now is static var _kron_prof
 * 22.Apr.87 MK asflag & line printing (outline, gtc, ungtc)
 * 23.Apr.87 MK ; for C lines instead of %
 * 09.Jun.87 SS asflag setting is done now in reader.c
 * 11.Jun.87 SS profiling controlled by -p flag (was -P)
 * 16.Jun.87 SS if Kronos2/5 then look for pccerr.msg in /ii/C dir
 * 18.Sep.87 MK & SS 
 * 25.Sep.87 SS removed qflag related stuff
 * 16.Oct.87 SS in excII environment the file pccerr.msg location depends on _C_HOME_DIR
 */

# include "mfile1.h"
# include <ctype.h>

#ifdef  PROFILE
int profnum = 0;
#endif

 /* lexical actions */

# define A_ERR 0  /* illegal character */
# define A_LET 1  /* saw a letter */
# define A_DIG 2  /* saw a digit */
# define A_1C 3    /* return a single character */
# define A_STR 4  /* string */
# define A_CC 5    /* character constant */
# define A_BCD 6  /* GCOS BCD constant */
# define A_SL 7    /* saw a / */
# define A_DOT 8  /* saw a . */
# define A_PL 9    /* + */
# define A_MI 10  /* - */
# define A_EQ 11  /* = */
# define A_NOT 12  /* ! */
# define A_LT 13  /* < */
# define A_GT 14  /* > */
# define A_AND 16  /* & */
# define A_OR 17  /* | */
# define A_WS 18  /* whitespace (not \n) */
# define A_NL 19  /* \n */

 /* character classes */

# define LEXLET 01
# define LEXDIG 02
# define LEXOCT 04
# define LEXHEX 010
# define LEXWS 020
# define LEXDOT 040

 /* reserved word actions */

# define AR_TY 0 /* type word */
# define AR_RW 1 /* simple reserved word */
# define AR_CL 2 /* storage class word */
# define AR_S 3   /* struct */
# define AR_U 4   /* union */
# define AR_E 5   /* enum */
# define AR_A 6   /* asm */

 /* text buffer */
#define  LXTSZ BUFSIZ
char yytext[LXTSZ];
char * lxgcp;

extern int cdebug, /* SS. 02.07.86 controls comment generating  HERE */
 proflg,
 verbose, /* SS. set in reader.c */
 nwarnings, /* SS. 29.dec.86 number of warnings */
 asflag,  /* Print C source lines in assembler text, set in reader.c */
 globsize, /* sizeof global aggregates for module  HERE */
 globlab, /* label for globals' number HERE */
 stklab,  /* label for stack size HERE */
 globno,  /* number of globals in a module HERE */
 fnr;  /* function number */

/* MK 22.Apr.87 */


char ftitle [100] = "";  /* title of the file */
int lineno;  /* line number of the input file */
CONSZ lastcon; /* the last constant read by the lexical analyzer */


 /* ARGSUSED */
mainp1( argc, argv ) int argc; char *argv[]; {  /* control multiple files */

 register i;
 register char *cp;
#ifndef BUG1
 extern int idebug, bdebug, tdebug, edebug, ddebug, xdebug, gdebug;
#endif
 int fdef = 0;
 extern char *release;   /* SS. 30.jan.87 moved to code.c */

 if( msginit("pccerr.msg") ){
#if excII 
  extern char *getenv();   /* SS. 18.Oct.87 from module env.cod */
  char msgfname[60];
  strcpy( msgfname,getenv("CDIR") );
  msginit( strcat( msgfname,"pccerr.msg") );
#endif
#if unix
  msginit("/usr1/kronos/pccerr.msg");
#endif
 }

 offsz = caloff();  /*  SS. 27.dec.86  512 Kb in case of Kronos */
 for( i=1; i<argc; ++i ){
  if( *(cp=argv[i]) == '-' && *++cp == 'X' ){
   while( *++cp ){
    switch( *cp ){
#ifndef  BUG4
    case 'd':
     ++ddebug;
     break;
    case 'i':
     ++idebug;
     break;
    case 'b':
     ++bdebug;
     break;
    case 't':
     ++tdebug;
     break;
    case 'e':
     ++edebug;
     break;
    case 'x':
     ++xdebug;
     break;
#endif
#ifdef PROFILE
    case 'p': /* profiling */
     ++proflg;
     break;
#endif
#ifdef VAX
    case 'g':
     ++gdebug;
     break;
#endif
    default:
     fprintf( stderr, 
  "\nCommand line warning: ignoring unknown flag %c\n", *cp );
     break;
     }
    }
   }
   else {
   if( *(argv[i]) != '-' ) switch( fdef++ ) {
    case 0:
    case 1:
     if( freopen(argv[i], fdef==1 ? "r" : "w", fdef==1 ? stdin : stdout) == NULL) {
      fprintf(stderr, "\npcc: Can't open %s\n", argv[i]);
      exit(1);
     }
 /* M.K.  06.02.86  - Set file title from command line */
     if (fdef == 1)
      strcpy (ftitle, argv[i]);
     break;

    default:
     ;
    }
   }
  }


 p2init( argc, argv );

/* MK 22.Apr.87 */
 if( verbose ){
  fprintf (stderr, "%s", release);
  fprintf (stderr, "\t\"%s\"\n", ftitle);
#ifdef BUFSTDERR
  fflush (stderr);
#endif
 }

 for( i=0; i<SYMTSZ; ++i ) stab[i].stype = TNULL;

 lxinit();
 tinit();
 mkdope();

 lineno = 1;
 fnr = 0; /* SS. 14.apr.87 function number */

 /* dimension table initialization */

 dimtab[NULL] = 0;
 dimtab[CHAR] = SZCHAR;
 dimtab[INT] = SZINT;
 dimtab[FLOAT] = SZFLOAT;
 dimtab[DOUBLE] = SZDOUBLE;
 dimtab[LONG] = SZLONG;
 dimtab[SHORT] = SZSHORT;
 dimtab[UCHAR] = SZCHAR;
 dimtab[USHORT] = SZSHORT;
 dimtab[UNSIGNED] = SZINT;
 dimtab[ULONG] = SZLONG;
 /* starts past any of the above */
 curdim = 16;
 reached = 1;

 if( cdebug )
/* tbl */ printf( "%% %s\n", release );

/* tbl */ D_MOD;
/* tbl */ printf( "%s\n", ftitle );

/* 27.Nov.86  if SWAP instruction is implemented, entr 2 is not needed! */
#if 0
#ifndef SWAPCMD
/* tbl */ D_BEGALLOC;
/* tbl */ printf("\tentr\t%d\n", SWAPINIT/SZINT );
/* tbl */ D_ENDALLOC;
#endif
#endif

#ifdef PROFILE
/* tbl */ prints("\t.svr\t_kron_prof\n");
#endif
 globno = 0; /* SS. 04.jan.87  GW2 always included */
   /* MK. 18.Sep.87  no profiling */

/*  MK 02.Jan.87 */

/* globlab = getlab(); MK. 18.Sep.87 no. of globals not needed */
 stklab  = getlab();
 globsize = 0;   /* AUTOINIT / SZINT + 1;  MK. 18.Sep.87 only actual
    size of aggregates; ASM will add 6 */

/* MK 18.Sep.87 This will be done by ASM
   tbl   D_BEGINIT;
   tbl   prints("\tla\t_kron_prof\n");
   tbl   printf("\tli\tM%d\n\tcx\t_zero\n", globlab);
   tbl   printf("\tli\tM%d\n\talloc\n\tli\tM%d\n\tcx\t_zero\n", stklab,stklab);
   tbl   D_ENDINIT;
*/
 yyparse();
 yyaccpt();

#ifdef PROFILE
 if (proflg) {
  globsize += (profnum + 1);
/* tbl */ D_BEGINIT;
/* tbl */ printf("\tli\t%d\n", globsize); 
/* tbl */ prints("\talloc\n\tsw\t_kron_prof\n");
/* tbl */ printf("\tlw\t_kron_prof\n\tli\t%d\n\tssw\t0\n", profnum);
/* tbl */ D_ENDINIT;
 }
#endif
  /* SS. 29.dec.86  stk0+sizeof array for profiling numbers + 6 */
/* tbl */ D_STK;
/* tbl */ printf("%d\n", globsize);

/* tbl    printf("M%d\t.equ\t%d\n", globlab, globno);
 MK. 18.Sep.87 ASM will count no. of global variables */
/* MK 09.01.87 alloc'i pole vaja teha nii palju: 6 on juba tehtud ! */
/* tbl   printf("M%d\t.equ\t%d\n", stklab, globsize);
 MK. 18.09.87 this number is not needed also */

/* tbl */ D_END;
/* tbl */ printf( "%s\n", ftitle );

 msgterm();
 if( verbose )
  fprintf( stderr, "# lines: %d\n", lineno-1 );  
     /* SS. 29.dec.86 # of lines */
 if(nwarnings)
  fprintf(stderr, "\n*** Notice: %d warning(s)\n", nwarnings );
 if(nerrors)
  fprintf(stderr,"\n*** Notice: %d error(s) detected\n", nerrors);
#ifdef BUFSTDERR
 fflush( stderr ); /* SS. 04.jan.87 we must flush the buffer */
#endif
 return(nerrors?1:0);

} /* mainp1 */


# define CSMASK 0177
# define CSSZ 128

/* MK 22.Apr.87
 * Function outline() forces the contents of linebuf to be printed.
 * Function gtc() behaves differently for asflag.
 * Function ungtc() is needed to manipulate line buffer
 */

static char linebuf [200], /* should be enough */
  * linepnt = linebuf;

void outline ()
{
 if (linepnt != linebuf) {
  int first = * linebuf;
  if (first != '#' && first != '\n' ) {
   if (* (linepnt-1) != '\n')
    *linepnt++ = '\n';
   * linepnt = '\0';
   printf("; #%d: %s", lineno, linebuf);
   }
  linepnt = linebuf;
  }
}

/* This function replaces getchar() macro expansions */

gtc ()
{
 if (asflag) {
  int code = getchar();
  * linepnt ++ = code;
  if (code == '\n')
   outline();
  return (code);
  }
 else 
  return (getchar());
}

ungtc (c, fp)
int c;
FILE * fp;
{
 if (asflag && linepnt > linebuf)
  linepnt --;
 ungetc (c, fp);
}

#define  getchar gtc
#define ungetc ungtc


short lxmask[CSSZ+1];

lxenter( s, m ) register char *s; register short m; {
 /* enter a mask into lxmask */
 register c;

 while( c= *s++ ) lxmask[c+1] |= m;

 }


# define lxget(c,m) (lxgcp=yytext,lxmore(c,m))

lxmore( c, m )  register c, m; {
 register char *cp;

 *(cp = lxgcp) = c;
 while( c=getchar(), lxmask[c+1]&m ){
  if( cp < &yytext[LXTSZ-1] ){
   *++cp = c;
   }
  }
 ungetc(c,stdin);
 *(lxgcp = cp+1) = '\0';
 }

struct lxdope {
 short lxch; /* the character */
 short lxact; /* the action to be performed */
 short lxtok; /* the token number to be returned */
 short lxval; /* the value to be returned */
 } lxdope[] = {

 '@', A_ERR, 0,  0, /* illegal characters go here... */
 '_', A_LET, 0,  0, /* letters point here */
 '0', A_DIG, 0,  0, /* digits point here */
 ' ', A_WS, 0, 0, /* whitespace goes here */
 '\n', A_NL, 0,  0,
 '"', A_STR, 0,  0, /* character string */
 '\'', A_CC, 0,  0, /* character constant */
 '`', A_ERR, 0,  0, /* GCOS BCD constant */
 '(', A_1C, LP,  0,
 ')', A_1C, RP,  0,
 '{', A_1C, LC,  0,
 '}', A_1C, RC,  0,
 '[', A_1C, LB,  0,
 ']', A_1C, RB,  0,
 '*', A_1C, MUL, MUL,
 '?', A_1C, QUEST, 0,
 ':', A_1C, COLON, 0,
 '+', A_PL, PLUS, PLUS,
 '-', A_MI, MINUS, MINUS,
 '/', A_SL, DIVOP, DIV,
 '%', A_1C, DIVOP, MOD,
 '&', A_AND, AND, AND,
 '|', A_OR, OR,  OR,
 '^', A_1C, ER,  ER,
 '!', A_NOT, UNOP, NOT,
 '~', A_1C, UNOP, COMPL,
 ',', A_1C, CM,  CM,
 ';', A_1C, SM,  0,
 '.', A_DOT, STROP, DOT,
 '<', A_LT, RELOP, LT,
 '>', A_GT, RELOP, GT,
 '=', A_EQ, ASSIGN, ASSIGN,
 -1, A_1C, 0, 0,
 };

struct lxdope *lxcp[CSSZ+1];

lxinit(){
 register struct lxdope *p;
 register i;
 register char *cp;
 /* set up character classes */

 lxenter( "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_$", LEXLET );
 lxenter( "0123456789", LEXDIG );
 lxenter( "0123456789abcdefABCDEF", LEXHEX );
  /* \013 should become \v someday; \013 is OK for ASCII and EBCDIC */
 lxenter( " \t\r\b\f\013", LEXWS );
 lxenter( "01234567", LEXOCT );
 lxmask['.'+1] |= LEXDOT;

 /* make lxcp point to appropriate lxdope entry for each character */

 /* initialize error entries */

 for( i= 0; i<=CSSZ; ++i ) lxcp[i] = lxdope;

 /* make unique entries */

 for( p=lxdope; ; ++p ) {
  lxcp[p->lxch+1] = p;
  if( p->lxch < 0 ) break;
  }

 /* handle letters, digits, and whitespace */
 /* by convention, first, second, and third places */

 cp = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ$";
 while( *cp ) lxcp[*cp++ + 1] = &lxdope[1];
 cp = "123456789";
 while( *cp ) lxcp[*cp++ + 1] = &lxdope[2];
 cp = "\t\b\r\f\013";
 while( *cp ) lxcp[*cp++ + 1] = &lxdope[3];

 /* first line might have title */
 lxtitle();

 }

int pflag = 1; /* Do we check for portable constructions?
      P.S. This is a very poor check indeed. */


/* SS. 16.Mar.87 makecc() from macdefs */

# define makecc(val,i)  lastcon = (lastcon<<8)|((val<<24)>>24);

int lxmatch;  /* character to be matched in char or string constant */

lxstr(ct){
 /* match a string or character constant, up to lxmatch */

 register c;
 register val;
 register i;

 i=0;
 while( (c=getchar()) != lxmatch ){
  switch( c ) {

  case EOF:
   uerror( 51 );  /* unexpected EOF */
   break;

  case '\n':
   uerror( 52 );  /* newline in string or char constant */
   ++lineno;
   break;

  case '\\':
   switch( c = getchar() ){

   case '\n':
    ++lineno;
    continue;

   default:
    val = c;
    goto mkcc;

   case 'n':
    val = '\n';
    goto mkcc;

   case 'r':
    val = '\r';
    goto mkcc;

   case 'b':
    val = '\b';
    goto mkcc;

   case 't':
    val = '\t';
    goto mkcc;

   case 'f':
    val = '\f';
    goto mkcc;

   case 'v':
    val = '\013';
    goto mkcc;

   case '0':
   case '1':
   case '2':
   case '3':
   case '4':
   case '5':
   case '6':
   case '7':
    val = c-'0';
    c=getchar();  /* try for 2 */
    if( lxmask[c+1] & LEXOCT ){
   /* SS. 8.apr.87 unsigned casts added to prevent overflow*/
     val = ((unsigned)val<<3) | (c-'0');
     c = getchar();  /* try for 3 */
     if( lxmask[c+1] & LEXOCT ){
      val = ((unsigned)val<<3) | (c-'0');
      }
     else ungetc( c ,stdin);
     }
    else ungetc( c ,stdin);

    goto mkcc1;

    }
  default:
   val =c;
  mkcc:
   val = CCTRANS(val);
  mkcc1:
   if( lxmatch == '\'' ){
    val = CHARCAST(val);  /* it is, after all, a "character" constant */
    lastcon = val;
/*
    makecc( val, i );
*/
   }
   else  /* stash the byte into the string */
    bycode( val, i );
   ++i;
   continue;
   }
  break;
  }
 /* end of string or  char constant */

 if( lxmatch == '"' ){
    /* the initializer gets a null byte */
/* MK asm string constants should meet C language standard :
  bycode( 0, i++ );
*/
  if (i == 0) {
   D_DATA;  prints("\"\"\n");
   dimtab[curdim]=1;
  }
  else {
   i ++;
   bycode( -1, i );
   dimtab[curdim] = i;  /* in case of later sizeof ... */
   }
  }
 else { /* end the character constant */
  if( i == 0 ) 
   uerror( 53 );  /* empty character constant */
  if( i>(SZINT/SZCHAR) || ( (pflag||hflag)&&i>1) )
   uerror( 54 );  /* too many characters in character constant */
  }
 }

lxcom(){
 register c;
 /* saw a /*: process a comment */

 for(;;){

  switch( c = getchar() ){

  case EOF:
   uerror( 51 );  /* unexpected EOF */
   return;

  case '\n':
   ++lineno;

  default:
   continue;

  case '*':
   if( (c = getchar()) == '/' ) return;
   else ungetc( c ,stdin);
   continue;

# ifdef LINT
  case 'V':
   lxget( c, LEXLET|LEXDIG );
   {
    extern int vaflag;
    int i;
    i = yytext[7]?yytext[7]-'0':0;
    yytext[7] = '\0';
    if( strcmp( yytext, "VARARGS" ) ) continue;
    vaflag = i;
    continue;
    }
  case 'L':
   lxget( c, LEXLET );
   if( strcmp( yytext, "LINTLIBRARY" ) ) continue;
   {
    extern int libflag;
    libflag = 1;
    }
   continue;

  case 'A':
   lxget( c, LEXLET );
   if( strcmp( yytext, "ARGSUSED" ) ) continue;
   {
    extern int argflag, vflag;
    argflag = 1;
    vflag = 0;
    }
   continue;

  case 'N':
   lxget( c, LEXLET );
   if( strcmp( yytext, "NOTREACHED" ) ) continue;
   reached = 0;
   continue;
# endif
   }
  }
 }

double dcon,  /* last double read by the lexical analyzer */
 atof ();


CONSZ maxint = ~((unsigned)~0L << (SZINT-1));

yylex(){
 for(;;){

  register lxchar;
  register struct lxdope *p;
  register struct symtab *sp;
  int id;

  switch( (p=lxcp[(lxchar=getchar())+1])->lxact ){

  onechar:
   ungetc( lxchar ,stdin);

  case A_1C:
   /* eat up a single character, and return an opcode */

   yylval.intval = p->lxval;
   return( p->lxtok );

  case A_ERR:
   uerror( 55, lxchar ); /* illegal character: %03o (octal) */
   break;

  case A_LET:
   /* collect an identifier, check for reserved word, and return */
   lxget( lxchar, LEXLET|LEXDIG );
   if( (lxchar=lxres()) > 0 ) return( lxchar ); /* reserved word */
   if( lxchar== 0 ) continue;
   id = lookup( hash(yytext),
    /* tag name for struct/union/enum */
    (stwart&TAGNAME)? STAG:
    /* member name for struct/union */
    (stwart&(INSTRUCT|INUNION|FUNNYNAME))?SMOS:0 );
   sp = &stab[id];
   if( sp->sclass == TYPEDEF && !stwart ){
    stwart = instruct;
    yylval.nodep = mkty( sp->stype, sp->dimoff, sp->sizoff );
    return( TYPE );
    }
   stwart = (stwart&SEENAME) ? instruct : 0;
   yylval.intval = id;
   return( NAME );

  case A_DIG:
   /* collect a digit string, then look at last one... */
   lastcon = 0;
   lxget( lxchar, LEXDIG );
   switch( lxchar=getchar() ){

   case 'x':
   case 'X': /* SS. 26.feb.87 error repaired */
    if( yytext[0] != '0' || yytext[1] ) 
     uerror( 56 ); /* illegal hex constant */
    lxmore( lxchar, LEXHEX );
    /* convert the value */
    {
     register char *cp;
     for( cp = yytext+2; *cp; ++cp ){
      /* this code won't work for all wild character sets,
         but seems ok for ascii and ebcdic */
 /* SS. 8.apr.87 the next line was: lastcon <<= 4; */
      lastcon = (unsigned)lastcon <<4;
      if( isdigit( *cp ) ) lastcon += *cp-'0';
      else if( isupper( *cp ) ) lastcon += *cp - 'A'+ 10;
      else lastcon += *cp - 'a'+ 10;
      }
     }

   hexlong:
   /* criterion for longness for hex and octal const is that it fit within 0x7FFFFFFF */
   /* SS. 25.feb.87 it does not matter fits or not, there
   are no difference between long and int in case of KC */
   /*
    if( lastcon > maxint ) yylval.intval = 1;
    else 
   */
     yylval.intval = 0;
    goto islong;

   case '.':
    lxmore( lxchar, LEXDIG );

   getfp:
    if( (lxchar=getchar()) == 'e' || lxchar == 'E' ){ /* exponent */

   case 'e':
   case 'E':
     if( (lxchar=getchar()) == '+' || lxchar == '-' ){
      *lxgcp++ = 'e';
      }
     else {
      ungetc(lxchar,stdin);
      lxchar = 'e';
      }
     lxmore( lxchar, LEXDIG );
     /* now have the whole thing... */
     }
    else {  /* no exponent */
     ungetc( lxchar ,stdin);
     }
    dcon = atof (yytext);
    return( FCON );

   default:
    ungetc( lxchar ,stdin);
    if( yytext[0] == '0' ){
     /* convert in octal */
     register char *cp;
     for( cp = yytext+1; *cp; ++cp ){
  /* SS. 26.feb.87  give the warning: bad octal digit *cp */
      if( *cp > '7' )
       werror( 32, *cp );
  /* SS. 8.apr.87 the next line was: (unsigned)lastcon <<= 3;  */
      lastcon = (unsigned)lastcon <<3;
      lastcon += *cp - '0';
      }
     goto hexlong;
     }
    else {
     /* convert in decimal */
     register char *cp;
     for( cp = yytext; *cp; ++cp ){
      lastcon = lastcon * 10 + *cp - '0';
      }
     }

    /* decide if it is long or not (decimal case) */

    /* if it is positive and fits in 31 bits, or negative and
       and fits in 31 bits plus an extended sign, it is int; otherwise long */
    /* if there is an l or L following, all bets are off... */
    /* SS. 26.feb.87 does not matter long or int*/
    /*
     if( lastcon > maxint )
      yylval.intval = 1;
     else 
    */
      yylval.intval = 0;

   islong:
    /* finally, look for trailing L or l */
    if( (lxchar = getchar()) == 'L' || lxchar == 'l' ) yylval.intval = 1;
    else ungetc( lxchar ,stdin);
    return( ICON );
    }

  case A_DOT:
   /* look for a dot: if followed by a digit, floating point */
   lxchar = getchar();
   if( lxmask[lxchar+1] & LEXDIG ){
    ungetc(lxchar,stdin);
    lxget( '.', LEXDIG );
    goto getfp;
    }
   stwart = FUNNYNAME;
   goto onechar;

  case A_STR:
   /* string constant */
   lxmatch = '"';
   return( STRING );

  case A_CC:
   /* character constant */
   lxmatch = '\'';
   lastcon = 0;
   lxstr(0);
   yylval.intval = 0;
   return( ICON );

  case A_SL:
   /* / */
   if( (lxchar=getchar()) != '*' ) goto onechar;
   lxcom();
  case A_WS:
   continue;

  case A_NL:
   ++lineno;
   lxtitle();
   continue;

  case A_NOT:
   /* ! */
   if( (lxchar=getchar()) != '=' ) goto onechar;
   yylval.intval = NE;
   return( EQUOP );

  case A_MI:
   /* - */
   if( (lxchar=getchar()) == '-' ){
    yylval.intval = DECR;
    return( INCOP );
    }
   if( lxchar != '>' ) goto onechar;
   stwart = FUNNYNAME;
   yylval.intval=STREF;
   return( STROP );

  case A_PL:
   /* + */
   if( (lxchar=getchar()) != '+' ) goto onechar;
   yylval.intval = INCR;
   return( INCOP );

  case A_AND:
   /* & */
   if( (lxchar=getchar()) != '&' ) goto onechar;
   return( yylval.intval = ANDAND );

  case A_OR:
   /* | */
   if( (lxchar=getchar()) != '|' ) goto onechar;
   return( yylval.intval = OROR );

  case A_LT:
   /* < */
   if( (lxchar=getchar()) == '<' ){
    yylval.intval = LS;
    return( SHIFTOP );
    }
   if( lxchar != '=' ) goto onechar;
   yylval.intval = LE;
   return( RELOP );

  case A_GT:
   /* > */
   if( (lxchar=getchar()) == '>' ){
    yylval.intval = RS;
    return(SHIFTOP );
    }
   if( lxchar != '=' ) goto onechar;
   yylval.intval = GE;
   return( RELOP );

 /* SS. 26.feb.87  the A_EQ case part replaced with part from v6.0 */

  case A_EQ:
   /* = */
   switch( lxchar = getchar() ){

   case '=':
    yylval.intval = EQ;
    return( EQUOP );

   case '-':
   case '*':
   case '&':
    /* "ambiguous assignment: simple assign, unary op assumed" */
    werror( 2 );
    goto onechar;

   case '+':
   case '/':
   case '%':
   case '|':
   case '^':
   case '<':
   case '>':
    /* "old style assign-op causes syntax error" */
    werror( 33 );

   default:
    goto onechar;

    }

  default:
   cerror( 201, lxchar );  /* yylex error, char %03o (octal) */
   }

  /* ordinarily, repeat here... */
  cerror( 202 );  /* out of switch in yylex */

  }

 }


struct lxrdope {
 /* dope for reserved, in alphabetical order */

 char *lxrch; /* name of reserved word */
 short lxract; /* reserved word action */
 short lxrval; /* value to be returned */
 } lxrdope[] = {

 "asm",   AR_A,  0,
 "auto",  AR_CL, AUTO,
 "break", AR_RW, BREAK,
 "char",  AR_TY, CHAR,
 "case",  AR_RW, CASE,
 "continue", AR_RW, CONTINUE,
 "double", AR_TY, DOUBLE,
 "default", AR_RW, DEFAULT,
 "do",   AR_RW,  DO,
 "extern", AR_CL, EXTERN,
 "else",  AR_RW, ELSE,
 "enum",  AR_E,  ENUM,
 "for",   AR_RW, FOR,
 "float", AR_TY, FLOAT,
/*  "fortran", AR_CL, FORTRAN,  SS. 05.sep.86 replaced with modula */
 "goto",  AR_RW, GOTO,
 "if",   AR_RW,  IF,
 "int",   AR_TY, INT,
 "long",  AR_TY, LONG,
 "modula", AR_CL, FORTRAN,
 "return", AR_RW, RETURN,
 "register", AR_CL, REGISTER,
 "switch", AR_RW, SWITCH,
 "struct", AR_S, 0,
 "sizeof", AR_RW, SIZEOF,
 "short", AR_TY, SHORT,
 "static", AR_CL, STATIC,
 "typedef", AR_CL, TYPEDEF,
 "unsigned", AR_TY, UNSIGNED,
 "union", AR_U,  0,
 "void",  AR_TY, UNDEF, /* tymerge adds FTN */
 "while", AR_RW, WHILE,
 "",  0, 0, /* to stop the search */
 };

lxres() {
 /* check to see of yytext is reserved; if so,
 /* do the appropriate action and return */
 /* otherwise, return -1 */

 register c, ch;
 register struct lxrdope *p;

 ch = yytext[0];

 if( !islower(ch) ) return( -1 );

 switch( ch ){

 case 'a':
  c=0; break;
 case 'b':
  c=2; break;
 case 'c':
  c=3; break;
 case 'd':
  c=6; break;
 case 'e':
  c=9; break;
 case 'f':
  c=12; break;
 case 'g':
  c=14; break;   /* SS. 08.sep.86  position in table is changed*/
 case 'i':
  c=15; break;   /* SS. 08.sep.86  position in table is changed*/
 case 'l':
  c=17; break;   /* SS. 08.sep.86  position in table is changed*/
 case 'm':
  c=18; break;   /* SS. 08.sep.86  this line is added */
 case 'r':
  c=19; break;
 case 's':
  c=21; break;
 case 't':
  c=26; break;
 case 'u':
  c=27; break;
 case 'v':
  c=29; break;
 case 'w':
  c=30; break;

 default:
  return( -1 );
  }

 for( p= lxrdope+c; p->lxrch[0] == ch; ++p ){
  if( !strcmp( yytext, p->lxrch ) ){ /* match */
   switch( p->lxract ){

   case AR_TY:
    /* type word */
    stwart = instruct;
    yylval.nodep = mkty( (TWORD)p->lxrval, 0, p->lxrval );
    return( TYPE );

   case AR_RW:
    /* ordinary reserved word */
    return( yylval.intval = p->lxrval );

   case AR_CL:
    /* class word */
    yylval.intval = p->lxrval;
    return( CLASS );

   case AR_S:
    /* struct */
    stwart = INSTRUCT|SEENAME|TAGNAME;
    yylval.intval = INSTRUCT;
    return( STRUCT );

   case AR_U:
    /* union */
    stwart = INUNION|SEENAME|TAGNAME;
    yylval.intval = INUNION;
    return( STRUCT );

   case AR_E:
    /* enums */
    stwart = SEENAME|TAGNAME;
    return( yylval.intval = ENUM );
   case AR_A:
    /* asm */
    lxget( ' ', LEXWS );
    if( getchar() != '(' ) goto badasm;
 /*   lxget( ' ', LEXWS ); */
    if( getchar() != '"' ) goto badasm;
   for (;;) {
    while( (c=getchar()) != '"' ){
     if( /*c=='\n' ||*/ c==EOF ) goto badasm;
# ifndef LINT
     putchar(c);
# endif
     }
    if ((c=getchar()) != ')') {
     if (c == EOF) goto badasm;
     putchar ('"');putchar(c);
     continue;
     }
    break;
    }
 /*   lxget( ' ', LEXWS );
    if( getchar() != ')' ) goto badasm; */
# ifndef LINT
    putchar('\n');
# endif
    return( 0 );

   badasm:
    uerror( 62 );  /* bad asm construction */
    return( 0 );

   default:
    cerror( 203 );  /* bad AR_?? action */
    }
   }
  }
 return( -1 );
 }

extern int labelno;

lxtitle(){
 /* called after a newline; set linenumber and file name */

 register c, val;
 register char *cp, *cq;

 for(;;){  /* might be several such lines in a row */
  if( (c=getchar()) != '#' ){
   if( c != EOF ) ungetc(c,stdin);
   return;
   }

  lxget( ' ', LEXWS );
  val = 0;
  for( c=getchar(); isdigit(c); c=getchar() ){
   val = val*10+ c - '0';
   }
  ungetc( c, stdin );
  lineno = val;
  lxget( ' ', LEXWS );
  if( (c=getchar()) != '\n' ){
   for( cp=ftitle; c!='\n'; c=getchar(),++cp ){
    *cp = c;
    }
   *cp = '\0';
/* MK 22.Apr.87 Output file name if asflag */
   if (asflag) {
    printf("; File: %s\n", ftitle);
    linepnt = linebuf;
    }
   }
  }
 }

#define  NSAVETAB 4096
char *savetab;
int saveleft;

char *
savestr(cp) /* place string into permanent string storage */
 register char *cp;
{
 register int len;

 len = strlen(cp) + 1;
 if (len > saveleft) {
  saveleft = NSAVETAB;
  if (len > saveleft)
   saveleft = len;
  if( (savetab = (char *)malloc(saveleft)) == 0)
   cerror( 204 );  /* Ran out of memory (savestr) */
 }
 strncpy(savetab, cp, len);
 cp = savetab;
 savetab += len;
 saveleft -= len;
 return (cp);
}

/*
 * The definition for the segmented hash tables.
 */
#define  MAXHASH 20
#define  HASHINC 1013
struct ht {
 char **ht_low;
 char **ht_high;
 int ht_used;
} htab[MAXHASH];

char *
hash(s)    /* look for s in seg. hash tables, if not found, make new entry */
 char *s;
{
 register char **h;
 register i;
 register char *cp;
 struct ht *htp;
 int sh;

 /*
  * The hash function is a modular hash of
  * the sum of the characters with the sum
  * doubled before each successive character
  * is added.
  */
 cp = s;
 i = 0;
 while (*cp)
  i = i*2 + *cp++;
 sh = (i&077777) % HASHINC;
 cp = s;
 /*
  * There are as many as MAXHASH active
  * hash tables at any given point in time.
  * Look through each table for name.
  * The search starts with the first table
  * and continues through the active tables
  * as necessary.
  */
 for (htp = htab; htp < &htab[MAXHASH]; htp++) {
  if (htp->ht_low == 0) {
   register char **hp =
       (char **) calloc(sizeof (char **), HASHINC);
   if (hp == 0)
    cerror( 205 );  /* ran out of memory (hash) */
   htp->ht_low = hp;
   htp->ht_high = hp + HASHINC;
  }
  h = htp->ht_low + sh;
  /*
   * quadratic rehash increment
   * starts at 1 and incremented
   * by two each rehash.
   */
  i = 1;
  do {
   if (*h == 0) {
    if (htp->ht_used > (HASHINC * 3)/4)
     break;
    htp->ht_used++;
    *h = savestr(cp);
    return (*h);
   }
   if (**h == *cp && strcmp(*h, cp) == 0)
    return (*h);
   h += i;
   i += 2;
   if (h >= htp->ht_high)
    h -= HASHINC;
  } while (i < HASHINC);
 }
 cerror( 206 );  /* ran out of hash tables */
}

int cdebug, globsize, globlab, stklab, globno, fnr;

