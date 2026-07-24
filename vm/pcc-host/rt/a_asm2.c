
/*
 * 23.Jan.87  liw -<const>  => li <const> / neg; jumps to next instruction
 * 25.Jan.87  jumps to jumps
 * 26.Jan.87  entr M%d
 * 02.Feb.87  jump optimization without flags
 * 02.Feb.87  version for new linker: LPC & header
 * 04.Feb.87  loop count for jumps; exit on loop
 * 09.Feb.87  store/load optimization
 * 10.Feb.87  KRONOS 2.5 proc.table -> #ifdef NEW
 * 11.Feb.87  no more 0-procedure labels
 * 19.Mar.87  -u (user program) => no optimization
 * 31.Mar.87  time in header
 * 04.Apr.87  BYTE macro
 * 05.Apr.87  header changed (time now no.2, 2 empty words)
 * 04.May.87  bug in jump optimization: conditional jumps
 * 04.Aug.87  bug in prtab size: 128 (should be 256!); NEW -> Kronos2/5
 * 17.Sep.87  new directives (.alb, .ale)
 * 18.sep.87  multiple files
 * NEWASM
 * 24.Sep.87  void functions marked in object files
 * 26.Sep.87  statics again: CheckExternals() moved to Pass1, some stuff added
 *       to import()
 * 09.Oct.87  entr 2 moved from PCC to Pass2
 * 22.Oct.87  entr 0 check
 * 27.Oct.87  no more entr 2 for whole module
 * 09.Nov.87  Kronos time
 * 19.Nov.87  stksize+5 in header 
 * 30.Dec.87  ENTR/ENTR optimization
 * 02.Jan.87  ENTR/ENTR rewritten (O_OPTE)
 * 
 * 05.Feb.88   New Version (LPC et al., cf. asm.c)
 * 12.Feb.88  pool directly to object file
 */

#include <stdio.h>
#include "asm.h"
#include "mkinstr.h"

#define  max(a,b)  ((a) > (b) ? (a) : (b))
#define abs(x)   ((x) < 0 ? -(x) : (x))

/* operand cookies */

#define  O_LEA 00 /* LEA   */
#define  O_LEW 01 /* LEW   */
#define O_SEW 02 /* SEW    */
#define  O_VAR 04 /* LEA, LEW, SEW */
#define  O_FUN 010 /* CX, LPC  */
#define  O_DLAB  020 /* LSTA   */
#define  O_JLAB1 040 /* JFL, JFLC, JBL, JBLC */
#define  O_JLAB2 0100 /* JFS, JFSC, JBS, JBSC */
#define  O_SPECIAL 0200  /* 0xFF - li M%d, entr  */
#define  O_MOD 0400 /* LEW, SEW   */
#define O_OPT 01000 /* store/load optim. */
#define O_JCND  02000 /* JFLC, JBLC  */
#define O_OPTE  04000 /* ENTR/ENTR  */

#define LD_OFS 16

extern STAB globvar [], globfun [], extvar [], extfun [];
extern int gv, gf, ev, ef;
extern int extv, extf, imptno;

extern DLAB * dlabels;
extern int dl;

extern JLAB * jlabels;
extern int jl;

extern FTAB funtab[];
extern int maxfun;

extern int loc, fsize, poolsize, codesize, stksize, nmain;
extern void error();

extern int verbose, kron2, jump0, jumpfs, jumpjj, pcc,
  stld0, stld1, entr, nerrors;

extern char fname[], ofname[];

extern FILE * fpcode, * fpinit, * fpobj, * fpallo;

static int fdobj, pc;
static char * buf1, * buf2, *src, *dest;
static int bufsize;
static int exptno, exptvar, exptfun, modbyte;

static int * lab, * lastlab, labno, curlen;
static FTAB * curftn;

int hd [HDSIZE];
static int prtab[PRTABSIZE], procsize;

#define  SETOFF(x,y)  (x % y) ?   (x/y + 1) * y : x


/* Functions */

 void Pass2 ();
static void header ();
static void export ();
static void import ();
static void code ();
static void proctab ();
static int codereset ();
static void move ();
static void moveb ();
static void moved ();
static void movew ();
static void skip ();
static int get2 ();
static void newjumps ();
static void correct ();
static void jumps ();
static void inittable ();


