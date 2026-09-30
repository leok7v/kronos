IMPLEMENTATION MODULE c_run; (* h.saar 12-aug-86. (c) KRONOS *)

(* 09-Jun-88 by SS  Adapted for new Modula compiler and Operating system *)

IMPORT  mcd: mCodeMnem;
IMPORT reso: Resource;

FROM SYSTEM    IMPORT  ADR, WORD, ADDRESS;
FROM k_run     IMPORT  kopen,kclose,kread,kwrite,klseek,err,kiocntrl,
                       ts,tc,pind,tprint,WriteLn,Show2,Show,WrStr,
                       trace,trfp,erro,kparm,options,
                       termstr,minfserr,errmsg, errnro,fserr,kunlink,krename,
                       kchd,kmkdir,
                       Smemlim,Descrout,Notop,Filelim,
                       sbrklim,bufpool,blksize,maxbuf,bufcnt,
                       O_WRONLY,O_RDWR,
                       O_NDELAY,O_APPEND,O_SYNC,O_CREAT,O_TRUNC,O_EXCL,
                       D_FS,D_TTY,D_LP,D_RA,
                       fmax,fail,ok,FR,opened,closed,PCCH,CSTR,CCH;
FROM Clock     IMPORT  SystemClock;
FROM Time      IMPORT  AppTime;
FROM Scheduler IMPORT  MyTask, Ticker;
FROM Heap      IMPORT  ALLOCATE, DEALLOCATE;
FROM Terminal  IMPORT  print,WriteString;
FROM Strings   IMPORT  App,SubStr,Str0,AppStr,Len,Str1;

IMPORT  face: myShell;
IMPORT   sys: SYSTEM;

PROCEDURE print0(VAL format: ARRAY OF CHAR; SEQ args: sys.WORD);
BEGIN END print0;

PROCEDURE shell(VAL cmd: ARRAY OF CHAR): INTEGER;
BEGIN
  face.system(cmd,print);
  RETURN face.result MOD 256;
END shell;

CONST ExternalBit=31;
    ChangeMaskBit=30;

PROCEDURE DROP(w: WORD); CODE mcd.drop END DROP;


VAR    ft         :ARRAY [0..fmax] OF ADDRESS;
       d          :POINTER TO FR;
       csp        :POINTER TO CSTR;
       sbrkmem,cp :ADDRESS;
       r,t        :BOOLEAN;
       pool       :ARRAY [0..fmax] OF FR;
       pid,sbrkptr:INTEGER;
       intime     :INTEGER;
       kstr,timestr:ARRAY [0..31] OF CHAR;
       p0          :PINT;

PROCEDURE pcch(a:ADDRESS; b:INTEGER):PCCH;
BEGIN RETURN a * 4 + b END pcch;

PROCEDURE KTOC(VAR k:ARRAY OF CHAR; c: PCCH);
  VAR l,i: INTEGER; ch: CHAR;
BEGIN
   c:=pind(c,csp); i:=0;
   REPEAT
     ch:=k[i]; csp^[c]:=ch; INC(i); INC(c);
   UNTIL ch=0c;
END KTOC;

PROCEDURE CTOK(c:PCCH; VAR k: ARRAY OF CHAR);
  VAR l,i: INTEGER;
