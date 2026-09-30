IMPLEMENTATION MODULE c_run; (* h.saar 12-aug-86. (c) KRONOS *)
  (* minimal console-only port to the current OS API. Real: _write/_sbrk/
     _stargs/_zero/_exit and the console _open path; everything else stubbed.
     Imports only present modules (k_run, Heap, StdIO, Strings, lowLevel).
     host-port 2026-07. *)

FROM SYSTEM    IMPORT  ADR, WORD, ADDRESS;
FROM k_run     IMPORT  kopen,kclose,kread,kwrite,klseek,
                       pind,WriteLn,Show2,Show,WrStr,
                       trace,trfp,kparm,options,
                       termstr,minfserr,errmsg,errnro,fserr,
                       Smemlim,Descrout,Notop,Filelim,
                       sbrklim,bufpool,blksize,maxbuf,bufcnt,
                       O_WRONLY,O_RDWR,O_NDELAY,O_APPEND,O_SYNC,O_CREAT,O_TRUNC,
                       O_EXCL,D_FS,D_TTY,D_LP,D_RA,
                       fmax,fail,ok,FR,opened,closed,PCCH,CSTR,CCH;
FROM Heap      IMPORT  ALLOCATE, DEALLOCATE;
FROM StdIO     IMPORT  WriteString;
FROM Strings   IMPORT  copy;   (* current API: was Str1 *)
IMPORT  low: lowLevel;

VAR    ft         : ARRAY [0..fmax] OF ADDRESS;
       d          : POINTER TO FR;
       csp        : POINTER TO CSTR;
       sbrkmem,cp : ADDRESS;
       r,t        : BOOLEAN;
       pool       : ARRAY [0..fmax] OF FR;
       pid,sbrkptr: INTEGER;
       kstr       : ARRAY [0..63] OF CHAR;
       p0         : PINT;

PROCEDURE pcch(a:ADDRESS; b:INTEGER):PCCH;
BEGIN RETURN a * 4 + b END pcch;

PROCEDURE KTOC(VAR k:ARRAY OF CHAR; c: PCCH);
  VAR i: INTEGER; ch: CHAR;
BEGIN
   c:=pind(c,csp); i:=0;
   REPEAT ch:=k[i]; csp^[c]:=ch; INC(i); INC(c); UNTIL ch=0c;
END KTOC;

PROCEDURE CTOK(c:PCCH; VAR k: ARRAY OF CHAR);
  VAR i,l: INTEGER;
