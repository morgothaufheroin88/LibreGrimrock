#!/usr/bin/env python3
"""Find every luax::Enum table used by the Lua bindings of grimrock2.exe (checkEnum takes
the table in EAX) and write reverse/log2/enums.json: {table address: {users, entries}}."""
import json, re, struct, sys, urllib.parse, urllib.request
sys.argv = sys.argv[:1]
exec(open(__file__.replace('enums_scan.py', 'luareg2.py')).read().split('def parse_module')[0])
CHECK_ENUM, PUSH_ENUM = 0x0042f1f0, 0x0042e7e0
bindings = json.load(open('reverse/log2/bindings.json'))
tables = {}
for mod, classes in bindings.items():
    for c in classes:
        for method, addr in c['methods']:
            lines = bridge('disassemble_function', address=addr).get('instructions') or []
            eax = None
            for l in lines:
                ins = l['instruction']
                m = re.match(r'MOV EAX,(0x[0-9a-f]+)$', ins)
                if m:
                    eax = int(m.group(1), 16)
                m = re.match(r'CALL (0x[0-9a-f]+)$', ins)
                if m and int(m.group(1), 16) in (CHECK_ENUM, PUSH_ENUM) and eax:
                    entries = []
                    va = eax
                    while True:
                        raw = rd(va, 8)
                        if len(raw) < 8:
                            break
                        sp, val = struct.unpack('<Ii', raw)
                        if sp == 0 or cstr(sp) is None:
                            break
                        entries.append((cstr(sp), val))
                        va += 8
                    t = tables.setdefault('%08x' % eax, {'users': [], 'entries': entries})
                    t['users'].append('%s_%s' % (c['name'], method))
json.dump(tables, open('reverse/log2/enums.json', 'w'), indent=1)
for a, t in sorted(tables.items()):
    print(a, ', '.join(sorted(set(t['users'])))[:100])
    for n, v in t['entries']:
        print('    %-32s %d' % (n, v))
