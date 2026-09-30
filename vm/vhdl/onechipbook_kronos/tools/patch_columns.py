#!/usr/bin/env python3
"""Widen the Excelsior console from 80 columns to the VGA console's full width.

The OS's terminal geometry lives in ONE place: TT2ANSI's tt_reset, which does

    type     :=52;   hbar:=HBAR;
    lines    :=50;   vbar:=VBAR;
    columns  :=80;   back:=-2;

lines:=50 already matches the VGA console's 50 rows, which is why the vertical
size is right. columns:=80 is why 48 of our 128 columns sit unused.

Rebuilding the driver from source is the "proper" route, but it needs write
access to /sys (guest gets "security violation") and a `config -b` that rewrites
the bootable system -- a lot of risk for one constant. In compiled M-code that
constant is a single `lib` immediate, so this patches it directly:

    10 34   lib 52    type:=52
    10 a4   lib 164   hbar:=HBAR
    10 32   lib 50    lines:=50
    10 b3   lib 179   vbar:=VBAR
    10 50   lib 80    columns:=80     <-- the byte this changes

The lib 164 / lib 179 in the same procedure are what prove the immediate is
ZERO-extended, so 128 (0x80) is safe and does not come out negative: those are
the HBAR/VBAR pseudographics codes, and the frames draw correctly.

The site is found by SEARCHING for that signature, not by fixed offsets, so it
works on an image that has been used and written to. Verified in the reference
VM: the shell's command-line clear grows from 70 to 118 spaces (columns-10),
and the boot output is byte-identical otherwise.
"""
import argparse
import shutil
import sys

# lib 52, .., lib 50, .., lib 80 -- the tt_reset constant run
SIG = bytes.fromhex("1034702610a47126058802c2020002d4261032722610b373"
                    "260588018802c2030002d426")
COLS_OFF = len(SIG) + 1          # the byte after the trailing "10"

# kc's own layout width. Widening the TERMINAL is not enough: kc.m does
#
#     LINES:=scn.state^.lines;  L:=78;  HALF:=L DIV 2;
#
# LINES is read from the terminal but L -- kc's total width -- is a hardcoded
# 78, which is why the panels stop at ~61% of a 128-column screen and why the
# right border cannot be dragged any further. Compiled:
#
#     10 4e   lib 78     push 78
#     b5      copt       duplicate
#     5f      sgw15      store L
#     01 8d   li1, shr   L DIV 2
#     31 10   sgw16      store HALF
#
# HALF is computed from the DUPLICATED value and w/x reload L, so patching the
# single immediate updates L, HALF, w and x together and the layout stays
# self-consistent. The signature is unique in the image.
KC_SIG = bytes.fromhex("104eb55f018d")
KC_OFF = 1                       # the byte after the leading "10"


def sites(d):
    out, i = [], 0
    while True:
        i = d.find(SIG, i + 1)
        if i < 0:
            return out
        if d[i + len(SIG) - 1] == 0x10 or d[i + len(SIG)] == 0x10:
            out.append(i)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("image", help="disk image (patched IN PLACE unless -o)")
    ap.add_argument("-o", "--out", help="write here instead of in place")
    ap.add_argument("-c", "--columns", type=int, default=128,
                    help="new column count, 1..255 (default 128)")
    ap.add_argument("-k", "--kc-width", type=int, nargs="?", const=-1, default=None,
                    help="also widen kc's hardcoded layout width L. Bare -k uses "
                         "COLUMNS-2, which is what you want; a value is accepted "
                         "but MUST be <= COLUMNS-2 (see below). Widening the "
                         "terminal alone leaves kc at 78")
    ap.add_argument("-n", "--dry-run", action="store_true")
    a = ap.parse_args()

    if not 1 <= a.columns <= 255:
        sys.exit("columns must be 1..255 (it is a single lib immediate)")

    if a.kc_width == -1:                 # bare -k: the only correct default
        a.kc_width = a.columns - 2

    if a.kc_width is not None:
        if not 1 <= a.kc_width <= 255:
            sys.exit("kc width must be 1..255 (it is a single lib immediate)")
        # THE BUG THIS GUARD EXISTS FOR. This script used to suggest "-k 128"
        # alongside "-c 128", and that combination BRICKS kc's command line.
        # strEditor.edit_str begins with
        #
        #     IF (HIGH(string)<0) OR (col1>col2) OR
        #        (col1<0) OR (col2>=tty.state^.columns) THEN
        #         desc^.last:=0c; RETURN            -- returns WITHOUT reading
        #     END;
        #
        # and kc passes L straight in as col2. With L=128 and columns=128 the
        # last term is TRUE, so edit_str returns instantly having consumed no
        # input, and kc's command loop -- which exits only on ESC (033c) -- spins
        # forever reprinting its prompt. The console floods and only a reboot
        # clears it; ESC cannot help, because nothing is ever read.
        #
        # Stock kc ships L=78 against columns=80, i.e. columns-2, which is
        # exactly what the shell itself computes (shell.m: tty.state^.columns-2).
        # Confirmed on hardware: L=128 livelocks, L=78 and L=126 both work.
        if a.kc_width > a.columns - 2:
            sys.exit("kc width %d is too wide for %d columns: kc passes L as "
                     "col2 to strEditor.edit_str, which bails out (without "
                     "reading input) when col2 >= columns, and kc then spins "
                     "forever reprinting its prompt. Use at most %d."
                     % (a.kc_width, a.columns, a.columns - 2))

    d = bytearray(open(a.image, "rb").read())
    found = []
    i = 0
    while True:
        i = d.find(SIG, i + 1)
        if i < 0:
            break
        off = i + len(SIG)
        if d[off] != 0x10:                       # must be the lib opcode
            continue
        found.append(off + 1)

    if not found:
        sys.exit("tt_reset signature not found -- wrong image, or a driver "
                 "build this script does not know")

    old = set(d[p] for p in found)
    print("found %d copies of tt_reset; current columns: %s"
          % (len(found), ", ".join(str(v) for v in sorted(old))))

    kc = []
    if a.kc_width is not None:
        i = -1
        while True:
            i = d.find(KC_SIG, i + 1)
            if i < 0:
                break
            kc.append(i + KC_OFF)
        if not kc:
            sys.exit("kc width signature not found -- is kc on this image?")
        print("found %d kc layout site(s); current kc width: %s"
              % (len(kc), ", ".join(str(d[p]) for p in kc)))

    if a.dry_run:
        print("dry run, nothing written")
        return

    for p in found:
        d[p] = a.columns
    for p in kc:
        d[p] = a.kc_width

    dst = a.out or a.image
    if a.out:
        shutil.copyfile(a.image, a.out)
    open(dst, "wb").write(bytes(d))
    print("patched %d sites -> columns = %d" % (len(found), a.columns))
    if kc:
        print("patched %d kc site(s) -> kc width = %d" % (len(kc), a.kc_width))
    print("wrote %s" % dst)


if __name__ == "__main__":
    main()
