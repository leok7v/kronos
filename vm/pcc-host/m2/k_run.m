IMPLEMENTATION MODULE k_run  ; (* Who 00-Apr-86. (c) KRONOS *)

(* 09-Jun-88 by SS  Adapted for new Modula compiler *)

IMPORT  mcd: mCodeMnem;

FROM SYSTEM    IMPORT  ADR,WORD,ADDRESS;
FROM BIO       IMPORT  CD,ChangeCD,OpenOnDir,Close,Link,GetEof,SetEof,
                       dummyErr,Driver,Flush,Go,Cut,CreateOn,ReOpen,
                       CreateDir,bRead,bWrite,Opened?,UnLink,IsDir?;
FROM Heap      IMPORT  ALLOCATE;
FROM Media     IMPORT  DrvVol;
FROM FsDrv     IMPORT  DevDrv,DoCFE,DeviceSize,WriteBlock,ReadBlock;
FROM FsPublic  IMPORT  VolumeName,VisFSerr,File,FileName,FileNotFound;
FROM Args      IMPORT  NumFlag?,TakeParm;
FROM TTYs      IMPORT  Read,Write,BusyRead,GetTerm,SetTerm,TermExist;
FROM Scheduler IMPORT  MyTask,ProcessId;
FROM ASCII     IMPORT  NL,CR,LF,FF;
FROM Terminal  IMPORT  Esc,print;
FROM Image     IMPORT  image;
FROM Strings   IMPORT  AppStr,Len,SubStr,Str1,Str0,App;


PROCEDURE MOVE(a,b: ADDRESS; s: INTEGER); CODE mcd.move END MOVE;

PROCEDURE Min(x,y: INTEGER): INTEGER;
BEGIN IF x<y THEN RETURN x ELSE RETURN y END END Min;

PROCEDURE Max(x,y: INTEGER): INTEGER;
BEGIN IF x>y THEN RETURN x ELSE RETURN y END END Max;

PROCEDURE Dig?(ch: CHAR): BOOLEAN;
BEGIN RETURN ("0"<=ch) & (ch<="9") END Dig?;

PROCEDURE Ord16?(ch: CHAR): INTEGER;
BEGIN
  IF ("0"<=ch) & (ch<="9") THEN RETURN ORD(ch)-ORD("0")    END;
  ch:=CAP(ch);
  IF ("A"<=ch) & (ch<="F") THEN RETURN ORD(ch)-ORD("A")+10 END;
END Ord16?;

PROCEDURE Letter?(ch: CHAR): BOOLEAN;
BEGIN
  IF ch>300c THEN RETURN TRUE END;
  ch:=CAP(ch);
  IF ("A"<=ch) & (ch<="Z") THEN RETURN TRUE END;
  RETURN FALSE
END Letter?;

TYPE   bbuf=ARRAY [0..blksize-1] OF CHAR;
VAR    ii,i,sz,termnri,myterm :INTEGER;
       p                             :POINTER TO bbuf;
       cd,f,dir                      :File;
       rd,r,t,tw,tr,ph,skip,tf,mytrace  :BOOLEAN;
       w       :WORD;
       eofch   :CHAR;
       trf     :FR;                 (* tracing file record *)
       mytask  :ProcessId;

PROCEDURE ABORT;
BEGIN HALT(1) END ABORT;

PROCEDURE kwrch(ch:CHAR);
BEGIN
  IF ch=LF THEN Write(CR); Write(LF)
  ELSE          Write(ch)
  END;
END kwrch;

PROCEDURE kprch(u:INTEGER; ch:CHAR);
BEGIN
   IF ch=NL THEN ch:=LF END;
   IF ph THEN tprint("lp -> %x\l",ORD(ch)) END;
   IF WriteBlock(u,0,ADR(ch),1) THEN END;
END kprch;

PROCEDURE WrStr(ss: ARRAY OF CHAR);
VAR i:INTEGER;
    s:ARRAY [0..79] OF CHAR;
BEGIN
   IF trace & mytrace THEN
      Str1(s,ss);
      IF tf THEN
         mytrace:=FALSE;
         IF kwrite(trfp^,ADR(s)*4,Len(s))#0 THEN END;
         mytrace:=TRUE;
      ELSE
         FOR i:=0 TO Len(s)-1 DO kwrch(s[i]) END
      END;
   END;
END WrStr;

