#!/usr/bin/env python3
"""Original functions that construct exceptions/luaL_error where ours has fewer."""
import json,os,re,sys,subprocess
root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cu=json.load(open(os.path.join(root,'reverse/cu-map.json')))
idx={e['address']:e['names'][0] for e in json.load(open(os.path.join(root,'reverse/native-index.json')))}
def sig_start(lines, i, j):
    # Allman: the brace is alone on its line; the signature is the line(s) before it.
    s = j
    if lines[j].strip() == '{':
        s = j - 1
        while s > i + 1 and lines[s-1].strip() and not lines[s-1].strip().startswith('//') \
                and not lines[s-1].rstrip().endswith((';', '}', '{')):
            s -= 1
    return s

ours={}
for dp,_,fs in os.walk(os.path.join(root,'src')):
    for f in fs:
        if not f.endswith(('.cpp','.h')): continue
        path=os.path.join(dp,f); lines=open(path).read().split('\n')
        for i,l in enumerate(lines):
            if not l.strip().startswith('//'): continue
            for m in re.finditer(r'0x(0?8[0-9a-f]{6})',l):
                a=m.group(1).zfill(8); j=i+1
                while j<len(lines) and j-i<6 and not lines[j].rstrip().endswith('{'): j+=1
                if j>=len(lines) or not lines[j].rstrip().endswith('{'): continue
                depth=0;k=j;sj=sig_start(lines,i,j)
                while k<len(lines):
                    depth+=lines[k].count('{')-lines[k].count('}')
                    if depth<=0: break
                    k+=1
                ours.setdefault(a,[]).append((os.path.relpath(path,root),sj+1,'\n'.join(lines[sj:k+1])))
for a,(f,how) in sorted(cu.items()):
    if f=='?' or a[2:].zfill(8) not in ours: continue
    a8=a[2:].zfill(8)
    p=os.path.join(root,'reverse/native',a8+'.c')
    if not os.path.exists(p): continue
    o=open(p).read()
    n_o=len(re.findall(r'Exception::Exception\(|luaL_error\(|luaL_argerror\(|luaL_typerror\(',o))
    txt='\n'.join(t for _,_,t in ours[a8])
    n_m=len(re.findall(r'\bthrow\b|luaL_error\(|luaL_argerror\(|luaL_typerror\(|checkObject<|checkEnum\(|checkBool\(|checkVector|checkColor\(|checkMatrix|checkBox\(|checkRay\(|checkUInt64|alError\(|checkGLErrors\(',txt))
    if n_o>n_m:
        name=subprocess.run(['c++filt',idx.get(a8,'?')],capture_output=True,text=True).stdout.strip()
        print(a8,f.split('|')[0],name[:70],'orig',n_o,'ours',n_m,'@',ours[a8][0][0]+':'+str(ours[a8][0][1]))