/*
 *  P A S S   2
 *  ===========
 */

void  Pass2 ()

{
char * strcat (), * malloc();

 if (verbose > 1)
  fprintf (stderr, "Pass 2  ---  Optimization\n");

 maxfun = SETOFF (maxfun, 4); 

 bufsize = max (poolsize, maxfun);
 /* host-port: the 0th procedure (module init) reads its allocs+inits
    (funtab[0].length, DATA not code) at a small preamble offset, which
    maxfun (max code size) does not cover -> heap overflow on big-data
    modules (found via ASan on match.c). Size the buffer for it too. */
 if (funtab[0].length + 64 > bufsize) bufsize = funtab[0].length + 64;
 if ((buf1 = malloc (bufsize)) == (char *)0 ||
     (buf2 = malloc (bufsize)) == (char *)0) {
  unlink(ofname);
  error ("Not enough memory\n\n");
  }

 fflush (fpcode);
 fflush (fpinit);
 fflush (fpallo);
 fflush (fpobj);
 fdobj = fileno(fpobj);

 fsize = HDSIZE * 4 + poolsize;
 lseek(fdobj, (long)fsize, 0);
 /* reuse the space of jump labels table */
 lab = (int *) jlabels;
 * lab = 0;

 inittable();
 export ();
 import ();
 code ();
 proctab ();
 header ();

#if excII
 fpobj->_flag |= _IONBF;
 fclose(fpobj);
#else
 close (fdobj);
#endif
 if (verbose > 1)
  fprintf (stderr, "\n");
}

/*
static void pool ()
  ----
    Write pool to object file 
{
int fd = fileno (fppool);

 lseek (fd, 0L, 0);
 read  (fd, buf1, (unsigned)poolsize);
 write (fdobj, buf1, (unsigned)poolsize);
 fsize += poolsize;
}
*/

static void header ()
/*  ------
    Fill and write out object file header */
{
extern long time();

 hd[0] = kron2 ? OBJ2_VERSION : OBJ25_VERSION;
 hd[1] = (int) time ((long *) 0);
#if unix
 hd[1] -= MAGICTIME;
#endif
 hd[2] = exptvar; /* export variables */
 hd[3] = exptfun; /* export functions */
 hd[4] = extv;   /* import variables */
 hd[5] = extf;   /* import functions */
 hd[6] = poolsize / 4; /* poollen */
 hd[7] = procsize / 4; /* proclen */
 hd[8] = codesize / 4;   /* codelen */
 hd[9] = gf + 1;  /* funno (incl. statics) */
 hd[10] = gv + 1;  /* varno (incl. statics) */
 hd[11] = modbyte;
 hd[12] = stksize + 5; /* stack size */
 hd[13] = 0;
 hd[14] = 0;
 lseek (fdobj, 0L, 0);
 write (fdobj, (char *) & hd[0], HDSIZE * 4);
}

static void export ()
/*  ------
    Write export symbols to object file */
{
int i;
STAB * st;

 for (i = 2; i <= gv; i ++) {
  st = & globvar[i];
  if (!st->flag) {
   exptvar ++;
   st->flag = i;
   write (fdobj, (char *) st, sizeof(STAB));
   }
  }
 for (i = 1; i <= gf; i ++) {
  st = & globfun[i];
  if (st->flag & STAT)
   continue;
  exptfun ++;
  st->flag = st->flag & VOID ? -i : i;
  /* for global void functions, the number is negative */
  write (fdobj, (char *) st, sizeof(STAB));
  }
 exptno = exptvar + exptfun;
 fsize += (exptno * sizeof (STAB));
}

