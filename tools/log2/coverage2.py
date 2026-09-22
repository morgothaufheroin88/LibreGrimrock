#!/usr/bin/env python3
"""Coverage of the grimrock2.exe engine and core code by the reconstruction: every
function of the binary in the given range that no // 0x<address> comment in the sources
claims. usage: coverage2.py [START END]

Without a range the engine/core region 0x004a0000-0x004fffff is reported, which is where
the rapid/engine/core compilation units of the second game live (the Lua bindings below
it are covered by missing_bindings.py)."""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
start = int(sys.argv[1], 16) if len(sys.argv) > 2 else 0x004a0000
end = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x00500000

claimed = set()
for directory in ('src', 'src2', 'tests'):
    for path in (ROOT / directory).rglob('*'):
        if path.suffix not in ('.cpp', '.h'):
            continue
        for m in re.finditer(r'0x00([0-9a-f]{6})', path.read_text()):
            claimed.add(m.group(1))

names = json.load(open(ROOT / 'reverse/log2/names.json'))
functions = json.load(open(ROOT / 'reverse/log2/functions.json'))['functions']
missing = []
total = 0
for f in functions:
    address = int(f['address'], 16)
    if not start <= address < end:
        continue
    total += 1
    if f['address'][2:] not in claimed:
        missing.append(f['address'])

print('%d of %d functions in %06x-%06x claimed by the sources, %d missing'
      % (total - len(missing), total, start, end, len(missing)))
for address in missing:
    path = ROOT / 'reverse/log2/native' / (address + '.c')
    size = path.read_text().count('\n') if path.exists() else 0
    name = names.get(address, {}).get('name', '')
    print('  %s %4d %s' % (address, size, name))
