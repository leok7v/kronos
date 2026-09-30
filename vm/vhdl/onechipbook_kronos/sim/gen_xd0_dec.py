#!/usr/bin/env python3
# Convert xd0.dsk (raw disk image) into xd0.dec for sd_model_xd0:
# one decimal byte per line, whole image.
# Usage: python3 gen_xd0_dec.py [../../../../xd0.dsk] [xd0.dec]
import sys, os

src = sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), "../../../../xd0.dsk")
out = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), "xd0.dec")

data = open(src, "rb").read()
with open(out, "w") as f:
    f.write("\n".join(str(b) for b in data))
    f.write("\n")
print("wrote %s: %d bytes (%d sectors)" % (out, len(data), (len(data) + 511) // 512))
