DEFINITION MODULE k_run;   (* H.Saar 12.06.87 *)
   (* minimal console-only port to the current OS API, 2026-07 *)

FROM SYSTEM   IMPORT WORD,ADDRESS;
IMPORT BIO;                         (* host-port 2026-07: real file I/O via BIO *)

TYPE
 File     = BIO.FILE;               (* host-port: real Excelsior file handle (was INTEGER stub) *)
 FileName = ARRAY [0..63] OF CHAR;  (* host-port: was FsPublic.FileName *)

TYPE
 CCH  = CHAR;                      (* C-character *)
 CSTR = ARRAY [0..00FFFFh] OF CCH; (* C-character address space *)
 PCCH = INTEGER;                   (* POINTER to CCH: word_addr*4 + byte_in_word *)

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
  termstr           : FileName;
  errnro            : POINTER TO INTEGER;
  trace,erro,tc,ts  : BOOLEAN;
  maxbuf,sbrklim,bufcnt: INTEGER;
  bufpool           : ADDRESS;
  options           : ARRAY [0..15] OF CHAR;
  trfp              : POINTER TO FR;

CONST
  fmax=19;
  blksize=4096;
  fail=-1; ok=0; eof=0;
  opened=FALSE; closed=TRUE;
  minfserr=100;

  cntrlD=4c;
  maxdev=16;
  termid='/dev/tty';

  O_WRONLY=0; O_RDWR=1; O_NDELAY=2; O_APPEND=3; O_SYNC=4;
  O_CREAT=8; O_TRUNC=9; O_EXCL=10;
  B_CHANGED=20; F_UNLINK=21;

  D_FS=0; D_TTY=1; D_LP=2; D_RA=3; D_NULL=4;

  Buflim=201; Dirop=202; Illmode=203; Alrclo=204; Smemlim=205;
  Descrout=206; Notop=207; Filelim=208; Notsupp=209;

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
