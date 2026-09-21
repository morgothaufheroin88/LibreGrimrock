#!/usr/bin/env python3
"""Compares float literals of each original function with the reconstruction that
carries its address comment. Reports literals of the original missing from ours."""
import json,os,re,struct,sys,subprocess
root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
cu=json.load(open(os.path.join(root,'reverse/cu-map.json')))
idx={e['address']:e['names'][0] for e in json.load(open(os.path.join(root,'reverse/native-index.json')))}
# Named constants (constexpr / static constexpr / enum values) defined anywhere under src:
# a function body that names the constant counts as containing its literal value.
NAMED_CONSTS = {}
def _collect_named_consts():
    pat = re.compile(r'\b(?:constexpr\s+(?:[\w:]+\s+)+|\b)(\w+)\s*=\s*(-?(?:0x[0-9a-fA-F]+|\d+\.\d+(?:e[+-]?\d+)?|\d+))(?:[uUlLfF]*)\s*[;,]')
    for dp,_,fs in os.walk(os.path.join(root,'src')):
        for f in fs:
            if not f.endswith(('.cpp','.h')): continue
            txt=open(os.path.join(dp,f)).read()
            for m in pat.finditer(txt):
                name, lit = m.group(1), m.group(2)
                line_start = txt.rfind('\n', 0, m.start())+1
                line = txt[line_start:m.end()]
                if 'constexpr' not in line and not re.match(r'\s*\w+\s*=', line): continue
                try:
                    if ('.' in lit or 'e' in lit.lower()) and not lit.startswith('0x'): v = float(lit)
                    elif re.match(r'-?0[0-7]+$', lit): v = int(lit, 8)   # C octal (0777)
                    else: v = int(lit, 0)
                except ValueError: continue
                NAMED_CONSTS.setdefault(name, set()).add(v)
_collect_named_consts()
def named_values(txt):
    vals=set()
    for name in set(re.findall(r'\b[A-Z][A-Za-z0-9_]*\b', txt)):
        vals |= NAMED_CONSTS.get(name, set())
    return vals

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
def floats_of(txt,hexfloats):
    vals=set()
    for m in re.finditer(r'(?<![\w.])(\d+\.\d+(?:e[+-]?\d+)?)f?',txt):
        vals.add(float(m.group(1)))
    if hexfloats:
        for m in re.finditer(r'0x([0-9a-f]{8})\b',txt):
            v=int(m.group(1),16)
            f=struct.unpack('<f',struct.pack('<I',v))[0]
            if f==f and 1e-6<abs(f)<1e7 and (v>>23)&0xff not in (0,0xff): vals.add(float('%.7g'%f))
    return vals
def close(a,b): return abs(a-b)<=1e-5*max(1,abs(a),abs(b))
want=sys.argv[1] if len(sys.argv)>1 else None
for a,(f,how) in sorted(cu.items()):
    if f=='?' or (want and want not in f): continue
    a8=a[2:].zfill(8)
    if a8 not in ours: continue
    p=os.path.join(root,'reverse/native',a8+'.c')
    if not os.path.exists(p): continue
    ov=floats_of(open(p).read(),True)
    ov={v for v in ov if v not in (0.0,1.0,0.5,2.0,255.0,-1.0)}
    if not ov: continue
    txt='\n'.join(t for _,_,t in ours[a8])
    mine=floats_of(txt,False)|{float(x) for x in re.findall(r'(?<![\w.])(\d+)(?![\w.])',txt)}|{float(v) for v in named_values(txt)}
    missing=[v for v in ov if not any(close(v,m) for m in mine) and not any(close(v,m*3.1415927/180) for m in mine) and not any(close(v*180/3.1415927,m) for m in mine)]
    if missing:
        name=subprocess.run(['c++filt',idx.get(a8,'?')],capture_output=True,text=True).stdout.strip()
        print(a8,f.split('|')[0],name[:80],'missing:',sorted(missing)[:8],'@',ours[a8][0][0]+':'+str(ours[a8][0][1]))
