#!/usr/bin/env python3
"""Guess the object layout of a Lua class of grimrock2.exe from its getter/setter bindings:
prints, per method, the field offsets the binding reads or writes through the object
pointer. usage: layout2.py CLASS..."""
import json, re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
bindings = json.load(open(ROOT / 'reverse/log2/bindings.json'))
for cls in sys.argv[1:]:
    for mod, classes in bindings.items():
        for c in classes:
            if c['name'] != cls:
                continue
            print('== %s : %s' % (c['name'], c['base']))
            for method, addr in c['methods']:
                p = ROOT / 'reverse/log2/native' / (addr + '.c')
                code = p.read_text() if p.exists() else ''
                m = re.search(r'(\w+) = (?:\([\w\s\*]*\))?FUN_0042[89a]\w+\([^;]*\);', code)
                var = m.group(1) if m else None
                offs = []
                if var:
                    for mm in re.finditer(r'\*\((\w+) \*\)\(' + re.escape(var) + r' \+ (0x[0-9a-f]+|\d+)\)( =)?', code):
                        offs.append('%s%s%s' % (mm.group(2), ':' + mm.group(1), '=' if mm.group(3) else ''))
                    for mm in re.finditer(re.escape(var) + r'\[(0x[0-9a-f]+|\d+)\]( =)?', code):
                        offs.append('[%s]%s' % (mm.group(1), '=' if mm.group(2) else ''))
                calls = re.findall(r'FUN_00(4[a-f][0-9a-f]{4})\(', code)
                vcalls = re.findall(r'\+ (0x[0-9a-f]+)\)\)\(', code)
                print('  %-32s %s %s %s' % (method, addr, ' '.join(dict.fromkeys(offs)), ('calls ' + ' '.join(dict.fromkeys(calls))) if calls else '') + (' vt' + ' '.join(vcalls) if vcalls else ''))
