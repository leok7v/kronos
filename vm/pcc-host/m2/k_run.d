DEFINITION MODULE k_run;   (* H.Saar 12.06.87 *)

FROM SYSTEM   IMPORT WORD,ADDRESS;
FROM FsPublic IMPORT File,FileName;

TYPE
 CCH  = CHAR;                      (* C-character *)
 CSTR = ARRAY [0..00FFFFh] OF CCH; (* C-character address space *)
 PCCH = INTEGER;                   (* POINTER to CCH e.g.
                                      PCCH:= ADR(word)*4 + ADR(byte in word)
                                    *)
TYPE
  FR=RECORD
    ffn                  : FileName;
    ffd,fdir             : File;
    fmode,fdev           : BITSET;
    funit,fcount,fsize,bl: INTEGER;
    oflag                : BOOLEAN;
    fbuf                 : ADDRESS;
  END;

VAR
  kparm,errmsg: ARRAY [0..255] OF CHAR;
  termstr           : FileName;              (* Terminal device name *)
  errnro            : POINTER TO INTEGER;    (* Pointer to errno *)
  trace,erro,tc,ts  : BOOLEAN;               (* Tracing options *)
  maxbuf,sbrklim,bufcnt: INTEGER;
  bufpool           : ADDRESS;
  options           : ARRAY [0..15] OF CHAR;
  trfp              : POINTER TO FR;  (* tracing file record pointer *)

CONST
  fmax=19;                   (* max file descriptor value (first is 0) *)
  blksize=4096;
  fail=-1; ok=0; eof=0;      (* return codes *)
  opened=FALSE; closed=TRUE; (* oflag values in FR *)
  minfserr=100;              (* minimal error code handled by Modula part*)

  cntrlD=4c;
  maxdev=16;                 (* max of device types *)
  termid='/dev/tty';

(* Bits in fmode of FR *)
  O_WRONLY=0; -- write only
  O_RDWR  =1; -- read or write
  O_NDELAY=2;
  O_APPEND=3;
  O_SYNC  =4; (* not used *)
  O_CREAT =8;
  O_TRUNC =9;
  O_EXCL  =10;
  B_CHANGED=20; (* current buffer changed *)
  F_UNLINK =21; -- must be linking after close

(* Device type bits in fdev of FR *)
  D_FS  = 0;  (* file system device *)
  D_TTY = 1;  (* terminal *)
  D_LP  = 2;  (* printer  *)
  D_RA  = 3;  (* random access device *)
  D_NULL= 4;  (* null device *)

(* Error codes *)
  Buflim   = 201;     Dirop    = 202;
  Illmode  = 203;     Alrclo   = 204;
  Smemlim  = 205;     Descrout = 206;
  Notop    = 207;     Filelim  = 208;
  Notsupp  = 209;

PROCEDURE pind(adr: PCCH; VAR csa: ADDRESS): INTEGER;
PROCEDURE WriteLn;
PROCEDURE tprint(form: ARRAY OF CHAR; w: WORD);
PROCEDURE WrStr(s: ARRAY OF CHAR);
PROCEDURE Show(s: ARRAY OF CHAR);
PROCEDURE Show2(s1,s2: ARRAY OF CHAR);
PROCEDURE err(m: ARRAY OF CHAR);
PROCEDURE kopen(VAR d: FR): BOOLEAN;
PROCEDURE kclose(VAR d: FR): BOOLEAN;
PROCEDURE kread (VAR d: FR; buf: PCCH;cnt: INTEGER): INTEGER;
PROCEDURE kwrite(VAR d: FR; buf: PCCH;cnt: INTEGER): INTEGER;
PROCEDURE klseek(VAR d: FR;offs,wh: INTEGER): INTEGER;
PROCEDURE kwrch(ch: CHAR);
PROCEDURE fserr(code: BOOLEAN): BOOLEAN;
PROCEDURE kunlink(pn: FileName): INTEGER;
PROCEDURE krename(pno,pnn: FileName): INTEGER;
PROCEDURE kchd(pn: FileName): INTEGER;
PROCEDURE kmkdir(pn: FileName): INTEGER;
PROCEDURE kiocntrl(VAR d: FR; op: INTEGER; buf: ADDRESS): BOOLEAN;

END k_run.
