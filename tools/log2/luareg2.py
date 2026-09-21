#!/usr/bin/env python3
"""Recover the luax class registrations of grimrock2.exe from the disassembly of the
*_mod functions (Ghidra HTTP bridge on 127.0.0.1:8089, program grimrock2.exe).

The method tables are built on the stack ({name, lua_CFunction} pairs, null terminated),
the property lists likewise (names, then a flag table pointer); registerSubclass is
__thiscall with ECX = class name and the stack arguments (base, methods, properties).

usage: luareg2.py [--json OUT] MOD_ADDR...   (default: every caller of registerSubclass)"""
import json, os, re, struct, sys, urllib.parse, urllib.request

BRIDGE = 'http://127.0.0.1:8089'
PROGRAM = 'grimrock2.exe'
REGISTER_SUBCLASS = 0x0042eee0  # luax::registerSubclass(name in ECX; base, methods, properties)
REGISTER_FUNCTIONS = 0x0042eea0  # luax::registerFunctions(L, table): globals
LUAL_REGISTER = 0x00455610      # luaL_register(L, libname, table): modules such as sys and Steam
EXE = os.environ.get('GRIMROCK2_EXE', os.path.expanduser(
    '~/.local/share/Steam/steamapps/common/Legend of Grimrock 2/grimrock2.exe'))
data = open(EXE, 'rb').read()
pe = struct.unpack_from('<I', data, 0x3c)[0]
num_sections = struct.unpack_from('<H', data, pe + 6)[0]
opt_size = struct.unpack_from('<H', data, pe + 20)[0]
image_base = struct.unpack_from('<I', data, pe + 24 + 28)[0]
sections = []
for i in range(num_sections):
    s = pe + 24 + opt_size + i * 40
    vsize, va, rsize, roff = struct.unpack_from('<IIII', data, s + 8)
    sections.append((data[s:s + 8].rstrip(b'\0').decode(), image_base + va, vsize, roff))
text = next(s for s in sections if s[0] == '.text')


def rd(va, n):
    for name, base, vsize, roff in sections:
        if base <= va < base + vsize:
            return data[roff + va - base:roff + va - base + n]
    return b''


def cstr(va):
    b = rd(va, 256).split(b'\0')[0]
    return b.decode('latin1')


def in_text(va):
    return text[1] <= va < text[1] + text[2]


def bridge(endpoint, **params):
    query = urllib.parse.urlencode(dict(program=PROGRAM, **params))
    with urllib.request.urlopen('%s/%s?%s' % (BRIDGE, endpoint, query), timeout=300) as r:
        return json.load(r)


