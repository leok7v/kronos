# Suspend-to-RAM OS utilities

Two small Modula-2 commands for the sleep feature. Full feature docs:
[`../SUSPEND_TO_RAM.md`](../SUSPEND_TO_RAM.md). They talk to the FPGA's
sleep registers with the standard Kronos I/O-out op (`out`, opcode `91h` =
`mem[iopage+reg]=val`), the same primitive the console driver uses.

| Register (I/O word) | Meaning |
|---|---|
| `0x7FF020` | sleep timeout, in **seconds**; `0` disables auto-sleep |
| `0x7FF021` | write anything = **sleep now** |

## Build (on the board, or in the vm/int VM)

Copy the two `.m` files onto the machine and compile each with the Modula-2
compiler, then they are ordinary commands:

```
mx sleeptime          # -> sleeptime.cod
mx sleep              # -> sleep.cod
```

Put the resulting `.cod` files somewhere on the command path (e.g. next to the
other utilities).

## Use

```
sleeptime 1200        # auto-sleep after 20 minutes of no key / no output
sleeptime 60          # ... after 1 minute
sleeptime 0           # disable auto-sleep entirely
sleep                 # sleep immediately; press any key to wake
```

The power-on default (before `sleeptime` is run) is 20 minutes, baked into the
bitstream (`SUSPEND_DEFAULT_SEC`). To make a chosen timeout stick across
reboots, run `sleeptime <n>` from your startup script.

## Notes

- Only a **keypress** wakes the machine; console output cannot (the CPU is
  halted while asleep). The screen is blanked (DPMS) during sleep and returns
  a second or so after waking, once the monitor re-locks sync.
- These were written against the OS's own command sources (`mkdir.m` style:
  `Terminal`/`tskArgs`/`Strings`) but have not been compiled here — if `mx`
  reports anything, send the message and it's a quick fix.
