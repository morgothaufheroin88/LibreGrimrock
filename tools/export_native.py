#!/usr/bin/env python3
"""Resumable Ghidra evidence export. Pseudocode is NOT compilable restored source."""
import argparse
import json
from pathlib import Path
import re
import time
import urllib.parse
import urllib.request

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--limit', type=int, default=0)
args = parser.parse_args()
symbols = {}
for line in (ROOT / 'reverse/symbols.txt').read_text().splitlines():
    match = re.match(r'([0-9a-f]+) [TtWw] (.+)', line)
    if match:
        address, name = match.groups()
        symbols.setdefault(address, []).append(name)
index = []
for address, names in sorted(symbols.items()):
    # Own engine plus bindings, before bundled binreloc/FLTK/LuaJIT code.
    owned = 0x080ae190 <= int(address, 16) <= 0x0815e13a
    index.append({'address': address, 'names': sorted(set(names)),
                  'scope': 'engine-or-binding' if owned else 'dependency-or-unclassified'})
(ROOT / 'reverse/native-index.json').write_text(json.dumps(index, indent=2))
out = ROOT / 'reverse/native'
out.mkdir(exist_ok=True)
pending = [x for x in index if x['scope'] == 'engine-or-binding' and not (out / (x['address'] + '.c')).exists()]
if args.limit:
    pending = pending[:args.limit]
print(f'{len(index)} unique native symbols; {len(pending)} functions to export', flush=True)
errors = []
for start in range(0, len(pending), 12):
    batch = pending[start:start+12]
    query = urllib.parse.urlencode({'program': 'Grimrock.bin.x86', 'functions': ','.join(x['address'] for x in batch), 'timeout': 30})
    try:
        with urllib.request.urlopen('http://127.0.0.1:8089/decompile_function?' + query, timeout=240) as response:
            result = json.load(response)
        for item in batch:
            code = result.get(item['address'])
            if isinstance(code, str) and code.strip() and not code.lstrip().startswith('Error:'):
                (out / (item['address'] + '.c')).write_text('/* Ghidra pseudocode, not build input. */\n' + code)
            else:
                errors.append({'address': item['address'], 'result': code})
    except Exception as error:
        errors.append({'batch': [x['address'] for x in batch], 'error': str(error)})
    print(f'{min(start+12, len(pending))}/{len(pending)} exported/attempted; {len(errors)} failures', flush=True)
    (ROOT / 'reverse/export-errors.json').write_text(json.dumps(errors, indent=2))
    time.sleep(.02)
