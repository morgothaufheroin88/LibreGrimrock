#!/usr/bin/env python3
"""Reconstruct the luax class registrations of a *_mod(lua_State*) function from its
Ghidra pseudocode snapshot plus the static tables in Grimrock.bin.x86.

usage: luareg.py <addr>   (e.g. 08138ee0 for engine_mod)

Prints, per luax::registerClass / registerSubclass / registerModule / registerFunctions /
registerEnums call, the class name, base class, method table (lua name -> C symbol)
and property list."""
import re, struct, subprocess, sys
import os

BIN = os.environ.get("GRIMROCK_BIN", os.path.expanduser("~/.local/share/Steam/steamapps/common/Legend of Grimrock/Grimrock.bin.x86.orig"))
segs = []
for l in subprocess.run(['readelf', '-lW', BIN], capture_output=True, text=True).stdout.splitlines():
    p = l.split()
    if p and p[0] == 'LOAD':
        segs.append((int(p[2], 16), int(p[1], 16), int(p[4], 16)))
data = open(BIN, 'rb').read()


def rd(v, n):
    for va, off, sz in segs:
        if va <= v < va + sz:
            return data[off + v - va:off + v - va + n]
    return b''


def cstr(v):
    return rd(v, 512).split(b'\0')[0].decode('latin1')


# symbol tables
sym_by_addr = {}
sym_by_name = {}
for l in open('reverse/symtab.txt'):
    p = l.split()
    if len(p) < 8 or p[3] not in ('FUNC', 'OBJECT'):
        continue
    addr = int(p[1], 16)
    size = int(p[2])
    name = p[7]
    sym_by_addr.setdefault(addr, name)
    sym_by_name[name] = (addr, size)


def demangle(n):
    out = subprocess.run(['c++filt', n], capture_output=True, text=True).stdout.strip()
    return out


addr = sys.argv[1]
src = open('reverse/native/%s.c' % addr).read()
m = re.search(r'/\* ([A-Za-z_0-9]+)\(lua_State\*\) \*/', src)
func = m.group(1)

# static tables of this function: _ZZL?NN<func>P9lua_StateE6C.NNNN
tables = {}
for name, (a, size) in sym_by_name.items():
    mm = re.match(r'_ZZL?\d+%sP9lua_StateE\d*C\.(\d+)$' % re.escape(func), name)
    if mm:
        tables[int(mm.group(1))] = (a, size)


def value_of_dword(v):
    """Interpret a dword from a static table."""
    if v == 0:
        return None
    if v in sym_by_addr:
        return ('func', sym_by_addr[v])
    s = cstr(v)
    return ('str', s)


# stack model: offset (bytes below the frame) -> value
stack = {}


def local_off(name):
    mm = re.match(r'local_([0-9a-f]+)$', name)
    return int(mm.group(1), 16) if mm else None


def resolve_string(expr):
    expr = expr.strip()
    mm = re.match(r'"(.*)"$', expr)
    if mm:
        return ('str', mm.group(1).encode().decode('unicode_escape'))
    mm = re.match(r'\(uint \*\)0x([0-9a-f]+)$', expr)
    if mm:
        return ('str', cstr(int(mm.group(1), 16)))
    mm = re.match(r'\(uint \*\)&DAT_([0-9a-f]+)$', expr)
    if mm:
        return ('str', cstr(int(mm.group(1), 16)))
    mm = re.match(r'&DAT_([0-9a-f]+)$', expr)
    if mm:
        return ('str', cstr(int(mm.group(1), 16)))
    mm = re.match(r'\(uint \*\)"(.*)"$', expr)
    if mm:
        return ('str', mm.group(1))
    return None