PROCEDURE tprint(form:ARRAY OF CHAR; w:WORD);
  VAR s:ARRAY [0..79] OF CHAR;
BEGIN
  Str0(s);
  image(s,form,w);
  WrStr(s)
END tprint;

PROCEDURE WriteLn;
VAR s:ARRAY [0..3] OF CHAR;
BEGIN Str0(s); App(s,LF); WrStr(s) END WriteLn;

PROCEDURE Show(s:ARRAY OF CHAR);
BEGIN WrStr(s); WriteLn END Show;

PROCEDURE Show2(s1,s2:ARRAY OF CHAR);
BEGIN WrStr(s1); Show(s2) END Show2;

PROCEDURE tty?(ffn:FileName; VAR t:INTEGER):BOOLEAN;
VAR name:FileName;
BEGIN
   SubStr(ffn,0,termnri-1,name);
   IF name=termstr THEN
      IF Dig?(ffn[termnri]) & (ffn[termnri+1]=0c) THEN
         t:=Ord16?(ffn[termnri]);
         IF NOT TermExist(t) THEN RETURN FALSE END;
      ELSIF ffn[termnri]=0c THEN 
         t:=myterm
      ELSE RETURN FALSE
      END;
   ELSE RETURN FALSE
   END;
   RETURN TRUE;
END tty?;

PROCEDURE reset;
BEGIN SetTerm(mytask,myterm) END reset;

PROCEDURE log2(n: WORD):INTEGER;
  VAR m: INTEGER;
BEGIN
  FOR m:=0 TO 31 DO IF m IN BITSET(n) THEN RETURN m END END;
  RETURN -1
END log2;

PROCEDURE dev?(s:FileName):BOOLEAN;
VAR n:FileName;
BEGIN
   SubStr(s,0,4,n);
   RETURN (n="/dev/")
END dev?;

PROCEDURE unit?(s:FileName; VAR u:INTEGER; VAR dev:BITSET):BOOLEAN;
VAR n:FileName;
BEGIN
   IF tty?(s,u) THEN dev:={D_TTY}; RETURN TRUE END;
   SubStr(s,5,Len(s)-1,n);
   IF n="null" THEN dev:={D_NULL}; RETURN TRUE
   ELSIF fserr(DevDrv(n,u)) THEN RETURN FALSE
   END;
   SubStr(s,5,6,n);
   IF (n="lp") OR (n="se") THEN dev:={D_LP}   (* SS 4-Jan-88 *)
   ELSIF ((n="fd") OR (n="fe") OR (n="mw") OR (n="dx")) THEN dev:={D_RA}
   ELSE  RETURN FALSE
   END;
   RETURN TRUE;
END unit?;

PROCEDURE kiocntrl(VAR d:FR; op:INTEGER; buf:ADDRESS):BOOLEAN;
VAR drv: INTEGER;
    name:FileName;
BEGIN
   WITH d DO
      CASE log2(fdev*{0..maxdev-1}) OF
         D_NULL: RETURN FALSE
        |D_TTY:  SubStr(ffn,5,6,name);
                 IF ffn[termnri]=0c THEN 
                    IF myterm < 10 THEN App(name,CHAR(ORD('0')+myterm))
                    ELSE                App(name,CHAR(ORD('a')+myterm-10))
                    END;
                 ELSE App(name,ffn[termnri])
                 END;
                 IF fserr(DevDrv(name,drv)) THEN RETURN TRUE END;
        |D_LP,D_RA: drv:=funit
        |D_FS:   IF fserr(Driver(ffd,drv)) THEN RETURN TRUE END
      ELSE
         RETURN fserr(BOOLEAN(Notsupp))
      END;
      IF ph THEN
         Show2("iocntrl:  filename: ",ffn);
         Show2("          unit: ",name);
         tprint("          driver: %x\l",drv);
         tprint("          op: %d\l",op)
      END;
      IF DoCFE(drv,op,buf) THEN END;
      RETURN FALSE;
   END;
END kiocntrl;

PROCEDURE pind(adr:PCCH; VAR csa:ADDRESS):INTEGER;
BEGIN
   csa:=adr DIV 4; RETURN adr MOD 4;
END pind;


PROCEDURE errf(VAR d:FR);
BEGIN
       AppStr(errmsg,":  file:");AppStr(errmsg,d.ffn);
       IF erro THEN
       Show2("       file: ",d.ffn)
       END;
END errf;

