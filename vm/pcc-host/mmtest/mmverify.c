/* mmverify.c -- a minimal Metamath proof verifier.
 *
 * Written in 1980s K&R C (no ANSI prototypes, no const/void*) so it compiles
 * with the 1988 Kronos "pcc".  Uses only the tiny Kronos clib:
 *   fopen fgetc feof fclose  malloc realloc free  printf  strcmp strcpy strlen
 * Handles $c $v $f $e $d $a $p ${ $} $( $), normal AND compressed proofs,
 * mandatory frames, substitution and disjoint-variable checks.
 *
 * Usage:  mmverify file.mm
 */

#include "stdio.h"

extern char *malloc();
extern char *realloc();
extern void  free();
extern void  exit();

/* ------------------------------------------------------------- growable int */
/* We use a lot of int arrays that grow; a tiny helper keeps the code short. */

char *xalloc(n) int n; {
    char *p;
    p = malloc((unsigned)n);
    if (p == (char *)0) { printf("?out of memory (%d bytes)\n", n); exit(1); }
    return p;
}
char *xrealloc(p, n) char *p; int n; {
    if (p == (char *)0) return xalloc(n);   /* Kronos realloc(NULL) may not work */
    p = realloc(p, (unsigned)n);
    if (p == (char *)0) { printf("?out of memory (%d bytes)\n", n); exit(1); }
    return p;
}

/* --------------------------------------------------------------- string pool */
char *spool; int spTop, spCap;
int intern_str(s) char *s; {           /* copy s into pool, return offset */
    int n, off;
    n = strlen(s) + 1;
    if (spTop + n > spCap) {
        spCap = (spTop + n) * 2 + 1024;
        spool = xrealloc(spool, spCap);
    }
    off = spTop;
    strcpy(spool + off, s);
    spTop += n;
    return off;
}

/* ------------------------------------------------------------ symbol table */
/* Math symbols (constants + variables). Interned via a hash table. */
int  *symName;    /* offset in spool          */
char *symVar;     /* 1 if variable, 0 const   */
int   nsym, symCap;
int  *symHash; int symHashSz;      /* hash -> sym index+1, linear probe */

int hashstr(s) char *s; {
    unsigned h; h = 0;
    while (*s) h = h * 31 + (unsigned)(*s++);
    return (int)(h & 0x7fffffff);
}
int findsym(s) char *s; {          /* return sym index or -1 */
    int h, i;
    h = hashstr(s) & (symHashSz - 1);
    for (;;) {
        i = symHash[h];
        if (i == 0) return -1;
        if (strcmp(spool + symName[i - 1], s) == 0) return i - 1;
        h = (h + 1) & (symHashSz - 1);
    }
}
void symrehash() {
    int i, h, ns;
    ns = symHashSz * 2;
    free((char *)symHash);
    symHash = (int *)xalloc(ns * (int)sizeof(int));
    memset((char *)symHash, 0, ns * (int)sizeof(int));
    symHashSz = ns;
    for (i = 0; i < nsym; i++) {
        h = hashstr(spool + symName[i]) & (symHashSz - 1);
        while (symHash[h]) h = (h + 1) & (symHashSz - 1);
        symHash[h] = i + 1;
    }
}
int addsym(s, isvar) char *s; int isvar; {
    int h, off;
    if (nsym + 1 > symCap) {
        symCap = symCap * 2 + 256;
        symName = (int *)xrealloc((char *)symName, symCap * (int)sizeof(int));
        symVar  = (char *)xrealloc(symVar, symCap);
    }
    if (nsym * 3 > symHashSz * 2) symrehash();
    off = intern_str(s);
    symName[nsym] = off;
    symVar[nsym]  = (char)isvar;
    h = hashstr(s) & (symHashSz - 1);
    while (symHash[h]) h = (h + 1) & (symHashSz - 1);
    symHash[h] = nsym + 1;
    return nsym++;
}

/* -------------------------------------------------------------- statements */
/* Each labelled statement ($f $e $a $p). Hypotheses and assertions alike.  */
#define S_FLOAT 1
#define S_ESS   2
#define S_AXIOM 3
#define S_PROV  4

