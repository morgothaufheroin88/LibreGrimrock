#!/usr/bin/env python3
"""Resumable export of the Ghidra pseudocode of every function of grimrock2.exe to
reverse/log2/native/<address>.c (Ghidra HTTP bridge on 127.0.0.1:8089). Pseudocode is
evidence, not build input."""
import json, time, urllib.parse, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'reverse/log2/native'
OUT.mkdir(parents=True, exist_ok=True)
BRIDGE = 'http://127.0.0.1:8089'


def bridge(endpoint, timeout=300, **params):
    query = urllib.parse.urlencode(dict(program='grimrock2.exe', **params))
    with urllib.request.urlopen('%s/%s?%s' % (BRIDGE, endpoint, query), timeout=timeout) as r:
        return json.load(r)


functions = bridge('list_functions')['functions']
(ROOT / 'reverse/log2/functions.json').write_text(json.dumps({'functions': functions}, indent=1))
pending = [f for f in functions
           if not f['name'].startswith(('Catch@', 'Catch_All@', 'Unwind@', 'thunk_'))
           and not (OUT / (f['address'] + '.c')).exists()]
print('%d functions, %d to export' % (len(functions), len(pending)), flush=True)
errors = []
batch_size = 16
for start in range(0, len(pending), batch_size):
    batch = pending[start:start + batch_size]
    try:
        result = bridge('decompile_function', functions=','.join(f['address'] for f in batch), timeout=60)
        for f in batch:
            code = result.get(f['address'])
            if isinstance(code, str) and code.strip() and not code.lstrip().startswith('Error:'):
                (OUT / (f['address'] + '.c')).write_text('/* Ghidra pseudocode of grimrock2.exe %s, not build input. */\n' % f['name'] + code)
            else:
                errors.append({'address': f['address'], 'result': code})
    except Exception as error:
        errors.append({'batch': [f['address'] for f in batch], 'error': str(error)})
    if (start // batch_size) % 20 == 0:
        print('%d/%d exported; %d failures' % (min(start + batch_size, len(pending)), len(pending), len(errors)), flush=True)
        (ROOT / 'reverse/log2/export-errors.json').write_text(json.dumps(errors, indent=1))
(ROOT / 'reverse/log2/export-errors.json').write_text(json.dumps(errors, indent=1))
print('done, %d failures' % len(errors))
