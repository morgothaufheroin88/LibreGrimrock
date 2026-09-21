#!/usr/bin/env python3
"""Integer literals (>= 8) used as values in the original (not as offsets) missing from ours."""
import json,os,re,sys,subprocess
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
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
def ints_of_orig(txt):
    vals=set()
    # remove offset-like contexts
    t=re.sub(r'(this|param_\d+|iVar\d+|piVar\d+|puVar\d+|pcVar\d+|local_[0-9a-f]+|pfVar\d+|uVar\d+)\s*\+\s*(-?0x[0-9a-f]+|-?\d+)','',txt)
    t=re.sub(r'\[\s*(0x[0-9a-f]+|\d+)\s*\]','',t)
    t=re.sub(r'\*\s*(0x[0-9a-f]+|\d+)','',t)   # strides
    t=re.sub(r'(<<|>>)\s*(0x[0-9a-f]+|\d+)','',t)
    t=re.sub(r'operator_new(__)?\((0x[0-9a-f]+|\d+)\)','',t)
    t=re.sub(r'(DAT|PTR|LAB|FUN|C)_[0-9a-f]+','',t)
    t=re.sub(r'0x8[0-9a-f]{6}\b','',t)
    for m in re.finditer(r'(?<![\w.])(0x[0-9a-f]+|\d+)(?![\w.])',t):
        s=m.group(1); v=int(s,16) if s.startswith('0x') else int(s)
        if 8<=v<0x7fffff00 and v not in (0xff,0xffff,0xffffffff,255,256,0x100): vals.add(v)
    return vals
def ints_of_ours(txt):
    vals=set()
    for m in re.finditer(r'(?<![\w.])(0x[0-9a-fA-F]+|\d+)(?![\w.])',txt):
        s=m.group(1); vals.add(int(s,16) if s.lower().startswith('0x') else int(s))
    return vals
want=sys.argv[1] if len(sys.argv)>1 else None
for a,(f,how) in sorted(cu.items()):
    if f=='?' or (want and want not in f): continue
    a8=a[2:].zfill(8)
    if a8 not in ours: continue
    p=os.path.join(root,'reverse/native',a8+'.c')
    if not os.path.exists(p): continue
    ov=ints_of_orig(open(p).read())
    if not ov: continue
    txt='\n'.join(t for _,_,t in ours[a8])
    mine=ints_of_ours(txt)|{int(v) for v in named_values(txt) if float(v).is_integer()}
    missing=sorted(v for v in ov if v not in mine and v-1 not in mine and v+1 not in mine and v//64 not in mine)
    if missing:
        name=subprocess.run(['c++filt',idx.get(a8,'?')],capture_output=True,text=True).stdout.strip()
        print(a8,f.split('|')[0],name[:70],'missing:',[hex(v) if v>=4096 else v for v in missing][:8],'@',ours[a8][0][0]+':'+str(ours[a8][0][1]))
