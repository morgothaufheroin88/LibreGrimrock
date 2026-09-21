#!/usr/bin/env python3
"""Show the Ghidra pseudocode of grimrock2.exe functions, cleaned of declaration noise,
optionally next to the LoG1 function they were matched with (reverse/log2/names.json).

usage: sbs2.py [-1] ADDR|NAME...   (-1: also print the matched LoG1 pseudocode)
NAME is a names.json name or a binding name such as Renderer_getSSAOFilter."""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
names = json.load(open(ROOT / 'reverse/log2/names.json'))
by_name = {}
for addr, info in names.items():
    by_name.setdefault(info['name'], addr)
    by_name.setdefault(info['name'].split('(')[0], addr)
NOISE = re.compile(r'^\s*(undefined\d?|int|uint|float|double|char|byte|bool|short|ushort|long|ulonglong|longlong|code|void|size_t|float10|LPCSTR|HMODULE|HICON|DWORD|BOOL|HANDLE|HWND|CHAR|undefined1|undefined2|undefined4|undefined8)\s*\**\s*[a-zA-Z_][a-zA-Z0-9_]*(\s*\[[^\]]*\])*;\s*$')
MSVC = re.compile(r'^\s*(local_8 = 0x?[0-9a-f]+;|puStack_c = &LAB_[0-9a-f]+;|local_10 = ExceptionList;|ExceptionList = &local_10;|ExceptionList = local_10;|local_\w+ = DAT_00615380 \^ .*;|__security_check_cookie\(.*\);|local_8\._0_1_ = \d+;|local_8 = CONCAT31\(.*\);|uVar\d+ = \(undefined1\)local_8;)\s*$')


def clean(text):
    out = []
    for line in text.splitlines():
        if NOISE.match(line) or MSVC.match(line) or line.strip() == '':
            continue
        out.append(line)
    return '\n'.join(out)


show1 = '-1' in sys.argv
for arg in sys.argv[1:]:
    if arg == '-1':
        continue
    addr = arg if re.match(r'^[0-9a-f]{8}$', arg) else by_name.get(arg)
    if not addr:
        print('unknown function %s' % arg)
        continue
    path = ROOT / 'reverse/log2/native' / (addr + '.c')
    info = names.get(addr, {})
    print('==== grimrock2.exe %s %s [%s]' % (addr, info.get('name', ''), info.get('how', '')))
    print(clean(path.read_text()) if path.exists() else '(not exported)')
    if show1 and info.get('log1'):
        p1 = ROOT / 'reverse/native' / (info['log1'] + '.c')
        print('---- LoG1 %s' % info['log1'])
        print(clean(p1.read_text()) if p1.exists() else '(no pseudocode)')
