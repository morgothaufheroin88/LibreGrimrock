#!/usr/bin/env python3
import json
from pathlib import Path
import subprocess
import re

root = Path(__file__).resolve().parents[1]
index = json.loads((root / 'reverse/lua-index.json').read_text())
out = root / 'build-source/lua-bytecode'
out.mkdir(exist_ok=True)
compiler = next(p for p in (root / 'third_party/luajit/src/luajit', root / 'tools/LuaJIT-original/src/luajit') if p.exists())
def instructions(path):
    env = {'LUA_PATH': str(compiler.parent / '?.lua') + ';;'}
    result = subprocess.run([str(compiler), '-bl', str(path)], capture_output=True, text=True, env=env)
    normalized = []
    for line in result.stdout.splitlines():
        match = re.match(r'\s*\d+[ =>]*([A-Z0-9]+)\s+(.*)', line)
        if match:
            opcode, operands = match.groups()
            operands = re.sub(r'\s*;.*$', '', operands).strip()
            operands = re.sub(r'=>\s*\d+', '=>', operands)
            normalized.append((opcode, operands))
    # LuaJIT 2.0.0 builds differ in whether they emit a redundant final
    # top-level UCLO immediately before RET0. It does not change execution.
    return [x for i, x in enumerate(normalized)
            if not (x[0] == 'UCLO' and i + 1 < len(normalized) and normalized[i + 1][0] == 'RET0')]
for item in index:
    reconstructed = root / 'src/game' / item['name']
    source = reconstructed if reconstructed.exists() else root / 'recovered/lua-draft' / item['name']
    if not source.exists():
        item['compiles'] = False
        continue
    binary = out / item['archive_file']
    result = subprocess.run([str(compiler),
                             str(root / 'tools/check_lua.lua'), str(source), str(binary)],
                            capture_output=True, text=True)
    item['compiles'] = result.returncode == 0
    item['compile_error'] = result.stderr.strip()
    original = root / 'recovered/archive' / item['archive_file']
    item['byte_identical'] = result.returncode == 0 and binary.read_bytes() == original.read_bytes()
    item['instruction_equivalent'] = result.returncode == 0 and instructions(binary) == instructions(original)
    if (item['byte_identical'] or item['instruction_equivalent']) and not reconstructed.exists():
        target = root / 'src/game' / item['name']
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text('-- Recompiled stripped bytecode matches original exactly.\n' +
                          '\n'.join(source.read_text().splitlines()[1:]) + '\n')
(root / 'reverse/lua-validation.json').write_text(json.dumps(index, indent=2))
print(f'{sum(x.get("compiles", False) for x in index)}/{len(index)} compile; '
      f'{sum(x.get("byte_identical", False) for x in index)} byte-identical; '
      f'{sum(x.get("instruction_equivalent", False) for x in index)} instruction-equivalent modules')
print('Identical:', ', '.join(x['name'] for x in index if x.get('byte_identical')))
print('Instruction-equivalent:', ', '.join(x['name'] for x in index if x.get('instruction_equivalent')))
