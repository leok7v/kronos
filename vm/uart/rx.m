MODULE rx;   (* rx <file>: read N bytes into RAM (only key.read), THEN write to disk *)

IMPORT sys: SYSTEM;
IMPORT bio: BIO;
IMPORT std: StdIO;
IMPORT key: Keyboard;
IMPORT arg: tskArgs;

VAR f: bio.FILE;  ch: CHAR;  n, i, sum: INTEGER;
    mem: ARRAY [0..40000] OF CHAR;

BEGIN
  IF HIGH(arg.words) < 0 THEN
    std.print("rx: no arg\n");
  ELSE
    bio.create(f, arg.words[0], 'w', 0);          (* create before receive (no content yet) *)
    IF NOT bio.done THEN
      std.print("rx: create failed\n");
    ELSE
      std.print("rx: ready\n");
      REPEAT key.read(ch) UNTIL (ch >= '0') & (ch <= '9');
      n := 0;
      WHILE (ch >= '0') & (ch <= '9') DO
        n := n*10 + (ORD(ch) - ORD('0'));  key.read(ch)
      END;
      i := 0;  sum := 0;
      WHILE i < n DO                                (* receive: ONLY key.read, no I/O *)
        key.read(ch);  mem[i] := ch;
        sum := (sum + ORD(ch)) MOD 65521;  INC(i)
      END;
      bio.write(f, sys.ADR(mem), n);               (* one bulk write, after all input *)
      bio.close(f);
      std.print("rx: %d bytes sum %d\n", n, sum);
    END;
  END;
END rx.
