MODULE sleeptime; (* set the suspend-to-RAM sleep timeout *)

IMPORT std: Terminal;
IMPORT arg: tskArgs;
IMPORT str: Strings;

(* out(reg,val): mem[iopage + (reg & 0xFFF)] = val -- the Kronos io1 op (91h),
   the same generic I/O write the console driver uses. The FPGA decodes word
   0x7FF020 as the sleep-timeout register (seconds; 0 disables auto-sleep). *)
PROCEDURE out(reg,val: INTEGER); CODE 91h END out;

CONST TIMEOUT_REG = 20h;

VAR n, pos: INTEGER; done: BOOLEAN;
BEGIN
  IF HIGH(arg.words)<0 THEN
    std.print('usage: sleeptime <seconds>     (0 disables auto-sleep)\n');
    HALT
  END;
  pos:=0;
  str.iscan(n,arg.words[0],pos,done);
  IF NOT done OR (n<0) THEN
    std.print('sleeptime: "%s" is not a valid number of seconds\n',arg.words[0]);
    HALT(1)
  END;
  out(TIMEOUT_REG,n);
  IF n=0 THEN std.print('auto-sleep disabled\n')
  ELSE         std.print('sleep timeout set to %d s\n',n) END
END sleeptime.