int  *stType;
int  *stLabel;     /* offset in spool */
int  *stMathOff;   /* into mstr[]  */
int  *stMathLen;
int  *stFrHypOff;  /* into frhyp[] (assertions only) */
int  *stFrHypLen;
int  *stFrDvOff;   /* into frdv[] (pairs, 2 ints each) */
int  *stFrDvLen;   /* number of pairs */
int   nst, stCap;

int  *mstr;  int mstrTop, mstrCap;      /* math strings (symbol ids) */
int  *frhyp; int frhypTop, frhypCap;    /* frame hyp lists (stmt indices) */
int  *frdv;  int frdvTop, frdvCap;      /* frame dv pairs (2 ints) */

/* label hash -> stmt index+1 */
int  *labHash; int labHashSz, nlab;

int findlab(s) char *s; {
    int h, i;
    h = hashstr(s) & (labHashSz - 1);
    for (;;) {
        i = labHash[h];
        if (i == 0) return -1;
        if (strcmp(spool + stLabel[i - 1], s) == 0) return i - 1;
        h = (h + 1) & (labHashSz - 1);
    }
}
void labrehash() {
    int i, h, ns;
    ns = labHashSz * 2;
    free((char *)labHash);
    labHash = (int *)xalloc(ns * (int)sizeof(int));
    memset((char *)labHash, 0, ns * (int)sizeof(int));
    labHashSz = ns;
    for (i = 0; i < nst; i++) {
        if (stLabel[i] < 0) continue;
        h = hashstr(spool + stLabel[i]) & (labHashSz - 1);
        while (labHash[h]) h = (h + 1) & (labHashSz - 1);
        labHash[h] = i + 1;
    }
}
int newstmt() {
    if (nst + 1 > stCap) {
        stCap = stCap * 2 + 256;
        stType   = (int *)xrealloc((char *)stType,   stCap*(int)sizeof(int));
        stLabel  = (int *)xrealloc((char *)stLabel,  stCap*(int)sizeof(int));
        stMathOff= (int *)xrealloc((char *)stMathOff,stCap*(int)sizeof(int));
        stMathLen= (int *)xrealloc((char *)stMathLen,stCap*(int)sizeof(int));
        stFrHypOff=(int *)xrealloc((char *)stFrHypOff,stCap*(int)sizeof(int));
        stFrHypLen=(int *)xrealloc((char *)stFrHypLen,stCap*(int)sizeof(int));
        stFrDvOff =(int *)xrealloc((char *)stFrDvOff, stCap*(int)sizeof(int));
        stFrDvLen =(int *)xrealloc((char *)stFrDvLen, stCap*(int)sizeof(int));
    }
    stType[nst]=0; stLabel[nst]=-1; stMathOff[nst]=0; stMathLen[nst]=0;
    stFrHypOff[nst]=0; stFrHypLen[nst]=0; stFrDvOff[nst]=0; stFrDvLen[nst]=0;
    return nst++;
}
void setlabel(st, s) int st; char *s; {
    int h;
    stLabel[st] = intern_str(s);
    if (nlab * 3 > labHashSz * 2) labrehash();
    h = hashstr(s) & (labHashSz - 1);
    while (labHash[h]) h = (h + 1) & (labHashSz - 1);
    labHash[h] = st + 1;
    nlab++;
}

/* --------------------------------------------------------------- scope */
int *actHyp; int actHypTop, actHypCap;   /* active $f/$e stmt indices */
int *actDv;  int actDvTop,  actDvCap;    /* active dv pairs (2 ints) */
int *scHyp;  int *scDv; int scTop, scCap;/* scope marks */

void pushi(pp, tp, cp, v) int **pp, *tp, *cp, v; {
    if (*tp + 1 > *cp) { *cp = *cp*2 + 64; *pp = (int *)xrealloc((char *)*pp, *cp*(int)sizeof(int)); }
    (*pp)[(*tp)++] = v;
}

