DEFINITION MODULE ModelIO; (* Sem 22-Jul-86. (c) KRONOS *)

FROM Model     IMPORT Object;

PROCEDURE ReadModel(Name: ARRAY OF CHAR): Object;

PROCEDURE ReadModelBody(o: Object);

PROCEDURE ReadShortModel(Name: ARRAY OF CHAR): Object;

PROCEDURE WriteModel(o: Object);

END ModelIO.
