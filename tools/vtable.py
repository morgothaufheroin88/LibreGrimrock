#!/usr/bin/env python3
"""Dump vtables from Grimrock.bin.x86: vtable.py <mangled-or-demangled class substring>..."""
import subprocess,struct,re,sys,os
BIN = os.environ.get("GRIMROCK_BIN", os.path.expanduser("~/.local/share/Steam/steamapps/common/Legend of Grimrock/Grimrock.bin.x86.orig"))
root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
segs=[]
for l in subprocess.run(['readelf','-lW',BIN],capture_output=True,text=True).stdout.splitlines():
    p=l.split()
    if p and p[0]=='LOAD': segs.append((int(p[2],16),int(p[1],16),int(p[4],16)))
data=open(BIN,'rb').read()
def rd(v,n):
    for va,off,sz in segs:
        if va<=v<va+sz: return data[off+v-va:off+v-va+n]
funcs={};vts=[]
for line in open(os.path.join(root,'reverse/symtab.txt')):
    p=line.split()
    if len(p)>=8 and re.match(r'^\d+:$',p[0]):
        if p[3]=='FUNC': funcs[int(p[1],16)]=p[7]
        if p[3]=='OBJECT' and p[7].startswith('_ZTV'): vts.append((int(p[1],16),int(p[2]),p[7]))
names={}
def dem(s):
    if s not in names: names[s]=subprocess.run(['c++filt',s],capture_output=True,text=True).stdout.strip()
    return names[s]
seen=set()
for addr,size,sym in vts:
    d=dem(sym)
    if not any(a in d for a in sys.argv[1:]) or addr in seen: continue
    seen.add(addr)
    b=rd(addr,size)
    print("==",d)
    for i in range(size//4):
        v=struct.unpack('<I',b[i*4:i*4+4])[0]
        print(f"  +{i*4:#05x} {v:08x} {dem(funcs[v])[:100] if v in funcs else ''}")