static void import ()
/*  ------
    Write import symbols to object file.
    The problem with these two tables is that if each file needs printf 
    then it will appear in the import list once for each file. We could
    search the tables here, but is it worth the time ?
    What is still worse: ASM can not distinguish between void & non-void
    calls to the same function from different files; this remains to be
    handled by LINK (with the cost of information loss as to which file
    actually had the wrong calls). */
{
STAB * p0, * p1;

 p1 = & extvar[ev] + 1;
 for (p0 = & extvar[1]; p0 < p1; p0 ++)
  if (p0->flag >= 0) {
   p0->flag = extv ++;
   write (fdobj, p0->name, MAXIDENT + 1);
   }
 imptno = extv;
 p1 = & extfun[ef] + 1;
 for (p0 = & extfun[1]; p0 < p1; p0++)
  if (p0->flag >= 0) {
   if (p0->flag & VOID)
  /* void external functions have '\1' at end of name */
    p0->name[MAXIDENT] = '\1';
   p0->flag = imptno ++;
   write (fdobj, p0->name, MAXIDENT + 1);
   }
 extf = imptno - extv;
 fsize += (imptno * (MAXIDENT + 1));
}

static void code ()
/*  ----
    process code of all functions and write it to object file */
{
int i, fd, offset = 0;

 if (kron2)
  procsize = ((gf + 2) / 2) * 4;
 else procsize = (gf + 1) * 4;
 write (fdobj, (char *) & prtab[0], (unsigned)procsize);

 fd = fileno (fpcode);
 lseek (fd, 0L, 0);
 for (i = 1; i <= gf; i ++) {
  curftn = & funtab[i];
  read (fd, buf1, (unsigned)(curlen = curftn->length));
  if (labno = curftn->labno)
   read (fd, (char *)(lab+1), (unsigned)(labno * 4));
  lastlab = lab + labno;
  correct ();
  jumps ();
  curftn->length = codereset();
  write (fdobj, buf1, (unsigned)curftn->length);
  codesize += curftn->length;
  if (verbose > 1)
   fprintf(stderr, "#");
  }
 
 /* procedure no. 0 - !! */

 if (nmain) modbyte = codesize - curftn->length + 15; /* ?? */
 if (pcc) {
  i = 0;
  buf1[i++] = (char) ENTR; /* SS. 19.Nov.87 */
  buf1[i++] = (char) 1;

  if (gv > 1) {
   buf1[i++] = (char) LGA;
   buf1[i++] = (char) 2;
   if (gv < 16)
    buf1[i++] = (char) (LI0 + gv - 1);
   else {
    buf1[i++] = (char) LIB;
    buf1[i++] = (char) (gv - 1);
    }
   buf1[i++] = (char) CX; /* _zero */
   buf1[i++] = (char) 0;
   buf1[i++] = (char) 1;
   }
  offset = i;
  }
 curftn = & funtab[0];
 /* next, read initial allocations file */
 fd = fileno (fpallo);
 lseek (fd, 0L, 0);
 i = read (fd, buf1 + offset, (unsigned)(curlen = curftn->length));
 /* now, read standard initializations file */
 fd = fileno (fpinit);
 lseek (fd, 0L, 0);
 read (fd, buf1 + i + offset, (unsigned)(curlen - i));
 labno = 0;
 curlen += offset;
 correct();  /* Note:  no jumps in the 0th procedure */
 curftn->length = codereset();
 write (fdobj, buf2, (unsigned)curftn->length);
 codesize += curftn->length;
 if (verbose > 1)
  fprintf (stderr, "#");
}

static void proctab ()
/*  -------
    Fill procedure table and write it to object file */
{
int num, j, entrsize;

 num = procsize;
 entrsize = kron2 ? 2 : 4; /* entry size in procedure table */
 for (j = 1; j <= gf; j ++) {
  prtab[j] = num;
  num += funtab[j].length;
  }
 prtab[0] = num;
 lseek (fdobj, (long) fsize, 0);
 for (j = 0; j < procsize / entrsize; j ++)
  write (fdobj, (char *) & prtab[j], (unsigned)entrsize);
 fsize += (codesize + procsize);
}

static int codereset ()
/*  ---------
    Add  1..3  zeroes to end of function if necessary */
{
int num;

 if (num = (curlen % 4)) {
  num = 4 - num;
  while (num --)
   * dest ++ = 0;
  curlen = SETOFF (curlen, 4);
  }
 return (curlen);
}


