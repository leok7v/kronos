#!/usr/bin/env python3
"""Stream a file to the board's `rx` receiver over UART (chunked, '.'-ack flow control).
  PORT=.. CONTENT_FILE=.. DEST=/usr1/c/foo GAP=0.008 python3 rxstream.py"""
import os, sys, termios, select, time, re
PORT=os.environ["PORT"]; CF=os.environ["CONTENT_FILE"]; DEST=os.environ["DEST"]
GAP=float(os.environ.get("GAP","0.008")); K=int(os.environ.get("K","64"))
CONTENT=open(CF,"rb").read()
fd=os.open(PORT,os.O_RDWR|os.O_NOCTTY)
a=termios.tcgetattr(fd)
a[0]=termios.IGNBRK;a[1]=0;a[2]=termios.CLOCAL|termios.CREAD|termios.CS8;a[3]=0
a[4]=termios.B57600;a[5]=termios.B57600;a[6][termios.VMIN]=0;a[6][termios.VTIME]=0
termios.tcsetattr(fd,termios.TCSANOW,a)
buf=bytearray()
def rd(t=0.2):
    r,_,_=select.select([fd],[],[],t)
    if r: buf.extend(os.read(fd,65536))
def wait_for(sub,tmo):
    e=time.time()+tmo
    while time.time()<e:
        rd()
        if sub.encode() in bytes(buf[-8000:]): return True
    return False
def slow(b):
    for x in b: os.write(fd,bytes([x])); time.sleep(GAP)
rd(0.5); os.write(fd,b"\r"); rd(1.0); buf.clear()
slow(("rx "+DEST+"\r").encode())
if not wait_for("rx: ready", 10): print("ERR: rx never became ready"); os.close(fd); sys.exit(1)
time.sleep(0.4)
mark=len(buf)                                   # ack-count only counts '.' after this point
slow(str(len(CONTENT)).encode()+b"\n")
t0=time.time()
if os.environ.get("SIMPLE"):
    slow(CONTENT)                                   # length already sent above
    wait_for("sum",20); rd(1.0); os.close(fd)
    s=bytes(buf).decode("latin1"); s=re.sub(r"\x1b\[[0-9;?]*[A-Za-z]","",s)
    m=re.search(r"rx: (\d+) bytes sum (\d+)",s); hs=sum(CONTENT)%65521
    print("RESULT:", m.group(0) if m else "NO REPORT")
    print("host: %d bytes sum %d"%(len(CONTENT),hs))
    print("MATCH" if (m and int(m.group(1))==len(CONTENT) and int(m.group(2))==hs) else "MISMATCH")
    print("elapsed: %.0fs"%(time.time()-t0)); sys.exit(0)
pos=0; want=0
while pos<len(CONTENT):
    for x in CONTENT[pos:pos+K]: os.write(fd,bytes([x])); time.sleep(GAP)
    pos+=K; want+=1
    e=time.time()+12; ok=False
    while time.time()<e:
        rd()
        if bytes(buf[mark:]).count(b".")>=want: ok=True; break
    if not ok: print("ERR: ack timeout at chunk %d (pos %d)"%(want,pos)); break
    time.sleep(float(os.environ.get("POSTACK","0.3")))   # let rx return to key.read before next chunk
elapsed=time.time()-t0
wait_for("sum", 12); rd(1.0)
os.close(fd)
s=bytes(buf).decode("latin1"); s=re.sub(r'\x1b\[[0-9;?]*[A-Za-z]','',s)
m=re.search(r"rx: (\d+) bytes sum (\d+)", s)
print("RESULT:", m.group(0) if m else "NO 'rx:' REPORT")
print("elapsed: %.0fs  (%d bytes, %d chunks)"%(elapsed,len(CONTENT),want))
# host expected
hs=sum(CONTENT)%65521
print("host: %d bytes sum %d"%(len(CONTENT),hs))
if m: print("MATCH" if (int(m.group(1))==len(CONTENT) and int(m.group(2))==hs) else "MISMATCH")
# echo check: did a distinctive content substring come back?
probe=CONTENT[:20].decode("latin1","replace")
print("console echoes input:", probe in s)
