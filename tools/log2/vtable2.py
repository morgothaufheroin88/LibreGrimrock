#!/usr/bin/env python3
"""List the vtables of grimrock2.exe with their class names from the MSVC RTTI
(RTTICompleteObjectLocator stored before each vftable) and the function of every slot.

usage: vtable2.py [--json OUT] [CLASS...]"""
import json, os, re, struct, sys

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
    sections.append((data[s:s + 8].rstrip(b'\0').decode(), image_base + va, vsize, roff, rsize))
text = next(s for s in sections if s[0] == '.text')
rdata = next(s for s in sections if s[0] == '.rdata')
sdata = next(s for s in sections if s[0] == '.data')


def rd(va, n):
    for name, base, vsize, roff, rsize in sections:
        if base <= va < base + vsize:
            return data[roff + va - base:roff + va - base + n]
    return b''


def u32(va):
    b = rd(va, 4)
    return struct.unpack('<I', b)[0] if len(b) == 4 else None


def in_text(va):
    return text[1] <= va < text[1] + text[2]


def in_data(va):
    return any(s[1] <= va < s[1] + s[2] for s in (rdata, sdata))


def demangle(name):
    # ".?AVFoo@ns@@" -> "ns::Foo"; templates are left mangled
    m = re.match(r'^\.\?A[VU](.*)@@$', name)
    if not m:
        return name
    return '::'.join(reversed(m.group(1).split('@')))


vtables = []
for sec in (rdata, sdata):
    name, base, vsize, roff, rsize = sec
    for va in range(base, base + min(vsize, rsize) - 4, 4):
        locator = u32(va)
        if locator is None or not in_data(locator):
            continue
        # RTTICompleteObjectLocator: signature 0, offset, cdOffset, pTypeDescriptor, pClassHierarchy
        sig, offset, cd, td, hier = struct.unpack('<IIIII', rd(locator, 20).ljust(20, b'\0'))
        if sig != 0 or not in_data(td) or not in_data(hier):
            continue
        tname = rd(td + 8, 256).split(b'\0')[0]
        if not tname.startswith(b'.?A'):
            continue
        first = u32(va + 4)
        if first is None or not in_text(first):
            continue
        slots = []
        p = va + 4
        while True:
            f = u32(p)
            if f is None or not in_text(f):
                break
            slots.append('%08x' % f)
            p += 4
            # the next vtable's locator ends this one
            nxt = u32(p)
            if nxt is not None and in_data(nxt) and u32(nxt) == 0 and u32(nxt + 12) is not None and in_data(u32(nxt + 12)):
                break
        vtables.append({'address': '%08x' % (va + 4), 'class': demangle(tname.decode('latin1')),
                        'offset': offset, 'slots': slots})

wanted = [a for a in sys.argv[1:] if not a.startswith('--') and a != sys.argv[sys.argv.index('--json') + 1] if '--json' in sys.argv] if '--json' in sys.argv else [a for a in sys.argv[1:]]
if '--json' in sys.argv:
    json.dump(vtables, open(sys.argv[sys.argv.index('--json') + 1], 'w'), indent=1)
for v in vtables:
    if wanted and not any(w in v['class'] for w in wanted):
        continue
    print('%s %s (offset %d, %d slots)' % (v['address'], v['class'], v['offset'], len(v['slots'])))
    if wanted:
        for i, s in enumerate(v['slots']):
            print('  [%2d] %s' % (i, s))
print('%d vtables' % len(vtables))