/* ------------------------------------------------------------- tokenizer */
FILE *fp;
char tok[8192];
int  toklen;
int  pushedback;
/* big scratch buffers kept OUT of main's stack frame (the Kronos default
   program stack is only ~8 KB, and a few call frames overflow it otherwise) */
char gLab[8192];    /* current label */
int  gVs[512];      /* $d variable list */

int rawtok() {                 /* read one whitespace-delimited token; 1, or 0 at EOF */
    int c, n;
    do { c = getc(fp); } while (c==' '||c=='\t'||c=='\n'||c=='\r'||c=='\f');
    if (c == EOF) { toklen = 0; return 0; }
    n = 0;
    while (c!=EOF && c!=' ' && c!='\t' && c!='\n' && c!='\r' && c!='\f') {
        if (n < (int)sizeof(tok)-1) tok[n++] = (char)c;
        c = getc(fp);
    }
    tok[n] = 0; toklen = n;
    return 1;
}
int gettok() {                 /* like rawtok but skips $( .. $) comments */
    for (;;) {
        if (!rawtok()) return 0;
        if (strcmp(tok, "$(") == 0) {           /* comment: skip to $) ITERATIVELY.
             Metamath comments do NOT nest, so inner '$(' is just text -- use rawtok,
             not gettok, or set.mm's header comment (full of $( examples) recurses
             the P-stack to death. */
            for (;;) {
                if (!rawtok()) { printf("?unterminated comment\n"); exit(1); }
                if (strcmp(tok, "$)") == 0) break;
            }
            continue;
        }
        return 1;
    }
}

/* ----------------------------------------------------- math-string builder */
int mbuf[65536]; int mbn;        /* scratch for reading a math string */

/* read symbols until "$." or "$=" ; store into mbuf[], return terminator:
   0 = "$."   1 = "$="   */
int readmath() {
    int id;
    mbn = 0;
    for (;;) {
        if (!gettok()) { printf("?EOF in statement\n"); exit(1); }
        if (strcmp(tok,"$.")==0) return 0;
        if (strcmp(tok,"$=")==0) return 1;
        id = findsym(tok);
        if (id < 0) { printf("?undeclared math symbol '%s'\n", tok); exit(1); }
        if (mbn >= (int)(sizeof(mbuf)/sizeof(int))) { printf("?math string too long\n"); exit(1); }
        mbuf[mbn++] = id;
    }
}
int storemath() {                /* append mbuf -> mstr[], return offset */
    int off, i;
    if (mstrTop + mbn > mstrCap) {
        mstrCap = (mstrTop + mbn)*2 + 4096;
        mstr = (int *)xrealloc((char *)mstr, mstrCap*(int)sizeof(int));
    }
    off = mstrTop;
    for (i = 0; i < mbn; i++) mstr[mstrTop++] = mbuf[i];
    return off;
}

/* ------------------------------------------------------- substitution map */
/* subOff/subLen indexed by symbol id; subGen marks validity per application */
int *subOff, *subLen, *subGen; int subCap, curGen;
void subensure() {
    int i;
    if (nsym > subCap) {
        int old = subCap;
        subCap = nsym*2 + 256;
        subOff = (int *)xrealloc((char *)subOff, subCap*(int)sizeof(int));
        subLen = (int *)xrealloc((char *)subLen, subCap*(int)sizeof(int));
        subGen = (int *)xrealloc((char *)subGen, subCap*(int)sizeof(int));
        for (i = old; i < subCap; i++) subGen[i] = 0;
    }
}

/* ---------------------------------------------------------- expression stack */
/* estk[] is a pool; stkOff[]/stkLen[] index expressions on the stack. */
int *estk; int estkTop, estkCap;
int *stkOff, *stkLen; int stkN, stkCap;
int *tmpe;  int tmpeCap;     /* scratch for substituted result */

