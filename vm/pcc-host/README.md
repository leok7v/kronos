# pcc-host — the Kronos C compiler, cross-built on modern Linux

This resurrects the 1986-88 Kronos port of the AT&T Portable C Compiler
(`excelsior/src/users/misc/pcc/`) as **host (Linux) binaries**. The result is a
cross-compiler: it runs on Linux and emits **Kronos M-code** objects (`.o`) and
linked `.cod` files that run under Excelsior on the FPGA board.

The board has no C toolchain installed (only `mx`/Modula-2), and pcc is a
bootstrap compiler (building it needs a working `c`/`asm`/`clink`). Cross-building
on the host breaks that bootstrap: once we can emit `.cod`, we can compile the
runtime and even rebuild pcc itself for the board.

## Build

    ./build.sh          # -> bin/{cpp,pcc,asm,clink,libr,c}

Requires gcc. Flags: `-Dunix=1 -std=gnu89 -fpermissive -w` (the passes are K&R C
with `#if unix` host paths; `-fpermissive` demotes modern hard-errors like
implicit-int back to warnings). The passes build against **host** system headers;
`../include` (the Kronos target libc) is deliberately NOT on the include path,
except the pure-data opcode tables `instr.h`/`mkinstr.h` copied into `doasm/`.

## Use (cross-compile a C file to Kronos)

    bin/cpp -Iinclude foo.c > foo.i     # preprocess (writes stdout)
    bin/pcc foo.i        > foo.a         # compile -> Kronos M-code assembly
    bin/asm foo.a -o foo.o               # assemble -> Kronos object
    bin/clink -o foo foo.o               # link -> foo.cod   (needs clib.lib for a full program)

See `test/hello.c`, `test/test2.c` (recursion, arrays, for/if) — both compile to
correct Kronos asm; `test/hello.cod` is a produced code file.

## Status: A C PROGRAM RUNS ON THE EXCELSIOR OS 🎉

`rt/khello.c` (`puts("Hello from resurrected Kronos C!")`) — compiled by this
toolchain, linked in the current .cod format, running through the ported Modula-2
bridge in `bridge/` — prints on the real OS (verified in the vm/int VM, the same
OS/mx the board runs). `write()` and `puts()` work end to end; printf still faults
in varargs (a follow-up). Reusable run loop: the scratchpad `runprog.sh <name>`.
Build the bridge with `mx` (see `bridge/` + the sym-regeneration trick in memory);
link a program with `clink -o prog prog.o` (clib.lib must contain c_run.o so
`c_run` is imported), then stage the module chain prog -> {clib,c_run}, clib ->
{env,c_run}, c_run -> k_run on one volume and run it by name.

WORKING END TO END ON THE HOST: `cpp -> pcc -> asm -> clink` compiles C to a
runnable Kronos `.cod`. Proven:
- pcc emits correct M-code for real C (functions, recursion, arrays, control
  flow, **structs/`->`**); `test/hello.c`, `test/test2.c`.
- The **entire C runtime library compiles**: `crts/{clib,env,c_run,setjmp,cmath,
  matherr}.c` (156 functions) -> objects -> archived by `libr` into `clib.lib`
  (built in `rt/`).
- `rt/hi.c` (a `printf` program) cross-compiles AND cross-links against clib.lib
  to `rt/hi.cod` — a real Kronos code file (header + embedded string + M-code).

Two more 64-bit host-port fixes were needed to get here (see the patch list):
`pftn.c` symbol-table hash `(int)name` truncated a 64-bit pointer to a NEGATIVE
index (found via ASan) -> `stab[neg]` OOB; and `clink.c` `getlibraries` did
`strcmp(NULL,..)` on unnamed library-module slots (glibc segfaults; old strcmp
tolerated it).

BUILD THE RUNTIME LIB:  `cd rt && bash -c '...'` — or just: compile each
`crts/*.c` through cpp|pcc|asm and `../bin/libr clib.o c_run.o env.o setjmp.o
cmath.o matherr.o -o clib`.

## NEXT: run on the board

The C program calls resolve `printf -> _write -> c_run.m/k_run.m (Modula-2) ->
Excelsior TTYs/BIO`. So to execute on the board:
1. **Build the Modula-2 OS bridge on the board** with `mx` (it's at
   `/usr1/users/misc/pcc/m2/`). BLOCKER hit 2026-07-21: `mx k_run.d` fails with
   `"FsPublic.sym": no such entry` — mx isn't finding the system-module symbol
   files. Fix: set mx's `.sym` search path to the system sym dir, and/or
   regenerate the needed `.sym` (FsPublic, BIO, TTYs, FsDrv, Media, Scheduler,
   Terminal, Heap, Strings, Args, ASCII, Image, mCodeMnem) from the `.d` sources
   now on xd2 (`/usr1/sys/...`). Watch for the known mx sym-timestamp friction.
2. Deploy `clib.lib` + `hi.cod` (and the built bridge `.cod`s) so the loader can
   resolve them; drop them via the FAT partition (xd3) or a rebuilt xd2.
3. Run via `bin/cx.@` (`ex -lf hi`).

## Host-port patches (all marked `host-port:` in the source)

These are the changes vs the pristine archive; keep them as a patch set so they
can be fed back so the board can self-host too:

1. **deyacc.py** — the yacc parsers `docpp/cpy.c` and `dopcc/cgram.c` emit their
   tables as Kronos `asm("...")` blocks (inline assembly); rewritten to standard
   C arrays (`yytabelem NAME[] = {...}`). Run automatically? No — already applied
   in place; re-run `python3 deyacc.py docpp/cpy.c dopcc/cgram.c` if you re-copy.
2. **doasm/**: `ln -s asm.h Asm.h` (case-sensitive `#include "Asm.h"`); `error()`
   made non-static in `clink.c`/`libr.c` (K&R fwd-decl conflict).
3. **docpp/cpp.c**: `#define STATIC` emptied (was `static`, clashed with K&R
   non-static forward decls); `#undef BUFSIZ` so cpp's intended `BUFSIZ 512` wins
   over host `<stdio.h>`'s 8192 (which overflowed the macro pool -> "too much
   defining", and segfaulted).
4. **dopcc/**: `cflag`/`paramstk`/`argoff` made non-static (extern in mfile1.h);
   `paintcast` de-static'd; `manifest.h` forces `NULL 0` (pcc uses NULL as its
   type-code 0, not host `(void*)0`); `mfile1.h` symbol/dim/param tables raised
   (SYMTSZ 1300->20000 etc.) — clib.c overflowed them (no guard) -> `cerror(214)`.
5. **include/sys_errno.h** — provided (the archive had the 8.3-truncated
   `sys_errn.h`; the source `#include "sys_errno.h"` couldn't find it, and cpp
   **segfaults on a missing include** — a latent cpp robustness bug worth fixing).

The pristine sources remain untouched at `excelsior/src/users/misc/pcc/` (and on
the board at `/usr1/users/misc/pcc`); this workspace is a build copy.
