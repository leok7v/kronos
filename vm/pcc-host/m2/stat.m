MODULE stat; (* Ned 30-Sep-86. (c) KRONOS *)

(* SS 11-Jan-88 version for CRTS *)

IMPORT  fs: FsPublic;

FROM Args      IMPORT   ScanFlags, Flag?, TakeWord, ArgC;
FROM StdIO     IMPORT   Show, WriteLn, print;
FROM FsDrv     IMPORT   ISetSize, DeviceSize;
FROM FsSets    IMPORT   TakeSets, FreezeSets;
FROM BIO       IMPORT   Driver, CD;
FROM Media     IMPORT   DrvVol, VolDrv;



PROCEDURE err(r: BOOLEAN; VAL head: ARRAY OF CHAR);
  VAR msg: ARRAY [0..63] OF CHAR;
BEGIN
  IF r=fs.ok THEN RETURN END;
  fs.VisFSerr(r,msg);
  print("%s %s\n",head,msg);
END err;

VAR
  Name: ARRAY [0..15] OF CHAR;
  u,i : INTEGER;
  info: POINTER TO INTEGER;
  ino,blo: POINTER TO ARRAY [0..1023] OF BITSET;
  ic,bc,k: INTEGER;

BEGIN
  ScanFlags;
  TakeWord(Name);
  IF Name[0]=0c THEN err(Driver(CD(),u) OR DrvVol(u,Name),"cd") END;
  err(VolDrv(Name,u),Name);
(*  print("Информация о носителе %s\n", Name);   *)
  err(TakeSets(u,info,blo,ino),Name);
  ic:=0;
  FOR i:=0 TO ISetSize(u)-1 DO
    IF (i MOD 32) IN ino^[i DIV 32] THEN INC(ic) END;
  END;
  bc:=0;
  k:=(ISetSize(u)+63) DIV 64;
  FOR i:=0 TO DeviceSize(u)-1-k DO
    IF (i MOD 32) IN blo^[i DIV 32] THEN INC(bc) END;
  END;
  err(FreezeSets(FALSE),Name);
(*print("Размер носителя в блоках  %d, из них занято/свободно %d/%d\n" *)
  print("Max # of blocks: %d, used/free %d/%d\n"
        ,DeviceSize(u),DeviceSize(u)-bc,bc);
(*print("Mаксимальное число файлов %d, из них занято/свободно %d/%d\n" *)
  print("Max # of files:  %d, used/free %d/%d\n"
        ,ISetSize(u),ISetSize(u)-ic,ic);
END stat.
