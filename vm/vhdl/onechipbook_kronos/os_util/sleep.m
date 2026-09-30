MODULE sleep; (* suspend the machine to RAM now; press any key to wake *)

(* out(reg,val): mem[iopage + (reg & 0xFFF)] = val -- the Kronos io1 op (91h).
   A write to word 0x7FF021 (the sleep-NOW register) makes the FPGA drop the
   SDRAM into self-refresh and freeze the CPU immediately; a keypress wakes it,
   after which this program returns to the shell. *)
PROCEDURE out(reg,val: INTEGER); CODE 91h END out;

CONST SLEEP_NOW_REG = 21h;

BEGIN
  out(SLEEP_NOW_REG,1)
END sleep.