def parse_module(addr):
    """Follow the stack stores of one *_mod function and collect its registrations."""
    lines = bridge('disassemble_function', address='%08x' % addr)
    if isinstance(lines, dict):
        lines = lines.get('instructions') or lines.get('disassembly')
    stack = {}      # frame offset -> ('imm', value) | ('reg', name)
    regs = {}       # register -> value or ('lea', frame offset)
    pushes = []
    classes = []

    def frame_off(text):
        m = re.search(r'\[EBP \+ (-?0x[0-9a-f]+|0x[0-9a-f]+)\]', text)
        if not m:
            return None
        v = int(m.group(1), 16)
        return v - 0x100000000 if v >= 0x80000000 else v

    def table_at(off):
        """{name, func} pairs stored at consecutive frame offsets."""
        out = []
        while True:
            a, b = stack.get(off), stack.get(off + 4)
            if a is None or a == 0:
                break
            if not isinstance(a, int) or not isinstance(b, int) or not in_text(b):
                break
            out.append((cstr(a), '%08x' % b))
            off += 8
        return out

    def props_at(off):
        out = []
        while True:
            a = stack.get(off)
            if not isinstance(a, int) or a == 0 or in_text(a):
                break
            s = cstr(a)
            if not s or not re.match(r'^[A-Za-z_][A-Za-z0-9_]*$', s):
                break
            out.append(s)
            off += 4
        return out

    for line in lines:
        ins = line['instruction'] if isinstance(line, dict) else line
        m = re.match(r'MOV dword ptr \[EBP \+ (-?0x[0-9a-f]+|0x[0-9a-f]+)\],(.*)$', ins)
        if m:
            off = frame_off(ins)
            src = m.group(2)
            if src.startswith('0x'):
                stack[off] = int(src, 16)
            elif src in regs and isinstance(regs[src], int):
                stack[off] = regs[src]
            else:
                stack[off] = None
            continue
        m = re.match(r'(MOV|LEA) (E[A-Z]{2}),(.*)$', ins)
        if m:
            op, reg, src = m.groups()
            if op == 'LEA' and frame_off(src) is not None:
                regs[reg] = ('lea', frame_off(src))
            elif src.startswith('0x'):
                regs[reg] = int(src, 16)
            else:
                regs[reg] = None
            continue
        m = re.match(r'XOR (E[A-Z]{2}),(E[A-Z]{2})$', ins)
        if m and m.group(1) == m.group(2):
            regs[m.group(1)] = 0
            continue
        m = re.match(r'PUSH (.*)$', ins)
        if m:
            src = m.group(1)
            if src.startswith('0x'):
                pushes.append(int(src, 16))
            elif src in regs:
                pushes.append(regs[src])
            else:
                pushes.append(None)
            continue
        m = re.match(r'CALL 0x([0-9a-f]+)$', ins)
        if m:
            target = int(m.group(1), 16)
            if target == LUAL_REGISTER and len(pushes) >= 3:
                table, libname = pushes[-3], pushes[-2]
                classes.append({
                    'name': cstr(libname) if isinstance(libname, int) else None,
                    'base': 'module',
                    'methods': table_at(table[1]) if isinstance(table, tuple) else [],
                    'properties': [],
                })
            elif target == REGISTER_FUNCTIONS and len(pushes) >= 2:
                table = pushes[-2]
                classes.append({
                    'name': '(globals)',
                    'base': 'functions',
                    'methods': table_at(table[1]) if isinstance(table, tuple) else [],
                    'properties': [],
                })
            elif target == REGISTER_SUBCLASS and len(pushes) >= 3:
                props, methods, base = pushes[-3], pushes[-2], pushes[-1]
                name = regs.get('ECX')
                entry = {
                    'name': cstr(name) if isinstance(name, int) else None,
                    'base': cstr(base) if isinstance(base, int) else None,
                    'methods': table_at(methods[1]) if isinstance(methods, tuple) else [],
                    'properties': props_at(props[1]) if isinstance(props, tuple) else [],
                }
                classes.append(entry)
            pushes = []
            regs = {}
            continue
    return classes


mods = [int(a, 16) for a in sys.argv[1:] if not a.startswith('--') and re.match(r'^[0-9a-f]+$', a)]
if not mods:
    mods = set()
    for target in (REGISTER_SUBCLASS, LUAL_REGISTER):
        for c in bridge('get_function_callers', address='%08x' % target)['callers']:
            if c['name'].startswith('FUN_'):
                mods.add(int(c['address'], 16))
    mods = sorted(mods)
result = {}
for mod in mods:
    classes = parse_module(mod)
    result['%08x' % mod] = classes
    print('module %08x: %d classes' % (mod, len(classes)))
    for c in classes:
        print('  %s : %s  (%d methods, %d properties)' % (c['name'], c['base'], len(c['methods']), len(c['properties'])))
        for name, func in c['methods']:
            print('    %-40s %s' % (name, func))
        if c['properties']:
            print('    properties: %s' % ', '.join(c['properties']))
if '--json' in sys.argv:
    json.dump(result, open(sys.argv[sys.argv.index('--json') + 1], 'w'), indent=1)
