#!/usr/bin/env python3
"""Name the functions of grimrock2.exe after the LoG1 symbols where the two binaries share
code. Evidence: the Lua binding tables (exact), string literals shared between the Ghidra
pseudocode of both binaries (weighted by rarity), and call-site propagation from matched
callers. Writes reverse/log2/names.json: {address: {name, how, score}}.

usage: match_log1.py [--print]"""
import json, math, re, sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
L1 = ROOT / 'reverse/native'
L2 = ROOT / 'reverse/log2/native'
STRING = re.compile(r'"((?:[^"\\\n]|\\.)*)"')
# library calls shared by both binaries (GL entry points, Lua API, OpenAL, FreeType, vorbis,
# zlib, CRT); the LoG2 GL pointers were resolved to names by glprocs.json
API = re.compile(r'\b((?:gl|lua|luaL|al|alc|FT_|ov_|inflate|deflate|unz|zip|str|mem|f|sscanf|sprintf|qsort|malloc|free|realloc|atoi|atof|strtol|strtod|floor|ceil|sqrt|pow|sin|cos|tan|atan|atan2|exp|log|fmod)[A-Za-z0-9_]*)\(')
CALL = re.compile(r'\b(FUN_[0-9a-f]{8}|[A-Za-z_][A-Za-z0-9_:~<>]*)\(')

# ---- LoG1 -------------------------------------------------------------------------
names1 = {}
for item in json.load(open(ROOT / 'reverse/native-index.json')):
    if item['scope'] == 'engine-or-binding':
        names1[item['address']] = item['names'][0]
code1 = {}
for path in L1.glob('*.c'):
    if path.stem in names1:
        code1[path.stem] = path.read_text()
def tokens(code):
    out = set('"%s"' % x for x in STRING.findall(code))
    out.update('%s()' % x for x in API.findall(code) if len(x) > 3)
    return out


strings1 = {a: tokens(c) for a, c in code1.items()}

# ---- LoG2 -------------------------------------------------------------------------
code2 = {p.stem: p.read_text() for p in L2.glob('*.c')}
strings2 = {a: tokens(c) for a, c in code2.items()}
functions2 = {f['address']: f['name'] for f in json.load(open(ROOT / 'reverse/log2/functions.json'))['functions']}

result = {}

# 1. Lua bindings: "<Class>_<method>" like the reconstruction's C function names
bindings = json.load(open(ROOT / 'reverse/log2/bindings.json'))
binding_addr1 = {n.split('(')[0]: a for a, n in names1.items()}
for mod, classes in bindings.items():
    for c in classes:
        for method, addr in c['methods']:
            cls = c['name'] if c['name'] != '(globals)' else 'lua'
            name = '%s_%s' % (cls.replace('.', '_'), method)
            result[addr] = {'name': name, 'how': 'binding', 'score': 1.0, 'log1': binding_addr1.get(name)}

# 2. shared string literals, rarity weighted
noise = {'""', '" "', '"\\n"', '"%s"', '"%d"', '"%f"', '"%s\\n"', '"%d\\n"', '"r"', '"w"', '"rb"', '"wb"', '"."', '"/"', '","', '":"',
         'free()', 'malloc()', 'memcpy()', 'memset()', 'strlen()', 'strcmp()', 'floor()', 'sqrt()'}
df = defaultdict(int)
for s in strings1.values():
    for x in s:
        df[x] += 1
for s in strings2.values():
    for x in s:
        df[x] += 1
total = len(strings1) + len(strings2)


def weight(x):
    if x in noise or len(x) < 3:
        return 0.0
    return math.log(total / df[x])


index1 = defaultdict(set)
for a, s in strings1.items():
    for x in s:
        index1[x].add(a)
for a2, s2 in strings2.items():
    if a2 in result or not s2:
        continue
    scores = defaultdict(float)
    for x in s2:
        w = weight(x)
        if w <= 0:
            continue
        for a1 in index1.get(x, ()):
            scores[a1] += w
    if not scores:
        continue
    best = sorted(scores.items(), key=lambda kv: -kv[1])
    a1, score = best[0]
    second = best[1][1] if len(best) > 1 else 0.0
    shared = s2 & strings1[a1]
    if score >= 5.0 and score > second * 1.3 and len(shared) >= 2:
        result[a2] = {'name': names1[a1], 'how': 'strings', 'score': round(score, 1), 'log1': a1}