/* Code modifications in Pass 2 :
 *  
 *  1. Function code is read from temporary file to buf1.
 *  2. It is rewritten from buf1 to buf2 with right external references
 *     (CX, LPC, LEW, LEA, SEW) being inserted (and some other minor
 *     changes, cf. correct()).
 *  3. Code in buf2 is then scanned to see which long jumps can be re-
 *     placed by short ones. This is done repetitively until no more
 *     replacements can be made.
 *  4. Code is rewritten from buf2 back to buf1 with correct jump
 *     values being inserted.
 *  5. Finally, code in buf2 is placed in object file.
 *
 *     Functions move(), moveb(), moved(), movew() perform transfers
 *  between buffers (source and destination). Pc denotes current code
 *  length in destination buffer.
 *     Functions skip(), get2() work on source buffer.
 *  NOTE: movew() & moved() put low byte first!
 */

static void move (num)
/*  ----  */
{
 pc += num;
 while (num --)
  * dest ++ = * src ++;
}

static void moveb (c)
/*  -----  */
{
 * dest ++ = c;
 pc ++;
}

static void moved (c)
/*  -----  */
{
 moveb (c & 0377);
 moveb ((c >> 8) & 0377);
}

static void movew (c)
/*  -----  */
{
 moveb (c & 0377);
 moveb ((c >> 8) & 0377);
 moveb ((c >> 16) & 0377);
 moveb ((c >> 24) & 0377);
}

static void skip (num)
/*  ----  */
{
 while (num --)
  src ++;
}

static int get2 ()
/*  ----  */
{
int num;

 num = BYTE(* src ++);
 num = (num << 8)  + BYTE(* src ++);
 return (num);
}

/* Instruction table for Pass2 optimizations */
struct instrtab {
  int flag;
  int len;
  };

typedef  struct  instrtab INSTR;

static INSTR instr[256];

static void newjumps (decr)
/*  --------
    Count new jump values after code has been cut at <pc> by <decr> bytes */
{
int * jp;

 if (labno) {
  jp = lastlab;
  while (jp > lab) {
   if (*jp > pc) (* jp) -= decr;
   jp --;
   }
  }
}

static int nojumps (l)
/*  ------- 
    Return 1 if there are no jumps to the location pc = <l>, 0 otherwise */
int l;
{
int * jp;

 jp = lastlab;
 while (jp > lab) {
  if (* jp == l)
   return (0);
  jp --;
  }
 return (1);
}

