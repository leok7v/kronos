# Kronos native C toolchain (`ktool`)

A self-contained C compiler that runs **on the Kronos/Excelsior machine** and
builds `.cod` executables from C sources. This is the AT&T `pcc` port (1986–88),
cross-built to Kronos M-code and proven to self-host (it recompiles its own
source byte-for-byte). See the `native-c-bootstrap` project memory for the story.

## What's here

```
bin/     20 .cod modules — the compiler + C runtime
lib/     clib.lib        — link-time C library
include/ *.h             — C headers (LF line endings, not RS)
examples/ ctest.c, chello.c
mk_ctool.py              — builds the Excelsior XD volume
```

The four compiler passes and their modules:

| pass  | invoked as | modules loaded (by import name)                                   |
|-------|-----------|------------------------------------------------------------------|
| cpp   | `cpp`     | `cpy`, `clib`, `c_run`                                            |
| pcc   | `p_code`  | `p_cgram p_common p_local2 p_match p_order p_pftn p_reader p_scan p_trees`, `clib`, `c_run` |
| asm   | `a_asm`   | `a_asm1`, `a_asm2`, `clib`, `c_run`                              |
| clink | `lnk`     | `clib`, `c_run`                                                   |

Runtime (`clib`, `c_run`, `k_run`, `env`) is shared by the passes **and** by the
programs they produce. The OS modules those pull in (`StdIO`, `BIO`, `Strings`,
`lowLevel`, `Heap`, `tskEnv`) already live on the system volume (xd1).

## Build the volume

```sh
# standalone toolchain volume (everything at root) — good for the VM:
./mk_ctool.py --out ctool.dsk

# or fold the toolchain into a sources volume as /c (this becomes xd2):
./mk_ctool.py --out xd2.dsk --size-mb 128 --src ../../../excelsior/src
```

`mk_ctool.py` stores `.c`/`.h` **raw (LF)** — `mkxd.py import` would RS-convert
them (0x1e), which the 1988 `cpp` does not understand. `.m`/`.d` sources brought
in via `--src` keep the Excelsior RS convention (they're for `mx`, not `cpp`).

## Compile on the machine

Mount the volume (as `/usr1`) and run the passes by hand — the shell has no pipe,
so each is a separate command:

```
cd /usr1/c              # (if built with --src; omit for a standalone volume)
cpp   foo.c -o foo.i
p_code foo.i foo.a
a_asm foo.a             # -> foo.o
lnk   foo.o             # -> foo.cod
foo                     # run it
```

## Writing C for this compiler

It is **1988 K&R C** — no ANSI prototypes. Define functions old-style:

```c
#include "stdio.h"          /* headers are found in the current directory */

int fib(n)                  /* NOT  int fib(int n)  — that is a syntax error */
int n;
{
    if (n < 2) return n;
    return fib(n-1) + fib(n-2);
}
```

`printf` (`%d %x %s %c %u %o`, width/pad), recursion, loops, `#include` all work.

## Deploy to the physical FPGA (SD card)

The card has four primary partitions (unit N → partition N+1): xd0 boot, xd1
system, xd2 Excelsior volume, xd3 FAT. Put the toolchain on **xd2**:

```sh
cd vm/vhdl/onechipbook_kronos
# xd2 = excelsior sources + /c toolchain
python3 ../../pcc-host/ktool/mk_ctool.py --out ../../../xd2.dsk \
        --size-mb 128 --src ../../../excelsior/src
# then bundle the card as usual
./mkcard.py --xd0 ../../../xd0.dsk --xd1 ../../../xd1.dsk \
            --xd2 ../../../xd2.dsk --out card.img
# ...and dd card.img to the microSD (mkcard prints the exact command)
```

The extended-RAM build (31.75 MB, `ram-32mb-extension`) has ample room for
`lnk`, which needs ~7.5 MB.
