#!/usr/bin/env python3
"""Compare the float literals of every grimrock2.exe function with the reconstruction that
claims its address: a constant the original uses and ours does not is a threshold, a bias
or a scale that was read wrong. usage: constcheck2.py [PATH FRAGMENT]

When two functions of a source carry the same address comment (an inlined helper), the
first one is measured, so the constants of the other are reported as missing.

The pseudocode writes floats both as decimals and as the hex bit pattern, so both forms
are read; the values that carry no information (0, 1, 0.5, 2, 255) are skipped, and a
constant our source names (a constexpr anywhere under src/ or src2/) counts as used."""
import re, struct, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
want = sys.argv[1] if len(sys.argv) > 1 else None
# values that carry no information, plus the artefacts of the pseudocode: 2^32 from an
# unsigned conversion and 32767 from the Windows RAND_MAX
BORING = (0.0, 1.0, 0.5, 2.0, 255.0, -1.0, 3.0, 4.0, 4294967296.0, 4294967295.0, 32767.0,
          3.4028235e+38)  # the last one is FLT_MAX, which our sources name

# constexpr values and tables by name, so a body that names one counts as containing them
named = {}
CONST = re.compile(r'constexpr\s+[\w:]+\s+(\w+)\s*=\s*(-?(?:0x[0-9a-fA-F]+|\d+\.?\d*(?:e[+-]?\d+)?))')
ARRAY = re.compile(r'constexpr\s+[\w:]+\s+(\w+)\s*\[[^\]]*\](?:\[[^\]]*\])?\s*=\s*\{(.*?)\};', re.S)


def literal_value(text):
    try:
        return float(int(text, 0)) if text.startswith('0x') else float(text)
    except ValueError:
        return None


DIVISION = re.compile(r'constexpr\s+[\w:]+\s+(\w+)\s*=\s*([\d.]+)f?\s*([*/])\s*([\d.]+)f?')


for directory in ('src', 'src2'):
    for path in (ROOT / directory).rglob('*'):
        if path.suffix not in ('.cpp', '.h'):
            continue
        text = path.read_text()
        for name, literal in CONST.findall(text):
            value = literal_value(literal)
            if value is not None:
                named.setdefault(name, set()).add(value)
        # a constant the reconstruction writes as a division or a product (1.0f / 42.0f)
        for name, left, operator, right in DIVISION.findall(text):
            a, b = literal_value(left), literal_value(right)
            if a is None or b is None or (operator == '/' and b == 0):
                continue
            named.setdefault(name, set()).add(a / b if operator == '/' else a * b)
        for name, block in ARRAY.findall(text):
            for literal in re.findall(r'-?(?:0x[0-9a-fA-F]+|\d+\.?\d*(?:e[+-]?\d+)?)', block):
                value = literal_value(literal)
                if value is not None:
                    named.setdefault(name, set()).add(value)
                    named.setdefault(name, set()).add(-value)


def floats(text, hex_patterns):
    out = set()
    for m in re.finditer(r'(?<![\w.])(\d+\.\d+(?:e[+-]?\d+)?)f?', text):
        out.add(float(m.group(1)))
    if hex_patterns:
        for m in re.finditer(r'0x([0-9a-f]{8})\b', text):
            bits = int(m.group(1), 16)
            value = struct.unpack('<f', struct.pack('<I', bits))[0]
            if value == value and 1e-6 < abs(value) < 1e7 and (bits >> 23) & 0xff not in (0, 0xff):
                out.add(float('%.7g' % value))
    return out


def body(lines, start):
    """the function that follows, signature and initialiser list included"""
    depth, out = 0, []
    for i in range(start, min(start + 600, len(lines))):
        line = lines[i]
        if depth == 0:
            out.append(line)          # the initialiser list carries constants too
            if line.startswith('{'):
                depth = 1
            elif line.rstrip().endswith(';'):
                return ''
            continue
        out.append(line)
        depth += line.count('{') - line.count('}')
        if depth <= 0:
            break
    return '\n'.join(out)

def static_helpers(lines):
    """the file-static functions of a unit by name, with their bodies"""
    helpers = {}
    for i, line in enumerate(lines):
        m = re.match(r'static (?:inline )?[\w:<>*& ]+?\b(\w+)\(', line)
        if m and not line.rstrip().endswith(';'):
            text = body(lines, i)
            if text:
                helpers[m.group(1)] = text
    return helpers


def with_helpers(text, helpers, depth=2):
    """a body together with the file helpers it calls (the reconstruction factors some
    of the original's code into them)"""
    seen = set()
    extra = []
    frontier = [text]
    for _ in range(depth):
        found = []
        for chunk in frontier:
            for name in set(re.findall(r'\b(\w+)\(', chunk)):
                if name in helpers and name not in seen:
                    seen.add(name)
                    found.append(helpers[name])
        extra += found
        frontier = found
    return '\n'.join([text] + extra)



def close(a, b):
    # the sign is dropped: our sources write the minus as an operator, the pseudocode
    # bakes it into the literal
    a, b = abs(a), abs(b)
    return abs(a - b) <= 1e-5 * max(1.0, a, b)


findings = 0
for directory in ('src', 'src2'):
    for path in sorted((ROOT / directory).rglob('*.cpp')):
        if want and want not in str(path):
            continue
        lines = path.read_text().split('\n')
        helpers = static_helpers(lines)
        for i, line in enumerate(lines):
            m = re.match(r'// 0x00([0-9a-f]{6})', line.strip())
            if not m:
                continue
            native = ROOT / 'reverse/log2/native' / ('00' + m.group(1) + '.c')
            if not native.exists():
                continue
            ours = body(lines, i + 1)
            if not ours:
                continue
            ours = with_helpers(ours, helpers)
            # the string literals of the pseudocode are not numbers ("1.2.5")
            pseudocode = re.sub(r'"(?:[^"\\\n]|\\.)*"', '""', native.read_text())
            theirs = {v for v in floats(pseudocode, True) if v not in BORING}
            if not theirs:
                continue
            mine = floats(ours, False)
            mine |= {float(x) for x in re.findall(r'(?<![\w.])(\d+)(?![\w.])', ours)}
            for name in set(re.findall(r'\b[A-Za-z_]\w*\b', ours)):
                mine |= named.get(name, set())
            missing = [v for v in sorted(theirs)
                       if not any(close(v, x) for x in mine)
                       and not any(x and close(v, 1.0 / x) for x in mine)
                       and not any(close(v, 1.0 - x) for x in mine)
                       and not any(close(v, x * 3.1415927 / 180) for x in mine)
                       and not any(close(v * 180 / 3.1415927, x) for x in mine)]
            if missing:
                findings += 1
                print('%s %s:%d missing %s' % ('00' + m.group(1), path.relative_to(ROOT), i + 1,
                                               ', '.join('%g' % v for v in missing[:6])))
print('%d functions use a constant the reconstruction does not' % findings)
