DEFINITION MODULE lowLevel; (* Leo 29-Nov-89. (c) KRONOS *) IMPORT  SYSTEM;

VAL
  cpu      : INTEGER;
  cpu_model: INTEGER;
  cpu_vers : INTEGER;   -- version of instruction set

PROCEDURE move(des,sou: SYSTEM.ADDRESS; size: INTEGER);

PROCEDURE cmove(des: SYSTEM.ADDRESS; des_ofs: INTEGER;
                sou: SYSTEM.ADDRESS; sou_ofs: INTEGER; bytes: INTEGER);

PROCEDURE _zero(adr: SYSTEM.ADDRESS; size: INTEGER);
PROCEDURE _fill(adr: SYSTEM.ADDRESS; size: INTEGER; val: SYSTEM.WORD);

PROCEDURE zero(VAR area: ARRAY OF SYSTEM.WORD);
PROCEDURE fill(VAR area: ARRAY OF SYSTEM.WORD; val: SYSTEM.WORD);

PROCEDURE QUIT;

END lowLevel.
