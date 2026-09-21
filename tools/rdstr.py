#!/usr/bin/env python3
"""Print C strings at addresses in Grimrock.bin.x86: rdstr.py addr [addr...]; or a
string table: rdstr.py --table addr count (pairs of {char*, int})."""
import subprocess,struct,sys
import os
BIN = os.environ.get("GRIMROCK_BIN", os.path.expanduser("~/.local/share/Steam/steamapps/common/Legend of Grimrock/Grimrock.bin.x86.orig"))
segs=[]
for l in subprocess.run(['readelf','-lW',BIN],capture_output=True,text=True).stdout.splitlines():
    p=l.split()
    if p and p[0]=='LOAD': segs.append((int(p[2],16),int(p[1],16),int(p[4],16)))
data=open(BIN,'rb').read()
def rd(v,n):
    for va,off,sz in segs:
        if va<=v<va+sz: return data[off+v-va:off+v-va+n]
    return b''
def cstr(v):
    b=rd(v,256); return b.split(b'\0')[0].decode('latin1')
if sys.argv[1]=='--table':
    a=int(sys.argv[2],16); n=int(sys.argv[3])
    for i in range(n):
        p,val=struct.unpack('<II',rd(a+i*8,8))
        print(f"  {cstr(p)!r}: {val}")
else:
    for a in sys.argv[1:]: print(hex(int(a,16)), repr(cstr(int(a,16))))
