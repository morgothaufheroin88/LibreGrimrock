#!/usr/bin/env python3
"""Print a luax::Enum table ({const char*, int} pairs, null terminated) of grimrock2.exe:
enum2.py ADDR..."""
import struct, sys
sys.argv, args = sys.argv[:1], sys.argv[1:]
exec(open(__file__.replace('enum2.py', 'luareg2.py')).read().split('def parse_module')[0])
for a in args:
    va = int(a, 16)
    print('enum %s:' % a)
    while True:
        sp, val = struct.unpack('<Ii', rd(va, 8))
        if sp == 0:
            break
        print('  %-32s %d' % (cstr(sp), val))
        va += 8
