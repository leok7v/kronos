#!/usr/bin/env python3
"""Minimal serial console driver for the Kronos board (57600 8N1, stdlib only).

Usage:  sercmd.py [SEND] [DURATION_SEC]
  SEND      : bytes to transmit first; C-style escapes (\\r \\n \\t \\xNN) honored.
              Empty string = listen only.
  DURATION  : seconds to read after sending (default 4).
Raw bytes received are written to stdout; also saved to sercmd_last.bin.
"""
import os, sys, termios, time, select

PORT = os.environ.get("PORT","/dev/ttyUSB0")
BAUD = termios.B57600
send = sys.argv[1] if len(sys.argv) > 1 else ""
dur = float(sys.argv[2]) if len(sys.argv) > 2 else 4.0

fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY)
a = termios.tcgetattr(fd)
a[0] = termios.IGNBRK                                   # iflag: raw
a[1] = 0                                                # oflag: raw
a[2] = termios.CLOCAL | termios.CREAD | termios.CS8     # cflag: 8N1, ignore modem lines
a[3] = 0                                                # lflag: raw (no echo/canon)
a[4] = BAUD
a[5] = BAUD
a[6][termios.VMIN] = 0
a[6][termios.VTIME] = 0
termios.tcsetattr(fd, termios.TCSANOW, a)

if send:
    data = send.encode().decode("unicode_escape").encode("latin1")
    for byte in data:                                  # byte-at-a-time, gentle pacing
        os.write(fd, bytes([byte]))
        time.sleep(0.02)

out = bytearray()
end = time.time() + dur
while time.time() < end:
    r, _, _ = select.select([fd], [], [], 0.2)
    if r:
        try:
            chunk = os.read(fd, 4096)
        except OSError:
            break
        if chunk:
            out.extend(chunk)
os.close(fd)

with open(os.environ.get("CAPFILE", "sercmd_last.bin"), "wb") as f:  # capture, cwd by default
    f.write(bytes(out))
sys.stdout.buffer.write(bytes(out))
