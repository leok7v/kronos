#!/usr/bin/env python3
"""Generate src/font_rom.vhdl -- the text-console font ROM.

The console must render the OS's FULL character set, not just ASCII:
  0x20..0x7E  ASCII
  0x80..0xBF  pseudographics (window frames, shading)
  0xC0..0xFF  CYRILLIC -- the system's own sources are commented in Russian,
              so without these, viewing any OS source shows garbage

The authoritative mapping is the reference VM's own translation table,
vm/int/SourceCode/cO_win32_display.cpp: `tr[256]` converts a Kronos character
code into the CP866 code the Windows console displays. So for any Kronos code:

    kronos -> tr[] -> CP866 -> Unicode -> glyph

That table is PARSED from the C++ source rather than transcribed, so it cannot
drift from the VM. Guessing the high range from appearances is exactly how this
went wrong before: an earlier build left Latin-1 accented letters at 0xC0..0xFF
and Russian text came out as noise. (The table also confirms the frame
characters independently: Kronos 0xA4 -> CP866 0xC4 = U+2500, 0xB3 -> U+2502.)

Glyphs come from an 8x14 console font, looked up BY UNICODE through the font's
own PSF unicode table -- console fonts share a single glyph between visually
identical Latin and Cyrillic letters, and the unicode table resolves that
correctly where a positional guess would not. Box-drawing characters absent
from the font are drawn procedurally.
"""
import gzip
import os
import re
import sys

FIRST, LAST = 0x20, 0xFF
WIDTH, HEIGHT = 8, 14
HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "src", "font_rom.vhdl")
TRSRC = os.path.join(HERE, "..", "..", "..", "int", "SourceCode",
                     "cO_win32_display.cpp")
FONTS = ["/usr/share/consolefonts/CyrKoi-VGA14.psf.gz",
         "/usr/share/consolefonts/Lat15-VGA14.psf.gz"]


def read_tr():
    """Kronos code -> CP866 code, parsed from the VM's own table."""
    src = open(TRSRC).read()
    m = re.search(r"const\s+int\s+tr\s*\[\s*256\s*\]\s*=\s*\{(.*?)\}", src, re.S)
    if not m:
        sys.exit("tr[256] not found in %s" % TRSRC)
    vals = [int(x, 0) for x in re.findall(r"0x[0-9A-Fa-f]+", m.group(1))]
    if len(vals) != 256:
        sys.exit("tr[] has %d entries, expected 256" % len(vals))
    return vals


def read_psf(path):
    """-> (glyphs by index, unicode -> index). PSF1, 8 px wide."""
    d = gzip.open(path, "rb").read()
    if d[0] != 0x36 or d[1] != 0x04:
        sys.exit("not a PSF1 font: %s" % path)
    mode, cs = d[2], d[3]
    n = 512 if mode & 1 else 256
    glyphs = [list(d[4 + i * cs: 4 + i * cs + cs]) for i in range(n)]
    uni = {}
    if mode & 2:                       # unicode table present
        tab, idx, pos = d[4 + n * cs:], 0, 0
        while pos + 1 < len(tab) and idx < n:
            v = tab[pos] | (tab[pos + 1] << 8)
            pos += 2
            if v == 0xFFFF:
                idx += 1
            elif v != 0xFFFE:
                uni.setdefault(v, idx)
    return glyphs, uni


def box_glyph(u):
    """Single-line frame characters, for anything the font lacks."""
    V, LEFT, RIGHT, FULL = 0x10, 0xF0, 0x1F, 0xFF
    mid = HEIGHT // 2 - 1
    spec = {0x2500: (0, 0, 1, 1), 0x2502: (1, 1, 0, 0),
            0x250C: (0, 1, 0, 1), 0x2510: (0, 1, 1, 0),
            0x2514: (1, 0, 0, 1), 0x2518: (1, 0, 1, 0),
            0x251C: (1, 1, 0, 1), 0x2524: (1, 1, 1, 0),
            0x252C: (0, 1, 1, 1), 0x2534: (1, 0, 1, 1),
            0x253C: (1, 1, 1, 1)}
    if u not in spec:
        return None
    up, down, left, right = spec[u]
    g = []
    for r in range(HEIGHT):
        v = 0
        if (r < mid and up) or (r > mid and down):
            v |= V
        if r == mid:
            if up or down:
                v |= V
            if left:
                v |= LEFT
            if right:
                v |= RIGHT
            if not (up or down):
                v = FULL
        g.append(v)
    return g