void estk_reserve(n) int n; {
    if (estkTop + n > estkCap) { estkCap=(estkTop+n)*2+4096; estk=(int *)xrealloc((char *)estk,estkCap*(int)sizeof(int)); }
}
void stk_push(off, len) int off, len; {
    if (stkN+1 > stkCap) { stkCap=stkCap*2+256; stkOff=(int *)xrealloc((char *)stkOff,stkCap*(int)sizeof(int)); stkLen=(int *)xrealloc((char *)stkLen,stkCap*(int)sizeof(int)); }
    stkOff[stkN]=off; stkLen[stkN]=len; stkN++;
}
/* push a raw math string (symbol-id array) as a new expr */
void push_raw(src, len) int *src; int len; {
    int off, i;
    estk_reserve(len);
    off = estkTop;
    for (i=0;i<len;i++) estk[estkTop++]=src[i];
    stk_push(off, len);
}

char *progname;
int errcount;

/* apply substitution (current gen) to math string src[len] -> tmpe, return len */
int dosubst(src, len) int *src; int len; {
    int i, j, n, s;
    n = 0;
    for (i=0;i<len;i++) {
        s = src[i];
        if (symVar[s] && subGen[s]==curGen) {
            int o=subOff[s], l=subLen[s];
            if (n+l > tmpeCap) { tmpeCap=(n+l)*2+256; tmpe=(int *)xrealloc((char *)tmpe,tmpeCap*(int)sizeof(int)); }
            for (j=0;j<l;j++) tmpe[n++]=estk[o+j];
        } else {
            if (n+1 > tmpeCap) { tmpeCap=(n+1)*2+256; tmpe=(int *)xrealloc((char *)tmpe,tmpeCap*(int)sizeof(int)); }
            tmpe[n++]=s;
        }
    }
    return n;
}

/* collect the variables of an expr (subst image) into a set marked by gen */
int *varSeen; int varSeenCap, varGen;
void varensure() {
    int i, old;
    if (nsym > varSeenCap) { old=varSeenCap; varSeenCap=nsym*2+256; varSeen=(int *)xrealloc((char *)varSeen,varSeenCap*(int)sizeof(int)); for(i=old;i<varSeenCap;i++) varSeen[i]=0; }
}

/* Check the disjoint-variable conditions of assertion `as` under current subst,
   against the frame dv of the statement `pf` we are proving. */
int checkdv(as, pf) int as, pf; {
    int d, o, n, a, b, i, k, x, y, pfo, pfn, found;
    o = stFrDvOff[as]; n = stFrDvLen[as];
    for (d=0; d<n; d++) {
        a = frdv[o + d*2]; b = frdv[o + d*2 + 1];
        /* for every var x in subst(a), y in subst(b): need x!=y and {x,y}
           in pf's frame dv */
        if (subGen[a]!=curGen || subGen[b]!=curGen) continue;
        for (i=0; i<subLen[a]; i++) {
            x = estk[subOff[a]+i];
            if (!symVar[x]) continue;
            for (k=0; k<subLen[b]; k++) {
                y = estk[subOff[b]+k];
                if (!symVar[y]) continue;
                if (x==y) { printf("  *dv fail: shared var %s\n", spool+symName[x]); return 0; }
                /* (x,y) must be disjoint in ALL $d active for pf -- NOT just pf's
                   mandatory frame dv: proofs legitimately use dummy variables whose
                   $d are active but not mandatory (x/y not in the conclusion). */
                found=0;
                { int dd;
                  for (dd=0; dd<actDvTop; dd+=2) {
                    int pa=actDv[dd], pb=actDv[dd+1];
                    if ((pa==x&&pb==y)||(pa==y&&pb==x)) { found=1; break; }
                  }
                }
                if (!found) { printf("  *dv fail: %s , %s not disjoint\n",
                                     spool+symName[x], spool+symName[y]); return 0; }
            }
        }
    }
    return 1;
}

