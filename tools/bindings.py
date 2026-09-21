#!/usr/bin/env python3
"""Dump cleaned pseudocode for the Lua bindings of classes listed in a luareg.py
output file: bindings.py REGFILE CLASS [CLASS...] (CLASS 'all' = every class)."""
import re, subprocess, sys

regfile = sys.argv[1]
wanted = set(sys.argv[2:])
sym = {}
for l in open('reverse/symtab.txt'):
    p = l.split()
    if len(p) >= 8 and p[3] == 'FUNC':
        sym.setdefault(p[7], int(p[1], 16))

NOISE = re.compile(r'^\s*(undefined4|int|float|uint|undefined1|byte|char|bool|size_t|short|undefined \*|longdouble|undefined3|undefined8|double|code \*|luax|Node \*|String|core|void \*|undefined2|ushort|long|ulonglong)\s[^=]*;\s*$')
cur = None
for line in open(regfile):
    m = re.match(r'^(class|module|functions|enums)\s*(\S*)', line)
    if m:
        cur = m.group(2) or m.group(1)
        continue
    m = re.match(r'^    (\w+) = (\S+)$', line)
    if not m or (cur not in wanted and 'all' not in wanted):
        continue
    name, fn = m.groups()
    addr = sym.get(fn)
    if addr is None:
        for cand in ('_ZL%d%sP9lua_State' % (len(fn), fn), '_Z%d%sP9lua_State' % (len(fn), fn)):
            if cand in sym:
                addr = sym[cand]
                break
    if addr is None:
        print('//// %s.%s = %s (no symbol)' % (cur, name, fn))
        continue
    try:
        src = open('reverse/native/%08x.c' % addr).read()
    except IOError:
        print('//// %s.%s = %s @%08x (no snapshot)' % (cur, name, fn, addr))
        continue
    out = []
    for s in src.splitlines():
        if s.startswith('/* Ghidra') or NOISE.match(s) or s.strip() == '':
            continue
        if '__cxa_' in s or 'CONCAT31(extraout' in s and False:
            continue
        out.append(s)
    print('//// %s.%s @%08x' % (cur, name, addr))
    print('\n'.join(out))
