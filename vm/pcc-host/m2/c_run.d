DEFINITION MODULE c_run  ; (* H.Saar 10-Aug-86. *)

(* C-Run Time System  C-RTS

     This module contains a set of low level procedures called
from  C-application  programs  and  C-RTS  itself. All calling
sequences   should   be   satisfy   conventions  for  MODULA-2
procedures  and  data  structures.  Some of procedures in turn
will  call  lower  level procedures from module k_run. All the
implementation  depending  constants  are  defined  in  module
k_run.d.

*)

FROM   k_run  IMPORT PCCH,CSTR,CCH;
FROM   SYSTEM IMPORT ADDRESS;

TYPE PINT=POINTER TO INTEGER;

PROCEDURE _getpid(): INTEGER;                            (* 1 *)

PROCEDURE _sbrk(cnt: INTEGER): PCCH;                     (* 2 *)
(*   sbrk  increments  the  current  highest  memory  location
allocated  for the program by cnt bytes. It returns an address
of  the  previous  highest  location.  Thus sbrk(0) returns an
address  of  the current highest location without altering its
value. If there is insufficient memory to satisfy the request,
-1 is returned.
     Memory  is  always  allocated  on word boundary. Returned
address  is  the  address  of the first allocated word. A word
contains 4 bytes.
*)

PROCEDURE _open(fn: PCCH; oflag: INTEGER): INTEGER;     (* 3 *)
(*

     open  is  the  fundamental  means  of  opening  files for
reading  and  writing. The file specified by fn is sought, and
if  found  is  opened  for  reading,  writing  or  both.  On a
successful  open,  a  file  descriptor  is returned. This is a
non-negative  integer  which  may be used to refer to the open
file  subsequently.  If  the  open  fails, -1 is returned. The
filename may be
     1) the full path from the root or
     2)  the  subpath  from the current directory.
     The  special  device name "/dev/tty" for console terminal
is also recognized.
*)

PROCEDURE _access(path: PCCH;  mode: INTEGER): INTEGER; (* 4 *)

PROCEDURE _close(fd: INTEGER): INTEGER;                 (* 5 *)
(*   This  procedure  closes  the  file  associated  with  the
descriptor  fd,  which will have been previously obtained from
call  to open(). Close returns 0 for a successful close, or -1
otherwise.
*)

PROCEDURE _unlink(path: PCCH): INTEGER;                 (* 6 *)

PROCEDURE _read(fd: INTEGER; buf: PCCH; cnt: INTEGER): INTEGER; (* 7 *)
(*   read()  will  read from the file associated with fd up to
cnt  bytes into a buffer located at buf. It returns the number
of bytes actually read. A zero return indicates end-of-file. A
negative  return indicates error. Fd should have been obtained
from  a  previous call to open(). It is possible for read() to
return  less  bytes than requested, e.g. when reading from the
console, in which case read() will read one line of input.
*)

PROCEDURE _write(fd: INTEGER; buf: PCCH; cnt: INTEGER): INTEGER; (* 8 *)
(*   write() will write from the buffer at buf up to cnt bytes
to the file associated with the file descriptor fd. The number
of bytes actually written will be returned. -1 or a value less
than  cnt  will  be returned on error. In any case, any return
value not equal to cnt should be treated as an error.
*)

PROCEDURE _lseek(fd,offs,wh: INTEGER): INTEGER;         (* 9 *)
(*   lseek()  positions  the "file pointer" (i.e. a pointer to
the  next  character  to  be read or written) of the specified
file as follows:
               wh     resultant
                0     relative begin of file
                1     relative current position
                2     relative file eof
*)

PROCEDURE _termid( terminal: PCCH);                     (* 10 *)
PROCEDURE _isatty(fd: INTEGER): BOOLEAN;                (* 11 *)
PROCEDURE _exit(status: INTEGER);                       (* 12 *)
PROCEDURE _abort();                                     (* 13 *)
PROCEDURE _stargs(cl,name: PCCH;  heap,bufc: INTEGER;  errnoadd: PINT);
                                                        (* 14 *)
PROCEDURE _ctime(clock: PINT): PCCH;                    (* 15 *)
PROCEDURE prch(c: CCH);                                 (* 16 *)
PROCEDURE _system(cmdl: PCCH): INTEGER;                 (* 17 *)
PROCEDURE _perror(): PCCH;                              (* 18 *)
PROCEDURE _look(i: INTEGER);                            (* 19 *)
PROCEDURE _time(tloc: PINT): INTEGER;                   (* 20 *)
PROCEDURE _rename(pno,pnn: PCCH): INTEGER;              (* 21 *)
PROCEDURE _zero(a: ADDRESS; sz: INTEGER);               (* 22 *)
PROCEDURE _iocntrl(fd,op: INTEGER; buf: PCCH): INTEGER; (* 23 *)
PROCEDURE _setipt(ipt: INTEGER;  flag: BOOLEAN);        (* 24 *)
PROCEDURE _chdir(pn: PCCH): INTEGER;                    (* 25 *)
PROCEDURE _mkdir(pn: PCCH): INTEGER;                    (* 26 *)
PROCEDURE _rmdir(pn: PCCH): INTEGER;                    (* 27 *)

END c_run.
