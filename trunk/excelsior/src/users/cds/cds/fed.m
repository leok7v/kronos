MODULE fed; (*  02-Feb-91. (c) KRONOS *)

FROM SYSTEM      IMPORT ADDRESS;

IMPORT  mem : libHeap;
IMPORT  wnd : libWindows;
IMPORT  bio : BIO;
IMPORT  tty : Terminal;
IMPORT  arg : tskArgs;
IMPORT  vg  : cdsGrafic;

CONST
  l_on =226c; m_on =227c; r_on =215c;
  l_off=236c; m_off=237c; r_off=225c;

TYPE
  font_rec = RECORD
    w,h : INTEGER;
    base: ADDRESS;
    p_w : POINTER TO ARRAY CHAR OF INTEGER;
  END;
  font = POINTER TO font_rec;

VAR
  fnt  : font;
  ch   : CHAR;
  ch_on: BOOLEAN;
  ch_w : wnd.window;
  ch_x : INTEGER;
  ch_y : INTEGER;
  pen  : BOOLEAN;
  pen_v: BOOLEAN;

PROCEDURE open_char_window; FORWARD;

PROCEDURE char_window(w: wnd.window; x,y: INTEGER; c: CHAR);
  VAR a: ADDRESS;
BEGIN
  x:=x-w^.x; y:=y-w^.y;
  IF (c=0c) & pen THEN
    x:=(x-10) DIV 11; y:=(y-20) DIV 11;
    IF (x<0) OR (x>=fnt^.p_w^[ch]) THEN RETURN END;
    IF (y<0) OR (y>=fnt^.h) THEN RETURN END;
    a:=fnt^.base+ORD(ch)*fnt^.h+fnt^.h-1-y;
    vg.color(w,2);
    IF pen_v THEN vg.mode(w,vg.rep) ELSE vg.mode(w,vg.bic) END;
    vg.box(w,x*11+10,y*11+20,x*11+19,y*11+29);
    IF pen_v THEN a^:=BITSET(a^)+{x} ELSE a^:=BITSET(a^)-{x} END;
    wnd.ref_box(w,y*11+20,10);
    RETURN
  ELSIF (c#r_on) & (c#m_on) & (c#l_on) THEN
    pen:=FALSE; RETURN
  END;
  IF (y>=5) & (y<=14) THEN
    x:=(x-10) DIV vg.sign_w;
    CASE x OF
      |0: ch_on:=FALSE; wnd.close(w);
      |1: IF fnt^.p_w^[ch]<=3 THEN RETURN END;
          ch_on:=FALSE; wnd.remove(w); DEC(fnt^.p_w^[ch]); open_char_window;
      |2: IF fnt^.p_w^[ch]>=15 THEN RETURN END;
          ch_on:=FALSE; wnd.remove(w); INC(fnt^.p_w^[ch]); open_char_window;
    ELSE
    END;
  ELSE
    x:=(x-10) DIV 11; y:=(y-20) DIV 11;
    IF (x<0) OR (x>=fnt^.p_w^[ch]) THEN RETURN END;
    IF (y<0) OR (y>=fnt^.h) THEN RETURN END;
    a:=fnt^.base+ORD(ch)*fnt^.h+fnt^.h-1-y;
    vg.color(w,2); vg.mode(w,vg.xor);
    vg.box(w,x*11+10,y*11+20,x*11+19,y*11+29);
    a^:=BITSET(a^)/{x};
    wnd.refresh(w);
    pen:=TRUE;
    pen_v:=x IN BITSET(a^);
  END;
END char_window;

PROCEDURE open_char_window;
  VAR w: wnd.window; sx,sy,i,j: INTEGER; a: ADDRESS;
BEGIN
  IF fnt^.p_w^[ch]<3 THEN fnt^.p_w^[ch]:=3 END;
  IF fnt^.p_w^[ch]>31 THEN fnt^.p_w^[ch]:=31 END;
  sx:=30+(fnt^.p_w^[ch]-1)*11;
  sy:=40+(fnt^.h-1)*11;
  w:=wnd.create(sx,sy);
  vg.color(w,1);
  vg.vect(w,0,0,sx-1,0);
  vg.vect(w,sx-1,1,sx-1,sy-1);
  vg.vect(w,9,19,9,sy-10);
  vg.vect(w,10,sy-10,sx-10,sy-10);
  FOR i:=1 TO fnt^.p_w^[ch]-1 DO j:=9+11*i; vg.vect(w,j,20,j,sy-11) END;
  FOR i:=1 TO fnt^.h-1 DO j:=19+11*i; vg.vect(w,10,j,sx-11,j) END;
  vg.color(w,2);
  vg.box(w,1,1,sx-10,18);
  vg.box(w,sx-9,1,sx-2,sy-10);
  vg.box(w,9,sy-9,sx-2,sy-2);
  vg.box(w,1,19,8,sy-2);
  a:=fnt^.base+ORD(ch)*fnt^.h;
  FOR i:=fnt^.h-1 TO 0 BY -1 DO
    FOR j:=0 TO fnt^.p_w^[ch]-1 DO
      IF j IN BITSET(a^) THEN vg.box(w,j*11+10,i*11+20,j*11+19,i*11+29) END;
    END;
    INC(a);
  END;
  vg.color(w,3);
  vg.vect(w,0,1,0,sy-1);
  vg.vect(w,1,sy-1,sx-2,sy-1);
  vg.vect(w,10,19,sx-10,19);
  vg.vect(w,sx-10,20,sx-10,sy-11);
  vg.mode(w,vg.rep);
  vg.sign(w,10            ,2,vg.sg_close);
  vg.sign(w,10+vg.sign_w  ,2,vg.sg_down);
  vg.sign(w,10+vg.sign_w*2,2,vg.sg_up);
  w^.x:=ch_x; w^.y:=ch_y;
  w^.job:=char_window;
  wnd.open(w);
  ch_on:=TRUE;
  ch_w:=w;
  pen:=FALSE;
END open_char_window;

PROCEDURE write_font(VAL nm: ARRAY OF CHAR);
  VAR f: bio.FILE; m: INTEGER; c: CHAR;
BEGIN
  m:=1;
  FOR c:=0c TO 377c DO
    IF fnt^.p_w^[c]>m THEN m:=fnt^.p_w^[c] END;
  END;
  fnt^.w:=m;
  bio.create(f,nm,'w',0);
  IF NOT bio.done THEN HALT(bio.error) END;
  bio.write(f,fnt,BYTES(fnt^));
  IF NOT bio.done THEN HALT(bio.error) END;
  bio.write(f,fnt^.base,fnt^.h*1024);
  IF NOT bio.done THEN HALT(bio.error) END;
  bio.write(f,fnt^.p_w,1024);
  IF NOT bio.done THEN HALT(bio.error) END;
  bio.close(f);
  IF NOT bio.done THEN HALT(bio.error) END;
END write_font;

PROCEDURE pack_font;
  VAR c: CHAR; i,j,min,max: INTEGER; a: ADDRESS; s: BITSET;
BEGIN
  FOR c:=0c TO 255c DO
    min:=1000; max:=0;
    a:=fnt^.base+ORD(c)*fnt^.h;
    FOR i:=0 TO fnt^.h-1 DO
      FOR j:=0 TO 31 DO
        IF j IN BITSET(a^) THEN
          IF j<min THEN min:=j END;
          IF j>max THEN max:=j END;
        END;
      END;
      INC(a);
    END;
    a:=fnt^.base+ORD(c)*fnt^.h;
    FOR i:=0 TO fnt^.h-1 DO
      s:={};
      FOR j:=min TO max DO
        IF j IN BITSET(a^) THEN INCL(s,j-min) END;
      END;
      a^:=s;
      INC(a);
    END;
    i:=max-min+1;
    IF i<3 THEN i:=3 END;
    fnt^.p_w^[c]:=i;
  END;
END pack_font;

PROCEDURE font_window(w: wnd.window; x,y: INTEGER; c: CHAR);
BEGIN
  IF (c#r_on) & (c#m_on) & (c#l_on) THEN RETURN END;
  x:=x-w^.x; y:=y-w^.y;
  IF (y>=5) & (y<=14) THEN
    x:=(x-2) DIV (vg.sign_w+4);
    CASE x OF
      |0: write_font(arg.words[0]); wnd.remove(w);
          IF ch_on THEN wnd.remove(ch_w); ch_on:=FALSE END;
          HALT;
      |1: IF ch_on THEN wnd.remove(ch_w); ch_on:=FALSE END;
          pack_font;
    ELSE
    END;
  ELSE
    x:=(x-10) DIV (vg.char_h+1);
    y:=(y-20) DIV (vg.char_h+1);
    IF (x<0) OR (x>15) THEN RETURN END;
    IF (y<0) OR (y>15) THEN RETURN END;
    IF ch_on THEN wnd.remove(ch_w); ch_on:=FALSE END;
    ch:=CHAR(x+y*16);
    open_char_window;
  END;
END font_window;

PROCEDURE open_font_window;
  VAR w: wnd.window; sx,sy,i,j: INTEGER;
BEGIN
  sx:=16*vg.char_h+35;
  sy:=16*vg.char_h+45;
  w:=wnd.create(sx,sy);
  vg.color(w,1);
  vg.vect(w,0,0,sx-1,0);
  vg.vect(w,sx-1,1,sx-1,sy-1);
  vg.vect(w,9,19,9,sy-10);
  vg.vect(w,10,sy-10,sx-10,sy-10);
  FOR i:=1 TO 15 DO
    j:=9+(vg.char_h+1)*i;
    vg.vect(w,j,20,j,sy-11);
    vg.vect(w,10,j+10,sx-11,j+10);
  END;
  vg.color(w,2);
  vg.box(w,1,1,sx-10,18);
  vg.box(w,sx-9,1,sx-2,sy-10);
  vg.box(w,9,sy-9,sx-2,sy-2);
  vg.box(w,1,19,8,sy-2);
  FOR i:=0 TO 15 DO
    FOR j:=0 TO 15 DO
      vg.write_char(w,11+j*(vg.char_h+1),20+i*(vg.char_h+1),CHAR(j+i*16));
    END;
  END;
  vg.color(w,3);
  vg.vect(w,0,1,0,sy-1);
  vg.vect(w,1,sy-1,sx-2,sy-1);
  vg.vect(w,10,19,sx-10,19);
  vg.vect(w,sx-10,20,sx-10,sy-11);
  vg.color(w,15); vg.mode(w,vg.rep);
  FOR i:=0 TO 7 DO
    vg.sign(w,2+i*(vg.sign_w+4),2,CHAR(i*4));
  END;
  w^.job:=font_window;
  ch_on:=FALSE; ch_x:=200; ch_y:=100;
  wnd.open(w);
  w:=wnd.create(300,20);
  FOR i:=1 TO 15 DO
    vg.color(w,i);
    vg.box(w,i*20-20,0,i*20-1,19);
  END;
  w^.x:=0; w^.y:=340;
  wnd.open(w);
END open_font_window;

PROCEDURE read_font(VAL nm: ARRAY OF CHAR): font;
  VAR f: bio.FILE; size,i: INTEGER; fnt: font; a: ADDRESS;
BEGIN
  fnt:=NIL;
  bio.open(f,nm,'r');
  IF NOT bio.done THEN HALT(bio.error) END;
  size:=(bio.eof(f)+3) DIV 4;
  mem.ALLOCATE(fnt,size);
  bio.read(f,fnt,bio.eof(f));
  IF NOT bio.done THEN HALT(bio.error) END;
  bio.close(f);
  IF NOT bio.done THEN HALT(bio.error) END;
  fnt^.base:=ADDRESS(fnt)+SIZE(fnt^);
  IF size<SIZE(fnt^)+fnt^.h*256+256 THEN
    mem.ALLOCATE(fnt^.p_w,256);
    FOR i:=0 TO 255 DO a^:=fnt^.w; INC(a) END;
  ELSE
    fnt^.p_w :=fnt^.base + fnt^.h*256;
  END;
  RETURN fnt;
END read_font;

VAR
  i: INTEGER;
  c: CHAR;
  a: ADDRESS;

BEGIN
  IF HIGH(arg.words)<0 THEN
    tty.print('fed file_name\n'); HALT
  ELSIF arg.flag('-','n') THEN
    mem.ALLOCATE(fnt,SIZE(fnt^));
    mem.ALLOCATE(fnt^.p_w,256);
    IF NOT arg.number('h',fnt^.h) THEN fnt^.h:=12 END;
    IF NOT arg.number('w',fnt^.w) THEN fnt^.w:=12 END;
    tty.print('Create new font %dx%d.\n',fnt^.w,fnt^.h);
    mem.ALLOCATE(fnt^.base,256*fnt^.h);
    FOR c:=0c TO 377c DO fnt^.p_w^[c]:=fnt^.w END;
    a:=fnt^.base;
    FOR i:=1 TO 256*fnt^.h DO a^:=0; INC(a) END;
  ELSE
    fnt:=read_font(arg.words[0]);
  END;
  open_font_window;
  wnd.job;
END fed.
