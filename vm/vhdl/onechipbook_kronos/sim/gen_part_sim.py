#!/usr/bin/env python3
# Partitioned reproduction image for the OneChipBook boot sim.
#
# Places xd0.dsk at sector 64 (a NON-ZERO partition base) behind a real MBR, to
# reproduce/validate the partitioned boot path the hardware exercises:
#   * boot loader is read from part_base(0)=64  (was absolute sector 0)
#   * MBR P1 and P2 BOTH point at base 64, so unit 1 (the OS/root device the
#     booter uses, "xd1") aliases to xd0 here -- letting read_system find a valid
#     SYSTEM.BOOT from the single xd0 image and reach "Loaded OK" if the
#     non-zero-base boot works. (On the real card P1/P2 are distinct.)
# Output: xd0.dec (decimal-per-line), SECTORS = 1644.
import sys

BASE = 64          # partition-1/2 start sector
XD0_SECS = 1580
SECTORS = BASE + XD0_SECS   # 1644

xd0 = open("../../../../xd0.dsk", "rb").read()
assert len(xd0) == XD0_SECS * 512, f"xd0.dsk is {len(xd0)} bytes, expected {XD0_SECS*512}"

img = bytearray(SECTORS * 512)
img[BASE*512 : BASE*512 + len(xd0)] = xd0

# minimal MBR at sector 0 (the parser only needs type/LBA/size/signature)
def entry(off, ptype, start, count):
    img[off+4]         = ptype
    img[off+8:off+12]  = start.to_bytes(4, "little")
    img[off+12:off+16] = count.to_bytes(4, "little")

entry(446,      0xDA, BASE, XD0_SECS)   # P1 -> unit 0 (boot loader)
entry(446 + 16, 0xDA, BASE, XD0_SECS)   # P2 -> unit 1 (OS/root), aliased to xd0
img[510], img[511] = 0x55, 0xAA

out = sys.argv[1] if len(sys.argv) > 1 else "xd0.dec"
with open(out, "w") as f:
    f.write("\n".join(str(b) for b in img))
print(f"wrote {out}: {SECTORS} sectors, xd0 at sector {BASE}")
