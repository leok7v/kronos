MODULE lint; (* H.SAAR *)

FROM SYSTEM IMPORT ADR;
FROM Media        IMPORT  VolDrv,ReadDisk;
FROM Strings      IMPORT  App,Str1,AppStr,SubStr,Str0;
FROM StdIO        IMPORT  Bell,ReadStr,Show,AskStr,Write,QueryLn
                        , WriteLn,WriteString,print;
FROM Image        IMPORT  GetNum;
FROM BIO          IMPORT  CD,UnLink,Close,bWrite,Link,GoPath,SetEof,
                          dummyErr,OpenOnDir,CreateOnVolume;
FROM FsPublic     IMPORT  VisFSerr,File,FileName,VolumeName;
FROM Args         IMPORT  Flag?,TakeNum,TakeWord,ScanFlags;

TYPE str=ARRAY [0..63] OF CHAR;

VAR buf,buf1   : ARRAY [0..4099] OF CHAR;
    look,all:BOOLEAN;
    dirto,ft,cd,f: File;
    vnimi:FileName;
    todsk:VolumeName;
    w,hdr,eof,nimi,todir,ap,yes: str;
    u,i,j,b,bl,ln:                     INTEGER;
    con : ARRAY [0..3]    OF INTEGER;
    cc              :                     CHAR;

PROCEDURE AskInt(s: ARRAY OF CHAR; VAR i: INTEGER): BOOLEAN;
  VAR s1: ARRAY [0..80] OF CHAR;
BEGIN
  AskStr(s,s1);
  RETURN GetNum(s1,i)>=0;
END AskInt;

PROCEDURE readlab(u,b: INTEGER; VAR buf: ARRAY OF CHAR): BOOLEAN;

VAR buff: ARRAY [0..4095] OF CHAR;
    noko,nok,trk,sec,i,ii,ph,ll,lk: INTEGER;

BEGIN
   noko:=0;trk:=b*32 DIV 26;sec:=b*32 MOD 26;ph:=0;
   IF sec#0 THEN FOR i:=0 TO sec-1 DO
                     ph:=(ph+con[i MOD 4]) MOD 26 END;
   END;
   FOR i:=0 TO 31 DO
       ll:=trk*26 MOD 32 + ph DIV 3 + ph MOD 3 * 9;
       lk:=ll MOD 32;
       nok:=trk*26 DIV 32 + ll DIV 32;
       IF nok#noko THEN
          IF ReadDisk(u,nok,ADR(buff),4096) THEN RETURN TRUE
          ELSE noko:=nok
          END;
       END;
       FOR ii:=0 TO 127 DO buf[i*128+ii]:=buff[lk*128+ii] END;
       IF sec=25 THEN sec:=0;INC(trk);ph:=0
                 ELSE ph:=(ph+con[sec MOD 4]) MOD 26;INC(sec)
       END;
   END;
   RETURN FALSE;
END readlab;

PROCEDURE err(r:BOOLEAN;s:ARRAY OF CHAR);
VAR msg:ARRAY [0..79] OF CHAR;
BEGIN
        IF r THEN
        VisFSerr(r,msg);WriteString(msg);WriteString(s);WriteLn;HALT
        END;
END err;

PROCEDURE Perr(s:ARRAY OF CHAR);
BEGIN
   Show(s);HALT
END Perr;

PROCEDURE rdnext;
  VAR ii: INTEGER;
BEGIN
 
   IF readlab(u,b,buf) THEN
      print("VIGA PLOKIS # %d",b); RETURN;
   END;
   WriteLn;
   print("PLOKK # %d",b);i:=0;
   WriteLn;
IF look THEN
   FOR ii:=0 TO 4095 DO cc:=buf[ii];
      IF ORD(cc) MOD 128 < 40b THEN Write('.') ELSE Write(cc) END;
   END;
END;
   b:=b+1;
END rdnext;

PROCEDURE check(pat: ARRAY OF CHAR): BOOLEAN;
  VAR ii: INTEGER;