PROCEDURE err (m:ARRAY OF CHAR);
BEGIN
 Str1(errmsg,m);
 IF erro THEN
 WriteLn;WrStr("C-RTS: ");Show(m);
 END;
END err;

PROCEDURE fserr(code:BOOLEAN):BOOLEAN;
VAR    m       :ARRAY [0..71] OF CHAR;
BEGIN
 IF INTEGER(code) >= 200 THEN
    errnro^:=INTEGER(code);
    CASE errnro^ OF
       Buflim  : print("Buffer limit exhausted\n");ABORT
      |Dirop   : err("Open for directory file not allowed")
      |Illmode : err("Illegal mode in open")
      |Alrclo  : err("File already closed")
      |Smemlim : print("sbrk memory limit exhausted\n");ABORT
      |Descrout: err("File descriptor out of range")
      |Notop   : err("File not opened")
      |Filelim : err("File limit exhausted")
      |Notsupp : err("Device not supported by RTS")
   ELSE
      err("Unknown error")
   END;
 ELSE
    IF code THEN errnro^:=INTEGER(code)+minfserr; VisFSerr(code,m);err(m) END;
 END;
 RETURN code
END fserr;
 
PROCEDURE trygo(VAR pn:FileName):BOOLEAN;
BEGIN RETURN fserr(Go(cd,dir,pn,dummyErr)) END trygo;

PROCEDURE tryopen(VAR fn:FileName):BOOLEAN;
BEGIN RETURN fserr(OpenOnDir(dir,f,fn)) END tryopen;

PROCEDURE tryclose(f:File);
BEGIN r:=(Close(f) OR r) END tryclose;

PROCEDURE trywrite(VAR d:FR; count:INTEGER):BOOLEAN;
VAR p:POINTER TO bbuf;
BEGIN
 WITH d DO
   IF (NOT (B_CHANGED IN fmode)) OR (count=0) OR (bl < 0) THEN
      RETURN FALSE
   END;
   p:=fbuf;
   IF ph THEN
      Show2("Physical write to file: ",ffn);
      tprint("Block # %d\l",bl);
      tprint("Buffer %x\l",p);
      tprint("Count = %d\l",count);
   END;
   IF D_FS IN fdev THEN
      IF fserr(bWrite(ffd,bl,p,count)) THEN RETURN TRUE END;
   ELSE
      IF fserr(WriteBlock(funit,bl,p,count)) THEN RETURN TRUE END;
   END;
   EXCL(fmode,B_CHANGED); RETURN FALSE
 END;
END trywrite;

PROCEDURE tryread(VAR d:FR; count:INTEGER):BOOLEAN;
VAR p:POINTER TO bbuf;
BEGIN
 WITH d DO
   IF count=0 THEN RETURN FALSE END;
   p:=fbuf;
   IF ph THEN
      Show2("Physical read from file: ",ffn);
      tprint("Block # %d\l",bl);
      tprint("Buffer %x\l",p);
      tprint("Count = %d\l",count);
   END;
   IF D_FS IN fdev THEN
      RETURN fserr(bRead(ffd,bl,p,count));
   ELSE
      RETURN fserr(ReadBlock(funit,bl,p,count))
   END;
 END;
END tryread;

PROCEDURE closedd;
BEGIN tryclose(dir) END closedd;

PROCEDURE closedf;
BEGIN tryclose(f); closedd END closedf;

PROCEDURE getbuf(VAR d:FR): BOOLEAN;
BEGIN
  WITH d DO
    IF fbuf = NIL THEN
      IF bufcnt >= maxbuf THEN
        r:=fserr(BOOLEAN(Buflim));
        IF D_FS IN fdev THEN closedf END;
        RETURN TRUE
      ELSE
        fbuf:=bufpool+bufcnt*blksize DIV 4; INC(bufcnt)
      END;
    END;
    EXCL(fmode,B_CHANGED);
    fcount:=0;
    bl:=-1;
    RETURN FALSE;
  END;
END getbuf;

PROCEDURE kopen(VAR d:FR):BOOLEAN;
  VAR e      : BOOLEAN;
      dr     : INTEGER;
      dp     : POINTER TO FR;
  CONST ctrl = 2;
