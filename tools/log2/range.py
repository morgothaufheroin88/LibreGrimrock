#!/usr/bin/env python3
"""List the grimrock2.exe functions in an address range with their size in pseudocode
lines, matched name and the strings/GL calls they use: range.py START END"""
import json, re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[2]
names = json.load(open(ROOT / 'reverse/log2/names.json'))
start, end = int(sys.argv[1], 16), int(sys.argv[2], 16)
for p in sorted((ROOT / 'reverse/log2/native').glob('*.c')):
    a = int(p.stem, 16)
    if not start <= a < end:
        continue
    code = p.read_text()
    toks = sorted(set(re.findall(r'"[^"\n]{3,40}"', code)) | set(x + '()' for x in re.findall(r'\b(gl[A-Z]\w+|al[A-Z]\w+|FT_\w+|ov_\w+)\(', code)))
    vt = re.findall(r'engine::(\w+)::vftable|core::(\w+)::vftable', code)
    vt = [x or y for x, y in vt]
    n = names.get(p.stem, {})
    print('%s %4d %-45s %s %s' % (p.stem, code.count('\n'), n.get('name', '')[:45], ('vt:' + ','.join(sorted(set(vt)))) if vt else '', ' '.join(toks)[:150]))