static void correct()
/*  -------
    Move code from buf1 to buf2 and
 1) do store/load optimization,
 2) check LPC, CX, SEW, LEW, LEA for referring to objects defined
    in the module,
 3) insert values for data labels,
 4) insert values for special labels (LI & ENTR),
 5) eliminate jumps to jumps and jumps to next instruction */
{
int c, fl, num, val, decr, jpc, cc, jcount, len;
int  op1, op2;
INSTR * i;
char * end, * oldsrc;

 src = buf1;
 dest = buf2;
 end = & buf1[curlen];
 pc = 0;
 while (src < end) {

  c = BYTE(* src ++);
  i = & instr [c];
  len = i->len;
  if (!(fl = i->flag)) {
   moveb (c);
   if (len)
    move (len);
   continue;
   }
/*
  if (fl == O_OPTE && pcc) {
   op1 = BYTE(*src);
   cc  = BYTE(*(src + 1));
   if (cc == ENTR && nojumps (pc + 2)) {
    op2 = BYTE(*(src + 2));
    if ((num = op1 + op2) < 256) {
     moveb(ENTR);
     moveb(num);
     entr ++;
     src += 3;
     newjumps(2);
     continue;
     }
    }
   moveb (ENTR);
   moveb (op1);
   continue;
   }
*/
  if (fl == O_OPT && pcc) {
   if (len) {
    op1 = BYTE(*src);
    cc  = BYTE(*(src + 1));
    op2 = BYTE(*(src + 2));
    if (op1 == op2 && c == (cc + LD_OFS) &&
      nojumps (pc + 2)) {
     moveb(COPT);
     moveb (c);
     moveb(op1);
     src += 3;
     stld1 ++;
     newjumps(1);
     }
    else {
     moveb (c);
     moveb (op1);
     src ++;
     }
    continue;
    }
   else {
    cc = BYTE(* src);
    if (c == (cc + LD_OFS) && nojumps(pc + 1)) {
     moveb (COPT);
     moveb (c);
     src ++;
     stld0 ++;
     }
    else moveb (c);
    continue;
    }
   }
  if (fl & O_FUN) {
   num = get2 ();
   if (c == LPC) {
       if (num < 256) {
    moveb (LPC);
    moved (num);
    continue;
    }
       num -= 255;
         }
   if ((val = extfun[num].flag) < 0) {
    val = abs (val);
    if (c == LPC) {
     moveb (LPC);
     moved (val);
     continue;
     }
    if (val < 16) {
     decr = 2;
     moveb (CL0 + val);
     }
    else {
     moveb (CL);
     moveb (val);
     decr = 1;
     }
    newjumps (decr);
    }
   else {
    moveb (c);
    if (c == LPC) moved (255 + val);
    else moved (val);
    }
   continue;
   }
  if (fl & O_VAR) {
   static struct {
    int op1, op2; } three [3] =
    { LGA, 0,
      LGW, LGW2-2,
      SGW, SGW2-2};
   num = get2();
   if ((val = extvar[num].flag) < 0) {
    val = abs (val);
    if ((fl & O_MOD) && val < 16) {
     decr = 2;
     moveb(three[fl&03].op2 + val);
     }
    else {
     moveb(three[fl&03].op1);
     moveb(val);
     decr = 1;
     }
    newjumps (decr);
    }
   else {
    moveb (c);
    moved (val);
    }
   continue;
   }
  if (fl & O_DLAB) {
   num = get2();
   moveb (c);
   moved (dlabels[num].val);
   continue;
   }
  if (fl & O_SPECIAL) {
   int neg;
   c = BYTE(* src ++);
   src ++;
   num = get2();
   if (c == ENTR) {

    num = dlabels[num].val & 0377;
    if (!num)
     newjumps(5);
    else {
     moveb(ENTR);
     moveb(num);
     newjumps (3);
     }
    continue;
/*
    int  new = 3;
    val = 0;
    cc = BYTE(* src);
    if (cc == ENTR) {
        val = BYTE(*++src);
        new = 5;   3 from 1st, 2 from 2nd 
        src ++;
        entr ++;
        }
    val += (dlabels[num].val & 0377);
    if (!val)
     newjumps(new + 2);
    else {
     moveb(ENTR);
     moveb(val);
     newjumps(new);
     }
    continue;
*/
    }
 /* Make negative constants into li / neg */
   val = dlabels[num].val;
   neg = (val < 0);
   val = abs (val);
   if (val > 65535) {
    moveb (LIW);
    movew (val);
    }
   else {
    if (val < 16) {
     moveb (LI0 + val);
     decr = 4;
     }
    else if (val < 256) {
     moveb (LIB);
     moveb (val);
     decr = 3;
     }
    else {
     moveb (LID);
     moved (val);
     decr = 2;
     }
    if (neg) decr --;
    newjumps (decr);
    }
   if (neg) moveb (NEG);
   continue;
   }

  if (!pcc) { /* handle all other cases */
   moveb (c);
   if (len)
    move (len);
   continue;
   }

 /* JUMPS OPTIMIZATION:  eliminate 
    1: jumps to jumps 
    2: jumps to next instruction that are generated by 
   a) empty else's 
   b) do {
     ...
     continue;
         } while (sth); 
   c) ??  */

  num = get2();
  jpc = * (lab + num);
  oldsrc = src;
  jcount = 0;
  cc = c;
  for (;;) {
  /* "Jump" in the code until a non-jump instruction is reached */
   if (++ jcount == 200 || /* awfully long chain */
       jpc == pc)      /* jump loops */
 error("\nERROR : 'jump loops' in assembler. Can't optimize - sorry!\n");
   if (jpc - pc == 3) {  /* jumps to next instruction */
       if (instr[cc].flag & O_JCND) {
    moveb (DROP);
    newjumps (2);
    }
       else newjumps (3);
       jump0 ++;
       goto next;
       }
   src = (jpc > pc) ? oldsrc + (jpc-pc) - 3 :
        buf2 + jpc;
   cc = BYTE(* src ++);
   if (instr[cc].flag == O_JLAB1) {
    /* note: not for conditional jumps */
    jumpjj ++;
    num = get2();
    jpc = * (lab + num);
    continue;
    }
   break;
   }
  /* instruction code may have changed: F -> B & v.v. */
  if (jpc > pc) {
   if (c == JBLC || c == JBL)
    c -= 4;
   }
  else {
   if (c == JFLC || c == JFL)
    c += 4;
   }
  moveb (c);
  moveb ((num >> 8) & 0377);
  moveb (num & 0377);
next:  src = oldsrc;
  continue;
  }
 curlen = pc;
}
 
