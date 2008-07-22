IMPLEMENTATION MODULE cdsHash; (* 09-Oct-87. (c) KRONOS *)

FROM Model      IMPORT  Tag, Object, Objects;
FROM ModelPbl   IMPORT  RaiseInMe, Message, MemoryOverflow;

CONST HashSize=991;

VAR Objs: ARRAY [0..HashSize-1] OF Object;
    cnt, cntMax: INTEGER;

PROCEDURE Hash(VAR nm: ARRAY OF CHAR; t: Objects; VAR obj: INTEGER): BOOLEAN;
  VAR i,sum: CARDINAL; o: Object; c: CHAR;
BEGIN
  i:=0; sum:=0;
  WHILE nm[i]#0c DO
    c:=nm[i];
    INC(sum,ORD(c)+CARDINAL(ODD(ORD(c)*i)));
    INC(i)
  END;
  sum:=CARDINAL(BITSET(sum*8)/BITSET(sum DIV 32)) MOD HashSize;
  LOOP
    IF Objs[sum]=NIL THEN
      obj:=sum; RETURN FALSE
    ELSE
      o:=Objs[sum];
      IF (nm=o^.Name)&(Tag(o)=t) THEN obj:=sum; RETURN TRUE END;
      sum:=(sum+1) MOD HashSize;
    END
  END;
END Hash;

PROCEDURE Insert(o: Object);
  VAR i: INTEGER;
BEGIN
  IF cnt>=cntMax THEN
    Message:='Переполнена таблица обектов, очень жаль ...';
    RaiseInMe(MemoryOverflow);
  END;
  IF Hash(o^.Name,Tag(o),i) THEN
    Objs[i]:=o;
  ELSE
    Objs[i]:=o; INC(cnt);
  END;
END Insert;

PROCEDURE LookUp(name: ARRAY OF CHAR; t: Objects): Object;
  VAR i: INTEGER;
BEGIN
  IF Hash(name,t,i) THEN RETURN Objs[i] ELSE RETURN NIL END;
END LookUp;

PROCEDURE Init(size: INTEGER);
BEGIN
  FOR cnt:=0 TO HIGH(Objs) DO Objs[cnt]:=NIL END;
  cnt:=0; cntMax:=HIGH(Objs)*3 DIV 4;
END Init;

END cdsHash.
