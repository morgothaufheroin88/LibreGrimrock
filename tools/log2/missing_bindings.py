#!/usr/bin/env python3
"""Lists the grimrock2.exe Lua bindings (reverse/log2/bindings.json) that the game-2 build
does not register: the {"name", Class_name} entries of the src/rapid + src2/rapid method
tables are matched by class table and method name.  Module functions (sys, Steam) are
matched by {"name", sys_name}."""
import json, re, glob
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
bindings = json.load(open(ROOT / 'reverse/log2/bindings.json'))
src = ''
for f in glob.glob(str(ROOT / 'src/rapid/*.cpp')) + glob.glob(str(ROOT / 'src2/rapid/*.cpp')):
    src += open(f).read() + '\n'
registered = set(re.findall(r'\{"([A-Za-z0-9_]+)",\s*([A-Za-z0-9_]+)_[A-Za-z0-9_]+\}', src))
# module level functions are registered as {"name", name}
registered |= set((n, None) for n in re.findall(r'\{"([A-Za-z0-9_]+)",\s*\1\}', src))
# (method name, prefix) pairs; prefix is the C function's class part
by_prefix = {}
for name, prefix in registered:
    by_prefix.setdefault(prefix, set()).add(name)
aliases = {'(globals)': None, 'CameraControls': 'CameraControls'}
missing = 0
for module, classes in bindings.items():
    for cls in classes:
        cname = cls['name']
        prefix = cname if cname != '(globals)' else None
        have = by_prefix.get(prefix, set()) if prefix else set()
        # allow any prefix for module-level globals
        if not prefix:
            have = set(n for n, p in registered)
        lost = [m for m, addr in cls['methods'] if m not in have and m != '__gc']
        if lost:
            missing += len(lost)
            print('%-28s %s' % (cname, ' '.join('%s@%s' % (m, a) for m, a in cls['methods'] if m in lost)))
print('missing: %d' % missing)
