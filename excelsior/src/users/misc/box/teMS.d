DEFINITION MODULE teMS; (*  03-Jul-91. (c) KRONOS *)

IMPORT  SYSTEM;
IMPORT  wn: pmWnd;

PROCEDURE query(x,y: INTEGER; f: ARRAY OF CHAR; SEQ p: SYSTEM.WORD): BOOLEAN;
PROCEDURE paint(w: wn.WINDOW; x,y,w,h: INTEGER);

END teMS.