for line in src.splitlines():
    line = line.strip()
    # copy loop: ppuVar2 = &func::C_NNNN; piVar3 = local_X; for (iVar1 = 0xN; ...
    mm = re.match(r'\w+ = &%s\(lua_State\*\)::C_(\d+);' % re.escape(func), line)
    if mm:
        pending_table = int(mm.group(1))
        continue
    mm = re.match(r'\w+ = (local_[0-9a-f]+)(?: \+ (\d+))?;$', line)
    if mm and 'pending_table' in dir() and pending_table is not None:
        base = local_off(mm.group(1)) - (int(mm.group(2)) * 4 if mm.group(2) else 0)
        pending_dest = base
        continue
    mm = re.match(r'for \(\w+ = (0x[0-9a-f]+|\d+); \w+ != 0;', line)
    if mm and 'pending_table' in dir() and pending_table is not None:
        count = int(mm.group(1), 0)
        ta, tsize = tables[pending_table]
        for i in range(count):
            v = struct.unpack('<I', rd(ta + i * 4, 4))[0]
            stack[pending_dest - i * 4] = value_of_dword(v)
        pending_table = None
        continue
    # inline assignment: local_X = "str" | Func | 0 | local_48[N] = ...
    mm = re.match(r'(local_[0-9a-f]+)(?:\[(\d+)\])? = (.*);$', line)
    if mm:
        off = local_off(mm.group(1)) - (int(mm.group(2)) * 4 if mm.group(2) else 0)
        rhs = mm.group(3).strip()
        s = resolve_string(rhs)
        if s:
            stack[off] = s
        elif rhs in ('0', '(int *)0x0', '(code *)0x0', '(undefined *)0x0'):
            stack[off] = None
        elif re.match(r'[A-Za-z_]\w*$', rhs):
            stack[off] = ('func', rhs)
        continue


def table_at(expr):
    """Return the stack offset a pointer expression refers to."""
    expr = expr.strip()
    mm = re.match(r'\(int \*\)&(local_[0-9a-f]+)$', expr) or re.match(r'&(local_[0-9a-f]+)$', expr)
    if mm:
        return local_off(mm.group(1))
    mm = re.match(r'(local_[0-9a-f]+)(?: \+ (\d+))?$', expr)
    if mm:
        return local_off(mm.group(1)) - (int(mm.group(2)) * 4 if mm.group(2) else 0)
    return None


def methods_at(off):
    out = []
    while True:
        name = stack.get(off)
        fn = stack.get(off - 4)
        if name is None:
            break
        out.append((name[1], fn[1] if fn else None))
        off -= 8
    return out


def strings_at(off):
    out = []
    while True:
        v = stack.get(off)
        if v is None:
            break
        out.append(v[1])
        off -= 4
    return out


def enums_at(off):
    out = []
    while True:
        v = stack.get(off)
        if v is None:
            break
        n = stack.get(off - 4)
        out.append((v[1], n))
        off -= 8
    return out


calls = re.findall(r'luax::(registerClass|registerSubclass|registerModule|registerFunctions|registerEnums|setGlobal)\s*\((.*?)\);', src, re.S)
for kind, args in calls:
    args = [a.strip() for a in re.split(r',(?![^()]*\))', args.replace('\n', ' '))]
    if kind == 'registerClass':
        name = resolve_string(args[1])[1]
        print('class %s' % name)
        t = table_at(args[2])
        if t is not None:
            for n, f in methods_at(t):
                print('    %s = %s' % (n, f))
        p = table_at(args[3])
        if p is not None:
            print('    properties: %s' % ', '.join(strings_at(p)))
    elif kind == 'registerSubclass':
        name = resolve_string(args[1])[1]
        base = resolve_string(args[2])[1]
        print('class %s : %s' % (name, base))
        t = table_at(args[3])
        if t is not None:
            for n, f in methods_at(t):
                print('    %s = %s' % (n, f))
        p = table_at(args[4])
        if p is not None:
            print('    properties: %s' % ', '.join(strings_at(p)))
    elif kind == 'registerModule':
        name = resolve_string(args[1])[1]
        print('module %s' % name)
        t = table_at(args[2])
        if t is not None:
            for n, f in methods_at(t):
                print('    %s = %s' % (n, f))
    elif kind == 'registerFunctions':
        print('functions')
        t = table_at(args[1])
        if t is not None:
            for n, f in methods_at(t):
                print('    %s = %s' % (n, f))
    elif kind == 'registerEnums':
        name = resolve_string(args[1])[1]
        print('enums %s' % name)
        t = table_at(args[2])
        if t is not None:
            for n, v in enums_at(t):
                print('    %s = %s' % (n, v))
    elif kind == 'setGlobal':
        print('setGlobal %s' % args[1:])