/* Apply assertion `as` while proving `pf`: pop its hyps, unify, push concl. */
int apply(as, pf) int as, pf; {
    int k, i, base, hi, ho, hl, s, tc, vv, en, off, eo, el, sl;
    k = stFrHypLen[as];
    if (stkN < k) { printf("  *stack underflow applying %s\n", spool+stLabel[as]); return 0; }
    base = stkN - k;
    curGen++;
    ho = stFrHypOff[as];
    for (i=0;i<k;i++) {
        hi = frhyp[ho+i];                 /* the i-th mandatory hyp stmt */
        eo = stkOff[base+i]; el = stkLen[base+i];
        if (stType[hi]==S_FLOAT) {
            tc = mstr[stMathOff[hi]];      /* typecode symbol */
            vv = mstr[stMathOff[hi]+1];    /* the variable    */
            if (el<1 || estk[eo]!=tc) {
                printf("  *typecode mismatch for %s\n", spool+stLabel[as]); return 0;
            }
            subOff[vv]=eo+1; subLen[vv]=el-1; subGen[vv]=curGen;
        } else {                          /* $e : must match after subst */
            sl = dosubst(&mstr[stMathOff[hi]], stMathLen[hi]);
            if (sl!=el) { printf("  *hyp mismatch (len) for %s\n", spool+stLabel[as]); return 0; }
            for (s=0;s<sl;s++) if (tmpe[s]!=estk[eo+s]) { printf("  *hyp mismatch for %s\n", spool+stLabel[as]); return 0; }
        }
    }
    if (!checkdv(as, pf)) return 0;
    /* build conclusion image, then pop k, then push */
    en = dosubst(&mstr[stMathOff[as]], stMathLen[as]);
    /* tmpe holds the conclusion image (separate from estk); now pop k and push */
    if (k>0) estkTop = stkOff[base];      /* free popped exprs' pool space */
    stkN = base;
    estk_reserve(en);
    off = estkTop;
    for (i=0;i<en;i++) estk[estkTop++]=tmpe[i];
    stk_push(off, en);
    return 1;
}

/* -------------------------------------------------- proof-step dispatch */
/* push/apply a referenced statement `st` while proving `pf` */
int dostep(st, pf) int st, pf; {
    if (st<0) return 0;
    if (stType[st]==S_FLOAT || stType[st]==S_ESS)
        push_raw(&mstr[stMathOff[st]], stMathLen[st]);
    else
        return apply(st, pf);
    return 1;
}

/* saved steps for compressed proofs: kept in a SEPARATE stable pool (savPool)
   so that stack pops on estk[] never clobber them. */
int *savOff, *savLen; int savN, savCap;
int *savPool; int savPoolTop, savPoolCap;
void save_top() {
    int o, l, i, no;
    if (stkN<1) { printf("  *nothing to save\n"); return; }
    o = stkOff[stkN-1]; l = stkLen[stkN-1];
    if (savN+1>savCap){savCap=savCap*2+64;savOff=(int *)xrealloc((char *)savOff,savCap*(int)sizeof(int));savLen=(int *)xrealloc((char *)savLen,savCap*(int)sizeof(int));}
    if (savPoolTop+l > savPoolCap){savPoolCap=(savPoolTop+l)*2+1024;savPool=(int *)xrealloc((char *)savPool,savPoolCap*(int)sizeof(int));}
    no = savPoolTop; for(i=0;i<l;i++) savPool[savPoolTop++]=estk[o+i];
    savOff[savN]=no; savLen[savN]=l; savN++;
}

