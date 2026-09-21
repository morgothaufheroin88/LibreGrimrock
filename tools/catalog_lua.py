#!/usr/bin/env python3
"""Recover archive names by testing bytecode string constants against the stored FNV hashes."""
import csv
import contextlib
import hashlib
import json
from pathlib import Path
import re
import shutil
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools/ljd'))
import main as ljd_main
import ljd.rawdump.parser
import ljd.bytecode.prototype
import ljd.bytecode.constants
import ljd.pseudoasm.writer

def hash_name(name):
    value = 0x811c9dc5
    for byte in name.encode(): value = ((value ^ byte) * 0x1000193) & 0xffffffff
    return value

def strings(value):
    if isinstance(value, str):
        yield value
    elif isinstance(value, ljd.bytecode.prototype.Prototype):
        for child in value.constants.complex_constants: yield from strings(child)
    elif isinstance(value, ljd.bytecode.constants.Table):
        for child in value.array: yield from strings(child)
        for key, child in value.dictionary:
            yield from strings(key)
            yield from strings(child)

def main():
    candidates = {'init.lua', 'Grimrock.lua'}
    all_strings = set()
    disasm = ROOT / 'reverse/lua-bytecode'
    disasm.mkdir(exist_ok=True)
    files = sorted((ROOT / 'recovered/archive').glob('*.luajit'))
    for file in files:
        header, prototype = ljd.rawdump.parser.parse(str(file), lambda h: ljd_main.set_luajit_version(2.0))
        if prototype is None: raise RuntimeError(f'Could not parse {file}')
        all_strings.update(strings(prototype))
        with (disasm / (file.stem + '.txt')).open('w') as output, (disasm / (file.stem + '.raw.txt')).open('w') as raw, contextlib.redirect_stdout(raw):
            ljd.pseudoasm.writer.write(output, header, prototype)
    for value in all_strings:
        if len(value) < 240 and re.fullmatch(r'[A-Za-z0-9_./ -]+', value):
            candidates.update([value, value + '.lua', value.replace('.', '/') + '.lua'])
    candidates.update('lib/' + name for name in list(candidates) if name.endswith('.lua'))
    for directory in all_strings:
        if directory.startswith('assets/dungeons/') and len(directory) < 100:
            candidates.add(directory + '/init.lua')
            candidates.add(directory + '/dungeon.lua')
            candidates.update(directory + f'/level{n:02}.lua' for n in range(100))
    matches = {}
    for name in sorted(candidates): matches.setdefault(hash_name(name), []).append(name)
    out = ROOT / 'recovered/lua-draft'
    out.mkdir(exist_ok=True)
    report = []
    for file in files:
        hash_value = int(file.stem.split('_')[1], 16)
        names = [x for x in matches.get(hash_value, []) if x.endswith('.lua')]
        name = names[0] if len(names) == 1 else 'unresolved/' + file.stem + '.lua'
        # Only use safe, relative paths from the verified hash match.
        if name.startswith('/') or '..' in Path(name).parts: raise ValueError(name)
        raw = ROOT / 'recovered/lua-raw' / (file.stem + '.luaji')
        failed = file.name in (ROOT / 'reverse/ljd.log').read_text()
        status = 'decompiler-failed' if failed or not raw.exists() else 'unverified-draft'
        if raw.exists():
            target = out / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text('-- UNVERIFIED DECOMPILATION: stripped locals/upvalues need reconstruction.\n' + raw.read_text())
        report.append({'archive_file': file.name, 'hash': f'{hash_value:08x}', 'name': name,
                       'name_candidates': names, 'status': status,
                       'bytecode_sha256': hashlib.sha256(file.read_bytes()).hexdigest()})
    (ROOT / 'reverse/lua-index.json').write_text(json.dumps(report, indent=2))
    (ROOT / 'reverse/lua-strings.json').write_text(json.dumps(sorted(all_strings), indent=2))
    print(f'{len(report)} bytecode files; {sum(len(x["name_candidates"]) == 1 for x in report)} names resolved; '
          f'{sum(x["status"] == "decompiler-failed" for x in report)} decompiler failures')

if __name__ == '__main__': main()
