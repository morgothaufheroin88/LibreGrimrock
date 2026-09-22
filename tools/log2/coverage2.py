#!/usr/bin/env python3
"""Coverage of the grimrock2.exe engine and core code by the reconstruction: every
function of the binary in the given range that no // 0x<address> comment in the sources
claims. usage: coverage2.py [START END] [--all]

Without a range the engine/core region 0x004a0000-0x004fffff is reported, which is where
the rapid/engine/core compilation units of the second game live (the Lua bindings below
it are covered by missing_bindings.py).

A function counts as reconstructed when a source claims its address or the address of
the function it was matched to in the first binary (match_log1.py), because the units the
two games share are reconstructed once, from the first binary, and carry its addresses.

The Direct3D 9, D3DX and XAudio2 back ends of the Windows binary have no counterpart in
the reconstruction, the same way the Linux build of the first game does not contain them.
They are left out unless --all is given: a function is taken as part of a back end when
its pseudocode names one (d3d9 shader paths, D3DX, XAudio2) or when the last function
before it that named any back end named that one, which follows the compilation units,
since the linker keeps them together."""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
args = [a for a in sys.argv[1:] if not a.startswith('--')]
everything = '--all' in sys.argv
start = int(args[0], 16) if len(args) > 1 else 0x004a0000
end = int(args[1], 16) if len(args) > 1 else 0x00500000
# the back ends the reconstruction does not have, and the ones it is written against
OUT_OF_SCOPE = re.compile(r'd3d9|D3D9|D3DX|IDirect3D|XAudio|XA2')
IN_SCOPE = re.compile(r'\b(?:gl[A-Z]\w+|al[A-Z]\w+|alc[A-Z]\w+|FT_\w+|ov_\w+|lua_\w+|luaL_\w+)\(|shaders/gl/')

claimed = set()
for directory in ('src', 'src2', 'tests'):
    for path in (ROOT / directory).rglob('*'):
        if path.suffix not in ('.cpp', '.h'):
            continue
        for m in re.finditer(r'0x([0-9a-f]{8})', path.read_text()):
            claimed.add(m.group(1))            # as written: 0815ccc0 of the first binary
            claimed.add(m.group(1)[2:])        # and 004e6990 of the second, without 00

names = json.load(open(ROOT / 'reverse/log2/names.json'))
functions = json.load(open(ROOT / 'reverse/log2/functions.json'))['functions']
missing = []
total = 0
skipped = 0
backend = 'in'
for f in functions:
    address = int(f['address'], 16)
    if not start <= address < end:
        continue
    path = ROOT / 'reverse/log2/native' / (f['address'] + '.c')
    code = path.read_text() if path.exists() else ''
    if OUT_OF_SCOPE.search(code):
        backend = 'out'
    elif IN_SCOPE.search(code):
        backend = 'in'
    if not everything and backend == 'out':
        skipped += 1
        continue
    total += 1
    log1 = names.get(f['address'], {}).get('log1')
    if f['address'][2:] in claimed or (log1 and log1 in claimed):
        continue
    missing.append(f['address'])

print('%d of %d functions in %06x-%06x claimed by the sources, %d missing%s'
      % (total - len(missing), total, start, end, len(missing),
         ', %d Direct3D/XAudio2 out of scope' % skipped if skipped else ''))
for address in missing:
    path = ROOT / 'reverse/log2/native' / (address + '.c')
    size = path.read_text().count('\n') if path.exists() else 0
    name = names.get(address, {}).get('name', '')
    print('  %s %4d %s' % (address, size, name))
