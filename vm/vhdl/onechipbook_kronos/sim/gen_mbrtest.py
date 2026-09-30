#!/usr/bin/env python3
# Build a tiny partitioned SD image for tb_mbr.vhdl (decimal-per-line, the
# format sd_model_xd0 loads). It exercises the MBR parser in sd_disk_controller:
#
#   * Every sector's first 4 bytes = its own ABSOLUTE sector number (LE), so a
#     read of unit U sector S must return part_base(U)+S -- the exact quantity
#     the controller is supposed to compute (disk_base + cur_sec).
#   * Sector 0 additionally carries a real MBR: three primary partitions plus
#     the 0xAA55 signature.
#
#   partition 1 (unit 0): base LBA 8,  16 sectors, type 0xDA (Excelsior)
#   partition 2 (unit 1): base LBA 32, 16 sectors, type 0xDA (Excelsior)
#   partition 3 (unit 2): base LBA 64, 16 sectors, type 0x0E (FAT16)
#
# Keep these numbers in sync with the expectations in tb_mbr.vhdl.
import sys

SECTORS = 128
PARTS = [  # (base_lba, num_sectors, type_byte)
    (8,  16, 0xDA),
    (32, 16, 0xDA),
    (64, 16, 0x0E),
]

img = bytearray(SECTORS * 512)

# self-identifying sectors: first word = absolute sector index
for i in range(SECTORS):
    img[i*512:i*512+4] = i.to_bytes(4, "little")

# MBR partition table at offset 446, 16 bytes per entry
for n, (base, size, ptype) in enumerate(PARTS):
    off = 446 + 16*n
    img[off+4]        = ptype                        # partition type
    img[off+8:off+12] = base.to_bytes(4, "little")   # LBA start
    img[off+12:off+16] = size.to_bytes(4, "little")  # sector count

# boot signature
img[510] = 0x55
img[511] = 0xAA

out = sys.argv[1] if len(sys.argv) > 1 else "mbrtest.dec"
with open(out, "w") as f:
    f.write("\n".join(str(b) for b in img))
print(f"wrote {out}: {SECTORS} sectors")
