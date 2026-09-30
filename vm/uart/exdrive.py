#!/usr/bin/env python3
"""Drive the Excelsior `ex` editor over UART to create a file.
  PORT=/dev/ttyUSB0 EXFILE=/usr1/c/zz.txt SRC=path_to_text python3 exdrive.py
Types the SRC text (newlines -> CR), then Ctrl-E to save+exit."""
import os, sys, termios, select, time
PORT=os.environ.get("PORT","/dev/ttyUSB0")
EXFILE=os.environ["EXFILE"]
SRC=os.environ["SRC"]
GAP=float(os.environ.get("GAP","0.02"))
text=open(SRC).read()
fd=os.open(PORT,os.O_RDWR|os.O_NOCTTY)
a=termios.tcgetattr(fd)
a[0]=termios.IGNBRK;a[1]=0;a[2]=termios.CLOCAL|termios.CREAD|termios.CS8;a[3]=0
a[4]=termios.B57600;a[5]=termios.B57600;a[6][termios.VMIN]=0;a[6][termios.VTIME]=0
termios.tcsetattr(fd,termios.TCSANOW,a)
buf=bytearray()
def drain(t):
    e=time.time()+t
    while time.time()<e:
        r,_,_=select.select([fd],[],[],0.1)
        if r: buf.extend(os.read(fd,8192))
def send(b):
    for x in b: os.write(fd,bytes([x])); time.sleep(GAP)
drain(0.5); os.write(fd,b"\r"); drain(1.0)
send(("ex "+EXFILE+"\r").encode()); drain(4.0)          # open editor (new file), let it settle
os.write(fd,b"\r"); time.sleep(0.3)                     # throwaway Enter: absorbs the eaten first keystroke
for ch in text:                                          # type the source
    if ch=="\n": os.write(fd,b"\r")                      # Enter = new line
    else: os.write(fd,ch.encode())
    time.sleep(GAP)
drain(1.0)
os.write(fd,bytes([0x05])); drain(2.5)                   # Ctrl-E = save + exit
os.write(fd,b"\r"); drain(1.0)
os.close(fd)
import re
s=bytes(buf).decode("koi8_r","replace")
s=re.sub(r'\x1b\[[0-9;?]*[A-Za-z]','',s); s=s.replace('\r','\n')
print("tail:", repr(s[-120:]))