BEGIN
   c:=pind(c,csp); i:=0; l:=HIGH(k);
   WHILE (csp^[c+i]#0c) & (i<l) DO k[i]:=csp^[c+i]; INC(i) END;
   k[i]:=0c;
END CTOK;

PROCEDURE _getpid():INTEGER;
BEGIN RETURN pid END _getpid;

PROCEDURE _sbrk(cnt:INTEGER):PCCH;
  VAR a:PCCH;
BEGIN
  cnt:=((cnt+3) DIV 4 ) * 4;
  IF sbrkptr+cnt > sbrklim THEN r:=fserr(BOOLEAN(Smemlim)); RETURN fail;
  ELSE a:=pcch(sbrkmem,sbrkptr); INC(sbrkptr,cnt); RETURN a
  END;
END _sbrk;

PROCEDURE findfree():INTEGER;
  VAR i:INTEGER;
BEGIN
   FOR i:=0 TO fmax DO
      IF ft[i] # NIL THEN d:=ft[i]; IF d^.oflag THEN RETURN i END;
      ELSE RETURN i END;
   END;
   r:=fserr(BOOLEAN(Filelim)); RETURN fail
END findfree;

PROCEDURE _open(fn:PCCH; mode:INTEGER):INTEGER;
  VAR i:INTEGER;
BEGIN
  CTOK(fn,kstr);
  i:=findfree();
  IF i=fail THEN RETURN i END;
  IF ft[i]=NIL THEN d:=cp; INC(cp,SIZE(FR)); d^.fbuf:=NIL; d^.oflag:=closed
  ELSE d:=ft[i] END;
  copy(d^.ffn,kstr);
  d^.fmode:=BITSET(mode)*{0..10};
  IF kopen(d^) THEN RETURN fail
  ELSE ft[i]:=d; d^.oflag:=opened; RETURN i END;
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
 IF check(fd) THEN RETURN fail END;
 r:=kclose(d^);
 IF r THEN RETURN fail ELSE d^.oflag:=closed; RETURN ok END;
END _close;

PROCEDURE _read(fd:INTEGER; buf:PCCH;cnt:INTEGER):INTEGER;
BEGIN
 IF check(fd) OR (O_WRONLY IN d^.fmode) THEN RETURN fail
 ELSE RETURN kread(d^,buf,cnt) END;
END _read;

PROCEDURE _write(fd:INTEGER;buf:PCCH;cnt:INTEGER):INTEGER;
BEGIN
 IF check(fd) OR NOT BOOLEAN(d^.fmode*{O_WRONLY,O_RDWR}) THEN RETURN fail
 ELSE RETURN kwrite(d^,buf,cnt) END;
END _write;

PROCEDURE _lseek(fd,offs,wh:INTEGER):INTEGER;
BEGIN
 IF check(fd) THEN RETURN fail ELSE RETURN klseek(d^,offs,wh) END;
END _lseek;

PROCEDURE _termid(terminal:PCCH);
BEGIN KTOC(termstr,terminal) END _termid;

PROCEDURE _isatty(fd:INTEGER):BOOLEAN;
BEGIN RETURN (NOT(check(fd))) & (D_TTY IN d^.fdev) END _isatty;

PROCEDURE _zero(a:ADDRESS; sz:INTEGER);
BEGIN
 a^:=0;
 IF sz>1 THEN low.move(a+1,a,sz-1) END;
END _zero;

PROCEDURE _exit(status:INTEGER);
BEGIN
 IF sbrkmem # NIL THEN DEALLOCATE(sbrkmem,sbrklim DIV 4) END;
 IF status=0 THEN HALT ELSE HALT(status) END;
END _exit;

PROCEDURE _abort();
BEGIN HALT(1) END _abort;

PROCEDURE _stargs(cl,name: PCCH; heap,bufc: INTEGER; errnoadd: PINT);
  VAR fn: PCCH; i: INTEGER;
BEGIN
   errnro:=ADDRESS(errnoadd);   (* PINT -> anonymous ptr via ADDRESS (name equiv) *)
   KTOC(kparm,cl);
   IF sbrklim < 0 THEN sbrklim:=heap END;
   sbrklim:=sbrklim*1024;
   IF sbrklim > 0 THEN ALLOCATE(sbrkmem,sbrklim DIV 4) ELSE sbrkmem:=NIL END;
   sbrkptr:=0;
   IF (sbrkmem=NIL) & (sbrklim > 0) THEN
      WriteString("no memory for C heap"); WriteLn; _abort
   END;
   fn:=pcch(ADR(termstr),0);
   i:=_open(fn,0); i:=_open(fn,1); i:=_open(fn,1);  (* open stdin/out/err; console fds are interchangeable here *)
END _stargs;

PROCEDURE prch(c:CCH); BEGIN END prch;

PROCEDURE _access(pn:PCCH; mode:INTEGER):INTEGER;
BEGIN RETURN fail END _access;
PROCEDURE _unlink(pn:PCCH):INTEGER;   BEGIN RETURN fail END _unlink;
PROCEDURE _rename(pno,pnn:PCCH):INTEGER; BEGIN RETURN fail END _rename;
PROCEDURE _chdir(pn:PCCH):INTEGER;    BEGIN RETURN fail END _chdir;
PROCEDURE _mkdir(pn:PCCH):INTEGER;    BEGIN RETURN fail END _mkdir;
PROCEDURE _rmdir(pn:PCCH):INTEGER;    BEGIN RETURN fail END _rmdir;
PROCEDURE _system(cmdl:PCCH):INTEGER; BEGIN RETURN fail END _system;
PROCEDURE _perror():PCCH;             BEGIN RETURN pcch(ADR(errmsg),0) END _perror;
PROCEDURE _look(i:INTEGER);           BEGIN END _look;
PROCEDURE _time(tloc:PINT):INTEGER;   BEGIN RETURN 0 END _time;
PROCEDURE _ctime(clock:PINT):PCCH;    BEGIN RETURN pcch(ADR(errmsg),0) END _ctime;
PROCEDURE _iocntrl(fd,op:INTEGER; buf:PCCH):INTEGER; BEGIN RETURN fail END _iocntrl;
PROCEDURE _setipt(ipt:INTEGER; flag:BOOLEAN); BEGIN END _setipt;

VAR ini: INTEGER;
BEGIN
  cp:=ADR(pool); sbrkptr:=0; sbrkmem:=NIL; pid:=1; p0:=NIL;
  FOR ini:=0 TO fmax DO ft[ini]:=NIL END;   (* clear the fd table: the loader does NOT zero BSS *)
END c_run.
