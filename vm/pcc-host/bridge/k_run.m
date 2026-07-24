IMPLEMENTATION MODULE k_run;
  (* C runtime kernel, ported to the current OS API. Console writes go through
     StdIO; real files go through BIO (open/create/read/write/seek/close).
     BIO.check_io(FALSE) so a short read / EOF / FS error reports via BIO.done
     instead of HALTing the whole program. host-port 2026-07. *)

FROM SYSTEM   IMPORT WORD, ADDRESS, ADR;
FROM StdIO    IMPORT Write;
FROM Strings  IMPORT len, copy;
IMPORT low: lowLevel;
IMPORT BIO;
IMPORT env: tskEnv;   (* host-port 2026-07: real command-line args *)

CONST CR = 15c; LF = 12c; NL = 12c;

VAR trec: FR;   (* a static FR for trfp so c_run's release/_system are safe *)

PROCEDURE pind(adr: PCCH; VAR csa: ADDRESS): INTEGER;
BEGIN
  csa := ADDRESS(adr DIV 4);
  RETURN adr MOD 4;
END pind;

PROCEDURE kwrch(ch: CHAR);
BEGIN
  IF ch = LF THEN Write(CR); Write(LF) ELSE Write(ch) END;
END kwrch;

PROCEDURE WrStr(s: ARRAY OF CHAR);
  VAR i: INTEGER;
BEGIN
  FOR i := 0 TO len(s)-1 DO kwrch(s[i]) END;
END WrStr;

PROCEDURE WriteLn;
BEGIN kwrch(LF) END WriteLn;

PROCEDURE Show(s: ARRAY OF CHAR);
BEGIN WrStr(s); WriteLn END Show;

PROCEDURE Show2(s1,s2: ARRAY OF CHAR);
BEGIN WrStr(s1); Show(s2) END Show2;

PROCEDURE tprint(form: ARRAY OF CHAR; w: WORD);
BEGIN WrStr(form) END tprint;   (* stub: no formatting (trace-only path) *)

PROCEDURE err(m: ARRAY OF CHAR);
BEGIN Show(m) END err;

PROCEDURE fserr(code: BOOLEAN): BOOLEAN;
BEGIN
  IF errnro # NIL THEN errnro^ := INTEGER(code) END;
  RETURN TRUE;   (* TRUE == an error occurred *)
END fserr;

(* null-terminated string equality, bounded by both arrays. *)
PROCEDURE streq(a, b: ARRAY OF CHAR): BOOLEAN;
  VAR i: INTEGER;
BEGIN
  i := 0;
  WHILE (i <= HIGH(a)) & (i <= HIGH(b)) DO
    IF a[i] # b[i] THEN RETURN FALSE END;
    IF a[i] = 0c THEN RETURN TRUE END;
    INC(i);
  END;
  RETURN TRUE;
END streq;

PROCEDURE kopen(VAR d: FR): BOOLEAN;
BEGIN
  IF streq(d.ffn, termid) THEN                 (* "/dev/tty" -- the console *)
    d.fdev := {D_TTY}; d.fcount := 0; d.fsize := 0; d.bl := 0; d.fbuf := NIL;
    RETURN FALSE;                              (* FALSE == success *)
  END;
  IF (O_CREAT IN d.fmode) OR (O_TRUNC IN d.fmode) THEN
    BIO.unlink(d.ffn);                         (* O_TRUNC: replace any existing file (BIO.create won't truncate) *)
    IF O_RDWR IN d.fmode THEN
      BIO.create(d.ffd, d.ffn, "rw", 0)        (* "w+" scratch files: readable too *)
    ELSE
      BIO.create(d.ffd, d.ffn, "w", 0)         (* size 0: eof grows with writes *)
    END;
  ELSIF O_RDWR IN d.fmode THEN
    BIO.open(d.ffd, d.ffn, "rw");
  ELSIF O_WRONLY IN d.fmode THEN
    BIO.open(d.ffd, d.ffn, "w");
  ELSE
    BIO.open(d.ffd, d.ffn, "r");
  END;
  IF NOT BIO.done THEN
    IF errnro # NIL THEN errnro^ := BIO.error END;   (* surface the BIO error code via errno *)
    RETURN TRUE
  END;
  d.fdev := {D_FS}; d.fcount := 0; d.fsize := 0; d.bl := 0; d.fbuf := NIL;
  RETURN FALSE;
END kopen;

PROCEDURE kclose(VAR d: FR): BOOLEAN;
BEGIN
  IF D_TTY IN d.fdev THEN RETURN FALSE END;    (* console: nothing to close *)
  BIO.close(d.ffd);
  RETURN NOT BIO.done;                         (* TRUE == error *)
END kclose;

PROCEDURE kread(VAR d: FR; buf: PCCH; cnt: INTEGER): INTEGER;
  VAR off, n, rem, i: INTEGER; p: POINTER TO CSTR; ch: CHAR;
BEGIN
  IF cnt <= 0 THEN RETURN 0 END;
  IF D_TTY IN d.fdev THEN RETURN eof END;      (* console read: EOF (compiler reads files) *)
  off := pind(buf, p);
  IF off = 0 THEN                              (* word-aligned buffer: block read *)
    rem := BIO.eof(d.ffd) - BIO.pos(d.ffd);    (* bytes remaining (eof = file size) *)
    IF rem <= 0 THEN RETURN 0 END;             (* C end-of-file *)
    n := cnt; IF n > rem THEN n := rem END;    (* never over-read (BIO.read HALTs on short) *)
    BIO.read(d.ffd, ADDRESS(p), n);
    IF NOT BIO.done THEN RETURN fail END;
    RETURN n;
  END;
  i := 0;                                      (* unaligned: byte-at-a-time *)
  WHILE i < cnt DO
    BIO.getch(d.ffd, ch);
    IF NOT BIO.done THEN RETURN i END;         (* EOF/error -> short read *)
    p^[off+i] := ch; INC(i);
  END;
  RETURN i;
END kread;

PROCEDURE kwrite(VAR d: FR; buf: PCCH; cnt: INTEGER): INTEGER;
  VAR off, i: INTEGER; p: POINTER TO CSTR;
BEGIN
  off := pind(buf, p);
  IF D_TTY IN d.fdev THEN                      (* console *)
    i := 0;
    WHILE i < cnt DO kwrch(p^[off+i]); INC(i) END;
    RETURN cnt;
  END;
  IF cnt <= 0 THEN RETURN 0 END;
  IF off = 0 THEN                              (* word-aligned buffer: block write *)
    BIO.write(d.ffd, ADDRESS(p), cnt);
    IF NOT BIO.done THEN RETURN fail END;
    RETURN cnt;
  END;
  i := 0;                                      (* unaligned: byte-at-a-time *)
  WHILE i < cnt DO
    BIO.putch(d.ffd, p^[off+i]);
    IF NOT BIO.done THEN RETURN i END;
    INC(i);
  END;
  RETURN cnt;
END kwrite;

PROCEDURE klseek(VAR d: FR; offs,wh: INTEGER): INTEGER;
BEGIN
  IF D_TTY IN d.fdev THEN RETURN 0 END;
  BIO.seek(d.ffd, offs, wh);
  IF NOT BIO.done THEN RETURN fail END;
  RETURN BIO.pos(d.ffd);
END klseek;

PROCEDURE kunlink(pn: FileName): INTEGER;
BEGIN
  BIO.unlink(pn);
  IF BIO.done THEN RETURN ok ELSE RETURN fail END;
END kunlink;

PROCEDURE krename(pno,pnn: FileName): INTEGER;
BEGIN RETURN fail END krename;

PROCEDURE kchd(pn: FileName): INTEGER;
BEGIN RETURN fail END kchd;

PROCEDURE kmkdir(pn: FileName): INTEGER;
BEGIN
  BIO.mkdir(pn, FALSE);
  IF BIO.done THEN RETURN ok ELSE RETURN fail END;
END kmkdir;

PROCEDURE kiocntrl(VAR d: FR; op: INTEGER; buf: ADDRESS): BOOLEAN;
BEGIN RETURN FALSE END kiocntrl;

(* fetch the task's raw argument string ("ARGS") into kparm; the C _getargs
   parses it C-style (so "-o file" stays two argv tokens, unlike tskArgs which
   would fold '-' tokens into flags). *)
PROCEDURE getcmdline;
  VAR s: STRING; i: INTEGER;
BEGIN
  kparm[0] := 0c;
  env.get_str(env.args, s);
  IF env.done THEN
    i := 0;
    WHILE (i <= HIGH(s)) & (i < HIGH(kparm)) & (s[i] # 0c) DO
      kparm[i] := s[i]; INC(i)
    END;
    kparm[i] := 0c;
  END;
END getcmdline;

BEGIN
  BIO.check_io(FALSE);   (* report I/O errors via BIO.done; do NOT HALT the program *)
  trace := FALSE; erro := FALSE; tc := FALSE; ts := FALSE;
  sbrklim := -1; maxbuf := -1; bufcnt := 0; bufpool := NIL;
  errnro := NIL;
  copy(termstr, termid);
  errmsg[0] := 0c; options[0] := 0c;
  getcmdline;            (* kparm := the task's argument string *)
  trec.fbuf := NIL; trec.oflag := closed; trfp := ADR(trec);
END k_run.