/* ---------------------------------------------------- verify one $p proof */
/* labels of a normal proof are read as tokens until $. ; compressed starts '(' */
int verify(pf) int pf; {
    int t, st, ok;
    stkN = 0; estkTop = 0; savN = 0; savPoolTop = 0;
    subensure();
    if (!gettok()) { printf("?EOF in proof\n"); return 0; }
    if (strcmp(tok,"(")==0 || strcmp(tok,"$(")==0) {
        /* --- compressed proof --- */
        int *labs; int nl, lc, i, num, m, mo;
        labs=(int *)0; nl=0; lc=0;
        for (;;) {
            if (!gettok()) { printf("?EOF in compressed proof\n"); return 0; }
            if (strcmp(tok,")")==0) break;
            st=findlab(tok);
            if (st<0){printf("?unknown label '%s' in proof of %s\n",tok,spool+stLabel[pf]);return 0;}
            if (nl+1>lc){lc=lc*2+32;labs=(int *)xrealloc((char *)labs,lc*(int)sizeof(int));}
            labs[nl++]=st;
        }
        m  = stFrHypLen[pf];             /* mandatory hyp count */
        mo = stFrHypOff[pf];
        num=0; ok=1;
        for (;;) {
            int c, done=0;
            if (!gettok()) { printf("?EOF in compressed proof\n"); ok=0; break; }
            if (strcmp(tok,"$.")==0) { done=1; }
            if (done) break;
            for (i=0; tok[i]; i++) {
                c=tok[i];
                if (c>='A'&&c<='T') {
                    num=num*20+(c-'A'+1);
                    /* resolve num */
                    if (num<=m) { if(!dostep(frhyp[mo+num-1],pf)){ok=0;} }
                    else if (num<=m+nl) { if(!dostep(labs[num-m-1],pf)){ok=0;} }
                    else {
                        int si=num-m-nl-1;
                        if (si<0||si>=savN){printf("?bad saved ref\n");ok=0;}
                        else push_raw(&savPool[savOff[si]], savLen[si]);
                    }
                    num=0;
                    if(!ok)break;
                } else if (c>='U'&&c<='Y') {
                    num=num*5+(c-'U'+1);
                } else if (c=='Z') {
                    save_top();
                } else {
                    printf("?bad char '%c' in compressed proof\n", c); ok=0; break;
                }
            }
            if(!ok)break;
        }
        if (labs) free((char *)labs);
    } else {
        /* --- normal proof: tok already holds first label --- */
        ok=1;
        for (;;) {
            if (strcmp(tok,"$.")==0) break;
            if (strcmp(tok,"?")==0) { printf("  (incomplete proof)\n"); ok=0; }
            else {
                st=findlab(tok);
                if (st<0){printf("?unknown label '%s' in proof of %s\n",tok,spool+stLabel[pf]);ok=0;break;}
                if(!dostep(st,pf)){ok=0;break;}
            }
            if (!gettok()){printf("?EOF in proof\n");ok=0;break;}
        }
    }
    if (ok) {
        /* stack must hold exactly the assertion */
        if (stkN!=1) { printf("  *stack has %d entries (want 1)\n", stkN); ok=0; }
        else {
            int el=stkLen[0], eo=stkOff[0], i;
            if (el!=stMathLen[pf]) ok=0;
            else for(i=0;i<el;i++) if(estk[eo+i]!=mstr[stMathOff[pf]+i]){ok=0;break;}
            if(!ok) printf("  *proof result does not match assertion\n");
        }
    }
    /* resync the parser: make sure we've consumed through the closing $. */
    while (strcmp(tok,"$.")!=0) { if(!gettok()) break; }
    return ok;
}

/* ---------------------------------------------------- build mandatory frame */
int *isMand; int isMandCap, mandGen;   /* var -> mandatory (per gen) */
void mandensure() { int i,old; if(nsym>isMandCap){old=isMandCap;isMandCap=nsym*2+256;isMand=(int *)xrealloc((char *)isMand,isMandCap*(int)sizeof(int));for(i=old;i<isMandCap;i++)isMand[i]=0;} }

