#!/usr/bin/env python3
"""Collect the object layout of a class of grimrock2.exe from the pseudocode of its
methods: every read or write through the this pointer, with the access width and the
functions that use it. Offsets are printed in object order, so the result is read as the
member declaration order of the reconstruction.

usage: fields.py START END [THISPARAM]     the functions in an address range
       fields.py FILE.cpp [THISPARAM]      the addresses a reconstructed source claims

THISPARAM is the name Ghidra gave the this pointer (param_1 by default; __thiscall
methods whose first argument is the object). The source form only looks at the addresses
of the // 0x... comments, so a file that holds one class gives that class's layout."""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
names = json.load(open(ROOT / 'reverse/log2/names.json'))
if sys.argv[1].endswith(('.cpp', '.h')):
    wanted = set(re.findall(r'// 0x00([0-9a-f]{6})', Path(sys.argv[1]).read_text()))
    this = sys.argv[2] if len(sys.argv) > 2 else 'param_1'
else:
    start, end = int(sys.argv[1], 16), int(sys.argv[2], 16)
    wanted = None
    this = sys.argv[3] if len(sys.argv) > 3 else 'param_1'

fields = {}


def note(offset, width, addr):
    entry = fields.setdefault(offset, {'widths': set(), 'users': set()})
    entry['widths'].add(width)
    entry['users'].add(addr)


for path in sorted((ROOT / 'reverse/log2/native').glob('*.c')):
    if wanted is not None:
        if path.stem[2:] not in wanted:
            continue
    elif not start <= int(path.stem, 16) < end:
        continue
    code = path.read_text()
    # *(type *)(this + 0x..)
    for m in re.finditer(r'\*\(([\w ]+) \*\)\(' + this + r' \+ (0x[0-9a-f]+|\d+)\)', code):
        note(int(m.group(2), 0), m.group(1), path.stem)
    # this[n], an int-sized field n * 4
    for m in re.finditer(re.escape(this) + r'\[(0x[0-9a-f]+|\d+)\]', code):
        note(int(m.group(1), 0) * 4, 'int', path.stem)
    # *this is the field at 0
    if re.search(r'\*' + this + r'\b', code):
        note(0, 'int', path.stem)

for offset in sorted(fields):
    entry = fields[offset]
    users = ' '.join(sorted(entry['users'])[:6])
    print('+0x%03x %-24s %s%s' % (offset, ','.join(sorted(entry['widths'])), users,
                                  ' ...' if len(entry['users']) > 6 else ''))
print('// %d fields, highest +0x%x' % (len(fields), max(fields) if fields else 0), file=sys.stderr)
