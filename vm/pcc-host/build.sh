#!/bin/bash
# Build the Kronos "pcc" C cross-toolchain as HOST (Linux) binaries.
#
# The passes are 1980s K&R C with `#if unix` host-build paths. We build them
# against the HOST system headers (NOT the Kronos target headers in ../include,
# which would shadow <stdio.h> etc. and pull in target libc internals like
# _iob/_ctype). doasm additionally needs the pure-data Kronos opcode tables
# instr.h / mkinstr.h, which we copy in locally.
#
# Result: host tools that read C and emit Kronos M-code objects (.o) / .cod.
set -e
cd "$(dirname "$0")"
ROOT="$(pwd)"
CC="gcc -Dunix=1 -std=gnu89 -fpermissive -w"
OUT="$ROOT/bin"
mkdir -p "$OUT"

stage() { echo; echo "==================== $* ===================="; }

stage "doasm: assembler, linker, librarian, driver"
cd "$ROOT/doasm"
cp -f ../include/instr.h ../include/mkinstr.h .    # pure-data opcode tables
ln -sf asm.h Asm.h                                 # case-insensitive #include "Asm.h"
$CC -I. -c asm.c asm1.c asm2.c
$CC asm.o asm1.o asm2.o -o "$OUT/asm"
$CC -I. clink.c -o "$OUT/clink"
$CC -I. libr.c  -o "$OUT/libr"
$CC -I. c.c     -o "$OUT/c"
echo "  -> asm clink libr c"

stage "docpp: C preprocessor"
cd "$ROOT/docpp"
$CC -Dkronos -DexcII -I. cpp.c cpy.c -o "$OUT/cpp"
echo "  -> cpp"

stage "dopcc: the C compiler proper"
cd "$ROOT/dopcc"
$CC -DKRONOS2 -DBUG4 -I. \
    cgram.c code.c local2.c common.c match.c order.c \
    reader.c trees.c pftn.c scan.c -o "$OUT/pcc"
echo "  -> pcc"

stage "done"
ls -la "$OUT"
