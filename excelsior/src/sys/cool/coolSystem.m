IMPLEMENTATION MODULE coolSystem; (* Leo  05-Jun-88. (c) KRONOS *)
                                  (* Ned  04-Mar-90. (c) KRONOS *)
                                  (* Hady 15-Sep-25. (c) KRONOS *)

IMPORT  sys: SYSTEM;
IMPORT  str: Strings;
IMPORT  env: tskEnv;
IMPORT  tty: Terminal;
IMPORT   tm: Time;
IMPORT  mem: Heap;

---------------------------  DEBUG  ----------------------------
                           ---------

PROCEDURE xprint(VAL fmt: ARRAY OF CHAR; SEQ args: sys.WORD);
BEGIN tty.print(fmt,args)
END xprint;

--------------------------  SERVICE  ---------------------------
                          -----------

PROCEDURE final(p: PROC);  BEGIN env.final(p) END final;

PROCEDURE time(): INTEGER; BEGIN RETURN tm.time() END time;

PROCEDURE app_time(VAR s: ARRAY OF CHAR; time: INTEGER);
  VAR y,m,d,h,mi,sc: INTEGER;
BEGIN
  tm.unpack(time,y,m,d,h,mi,sc);
  str.append(s,'%$2d-%$2d-%$2d  %$2d:%$2d.%$2d',d,m,y MOD 100,h,mi,sc);
END app_time;

PROCEDURE append(VAR s: ARRAY OF CHAR; VAL f: ARRAY OF CHAR; SEQ args: sys.WORD);
BEGIN str.append(s,f,args) END append;

PROCEDURE sprint(VAR s: ARRAY OF CHAR; VAL f: ARRAY OF CHAR; SEQ args: sys.WORD);
BEGIN str.print(s,f,args) END sprint;

----------------------------  HEAP  ----------------------------
                            --------

TYPE
  slot_ptr = POINTER TO slot_rec;
  slot_rec = RECORD next: slot_ptr END;

VAR slots: ARRAY [0..15] OF slot_ptr;

PROCEDURE ini_heap;
  VAR i: INTEGER;
BEGIN
  FOR i:=0 TO HIGH(slots) DO slots[i]:=NIL END;
END ini_heap;

PROCEDURE DEALLOCATE(VAR a: sys.ADDRESS; size: INTEGER);
  VAR s: slot_ptr;
BEGIN
  IF (a=NIL) OR (size<=0) THEN RETURN END;
  IF size<=HIGH(slots) THEN s:=a;
    s^.next:=slots[size]; slots[size]:=s;
  ELSE mem.DEALLOCATE(a,size);
  END;
  a:=NIL;
END DEALLOCATE;

PROCEDURE release_slots(): BOOLEAN;
  VAR i: INTEGER; l,s: slot_ptr;
BEGIN
  FOR i:=1 TO HIGH(slots) DO
    IF slots[i]#NIL THEN l:=slots[i];
      REPEAT s:=l; l:=s^.next; mem.DEALLOCATE(s,i) UNTIL l=NIL;
      slots[i]:=NIL;
    END;
  END;
  RETURN TRUE
END release_slots;

PROCEDURE ALLOCATE(VAR a: sys.ADDRESS; size: INTEGER);
  VAR s: slot_ptr;
BEGIN
  IF (size<=HIGH(slots)) & (slots[size]#NIL) THEN
    s:=slots[size]; slots[size]:=s^.next; a:=s;
  ELSE
    mem.ALLOCATE(a,size);
    IF a#NIL THEN RETURN END;
    IF release_slots() THEN END;
    mem.ALLOCATE(a,size);
  END;
END ALLOCATE;

PROCEDURE REALLOCATE(VAR a: sys.ADDRESS; VAR hi: INTEGER;
                                      len,bytes: INTEGER);
BEGIN mem.REALLOCATE(a,hi,len,bytes);
END REALLOCATE;

WITH STORAGE (NEW: ALLOCATE; DISPOSE: DEALLOCATE);

---------------------------  QUEUEs  ---------------------------
                           ----------

TYPE
  NODE  = POINTER TO node;
  node  = RECORD
            info: sys.WORD;
            next: NODE;
          END;
  QUEUE = POINTER TO queue;
  queue = RECORD
            fifo: BOOLEAN;
            head: NODE;
            tail: NODE;
          END;

PROCEDURE open(VAR q: QUEUE; fifo: BOOLEAN);
BEGIN NEW(q);
  q^.head:=NIL; q^.tail:=NIL; q^.fifo:=fifo;
END open;

PROCEDURE lifo(VAR q: QUEUE); BEGIN open(q,FALSE) END lifo;

PROCEDURE fifo(VAR q: QUEUE); BEGIN open(q,TRUE ) END fifo;

PROCEDURE clear(VAR q: QUEUE);
  VAR l,n: NODE;
BEGIN
  ASSERT(q#NIL);
  IF q^.head#NIL THEN l:=q^.head;
    WHILE l#NIL DO n:=l; l:=l^.next; DISPOSE(n) END;
  END;
  DISPOSE(q);
END clear;

PROCEDURE push(q: QUEUE; one: sys.WORD);
  VAR n: NODE;
BEGIN ASSERT(q#NIL);
  NEW(n); n^.info:=one; n^.next:=NIL;
  IF    q^.head=NIL THEN q^.head:=n; q^.tail:=n
  ELSIF q^.fifo     THEN q^.tail^.next:=n; q^.tail:=n;
  ELSE(*q^.stack*)       n^.next:=q^.head; q^.head:=n;
  END;
END push;

PROCEDURE pop(q: QUEUE; VAR one: sys.WORD): BOOLEAN;
  VAR n: NODE;
BEGIN
  ASSERT(q#NIL);
  IF q^.head=NIL THEN RETURN FALSE END;
  n:=q^.head; one:=n^.info;
  q^.head:=n^.next;
  DISPOSE(n);
  RETURN TRUE
END pop;

---------------------------------------------------------------

PROCEDURE unimplemented; BEGIN ASSERT(FALSE,51h) END unimplemented;

BEGIN
  ini  :=sys.WORD(unimplemented);
  exi  :=sys.WORD(unimplemented);
  error:=sys.WORD(unimplemented);
  print:=sys.WORD(unimplemented);
  ini_heap;
  mem.install_gc(release_slots);
END coolSystem.