BEGIN
   c:=pind(c,csp);
   i:=0; l:=HIGH(k);
   WHILE (csp^[c+i]#0c) & (i<l) DO
      k[i]:=csp^[c+i];
      INC (i)
   END;
   k[i]:=0c; k[HIGH(k)]:=CHAR(i);
   IF t THEN Show2 ("C to K ",k) END;
END CTOK;

PROCEDURE _system(cmdl:PCCH):INTEGER;
VAR    i       :INTEGER;
       line    :ARRAY [0..255] OF CHAR;
BEGIN
   CTOK(cmdl,line);
   IF t THEN Show2("_system call: ",line) END;
   IF Len(options) > 0 THEN
      App(line,' '); AppStr(line,options); App(line,'a');
   END;
   IF t THEN Show2("internal _system call: ",line) END;
   IF trfp^.fbuf # NIL THEN
      trace:=FALSE;
      DROP(kclose(trfp^));
      trfp^.oflag:=closed;
   END;
   i:=shell(line);
   IF trfp^.fbuf # NIL THEN
      trfp^.fmode:={O_WRONLY,O_CREAT,O_APPEND};
      DROP(kopen(trfp^));
      trfp^.oflag:=opened;
      trace:=TRUE;
   END;
   IF t THEN tprint("return from _system with code %d\l",i) END;
   RETURN i
END _system;

PROCEDURE _time(tloc:PINT):INTEGER;
VAR ct:INTEGER;
BEGIN
   ct:=SystemClock();
   IF tloc # p0 THEN tloc^:=ct END;
   IF t THEN tprint("System Clock: %d\l",ct) END;
   RETURN ct
END _time;

PROCEDURE _ctime(clock:PINT):PCCH;
BEGIN
   Str0(timestr);
   AppTime(timestr,"%w3 %m3 %d %h:%':%s %y ",clock^); (* SS 10-Jan-88 *)
   RETURN pcch(ADR(timestr),0);
END _ctime;

PROCEDURE _perror():PCCH;
BEGIN
   IF errnro^ < minfserr THEN
      Str1(errmsg,"Unknown error")
   ELSIF errnro^ < 200 THEN
        r:=fserr(BOOLEAN(errnro^ - minfserr))
   ELSE r:=fserr(BOOLEAN(errnro^))
   END;
   RETURN pcch(ADR(errmsg),0);
END _perror;

PROCEDURE _look(i:INTEGER);
BEGIN                        WriteLn;
tprint("FILE RECORD # %d\l",i);
IF ft[i]=NIL THEN Show("not used"); RETURN END;
d:=ft[i];
tprint("FR ADDRESS    %x\l",d);
Show2("FILENAME      ",d^.ffn);
tprint("FFD           %d\l",d^.ffd);
tprint("FDIR          %d\l",d^.fdir);
WrStr("FMODE         ");
CASE INTEGER(d^.fmode*{0,1}) OF
   0: WrStr("rdonly")
  |1: WrStr("wronly")
  |2: WrStr("rdwr")
ELSE  WrStr("illegal")
END;
IF O_NDELAY IN d^.fmode THEN WrStr(",ndelay") END;
IF O_APPEND IN d^.fmode THEN WrStr(",append") END;
IF O_SYNC   IN d^.fmode THEN WrStr(",sync"  ) END;
IF O_CREAT  IN d^.fmode THEN WrStr(",creat" ) END;
IF O_TRUNC  IN d^.fmode THEN WrStr(",trunc" ) END;
IF O_EXCL   IN d^.fmode THEN WrStr(",excl"  ) END;
IF d^.oflag THEN Show(",closed") ELSE Show(",opened") END;
tprint("FUNIT         %x\l",d^.funit);
tprint("FCOUNT        %d\l",d^.fcount);
tprint("FSIZE         %d\l",d^.fsize);
tprint("BLOCK         %d\l",d^.bl);
tprint("FDEV          %x\l",d^.fdev);
tprint("FBUF ADDR     %x\l",d^.fbuf);
END _look;

PROCEDURE _sbrk(cnt:INTEGER):PCCH;
VAR a:PCCH;
BEGIN
  cnt:=((cnt+3) DIV 4 ) * 4;
  IF sbrkptr+cnt > sbrklim THEN r:=fserr(BOOLEAN(Smemlim)); RETURN fail;
  ELSE a:=pcch(sbrkmem,sbrkptr); INC(sbrkptr,cnt);
     IF t THEN tprint(" sbrk address %x",a);tprint(" length %d\l",cnt) END;
     RETURN a
  END;
END _sbrk;
 
PROCEDURE findfree():INTEGER;
VAR    i:INTEGER;
       k:INTEGER;
BEGIN
   FOR i:=0 TO fmax DO
      IF ft[i] # NIL THEN
         d:=ft[i];
         IF d^.oflag THEN RETURN i END;
      ELSE RETURN i
      END;
   END;
   r:=fserr(BOOLEAN(Filelim));
 RETURN fail
END findfree;
 
PROCEDURE _open(fn:PCCH; mode:INTEGER):INTEGER;
  VAR i:INTEGER;
BEGIN
  IF t THEN tprint("open entry %x\l",fn) END;
  CTOK(fn,kstr);
  i:=findfree();
  IF i=fail THEN RETURN i END;
  IF ft[i]=NIL THEN d:=cp; INC(cp,SIZE(FR)); d^.fbuf:=NIL; d^.oflag:=closed
  ELSE  d:=ft[i]
  END;
  Str1(d^.ffn,kstr);
  d^.fmode:=BITSET(mode)*{0..10};
  IF kopen(d^) THEN
    IF t THEN Show("Open failed") END;
    RETURN fail
  ELSE
    ft[i]:=d; d^.oflag:=opened;
    IF t THEN _look(i) END;
    RETURN i
  END;
END _open;

PROCEDURE check(fd:INTEGER):BOOLEAN;
BEGIN
 IF (fd<0) OR (fd>fmax) THEN r:=fserr(BOOLEAN(Descrout)); RETURN TRUE END;
 d:=ft[fd];
 IF (d=NIL) OR (d^.oflag=closed) THEN r:=fserr(BOOLEAN(Notop));RETURN TRUE END;
 RETURN FALSE;
END check;

PROCEDURE _close(fd: INTEGER): INTEGER;
BEGIN
 IF t THEN tprint(" close entry fd: %d\l",fd) END;
 IF check(fd) THEN RETURN fail END;
 r:=kclose(d^) ;
 IF r THEN AppStr(errmsg,":  file:"); AppStr(errmsg,d^.ffn);
    IF t THEN _look(fd) END;
    RETURN fail
 ELSE d^.oflag:=closed;
    IF t THEN _look(fd) END;
    RETURN ok
 END;
END _close;

PROCEDURE _read(fd:INTEGER; buf:PCCH;cnt:INTEGER):INTEGER;
BEGIN
       IF t THEN WriteLn; tprint("read from fd: %d",fd);
                 tprint(" into buffer at %x",buf);
                 tprint(" count %d\l",cnt)
       END;
 IF check(fd) OR (O_WRONLY IN d^.fmode) THEN RETURN fail
 ELSE RETURN kread (d^,buf,cnt)
 END;
END _read;

PROCEDURE _write(fd:INTEGER;buf:PCCH;cnt:INTEGER):INTEGER;
VAR i:INTEGER;
BEGIN
       IF t THEN tprint("write to fd: %d",fd);
                 tprint(" from buffer at %x",buf);
                 tprint(" count %d\l",cnt)
       END;
 IF check(fd) OR NOT BOOLEAN(d^.fmode*{O_WRONLY,O_RDWR}) THEN RETURN fail
 ELSE 
    IF (d^.fcount > d^.fsize) & BOOLEAN(d^.fdev*{D_FS,D_RA}) THEN
       i:=d^.fcount; d^.fcount:=d^.fsize;
       IF kwrite(d^,0,i-d^.fsize)=fail THEN RETURN fail END;
    END;
    RETURN kwrite(d^,buf,cnt) 
 END;
END _write;
 
PROCEDURE _lseek(fd,offs,wh:INTEGER):INTEGER;
VAR i:INTEGER;
BEGIN
   IF t THEN
      tprint("lseek fd: %d",fd);tprint(" offs: %d",offs);tprint(" wh%d\l: ",wh)
   END;
   IF check(fd) THEN RETURN fail ELSE i:=klseek(d^,offs,wh) END;
   IF t THEN Show2("file ",d^.ffn); tprint(" positioned at %d\l",i) END;
   RETURN i
END _lseek;
 
PROCEDURE _termid(terminal:PCCH);
BEGIN
       KTOC(termstr,terminal);
END _termid;

PROCEDURE _isatty(fd:INTEGER):BOOLEAN;
BEGIN
       RETURN (NOT(check(fd))) & (D_TTY IN d^.fdev)
END _isatty;

PROCEDURE release;
VAR i,j:INTEGER;
BEGIN
 IF ts THEN
   i:=sbrkptr DIV 1024;
   IF sbrkptr MOD 1024 # 0 THEN INC(i) END;
   tprint("Heap    used %d",i);Show(" K");
   tprint("Buffers used %d",bufcnt*blksize DIV 1024);Show(" K");
 END;
   FOR i:=0 TO fmax DO
      IF ft[i]#NIL THEN
         d:=ft[i];
         IF d^.oflag=opened THEN j:=_close(i);
         END;
      END;
   END;
   IF ts THEN tprint("Time    used %d",_time(p0)-intime);Show(" sec") END;
   IF trfp^.fbuf # NIL THEN
      trace:=FALSE;
      DROP(kclose(trfp^));
      DEALLOCATE(trfp^.fbuf,blksize DIV 4)
   END;
END release;

PROCEDURE deallocate; -- (w:INTEGER);
BEGIN
   IF sbrkmem # NIL THEN
      IF t THEN
         tprint("Heap    released: %x",sbrkmem);tprint("  %d",sbrklim DIV 1024)
;
         Show(" K");
      END;
      DEALLOCATE(sbrkmem,sbrklim DIV 4);
   END;
   IF bufpool # NIL THEN
      IF t THEN
         tprint("Buffers released: %x",bufpool);
         tprint("  %d",maxbuf*blksize DIV 1024); Show(" K");
      END;
      DEALLOCATE(bufpool,maxbuf*blksize DIV 4) ;
   END;
END deallocate;

PROCEDURE _exit(status:INTEGER);
  VAR i:INTEGER;
BEGIN
   IF t THEN tprint("_exit entry. Status = %d\l",status) END;
   release;
   IF status=0 THEN HALT ELSE HALT(status) END;
END _exit;

PROCEDURE _abort();
BEGIN
   IF t THEN Show(" _abort entry") END;
   release; HALT(1)
END _abort;

TYPE ProcLink =
     RECORD
        G?: ADDRESS;
        L?: ADDRESS;
       PC?: BITSET;
        M?: BITSET;
     END;

TYPE Link=POINTER TO ProcLink;
TYPE Str=ARRAY [0..31] OF CHAR;

-- Module KRONOS does not include this procedure 09-Jun-88 SS
PROCEDURE DeRef(w:WORD):ADDRESS;
CODE mcd.lsw0 END DeRef;

PROCEDURE MyHost(VAR s:ARRAY OF CHAR);
VAR u,d:Link;
    p:POINTER TO Str;
    g:ADDRESS;

  PROCEDURE LGA00(): ADDRESS; CODE mcd.lga 00 END LGA00;
  PROCEDURE LLA00(): ADDRESS; CODE mcd.lla 00 END LLA00;

BEGIN
   Str0(s); g:=LGA00(); u:=LLA00(); d:=u^.L?;
   WHILE d^.L?#NIL DO
      IF (ExternalBit IN u^.PC?) THEN
         g:=u^.G?
      END;
      u:=d;d:=d^.L?
   END;
   IF (ExternalBit IN u^.PC?) THEN
      p:=DeRef(u^.G?+1);
      IF (p^ # "Tasks") THEN AppStr(s,p^); RETURN END;
   END;
   p:=DeRef(g+1); AppStr(s,p^);
END MyHost;

PROCEDURE finish; BEGIN deallocate END finish;

PROCEDURE _stargs(cl,name: PCCH; heap,bufc: INTEGER; erradd: ADDRESS);
  VAR fn: PCCH;
       i: INTEGER;
BEGIN
   errnro:=ADDRESS(erradd);    ----- !!!!!!!!!!!!!!!!!!!!!!!!
   KTOC(kparm,cl); MyHost(kstr); KTOC(kstr,name);
   IF t THEN Show("_stargs limits:");tprint("Heap=%d",heap);
             tprint("   Bufc=%d\l",bufc);
             print("   Args:%s\n",kparm)
   END;
   IF sbrklim < 0 THEN sbrklim:=heap END;
   IF maxbuf  < 0 THEN maxbuf :=bufc END;
   sbrklim:=sbrklim*1024;
   IF sbrklim > 0 THEN ALLOCATE(sbrkmem,sbrklim DIV 4);
   ELSE sbrkmem:=NIL
   END;
   IF maxbuf > 0 THEN ALLOCATE(bufpool,maxbuf*blksize DIV 4);
   ELSE bufpool:=NIL
   END;
   IF t THEN
      tprint("Heap    %d",sbrklim DIV 1024);
      tprint("K at %x\l",sbrkmem);
      tprint("Buffers %d",maxbuf*blksize DIV 1024);
      tprint("K at %x\l",bufpool)
   END;
   IF ((sbrkmem=NIL) & (sbrklim > 0)) OR
      ((bufpool=NIL) & (maxbuf  > 0)) THEN
         print("Not enough memory for heap or buffers\n");
         deallocate; (* ShowMem; *) _abort
   ELSE
     reso.Final(finish);
(*
     my:=MyTask();
     my^.StorageLink:=WORD(deallocate)
*)
   END;
   fn:=pcch(ADR(termstr),0);
   IF (_open(fn,0)#0) OR
      (_open(fn,1)#1) OR
      (_open(fn,1)#2) THEN print("Can't open standard files\n"); _abort
   END;
END _stargs;

PROCEDURE prch(c:CCH);
BEGIN
END prch;

PROCEDURE _zero(a:ADDRESS; sz:INTEGER);
  PROCEDURE move(to,from: ADDRESS; size: INTEGER);
  CODE mcd.move END move;
BEGIN
 a^:=0;
 IF sz>1 THEN move(a+1,a,sz-1) END;
END _zero;

PROCEDURE _access(pn:PCCH; mode:INTEGER):INTEGER;
VAR f:INTEGER;
BEGIN
   f:=_open(pn,0);
   IF f=fail THEN RETURN fail ELSE RETURN _close(f) END;
END _access;

PROCEDURE _getpid():INTEGER;
BEGIN RETURN pid END _getpid;

PROCEDURE _unlink(pn:PCCH):INTEGER;
BEGIN
   CTOK(pn,kstr);RETURN kunlink(kstr)
END _unlink;

PROCEDURE _rename(pno,pnn:PCCH):INTEGER;
VAR kno,knn:ARRAY [0..31] OF CHAR;
BEGIN
   CTOK(pno,kno); CTOK(pnn,knn);
   RETURN krename(kno,knn)
END _rename;

PROCEDURE _chdir(pn:PCCH):INTEGER;
VAR kn:ARRAY [0..31] OF CHAR;
    line:ARRAY [0..35] OF CHAR;
BEGIN
   CTOK(pn,kn);
   IF kchd(kn)=ok THEN
      Str1(line,"cd "); CTOK(pn,kn); AppStr(line,kn);
      IF shell(line)=0 THEN
         IF t THEN WrStr("chdir ");Show2(kn,": ok") END;
         RETURN ok 
      END;
   END;
   IF t THEN WrStr("chdir ");Show2(kn,": failed") END;
   RETURN fail
END _chdir;

PROCEDURE _mkdir(pn:PCCH):INTEGER;
VAR kn:ARRAY [0..31] OF CHAR;
BEGIN
   CTOK(pn,kn);
   IF t THEN Show2("mkdir ",kn) END;
   RETURN kmkdir(kn)
END _mkdir;

PROCEDURE _rmdir(pn:PCCH):INTEGER;
VAR kn:ARRAY [0..31] OF CHAR;
    line:ARRAY [0..35] OF CHAR;
BEGIN
   Str1(line,"rm "); CTOK(pn,kn); AppStr(line,kn); AppStr(line," -ql");
   IF t THEN Show2("rmdir ",kn) END;
   IF shell(line)=0 THEN RETURN ok END;
   RETURN fail
END _rmdir;

PROCEDURE _iocntrl(fd,op:INTEGER; buf:PCCH):INTEGER;
VAR n:INTEGER;  bufa:ADDRESS;
BEGIN
   IF check(fd) THEN RETURN fail
   ELSE
      n:=pind(buf,bufa);
      IF kiocntrl(d^,op,bufa) THEN RETURN fail
      ELSE RETURN ok
      END;
   END;
END _iocntrl;

(****************** Interrupt handler ************************)

PROCEDURE _setipt(ipt:INTEGER; flag:BOOLEAN);

  PROCEDURE lla03(): ADDRESS; CODE mcd.lla 03 END lla03;
  PROCEDURE getm(): BITSET;   CODE mcd.getm   END getm;

  VAR mask: POINTER TO BITSET;
        pc: POINTER TO BITSET;

BEGIN mask:=lla03(); pc:=lla03()-1;
  IF    ipt>=40h THEN ipt:=31
  ELSIF ipt>=20h THEN ipt:=0
  END;
  IF flag     THEN mask^:=getm()+{ipt}
  ELSE             mask^:=getm()-{ipt}
  END;
  INCL(pc^,ChangeMaskBit);
END _setipt;

(************************** MAIN *****************************)

VAR i: INTEGER;

BEGIN
 FOR i:=0 TO HIGH(ft)   DO ft[i]:=NIL END;
 cp:=ADR(pool[0]);
 _zero(cp,(fmax+1)*SIZE(FR));
 sbrkptr:=0;
 t:=tc;
 pid:=Ticker(); p0:=ADDRESS(0); intime:=_time(p0);
 IF t THEN tprint("Process Id %d\l",pid);WriteLn; END;
(* initipt; *)
END c_run.
