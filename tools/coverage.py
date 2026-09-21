#!/usr/bin/env python3
"""Lists original functions (reverse/native-index.json) that no source file under src/
references by address (// 0x........ comments). usage: coverage.py [--all] [CU-substring]"""
import json,os,re,sys,subprocess
root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
idx=json.load(open(os.path.join(root,'reverse/native-index.json')))
cu=json.load(open(os.path.join(root,'reverse/cu-map.json')))
refs=set()
for dp,_,fs in os.walk(os.path.join(root,'src')):
    for f in fs:
        if f.endswith(('.cpp','.h','.c')):
            for m in re.finditer(r'0x(0?8[0-9a-f]{6})',open(os.path.join(dp,f)).read()):
                refs.add(m.group(1).zfill(8))
want=[a for a in sys.argv[1:] if not a.startswith('--')]
missing=[];total=0
for e in idx:
    a=e['address'].zfill(8); name=e['names'][0]
    c=cu.get('0x'+a.lstrip('0'),['?'])[0]
    if want and not any(w in c for w in want): continue
    total+=1
    if a not in refs: missing.append((c,a,name))
missing.sort()
names=subprocess.run(['c++filt']+[m[2] for m in missing],capture_output=True,text=True).stdout.split('\n')
for (c,a,_),n in zip(missing,names): print(a,c,n)
print('%d of %d unreferenced'%(len(missing),total),file=sys.stderr)
