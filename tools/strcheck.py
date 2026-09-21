#!/usr/bin/env python3
"""String literals of the original pseudocode missing from the reconstruction."""
import json,os,re,sys,subprocess
root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cu=json.load(open(os.path.join(root,'reverse/cu-map.json')))
idx={e['address']:e['names'][0] for e in json.load(open(os.path.join(root,'reverse/native-index.json')))}
allsrc=''
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
        path=os.path.join(dp,f); txt=open(path).read(); allsrc+=txt; lines=txt.split('\n')
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
                ours.setdefault(a,[]).append('\n'.join(lines[sj:k+1]))
for a,(f,how) in sorted(cu.items()):
    if f=='?': continue
    a8=a[2:].zfill(8)
    p=os.path.join(root,'reverse/native',a8+'.c')
    if not os.path.exists(p): continue
    strs=set(re.findall(r'"((?:[^"\\]|\\.){3,})"',open(p).read()))
    strs={s for s in strs if not s.startswith('/* ')}
    txt='\n'.join(ours.get(a8,[]))
    missing=[s for s in strs if s not in txt]
    if not missing: continue
    missing_all=[s for s in missing if s not in allsrc]
    name=subprocess.run(['c++filt',idx.get(a8,'?')],capture_output=True,text=True).stdout.strip()
    tag='NOWHERE' if missing_all else 'elsewhere'
    if tag=='NOWHERE' or a8 in ours:
        print(a8,f.split('|')[0],name[:60],tag,[s[:50] for s in (missing_all or missing)][:5])