BEGIN
   IF i>4069 THEN RETURN FALSE END;
   FOR ii:=0 TO 8 DO cc:=buf[i+ii];
       IF ORD(cc) MOD 128 < 40b THEN RETURN FALSE
       ELSE w[ii]:=cc END;END;
   w[9]:=0c;
   IF pat=w THEN
     IF pat=hdr THEN
      Str0(ap);
      FOR ii:=0 TO 15 DO
          IF buf[i+10+ii]='.' THEN nimi[ii]:=0c;nimi[HIGH(nimi)]:=CHAR(ii);
             WHILE buf[i+10+ii] # ' ' DO App(ap,buf[i+10+ii]);INC(ii) END;
          ELSE
          nimi[ii]:=buf[i+10+ii] END;
      END;
    END;
    RETURN TRUE;
   ELSE RETURN FALSE
   END;
END check;
 
PROCEDURE header(): BOOLEAN;
BEGIN
   WHILE  i<4069  DO
      IF check(hdr) THEN i:=i+27; RETURN FALSE;
      ELSE i:=i+1
      END;
   END;
   RETURN TRUE;
END header;

PROCEDURE otsi;
BEGIN
   WHILE header() DO rdnext END;
   WriteLn;
   print("OLD NAME:%s%s",nimi,ap);
   WriteString("     NEW NAME:");
   IF all THEN
      IF    ap=".l" THEN AppStr(nimi,".cod")
(*      ELSIF ap=".c" THEN AppStr(nimi,".s") *)
      ELSE AppStr(nimi,ap);
      END;
      Show(nimi);Str1(vnimi,nimi);
  ELSE Bell;ReadStr(vnimi);WriteLn;
  END;
END otsi;

PROCEDURE sulge;
BEGIN
   err(bWrite(ft,bl,ADR(buf1),j),todsk);
   ln:=ln+j;print("PIKKUS %d",ln);
   SetEof(ft,ln);
   err(Link(dirto,vnimi,ft),vnimi);
   err(Close(ft),vnimi);
   err(Close(dirto),todir);
END sulge;
 
PROCEDURE nextfile(): BOOLEAN;
BEGIN
   IF vnimi[0]=0c THEN
      WHILE NOT (check(hdr) OR check(eof)) DO 
               IF i>4069 THEN rdnext ELSE INC(i) END;
      END;
      RETURN check(hdr)
   END;
   IF check(hdr) THEN Show("SISENDFAILI PIKKUS 0");RETURN TRUE
   END;
   Str1(w,todir);GoPath(cd,dirto,w,Perr);
   IF NOT OpenOnDir(dirto,f,vnimi) THEN
      err(UnLink(dirto,vnimi),vnimi);
      err(Close(f),vnimi);
   END;
   err(CreateOnVolume(ft,todsk),todsk);
   bl:=0;j:=0;ln:=0;
   LOOP
      IF buf[i]='#' THEN
         IF check(eof) THEN sulge;RETURN FALSE END;
         IF check(hdr) THEN sulge;RETURN TRUE  END;
      END;
      IF i=4096     THEN rdnext;            END;
      IF j=4096     THEN
         err(bWrite(ft,bl,ADR(buf1),j),todsk);
         ln:=ln+4096;bl:=bl+1;j:=0;
      END;
      buf1[j]:=buf[i];j:=j+1;i:=i+1
   END;
END nextfile;
 
BEGIN
   con[0]:=13;
   con[1]:=8;
   con[2]:=13;
   con[3]:=21;
   cd:=CD();
   ScanFlags;all:=Flag?('a');look:=Flag?('l');
   TakeWord(w);
   err(VolDrv(w,u),w);
   AskStr("  TO DISK:",todsk);
   AskStr("  TO DIR :",todir);
   print("   From %s to ",w); print('%s %s\n',todsk,todir);
   IF NOT QueryLn("OK?") THEN HALT END;
   AppStr(todir,"/*");
   i:=4096;
   b:=0;
   WHILE (b<4) OR (b>58) DO
      IF AskInt("Millisest plokist alates? (4..58), RETURN-algusest ",b)
      THEN b:=4 END;
      WriteLn;
   END;
   Str1(hdr,"###HDR###");
   Str1(eof,"###EOF###");
   otsi;
   WHILE nextfile() DO otsi END;
   WriteLn;Show("BYE");
END lint.