void buildframe(as) int as; {
    int i, j, s, ho, hl, fo, dvo, dn;
    mandensure(); mandGen++;
    /* mark mandatory vars: those in the assertion + in active $e hyps */
    for (i=stMathOff[as]; i<stMathOff[as]+stMathLen[as]; i++) { s=mstr[i]; if(symVar[s]) isMand[s]=mandGen; }
    for (i=0;i<actHypTop;i++){ int h=actHyp[i]; if(stType[h]==S_ESS){ for(j=stMathOff[h];j<stMathOff[h]+stMathLen[h];j++){s=mstr[j];if(symVar[s])isMand[s]=mandGen;} } }
    /* frame hyps: active hyps in order, $e always, $f if var mandatory */
    fo = frhypTop;
    for (i=0;i<actHypTop;i++){
        int h=actHyp[i];
        if (stType[h]==S_ESS) {
            if(frhypTop+1>frhypCap){frhypCap=frhypCap*2+256;frhyp=(int *)xrealloc((char *)frhyp,frhypCap*(int)sizeof(int));}
            frhyp[frhypTop++]=h;
        } else { /* $f */
            int v=mstr[stMathOff[h]+1];
            if (isMand[v]==mandGen) {
                if(frhypTop+1>frhypCap){frhypCap=frhypCap*2+256;frhyp=(int *)xrealloc((char *)frhyp,frhypCap*(int)sizeof(int));}
                frhyp[frhypTop++]=h;
            }
        }
    }
    stFrHypOff[as]=fo; stFrHypLen[as]=frhypTop-fo;
    /* frame dv: active dv pairs where both vars mandatory */
    dvo=frdvTop; dn=0;
    for (i=0;i<actDvTop;i+=2){
        int a=actDv[i], b=actDv[i+1];
        if (isMand[a]==mandGen && isMand[b]==mandGen) {
            if(frdvTop+2>frdvCap){frdvCap=frdvCap*2+256;frdv=(int *)xrealloc((char *)frdv,frdvCap*(int)sizeof(int));}
            frdv[frdvTop++]=a; frdv[frdvTop++]=b; dn++;
        }
    }
    stFrDvOff[as]=dvo; stFrDvLen[as]=dn;
}

/* ------------------------------------------------------------------- main */
int nverified, nfailed, naxiom;

