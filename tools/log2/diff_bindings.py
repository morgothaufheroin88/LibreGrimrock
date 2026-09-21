#!/usr/bin/env python3
"""Compare the luax registrations of grimrock2.exe (reverse/log2/bindings.json) with the
LoG1 reconstruction in src/rapid: classes and methods added, removed and shared."""
import glob, json, re

log2 = json.load(open('reverse/log2/bindings.json'))
src = ''.join(open(f).read() for f in glob.glob('src/rapid/*.cpp'))
tables = {}
for m in re.finditer(r'(?:static )?const luaL_Reg (\w+)\[\] = \{(.*?)\};', src, re.S):
    tables[m.group(1)] = re.findall(r'\{"(\w+)",', m.group(2))
classes1 = {}
for m in re.finditer(r'register(?:Sub)?[Cc]lass\w*\(\s*L,\s*"(\w+)",(?:\s*"(\w+)",)?\s*(\w+)', src):
    classes1[m.group(1)] = set(tables.get(m.group(3), []))
for m in re.finditer(r'registerModule\(\s*L,\s*"(\w+)",\s*(\w+)', src):
    classes1[m.group(1)] = set(tables.get(m.group(2), []))
print('LoG1: %d classes, %d methods' % (len(classes1), sum(len(v) for v in classes1.values())))
classes2 = {c['name']: set(x[0] for x in c['methods']) for cl in log2.values() for c in cl}
print('LoG2: %d classes, %d methods' % (len(classes2), sum(len(v) for v in classes2.values())))
same = added = removed = 0
for n, m2 in classes2.items():
    if n not in classes1:
        print('NEW CLASS %-24s %d methods: %s' % (n, len(m2), ', '.join(sorted(m2))))
        added += len(m2)
        continue
    m1 = classes1[n]
    same += len(m1 & m2)
    added += len(m2 - m1)
    removed += len(m1 - m2)
    if m2 - m1 or m1 - m2:
        print('%-24s +%s  -%s' % (n, sorted(m2 - m1), sorted(m1 - m2)))
for n in classes1:
    if n not in classes2:
        print('GONE CLASS %s (%d methods)' % (n, len(classes1[n])))
print('shared %d, added %d, removed %d' % (same, added, removed))
