DEFINITION MODULE StdIO; (* Andy 17-Oct-89. (c) KRONOS *)
                         (* Leo  27-Jun-90. (c) KRONOS *)

IMPORT  SYSTEM;
IMPORT  BIO;

             (* Стандартный потоковый ввод/вывод *)

VAL done: BOOLEAN;
   error: INTEGER;
   iolen: INTEGER;

VAL EOF: CHAR;

VAR
  in : BIO.FILE;
  out: BIO.FILE;

PROCEDURE Write(ch: CHAR);
PROCEDURE Read(VAR ch: CHAR);
PROCEDURE WriteString(s: ARRAY OF CHAR);
PROCEDURE WriteLn;
PROCEDURE ReadString(VAR s: ARRAY OF CHAR);

PROCEDURE is_tty(s: BIO.FILE): BOOLEAN;

PROCEDURE read    (VAR  ch: CHAR);
PROCEDURE readstr (VAR str: ARRAY OF CHAR);
PROCEDURE write   (     ch: CHAR);
PROCEDURE writestr(    str: ARRAY OF CHAR);
PROCEDURE writeln;

PROCEDURE print (format: ARRAY OF CHAR; SEQ args: SYSTEM.WORD);
PROCEDURE perror(errcod: INTEGER;
                 format: ARRAY OF CHAR; SEQ args: SYSTEM.WORD);


(*************************************************************

VAL done: BOOLEAN; -- Результат выполнения последней операции
   error: INTEGER;
   iolen: INTEGER;

VAL EOF: CHAR;

VAR
  in : BIO.FILE;
  out: BIO.FILE;

PROCEDURE  is_tty(f: BIO.FILE): BOOLEAN;

PROCEDURE Write(ch: CHAR);
PROCEDURE write(ch: CHAR);
(* Выводит один символ в файл стандартного вывода. *)

PROCEDURE Read(VAR ch: CHAR);
PROCEDURE read(VAR ch: CHAR);
(* Читает один символ из файла стандартного ввода;   *)
(* возвращает EOF при попытке чтения за концом файла *)

PROCEDURE WriteString(s: ARRAY OF CHAR);
PROCEDURE writestr   (s: ARRAY OF CHAR);
(* Выдает  в файл стандартного вывода содержимое строки символ
   за  символом. Концом строки считается либо физический конец
   предоставленного  массива, либо символ 0c, в зависимости от
   того, что встретится раньше.
*)

PROCEDURE WriteLn;
PROCEDURE writeln;

PROCEDURE ReadString(VAR s: ARRAY OF CHAR);
PROCEDURE readstr   (VAR s: ARRAY OF CHAR);

PROCEDURE print (format: ARRAY OF CHAR; SEQ args: SYSTEM.WORD);
PROCEDURE perror(errcod: INTEGER;
                 format: ARRAY OF CHAR; SEQ args: SYSTEM.WORD);

*************************************************************)
END StdIO.
