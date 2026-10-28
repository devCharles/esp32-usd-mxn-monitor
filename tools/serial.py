#!/usr/bin/env python3
"""POSIX USB monitor and framebuffer capture; no external dependencies."""
import argparse
import os
from pathlib import Path
import select
import termios
import time

parser=argparse.ArgumentParser()
parser.add_argument('port')
parser.add_argument('--seconds',type=float,default=15)
parser.add_argument('--send',default='d')
parser.add_argument('--capture',type=Path)
args=parser.parse_args()
fd=os.open(args.port,os.O_RDWR|os.O_NOCTTY|os.O_NONBLOCK)
try:
    attrs=termios.tcgetattr(fd)
    attrs[0]=0;attrs[1]=0;attrs[2]=termios.CS8|termios.CREAD|termios.CLOCAL;attrs[3]=0
    attrs[4]=termios.B115200;attrs[5]=termios.B115200
    attrs[6][termios.VMIN]=0;attrs[6][termios.VTIME]=0
    termios.tcsetattr(fd,termios.TCSANOW,attrs)
    time.sleep(0.25)
    os.write(fd,(('s' if args.capture else args.send)).encode())
    deadline=time.monotonic()+args.seconds
    pending=b''
    header=b'FRAME RGB565 240 284\r\n'
    while time.monotonic()<deadline:
        if not select.select([fd],[],[],min(.25,max(0,deadline-time.monotonic())))[0]:continue
        chunk=os.read(fd,65536)
        if not chunk:continue
        if args.capture:
            pending+=chunk
            start=pending.find(header)
            if start>=0 and len(pending)>=start+len(header)+240*284*2:
                data=pending[start+len(header):start+len(header)+240*284*2]
                args.capture.parent.mkdir(parents=True,exist_ok=True)
                args.capture.write_bytes(data)
                print('Captura RGB565 guardada:',args.capture)
                break
        else:
            pending+=chunk
            while b'\n' in pending:
                line,pending=pending.split(b'\n',1)
                print(line.decode(errors='replace').rstrip(),flush=True)
    else:
        if args.capture:raise SystemExit('No se recibió una captura completa.')
finally:
    os.close(fd)