BEGIN
  dp:=ADR(d);
  WITH d DO
    IF dev?(ffn) THEN
      IF unit?(ffn,funit,fdev) THEN
        CASE log2(fdev*{0..maxdev-1}) OF
               D_NULL:
              |D_TTY : dr:=ctrl;
                       IF kiocntrl(dp^,21,ADR(dr)) THEN END; (* set ^Q, ^S *)
              |D_LP  : IF DoCFE(funit,20,NIL) THEN END;      (* Lock LP *)
              |D_RA  : IF getbuf(dp^) THEN RETURN TRUE END;
                       IF O_TRUNC IN fmode THEN fsize:=0
                       ELSE fsize:=DeviceSize(funit)*blksize
                       END;
        ELSE RETURN fserr(BOOLEAN(Notsupp))
        END;
        RETURN FALSE
      ELSE
        RETURN TRUE
      END
    END;

 (* Open a real file-system file *)

    IF trygo(ffn) THEN RETURN TRUE END;
    e:= OpenOnDir(dir,f,ffn);
    IF e & (e#FileNotFound) THEN closedd; RETURN TRUE END;
    IF t & (NOT e) THEN Show2("Old file found: ",ffn) END;
    IF (NOT e) & (O_CREAT IN fmode) & (O_EXCL IN fmode) THEN
      closedd; RETURN TRUE
    END;
    IF e & (O_CREAT IN fmode) THEN
      fsize:=0;
      IF fserr(CreateOn(f,dir)) THEN
        closedd; RETURN TRUE
      ELSIF t THEN
        Show2("New file created: ",ffn);
      END;
      INCL(fmode,F_UNLINK);
    ELSIF e THEN
      r:=fserr(e); closedd; RETURN TRUE
    ELSIF IsDir?(f) THEN
      r:=fserr(BOOLEAN(Dirop)); closedf; RETURN TRUE
    ELSIF O_TRUNC IN fmode THEN fsize:=0; SetEof(f,0);
    ELSE fsize:=GetEof(f)
    END;
    IF getbuf(dp^) THEN RETURN TRUE END;
    ffd:=f;
    fdev:={D_FS};
    fdir:=dir;
    RETURN FALSE
  END;
END kopen;

PROCEDURE kclose(VAR d:FR): BOOLEAN;
  VAR dp: POINTER TO FR;
BEGIN
 WITH d DO
   IF oflag = closed THEN RETURN FALSE END;
   r:=FALSE; p:=fbuf; dp:=ADR(d);
   CASE log2(fdev*{0..maxdev-1}) OF
         D_NULL:
        |D_TTY :
        |D_LP  : (* kprch(funit,FF); *) (* SS 8-Jan-88 *)
                 IF DoCFE(funit,21,NIL) THEN END; (* UnLock LP *)
        |D_RA  : r:=trywrite(dp^,Min(fsize-bl*blksize,blksize))
        |D_FS  : IF NOT ( Opened?(ffd) & Opened?(fdir) ) THEN
                   r:=fserr(BOOLEAN(Alrclo)); RETURN TRUE
                 END;
                 IF (O_WRONLY IN fmode) OR (O_RDWR IN fmode) THEN
                   SetEof(ffd,fsize);
                   r:=trywrite(dp^,Min(fsize-bl*blksize,blksize));
                 END;
                 IF F_UNLINK IN fmode THEN
                   r:=r OR Link(fdir,ffn,ffd); EXCL(fmode,F_UNLINK);
                 END;
                 tryclose(ffd); tryclose(fdir);
                 IF t & (NOT r) THEN
                    WrStr("file "); WrStr(ffn); WrStr(" closed: ");
                    tprint("length = %d\l",fsize)
                 END;
   ELSE RETURN TRUE
   END;
   RETURN r;
 END;
END kclose;


PROCEDURE rwtrace(s:ARRAY OF CHAR; from,cnt:INTEGER; a:ADDRESS);
VAR i:INTEGER;
    ch:CHAR;  ss:ARRAY [0..3] OF CHAR;
    p:POINTER TO bbuf;
BEGIN
   IF NOT mytrace THEN RETURN END;
   p:=a;
   from:=from MOD blksize;
   WriteLn; WrStr(s);
   tprint("   Buffer at %x",p);
   tprint("   offset %x",from); tprint("   count %d\l",cnt);
   FOR i:=from TO from+cnt-1 DO
      ch:=p^[i]; tprint("%x",i); tprint("   %x",ORD(ch));
      IF ORD(ch) MOD 128 >= 40b THEN  (* Letter?(ch) OR Special?(ch) THEN *)
         Str0(ss); App(ss,' ');App(ss,ch); WrStr(ss)
      END;
      WriteLn;
   END;
END rwtrace;

PROCEDURE move(buf,cnt:INTEGER; VAR cm,c,i:INTEGER; a,b:ADDRESS);
VAR ca,cf,l:INTEGER;
    cw,caw :ADDRESS;
    csp    :POINTER TO CSTR;
    p      :POINTER TO bbuf;
    dcw,dcaw: DYNARR OF INTEGER;
BEGIN
   csp:=a; p:=b;
   cm:=Min(blksize-c,cnt); ca:=buf+i; cf:=c+cm-1;
   IF (c MOD 4 = 0) & (ca MOD 4 = 0) THEN
      l:=cm DIV 4;
      IF l>0 THEN
       cw:=ADDRESS(p) + ADDRESS(c DIV 4); caw:=ADDRESS(csp) + ADDRESS(ca DIV 4);
       IF    rd   THEN MOVE(caw,cw,l)
       ELSIF skip THEN cw^:=0;
         IF l>1 THEN   MOVE(cw+1,cw,l-1) END;
       ELSE            MOVE(cw,caw,l)
       END;
       l:=l*4; INC(c,l); INC(ca,l)
      END;
   END;
   IF rd THEN
   (* FOR c:=c TO cf DO csp^[ca]:=p^[c]; INC(ca) END; *)
      WHILE c<=cf    DO csp^[ca]:=p^[c]; INC(ca); INC(c) END;
   ELSE
   (* FOR c:=c TO cf DO *)
      WHILE c<=cf    DO
        IF skip THEN p^[c]:=0c
        ELSE         p^[c]:=csp^[ca]; INC(ca)
        END;
        INC(c);
      END
   END;
   IF c=cf THEN INC(c) END;
   INC(i,cm);
END move;

PROCEDURE kread(VAR d:FR; buf:PCCH; cnt:INTEGER):INTEGER;
VAR c,cm,i,cb:INTEGER;
    ch       :CHAR;
    csp      :POINTER TO CSTR;
    dp       :POINTER TO FR;
BEGIN
  csp:=ADDRESS(buf DIV 4); buf:=buf MOD 4;
  WITH d DO
    IF NOT (D_FS IN fdev) THEN
      CASE log2(fdev*{0..maxdev-1}) OF
         D_NULL: RETURN eof
        |D_TTY :
              IF funit # myterm THEN SetTerm(mytask,funit) END;
              FOR i:=0 TO cnt-1 DO
                 IF O_NDELAY IN fmode THEN ch:=BusyRead()
                 ELSE                      ch:=Read()
                 END;
                 IF ch=CR THEN ch:=LF
                 ELSIF ch=eofch THEN reset; RETURN i
                 END;
                 IF (ch=177c) OR (ch=377c) THEN
                    IF i > 0 THEN
                       Esc('D'); Write(' '); Esc('D');
                       DEC(i,2); csp^[buf+i+1]:=' ';
                    END;
                 ELSE
                    csp^[buf+i]:=ch;
                    IF (ch > 37c) OR (ch=LF) THEN kwrch(ch) END;
                 END;
                 IF ch=LF THEN reset; RETURN i+1 END;
              END;
              reset; RETURN cnt;
        |D_RA  :
      ELSE RETURN fail
      END;
    END;
(* Read from a real file *)

   IF fcount >= fsize THEN RETURN eof END;
   c:=fcount MOD blksize;
   IF fcount+cnt > fsize THEN cnt:=fsize-fcount END;
   IF tr THEN tprint("kread entry: Buffer pointer = %x",fbuf);
              tprint("   Offset = %x\l",c)
   END;
   i:=0; dp:=ADR(d);
   WHILE i < cnt DO
      cb:=fcount DIV blksize;
      c:=c MOD blksize;
      IF cb # bl THEN
         IF O_RDWR IN fmode THEN
            IF trywrite(dp^,Min(fsize-bl*blksize,blksize)) THEN
               errf(d); RETURN fail
            END;
         END;
         bl:=cb;
         IF tryread(dp^,Min(fsize-bl*blksize,blksize)) THEN
            errf(d); RETURN fail
         END;
      END;
      rd:=TRUE;
      move(buf,cnt-i,cm,c,i,csp,fbuf);
      IF tr THEN rwtrace("kread",fcount,cm,fbuf) END;
      INC(fcount,cm);
   END;
   RETURN cnt;
END;
END kread;

PROCEDURE kwrite(VAR d:FR; buf:PCCH; cnt:INTEGER):INTEGER;
VAR    ch         :CHAR;
       cm,c,i,cb  :INTEGER;
       dp         :POINTER TO FR;
       csp        :POINTER TO CSTR;
BEGIN
 skip:=(buf=0);
 buf:=pind(buf,csp);
 WITH d DO
    CASE log2(fdev*{0..maxdev-1}) OF
       D_NULL: RETURN cnt
      |D_LP  : FOR i:=0 TO cnt-1 DO kprch(funit,csp^[buf+i]) END;
               RETURN cnt
      |D_TTY :
               IF funit # myterm THEN SetTerm(mytask,funit) END;
               FOR i:=0 TO cnt-1 DO ch:=csp^[buf+i]; kwrch(ch) END;
               reset;
               RETURN cnt
      |D_FS  :
      |D_RA  :
    ELSE RETURN fail
    END;

(* write to real file *)

  dp:=ADR(d);
  IF (O_APPEND IN fmode) & (fcount # fsize) THEN
    IF klseek(dp^,0,2)=fail THEN RETURN fail END;
  END;
  c:=fcount MOD blksize; i:=0;
  WHILE i < cnt DO
    cb:=fcount DIV blksize; c:=c MOD blksize;
    IF cb # bl THEN
       IF trywrite(dp^,blksize) THEN errf(d); RETURN fail END;
       bl:=cb;
       IF (fcount < fsize) OR ((fcount=fsize) & (fsize > 0)) THEN
          IF tryread(dp^,Min(fsize-cb*blksize,blksize)) THEN
             errf(d); RETURN fail
          END;
       END;
    END;
    rd:=FALSE;
    move(buf,cnt-i,cm,c,i,csp,fbuf);
    IF tw THEN rwtrace("kwrite",fcount,cm,fbuf) END;
    INC(fcount,cm); INCL(fmode,B_CHANGED);
 END;
 IF (fcount > 0) & (fcount MOD blksize = 0) THEN
     IF trywrite(dp^,blksize) THEN errf(d);RETURN fail END;
 END;
 fsize:=Max(fcount,fsize);
 RETURN cnt
END;
END kwrite;
 
PROCEDURE klseek(VAR d:FR;offs,wh:INTEGER):INTEGER;
VAR fco,lbl:INTEGER;
    dp     :POINTER TO FR;
BEGIN
WITH d DO
 IF NOT((D_FS IN fdev) OR (D_RA IN fdev)) THEN RETURN ok END;
 CASE wh OF
       0:      fco:=0
      |1:      fco:=fcount
      |2:      fco:=fsize
 ELSE RETURN fail END;
 INC(fco,offs);
 dp:=ADR(d);
 IF fco < 0 THEN RETURN fail END;
 lbl:=fco DIV blksize;
 IF lbl # bl THEN
    IF (O_WRONLY IN fmode) OR (O_RDWR IN fmode) THEN
       IF trywrite(dp^,Min(fsize-bl*blksize,blksize)) THEN
          errf(d); RETURN fail
       END;
    END;
 END;
 fcount:=fco; RETURN fco
END;
END klseek;

PROCEDURE kunlink(pn:FileName):INTEGER;
BEGIN
   IF    trygo(pn)             THEN          RETURN fail
   ELSIF fserr(UnLink(dir,pn)) THEN closedd; RETURN fail
   ELSIF fserr(Close(dir))     THEN          RETURN fail
   ELSE  RETURN ok
   END;
END kunlink;
 
PROCEDURE krename(pno,pnn:FileName):INTEGER;
VAR olddir,oldf:File;
BEGIN
   r:=FALSE;
   IF    trygo(pno) THEN                                       RETURN fail
   ELSIF tryopen(pno) THEN                                     RETURN fail
   ELSIF IsDir?(f) THEN tryclose(f); r:=fserr(BOOLEAN(Dirop)); RETURN fail
   ELSE  olddir:=dir; oldf:=f
   END;
   IF    trygo(pnn) THEN tryclose(oldf); tryclose(olddir);     RETURN fail
   ELSIF NOT tryopen(pnn) THEN tryclose(f); r:=TRUE
   ELSIF fserr(Link(dir,pnn,oldf)) THEN     r:=TRUE
   ELSIF fserr(UnLink(olddir,pno)) THEN     r:=TRUE
   END;
   tryclose(oldf); tryclose(olddir); closedd;
   IF r THEN RETURN fail ELSE RETURN ok END;
END krename;

PROCEDURE kchd(pn:FileName):INTEGER;
BEGIN
   IF    trygo(pn)            THEN RETURN fail
   ELSIF fserr(ChangeCD(dir)) THEN RETURN fail
   ELSE  cd:=dir
   END;
   RETURN ok
END kchd;

PROCEDURE kmkdir(pn:FileName):INTEGER;
BEGIN
   IF    trygo(pn)                THEN RETURN fail
   ELSIF fserr(CreateDir(dir,pn)) THEN RETURN fail
   END;
   RETURN ok
END kmkdir;

PROCEDURE setflags(tt:BOOLEAN);
BEGIN
   t:=tt;ts:=tt;ph:=tt;tr:=tt;tw:=tt;tc:=tt;erro:=tt;tf:=tt;
END setflags;

PROCEDURE initrace;       (* Set tracing options *)
  VAR ta:BOOLEAN;
BEGIN
   trace:=FALSE; Str0(options); setflags(FALSE); ta:=FALSE;
   i:=0;
   WHILE (kparm[i]#0c) & (kparm[i] # '#') DO INC(i) END;
   IF kparm[i]="#" THEN
      App(options,kparm[i]);
      kparm[i]:=' '; INC(i);
      IF (kparm[i] = ' ') OR (kparm[i] = 0c) THEN setflags(TRUE)
      ELSIF (kparm[i]='a') & ((kparm[i+1] = ' ') OR (kparm[i+1] = 0c)) THEN
         setflags(TRUE); ta:=TRUE;
         kparm[i]:=' '
      ELSE
         WHILE NOT ((kparm[i] = ' ') OR (kparm[i] = 0c)) DO
            IF    kparm[i] = 'k' THEN t:=TRUE
            ELSIF kparm[i] = 'c' THEN tc:=TRUE
            ELSIF kparm[i] = 'e' THEN erro:=TRUE
            ELSIF kparm[i] = 'w' THEN tw:=TRUE
            ELSIF kparm[i] = 'r' THEN tr:=TRUE
            ELSIF kparm[i] = 'p' THEN ph:=TRUE
            ELSIF kparm[i] = 's' THEN ts:=TRUE
            ELSIF kparm[i] = 'f' THEN tf:=TRUE
            ELSIF kparm[i] = 'a' THEN ta:=TRUE
            END;
            IF kparm[i] # 'a' THEN App(options,kparm[i]) END;
            kparm[i]:= ' '; INC(i)
         END;
      END;
   END;
   WITH trf DO
      trfp:=ADR(trf);
      IF tf THEN
         ALLOCATE(fbuf,blksize DIV 4);
         IF fbuf=NIL THEN tf:=FALSE
         ELSE
            Str1(ffn,"trace.t");
            IF NOT ta THEN fmode:={O_WRONLY,O_CREAT,O_TRUNC}
            ELSE           fmode:={O_WRONLY,O_CREAT,O_APPEND}
            END;
            IF kopen(trfp^) THEN tf:=FALSE END;
         END;
      END;
      IF NOT tf THEN
         Str1(ffn,termstr);
         fbuf:=NIL;
         fmode:={O_WRONLY};
         IF kopen(trfp^) THEN END;
      END;
      mytrace:=TRUE; trace:=TRUE; oflag:=opened;
   END;
END initrace;

VAR ik: INTEGER;

BEGIN
  Str1(termstr,termid); termnri:=Len(termstr);
  mytask:=MyTask(); myterm:=GetTerm(mytask);
  cd:=CD(); bufcnt:=0; eofch:=cntrlD;
  IF NumFlag?('h',sbrklim) THEN sbrklim:=-1 END;
  IF NumFlag?('f',maxbuf)  THEN maxbuf :=-1 END;
  TakeParm(kparm);
  ik:=0; WHILE kparm[ik]#0c DO INC(ik) END;
  WHILE (ik<HIGH(kparm)-2) & (ik>0) & (kparm[ik-1]=' ') DO DEC(ik) END;
  kparm[ik]:=0c; kparm[HIGH(kparm)]:=CHAR(ik);
  initrace;
END k_run.
