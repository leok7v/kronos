DEFINITION MODULE fGen; (* Max *)

FROM SYSTEM    IMPORT WORD;
FROM fScan     IMPORT Filename;

VAR cp     : INTEGER;   (* указатель в COD *)
    codfile: Filename;  (* имя кодофайла main *)
    MinPS  : INTEGER;   (* размер мультизначений *)

PROCEDURE LoadVal(idno: INTEGER);
PROCEDURE LoadAdr(idno: INTEGER);
PROCEDURE LoadLocVal(idno: INTEGER);
PROCEDURE StoreInVar(idno: INTEGER);
PROCEDURE MarkC; PROCEDURE MarkC1;
PROCEDURE BackC; PROCEDURE BackC1;
PROCEDURE epush;
PROCEDURE epop;
PROCEDURE c (Command: INTEGER);
PROCEDURE c1(Command: INTEGER; byte: INTEGER);
PROCEDURE li  (n: INTEGER);
PROCEDURE llw (n: INTEGER);
PROCEDURE slw (n: INTEGER);
PROCEDURE lgw (n: INTEGER);
PROCEDURE sgw (n: INTEGER);
PROCEDURE lsw (n: INTEGER);
PROCEDURE ssw (n: INTEGER);
PROCEDURE SetJLF(): INTEGER;
PROCEDURE SetJLFC(): INTEGER;
PROCEDURE JB (cond:BOOLEAN; To: INTEGER);
PROCEDURE Jump(From,To: INTEGER);
PROCEDURE Alloc(sz: INTEGER);
PROCEDURE Trap(no: INTEGER);
PROCEDURE CL(p: INTEGER);
PROCEDURE CallExt(m: INTEGER);
PROCEDURE Getdepth(): INTEGER;
PROCEDURE Setdepth(d: INTEGER);
PROCEDURE Store;
PROCEDURE Lodfv;
PROCEDURE InsertCode(bou,byte: INTEGER);
PROCEDURE Enter(VAR s: INTEGER);
PROCEDURE PutCod(pos,cod: INTEGER);
PROCEDURE FinishProc0;
PROCEDURE StartProc1;
PROCEDURE StartFunc(): INTEGER;
PROCEDURE InitGen(d: INTEGER);
PROCEDURE WriteCodeFile;
PROCEDURE PutExt(idno: INTEGER): INTEGER;
PROCEDURE FinishCod;
PROCEDURE SvalToPool(): INTEGER;

END fGen.