static void jumps()
/*  -----
    Scan function code repetitively until all long jumps are replaced with
    short ones where possible; write back to buf1 and insert actual jump
    values */
{
char * end;
INSTR * i;
int flag, num, fl, c, val;

 if (!labno) {
  src = buf2;
  dest = buf1;
  num = curlen;
  while (num --)
   * dest ++ = * src ++;
  return;
  }
 end = & buf2[curlen];
 do {
 /* scan function code until flag = FALSE, i.e. no replacements
    were made during the last iteration; note that this is always
    due to happen at the cost that pairs of dependent on each
    other jumps may be left unchanged */
  flag = 0;
  src = buf2;
  pc = 0;
  while (src < end) {
   c = BYTE(* src ++);
   pc ++;
   i = & instr[c];
   if (i->flag & O_JLAB1) {
    num = get2();
    num = *(lab + num) - pc -1;
    if (abs(num) <= 255) {
     jumpfs ++;
     * (src - 3) = c + 2;
     flag = 1;
     newjumps (1);
     pc += 1;
     continue;
     }
    pc += 2;
    continue;
    }
   if (i->flag & O_JLAB2) {
    skip (2);
    pc += 1;
    continue;
    }
   if (num = i->len) {
    skip (num);
    pc += num;
    }
   }
  }
 while (flag);

 /* write back to buf1 with correct jump values */

 src = buf2;
 dest = buf1;
 pc = 0;
 while (src < end) {
  c = BYTE(* src ++);
  i = & instr[c];
  if (!(fl = i->flag)) {
   moveb (c);
   if (i->len)
    move (i->len);
   continue;
   }
  if (fl & O_JLAB2) {
   num = get2();
   val = *(lab + num) - pc - 2;
   moveb (c);
   moveb (abs(val));
   continue;
   }
  if (fl & O_JLAB1) {
   num = get2();
   val = *(lab + num) - pc - 3;
   moveb (c);
   moved (abs(val));
   continue;
   }
  /* other flags */
  moveb (c);
  move (i->len);
  }
 curlen = pc;

}

static struct {
  int code, flag, len; } tab [30] = {

 LEA, O_LEA|O_VAR,  2,
 LEW, O_LEW|O_VAR|O_MOD, 2,
 SEW, O_SEW|O_VAR|O_MOD, 2,
 CX, O_FUN,   2,
 LPC, O_FUN,   2,
 0xFF, O_SPECIAL,  4,
 JFL, O_JLAB1,   2,
 JFLC, O_JLAB1|O_JCND,   2,
 JBL, O_JLAB1,   2,
 JBLC, O_JLAB1|O_JCND,   2,
 JFS, O_JLAB2,   2,
 JFSC, O_JLAB2,   2,
 JBS, O_JLAB2,   2,
 JBSC, O_JLAB2,   2,
 LSTA, O_DLAB,    2,
 LIB, 0,   1,
 LID, 0,   2,
 LIW, 0,   4,
 LLA, 0,   1,
 LGA, 0,   1,
 LSA, 0,   1,
 LLW, 0,   1,
 LGW, 0,   1,
 LSW, 0,   1,
 SLW, 0,   1,
 SGW, 0,   1,
 SSW, 0,   1,
 FFCT, 0,   1,
 ENTR, /*O_OPTE*/ 0,  1,
 CL, 0,    1};

static void inittable ()
/*  ---------
   Initialize instruction table <instr> for Pass2 optimizations */
{
int i, num;

 for (i = 0; i < 30; i ++) {
  num = tab[i].code;
  instr[num].len = tab[i].len;
  instr[num].flag = tab[i].flag;
  }
 for (i = SLW4; i <= SLW0F; i ++)
  instr[i].flag = O_OPT;
 for (i = SGW2; i <= SGW0F; i ++)
  instr[i].flag = O_OPT;
 instr[SLW].flag = O_OPT;
 instr[SGW].flag = O_OPT;
}