def main():
    tr = read_tr()
    fonts = [read_psf(p) for p in FONTS if os.path.exists(p)]
    if not fonts:
        sys.exit("no 8x14 console font found (install console-data)")

    def glyph_for(u):
        for glyphs, uni in fonts:
            i = uni.get(u)
            if i is not None:
                return glyphs[i][:HEIGHT]
        return box_glyph(u)

    def uni_of(code):
        return code if code < 0x80 else ord(bytes([tr[code]]).decode("cp866"))

    stride = 16                        # power of two: address is a concatenation
    rows, missing = [], []
    for code in range(FIRST, LAST + 1):
        g = glyph_for(uni_of(code))
        if g is None:
            missing.append((code, uni_of(code)))
            g = [0] * HEIGHT
        rows.extend((g + [0] * stride)[:stride])

    depth = 1
    while depth < len(rows):
        depth *= 2
    padded = rows + [0] * (depth - len(rows))
    lines = ["        " + ", ".join('x"%02X"' % b for b in padded[i:i + 16])
             for i in range(0, len(padded), 16)]
    abits = (depth - 1).bit_length()

    with open(OUT, "w") as f:
        f.write('''library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_unsigned.all;

-- %dx%d text font ROM -- GENERATED by tools/gen_font_rom.py, do not hand-edit.
--
-- Covers 0x%02X..0x%02X: ASCII, the OS's pseudographics, and CYRILLIC. The high
-- range is derived from the reference VM's own translation table
-- (vm/int/SourceCode/cO_win32_display.cpp, tr[256]) through CP866 and Unicode,
-- so it cannot drift from what the VM shows. Glyphs are %d rows, left-aligned
-- with MSB = leftmost pixel; the stride is %d so the console builds the address
-- by concatenation instead of a multiply (which failed timing at 64 MHz).
entity font_rom is
    port (
        clk  : in  std_logic;
        addr : in  std_logic_vector(%d downto 0);   -- (char-32)*%d + row
        data : out std_logic_vector(7 downto 0));   -- registered, 1 cycle late
end font_rom;

architecture rom of font_rom is
    type rom_t is array(0 to %d) of std_logic_vector(7 downto 0);
    constant FONT : rom_t := (
%s);
begin
    process (clk)
    begin
        if rising_edge(clk) then
            data <= FONT(conv_integer(addr));
        end if;
    end process;
end architecture rom;
''' % (WIDTH, HEIGHT, FIRST, LAST, HEIGHT, stride, abits - 1, stride,
       depth - 1, ",\n".join(lines)))

    print("wrote %s: %dx%d, %d codes, %d ROM bytes, addr %d bits"
          % (OUT, WIDTH, HEIGHT, LAST - FIRST + 1, depth, abits))
    if missing:
        print("WARNING: no glyph for %d codes: %s"
              % (len(missing), ", ".join("%02X(U+%04X)" % m for m in missing[:12])))

    for code, nm in ((0xC1, "Cyrillic a"), (0xE1, "Cyrillic A"),
                     (0xA4, "frame HBAR")):
        g = glyph_for(uni_of(code)) or [0] * HEIGHT
        print("--- 0x%02X %s (U+%04X) ---" % (code, nm, uni_of(code)))
        for r in g:
            print("   " + "".join("#" if r & (0x80 >> b) else "." for b in range(WIDTH)))


if __name__ == "__main__":
    main()
