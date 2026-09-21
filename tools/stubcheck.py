#!/usr/bin/env python3
"""Flags functions whose reconstruction is much shorter than the original pseudocode.
Pairs '// 0xADDR' comments in src/ with the following function body."""
import os,re,sys
root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
def orig_len(a):
    p=os.path.join(root,'reverse/native',a+'.c')
    if not os.path.exists(p): return None
    n=0
    for l in open(p):
        s=l.strip()
        if not s or s.startswith('/*') or s.startswith('*') or re.match(r'^(int|uint|char|float|double|void|undefined\w*|bool|byte|short|long|size_t|code|[A-Za-z_:<>]+) \**\w+( \[\d+\])?;$',s): continue
        n+=1
    return n
out=[]
for dp,_,fs in os.walk(os.path.join(root,'src')):
    for f in fs:
        if not f.endswith(('.cpp','.h')): continue
        path=os.path.join(dp,f); lines=open(path).read().split('\n')
        for i,l in enumerate(lines):
            m=re.match(r'\s*// (0x[0-9a-f]{8})',l)
            if not m: continue
            addr=m.group(1)[2:]
            # find function start: first following line ending with '{'
            j=i+1
            while j<len(lines) and not lines[j].rstrip().endswith('{'):
                if j-i>4: break
                j+=1
            if j>=len(lines) or not lines[j].rstrip().endswith('{'): continue
            # Allman: the brace is alone on its line and the signature precedes it.
            sj=j-1 if lines[j].strip()=='{' else j
            depth=0;body=0;k=j
            while k<len(lines):
                depth+=lines[k].count('{')-lines[k].count('}')
                body+=1
                if depth<=0: break
                k+=1
            if sj!=j: body-=1
            ol=orig_len(addr)
            if ol is None: continue
            if body<=4 and ol>=12:
                out.append((ol,body,addr,os.path.relpath(path,root),sj+1,lines[sj].strip()[:90]))
out.sort(reverse=True)
for o in out: print('%3d/%d %s %s:%d %s'%o)