int main(argc, argv) int argc; char **argv; {
    int i, term, st, id;
    /* The Kronos loader does NOT zero BSS, so every global that we assume
       starts at 0/NULL must be initialised explicitly here. */
    spool=(char *)0; spTop=0; spCap=0;
    symName=(int *)0; symVar=(char *)0; nsym=0; symCap=0; symHash=(int *)0; symHashSz=0;
    stType=(int *)0; stLabel=(int *)0; stMathOff=(int *)0; stMathLen=(int *)0;
    stFrHypOff=(int *)0; stFrHypLen=(int *)0; stFrDvOff=(int *)0; stFrDvLen=(int *)0; nst=0; stCap=0;
    mstr=(int *)0; mstrTop=0; mstrCap=0;
    frhyp=(int *)0; frhypTop=0; frhypCap=0; frdv=(int *)0; frdvTop=0; frdvCap=0;
    labHash=(int *)0; labHashSz=0; nlab=0;
    actHyp=(int *)0; actHypTop=0; actHypCap=0; actDv=(int *)0; actDvTop=0; actDvCap=0;
    scHyp=(int *)0; scDv=(int *)0; scTop=0; scCap=0;
    subOff=(int *)0; subLen=(int *)0; subGen=(int *)0; subCap=0; curGen=0;
    estk=(int *)0; estkTop=0; estkCap=0; stkOff=(int *)0; stkLen=(int *)0; stkN=0; stkCap=0;
    tmpe=(int *)0; tmpeCap=0;
    varSeen=(int *)0; varSeenCap=0; varGen=0;
    savOff=(int *)0; savLen=(int *)0; savN=0; savCap=0; savPool=(int *)0; savPoolTop=0; savPoolCap=0;
    isMand=(int *)0; isMandCap=0; mandGen=0;
    nverified=0; nfailed=0; naxiom=0; errcount=0;
    mbn=0; toklen=0; pushedback=0;
    progname="mmverify";
    if (argc<2) { printf("usage: mmverify file.mm\n"); return 1; }
    fp=fopen(argv[1],"r");
    if (!fp) { printf("?cannot open %s\n", argv[1]); return 1; }

    /* init tables */
    spCap=65536; spool=xalloc(spCap); spTop=0;
    symHashSz=1024; symHash=(int *)xalloc(symHashSz*(int)sizeof(int)); memset((char *)symHash,0,symHashSz*(int)sizeof(int));
    labHashSz=4096; labHash=(int *)xalloc(labHashSz*(int)sizeof(int)); memset((char *)labHash,0,labHashSz*(int)sizeof(int));

    printf("mmverify: checking %s\n", argv[1]);

    while (gettok()) {
        if (tok[0]=='$' && toklen==2) {
            int c=tok[1];
            if (c=='c'||c=='v') {
                for(;;){ if(!gettok()){printf("?EOF\n");return 1;} if(strcmp(tok,"$.")==0)break;
                         if(findsym(tok)>=0){/*redeclare - skip*/} else addsym(tok, c=='v'); }
            } else if (c=='d') {
                /* $d v1 v2 ... $. : all pairwise */
                int nv=0;
                for(;;){ if(!gettok()){printf("?EOF\n");return 1;} if(strcmp(tok,"$.")==0)break;
                         id=findsym(tok); if(id<0){printf("?unknown var in $d\n");return 1;} if(nv<512)gVs[nv++]=id; }
                for(i=0;i<nv;i++) for(term=i+1;term<nv;term++){ pushi(&actDv,&actDvTop,&actDvCap,gVs[i]); pushi(&actDv,&actDvTop,&actDvCap,gVs[term]); }
            } else if (c=='{') {
                if (scTop+1 > scCap) { scCap=scCap*2+64;
                    scHyp=(int *)xrealloc((char *)scHyp,scCap*(int)sizeof(int));
                    scDv =(int *)xrealloc((char *)scDv, scCap*(int)sizeof(int)); }
                scHyp[scTop]=actHypTop; scDv[scTop]=actDvTop; scTop++;
            } else if (c=='}') {
                if(scTop>0){ scTop--; actHypTop=scHyp[scTop]; actDvTop=scDv[scTop]; }
            } else if (c=='[') {   /* $[ file $] : skip (excerpts are self-contained) */
                for(;;){ if(!gettok()){printf("?EOF in $[\n");return 1;} if(strcmp(tok,"$]")==0)break; }
            } else {
                printf("?unexpected token %s\n", tok); return 1;
            }
        } else if (tok[0]=='$') {
            printf("?unexpected %s (missing label?)\n", tok); return 1;
        } else {
            /* a label: next must be $f/$e/$a/$p */
            strcpy(gLab, tok);
            if(!gettok()){printf("?EOF after label\n");return 1;}
            if(strcmp(tok,"$f")==0){
                term=readmath(); st=newstmt(); setlabel(st,gLab);
                stType[st]=S_FLOAT; stMathOff[st]=storemath(); stMathLen[st]=mbn;
                pushi(&actHyp,&actHypTop,&actHypCap,st);
            } else if(strcmp(tok,"$e")==0){
                term=readmath(); st=newstmt(); setlabel(st,gLab);
                stType[st]=S_ESS; stMathOff[st]=storemath(); stMathLen[st]=mbn;
                pushi(&actHyp,&actHypTop,&actHypCap,st);
            } else if(strcmp(tok,"$a")==0){
                term=readmath(); st=newstmt(); setlabel(st,gLab);
                stType[st]=S_AXIOM; stMathOff[st]=storemath(); stMathLen[st]=mbn;
                buildframe(st); naxiom++;
            } else if(strcmp(tok,"$p")==0){
                term=readmath(); /* reads until $= */
                if(term!=1){printf("?$p without proof: %s\n",gLab);return 1;}
                st=newstmt(); setlabel(st,gLab);
                stType[st]=S_PROV; stMathOff[st]=storemath(); stMathLen[st]=mbn;
                buildframe(st);
                if (verify(st)) { nverified++; }
                else { nfailed++; printf("  FAILED: %s\n", gLab); }
                if ((nverified+nfailed) % 200 == 0)
                    printf("  .. %d proofs checked (%d ok, %d bad)\n",
                           nverified+nfailed, nverified, nfailed);
            } else {
                printf("?expected $f/$e/$a/$p after label %s, got %s\n", gLab, tok); return 1;
            }
        }
    }
    fclose(fp);
    printf("\n=== done: %d proofs verified, %d FAILED, %d axioms, %d symbols ===\n",
           nverified, nfailed, naxiom, nsym);
    return nfailed ? 1 : 0;
}