# 3. call-site propagation: same number of calls in caller pairs -> pair the callees in order
def calls(code, own):
    out = []
    for m in CALL.finditer(code):
        name = m.group(1)
        if name.startswith('FUN_'):
            out.append(name[4:])
        elif own is not None and name in own:
            out.append(own[name])
    return out


by_name1 = {}
for a, n in names1.items():
    short = n.split('(')[0]
    by_name1.setdefault(short, a)
addr_by_sym1 = {}
for a, c in code1.items():
    m = re.search(r'/\* (.+?) \*/', c)
    if m:
        addr_by_sym1[m.group(1).split('(')[0]] = a


def calls1(a):
    out = []
    for m in CALL.finditer(code1[a]):
        name = m.group(1)
        if name.startswith('FUN_'):
            out.append(name[4:])
        elif name in addr_by_sym1:
            out.append(addr_by_sym1[name])
        elif name in by_name1:
            out.append(by_name1[name])
        else:
            out.append(None)
    return out


def calls2(a):
    return [m.group(1)[4:] if m.group(1).startswith('FUN_') else None for m in CALL.finditer(code2[a])]


# The call lists of a matched pair are aligned on the library calls they share (anchors);
# between two anchors, segments with the same number of own calls pair up in order. Votes
# from all pairs are counted; a callee gets the name most pairs agree on.
from collections import Counter


def call_list1(a):
    out = []
    for m in CALL.finditer(code1[a]):
        name = m.group(1)
        if name in addr_by_sym1:
            out.append(('own', addr_by_sym1[name]))
        elif name in by_name1:
            out.append(('own', by_name1[name]))
        elif name.startswith('FUN_'):
            out.append(('own', None))
        elif name.startswith(('luax::', 'core::', 'engine::', 'std::')):
            out.append(('own', None))
        else:
            out.append(('lib', name.split('::')[-1]))
    return out


def call_list2(a):
    out = []
    for m in CALL.finditer(code2[a]):
        name = m.group(1)
        if name.startswith('FUN_'):
            out.append(('own', name[4:]))
        elif name.startswith('thunk_FUN_'):
            out.append(('own', name[10:]))
        else:
            out.append(('lib', name))
    return out


def align(c1, c2):
    pairs = []
    # anchors: library calls in the same order on both sides (greedy)
    i = j = 0
    seg1, seg2 = [], []
    while True:
        # advance to next lib call on each side
        while i < len(c1) and c1[i][0] == 'own':
            seg1.append(c1[i][1]); i += 1
        while j < len(c2) and c2[j][0] == 'own':
            seg2.append(c2[j][1]); j += 1
        anchor1 = c1[i][1] if i < len(c1) else None
        anchor2 = c2[j][1] if j < len(c2) else None
        if anchor1 == anchor2:
            if len(seg1) == len(seg2):
                pairs.extend(zip(seg1, seg2))
            seg1, seg2 = [], []
            if anchor1 is None:
                break
            i += 1; j += 1
        else:
            # skip the anchor that the other side lacks
            if anchor1 is not None and anchor1 not in [c[1] for c in c2[j:]]:
                i += 1
            elif anchor2 is not None and anchor2 not in [c[1] for c in c1[i:]]:
                j += 1
            else:
                # both anchors exist later on the other side: drop the segment
                seg1, seg2 = [], []
                i += 1; j += 1
    return pairs


for rounds in range(4):
    votes = defaultdict(Counter)
    for a2, info in list(result.items()):
        a1 = info.get('log1')
        if a1 is None or a2 not in code2 or a1 not in code1:
            continue
        for x1, x2 in align(call_list1(a1), call_list2(a2)):
            if x1 and x2 and x2 not in result and x1 in names1:
                votes[x2][x1] += 1
    added = 0
    for x2, counter in votes.items():
        (x1, n), = counter.most_common(1)
        total_votes = sum(counter.values())
        if n >= 2 and n * 2 > total_votes or (n == 1 and total_votes == 1):
            result[x2] = {'name': names1[x1], 'how': 'calls', 'score': round(n / total_votes, 2), 'log1': x1, 'votes': n}
            added += 1
    if not added:
        break

json.dump(result, open(ROOT / 'reverse/log2/names.json', 'w'), indent=1)
how = defaultdict(int)
for v in result.values():
    how[v['how']] += 1
print('%d of %d functions named: %s' % (len(result), len(code2), dict(how)))
if '--print' in sys.argv:
    for a in sorted(result):
        print(a, result[a]['how'], result[a]['name'])
