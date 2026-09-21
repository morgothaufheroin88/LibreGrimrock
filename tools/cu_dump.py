#!/usr/bin/env python3
"""Concatenate Ghidra pseudocode snapshots for one original compilation unit.
usage: cu_dump.py <CU name, e.g. String.cpp> [--list]"""
import json,sys,os
here=os.path.dirname(os.path.abspath(__file__))
root=os.path.dirname(here)
cu=json.load(open(os.path.join(root,'reverse/cu-map.json')))
idx={e['address']:e['names'][0] for e in json.load(open(os.path.join(root,'reverse/native-index.json')))}
want=sys.argv[1]
addrs=sorted(a for a,(f,how) in cu.items() if want in f.split('|'))
for a in addrs:
    a8=a[2:].zfill(8)
    name=idx.get(a8,'?')
    if '--list' in sys.argv:
        print(a8, cu[a][1], name[:120]); continue
    p=os.path.join(root,'reverse/native',a8+'.c')
    print('//////// %s %s [%s]'%(a8,name,cu[a][1]))
    if os.path.exists(p):
        lines=open(p).read().split('\n')
        print('\n'.join(l for l in lines[1:] if 'has its CatchHandler' not in l and 'Subroutine does not return' not in l))
