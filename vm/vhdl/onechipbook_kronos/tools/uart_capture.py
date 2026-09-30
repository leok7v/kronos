#!/usr/bin/env python3
"""Record the board's console stream off the UART, byte-exact.

The board mirrors every console byte to uart_txd (PIN_3) at 57600 8N1, so this
is the hardware's own output -- the thing that was missing every time a bug had
to be diagnosed from the reference VM instead.

RAW mode matters. The tty layer would otherwise translate CR/LF and eat control
characters, and the escape sequences ARE the evidence: ESC J, ESC[50;1H, ESC[1T
are exactly what distinguishes a console bug from an OS one.

Output format matches tools the analysis already uses:

    @<seconds> rx <hex bytes>

so a capture from the board can be fed to the same decoders as a capture from
the VM. Ctrl-C to stop.

    python3 uart_capture.py                     -> kronos.log at 57600
    python3 uart_capture.py -o boot.log -b 9600
    python3 uart_capture.py --text              -> also print decoded text live
"""
import argparse
import os
import sys
import termios
import time
import tty

BAUDS = {9600: termios.B9600, 19200: termios.B19200, 38400: termios.B38400,
         57600: termios.B57600, 115200: termios.B115200}


def open_raw(dev, baud):
    fd = os.open(dev, os.O_RDONLY | os.O_NOCTTY)
    attrs = termios.tcgetattr(fd)
    iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attrs
    # 8N1, receiver on, ignore modem control lines
    cflag |= termios.CLOCAL | termios.CREAD
    cflag &= ~(termios.PARENB | termios.CSTOPB | termios.CSIZE)
    cflag |= termios.CS8
    cflag &= ~termios.CRTSCTS
    # RAW: no translation of any kind, no echo, no signal chars
    iflag &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK | termios.ISTRIP |
               termios.INLCR | termios.IGNCR | termios.ICRNL | termios.IXON)
    oflag &= ~termios.OPOST
    lflag &= ~(termios.ECHO | termios.ECHONL | termios.ICANON |
               termios.ISIG | termios.IEXTEN)
    cc[termios.VMIN] = 0
    cc[termios.VTIME] = 1                      # 0.1 s read timeout
    spd = BAUDS[baud]
    termios.tcsetattr(fd, termios.TCSANOW,
                      [iflag, oflag, cflag, lflag, spd, spd, cc])
    return fd


def show(b):
    out = []
    for c in b:
        if c == 0x1B:
            out.append("<ESC>")
        elif c == 0x0D:
            out.append("<CR>")
        elif c == 0x0A:
            out.append("<LF>\n")
        elif c < 0x20 or c > 0x7E:
            out.append("<%02X>" % c)
        else:
            out.append(chr(c))
    return "".join(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("-d", "--device", default="/dev/ttyUSB0")
    ap.add_argument("-b", "--baud", type=int, default=57600, choices=sorted(BAUDS))
    ap.add_argument("-o", "--out", default="kronos.log")
    ap.add_argument("--text", action="store_true",
                    help="also print decoded text as it arrives")
    a = ap.parse_args()

    try:
        fd = open_raw(a.device, a.baud)
    except PermissionError:
        sys.exit("no permission for %s -- you are in 'dialout', so try "
                 "re-plugging the adapter or logging out and back in" % a.device)
    except FileNotFoundError:
        sys.exit("%s not found -- is the adapter plugged in? "
                 "check: ls /dev/ttyUSB* /dev/ttyACM*" % a.device)

    total, start = 0, time.time()
    print("recording %s at %d 8N1 -> %s   (Ctrl-C to stop)"
          % (a.device, a.baud, a.out))
    try:
        with open(a.out, "w") as f:
            while True:
                d = os.read(fd, 4096)
                if not d:
                    continue
                f.write("@%.3f rx %s\n" % (time.time() - start, d.hex()))
                f.flush()
                total += len(d)
                if a.text:
                    sys.stdout.write(show(d))
                    sys.stdout.flush()
                else:
                    print("\r%d bytes" % total, end="", flush=True)
    except KeyboardInterrupt:
        pass
    finally:
        os.close(fd)
    print("\n%d bytes written to %s" % (total, a.out))


if __name__ == "__main__":
    main()
