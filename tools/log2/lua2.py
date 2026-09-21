#!/usr/bin/env python3
"""Extract the LuaJIT bytecode of grimrock2.dat, recover the script names from the FNV-1a
hashes (candidates come from the string constants) and decompile drafts with ljd into
recovered/log2/lua-draft/. The drafts are evidence, not sources."""
import contextlib, io, json, os, re, struct, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/ljd'))
import main as ljd_main
import ljd.rawdump.parser, ljd.bytecode.prototype, ljd.bytecode.constants

DAT = os.environ.get('GRIMROCK2_DAT', os.path.expanduser(
    '~/.local/share/Steam/steamapps/common/Legend of Grimrock 2/grimrock2.dat'))
OUT = ROOT / 'recovered/log2'
archive_dir = OUT / 'archive'
draft_dir = OUT / 'lua-draft'
archive_dir.mkdir(parents=True, exist_ok=True)
draft_dir.mkdir(parents=True, exist_ok=True)


def hash_name(name):
    value = 0x811c9dc5
    for byte in name.encode():
        value = ((value ^ byte) * 0x1000193) & 0xffffffff
    return value


def strings(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, ljd.bytecode.prototype.Prototype):
        for child in value.constants.complex_constants:
            yield from strings(child)
    elif isinstance(value, ljd.bytecode.constants.Table):
        for child in value.array:
            yield from strings(child)
        for key, child in value.dictionary:
            yield from strings(key)
            yield from strings(child)


with open(DAT, 'rb') as f:
    magic, count = struct.unpack('<II', f.read(8))
    items = [struct.unpack('<IIIII', f.read(20)) for _ in range(count)]
    scripts = []
    for name_hash, offset, stored, size, flags in items:
        f.seek(offset)
        data = f.read(size)
        if data.startswith(b'\x1bLJ'):
            path = archive_dir / ('%08x.luajit' % name_hash)
            path.write_bytes(data)
            scripts.append((name_hash, path))
print('%d scripts' % len(scripts))

all_strings = set()
protos = {}
for name_hash, path in scripts:
    header, proto = ljd.rawdump.parser.parse(str(path), lambda h: ljd_main.set_luajit_version(2.0))
    protos[name_hash] = (header, proto)
    all_strings.update(strings(proto))
candidates = {'init.lua', 'Grimrock.lua'}
for value in all_strings:
    if len(value) < 240 and re.fullmatch(r'[A-Za-z0-9_./ -]+', value):
        candidates.update([value, value + '.lua', value.replace('.', '/') + '.lua'])
candidates.update('lib/' + n for n in list(candidates) if n.endswith('.lua'))
matches = {}
for name in candidates:
    matches.setdefault(hash_name(name), []).append(name)
(OUT / 'lua-strings.json').write_text(json.dumps(sorted(all_strings), indent=1))

report = []
for name_hash, path in scripts:
    names = sorted(n for n in matches.get(name_hash, []) if n.endswith('.lua'))
    name = names[0] if len(names) == 1 else 'unresolved/%08x.lua' % name_hash
    target = draft_dir / name
    target.parent.mkdir(parents=True, exist_ok=True)
    status = 'draft'
    if not target.exists():
        r = subprocess.run([sys.executable, str(ROOT / 'tools/ljd/main.py'), '-f', str(path), '-o', str(target), '-c'],
                           capture_output=True, text=True)
        if r.returncode != 0 or not target.exists():
            status = 'decompiler-failed'
            target.write_text('-- ljd failed:\n-- ' + (r.stderr or r.stdout).replace('\n', '\n-- '))
        else:
            target.write_text('-- UNVERIFIED DECOMPILATION of grimrock2.dat %08x: stripped locals/upvalues need reconstruction.\n' % name_hash + target.read_text())
    report.append({'hash': '%08x' % name_hash, 'name': name, 'candidates': names, 'status': status})
(OUT / 'lua-index.json').write_text(json.dumps(report, indent=1))
print('%d resolved names, %d decompiler failures' % (sum(len(r['candidates']) == 1 for r in report),
                                                     sum(r['status'] != 'draft' for r in report)))
