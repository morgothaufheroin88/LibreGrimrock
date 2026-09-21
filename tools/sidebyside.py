#!/usr/bin/env python3
"""For a CU: prints each original function's cleaned pseudocode followed by the
reconstructed function that carries its '// 0xADDR' comment. usage: sidebyside.py CU [minlines]"""
import json,os,re,sys,subprocess
root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cu=json.load(open(os.path.join(root,'reverse/cu-map.json')))
idx={e['address']:e['names'][0] for e in json.load(open(os.path.join(root,'reverse/native-index.json')))}
want=sys.argv[1]; minlines=int(sys.argv[2]) if len(sys.argv)>2 else 8
# index our functions by address comment
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
            for m in re.finditer(r'0x(0?8[0-9a-f]{6})',l):
                if not l.strip().startswith('//'): continue
                a=m.group(1).zfill(8)
                j=i+1
                while j<len(lines) and j-i<6 and not lines[j].rstrip().endswith('{'): j+=1
                if j>=len(lines) or not lines[j].rstrip().endswith('{'): continue
                depth=0;k=j;sj=sig_start(lines,i,j)
                while k<len(lines):
                    depth+=lines[k].count('{')-lines[k].count('}')
                    if depth<=0: break
                    k+=1
                ours.setdefault(a,[]).append((os.path.relpath(path,root),sj+1,'\n'.join(lines[sj:k+1])))
NOISE=re.compile(r'^\s*(undefined\w*|int|uint|float|double|char|bool|byte|short|long|size_t|code|void|[A-Za-z_:<>]+)\s+\**\w+(\s*\[\d*\])?;\s*$')
addrs=sorted(a for a,(f,how) in cu.items() if want in f.split('|'))
for a in addrs:
    a8=a[2:].zfill(8); name=idx.get(a8,'?')
    if 'Array<' in name or 'SharedPtr<' in name or '::~' in name or 'global constructors' in name or 'std::' in name: continue
    p=os.path.join(root,'reverse/native',a8+'.c')
    if not os.path.exists(p): continue
    body=[l for l in open(p).read().split('\n')[1:] if l.strip() and not NOISE.match(l) and 'CatchHandler' not in l and 'does not return' not in l]
    if len(body)<minlines: continue
    print('\n######## ORIGINAL %s %s'%(a8,subprocess.run(['c++filt',name],capture_output=True,text=True).stdout.strip()))
    print('\n'.join(body))
    print('-------- OURS')
    for path,ln,txt in ours.get(a8,[]):
        print('// %s:%d'%(path,ln)); print(txt)
    if a8 not in ours: print('(no function carries this address)')
