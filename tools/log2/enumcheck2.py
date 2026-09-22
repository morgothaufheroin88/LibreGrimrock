#!/usr/bin/env python3
"""Compare the luax::Enum tables of the reconstruction with the ones in grimrock2.exe
(reverse/log2/enums.json, recovered by enums_scan.py): the names and their order have to
match, because the scripts pass the strings and the engine stores the value.

usage: enumcheck2.py [--all]

Without --all only the tables that differ are printed. A table of the binary that the
game-2 build does not have at all is reported as missing, with the bindings that use it."""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
everything = '--all' in sys.argv
binary = json.load(open(ROOT / 'reverse/log2/enums.json'))

def game2(text):
    """the source as the GRIMROCK_GAME=2 build sees it (the guards are the only
    conditional the enum tables use)"""
    out, stack = [], []
    for line in text.split('\n'):
        stripped = line.strip()
        if re.match(r'#if\s+GRIMROCK_GAME\s*(>=\s*2|==\s*2)', stripped):
            stack.append(True)
            continue
        if stripped.startswith('#if') and stack:
            stack.append(stack[-1])          # nested, keep the state
            continue
        if stripped.startswith('#else') and stack:
            stack[-1] = not stack[-1]
            continue
        if stripped.startswith('#endif') and stack:
            stack.pop()
            continue
        if all(stack):
            out.append(line)
    return '\n'.join(out)


# luax::Enum g_name[] = {{"a", X}, {"b", Y}, ..., {0, 0}};
TABLE = re.compile(r'luax::Enum\s+(\w+)\s*\[\]\s*=\s*\{(.*?)\};', re.S)  # also inside a function
ENTRY = re.compile(r'\{\s*"([^"]+)"\s*,')

ours = {}
for directory in ('src/rapid', 'src2/rapid'):
    for path in sorted((ROOT / directory).glob('*.cpp')):
        text = game2(path.read_text())
        # the game-2 build takes the src2 file of the same name
        if directory == 'src/rapid' and (ROOT / 'src2/rapid' / path.name).exists():
            continue
        for name, body in TABLE.findall(text):
            ours[name] = (ENTRY.findall(body), str(path.relative_to(ROOT)))

differences = 0
for address, table in sorted(binary.items()):
    names = [e[0] for e in table['entries']]
    if not names:
        continue
    # an exact table wins: several bindings build tables that share most of their names
    match = None
    for our_name, (our_names, path) in ours.items():
        if our_names == names:
            match = (len(names), our_name, our_names, path)
            break
        common = len(set(names) & set(our_names))
        if common and (match is None or common > match[0]):
            match = (common, our_name, our_names, path)
    if match is None:
        differences += 1
        print('%s missing: %s' % (address, ', '.join(names)))
        print('    used by %s' % ', '.join(table['users']))
        continue
    _, our_name, our_names, path = match
    if our_names == names:
        if everything:
            print('%s %s ok (%d)' % (address, our_name, len(names)))
        continue
    differences += 1
    print('%s %s in %s' % (address, our_name, path))
    print('    binary: %s' % ', '.join(names))
    print('    ours:   %s' % ', '.join(our_names))
print('%d of %d enum tables differ' % (differences, len(binary)))
