#!/usr/bin/env python3
"""Check every round-to-nearest conversion (lrint, lrintf, llrint) of the sources against
the conversions the original function actually performs.

Ghidra writes an x87 FIST/FISTP as ROUND(x) whatever the control word says, and both
compilers of the originals wrap the store in an FLDCW that switches the rounding to
truncation when the source was a plain (int) cast. So ROUND() in the pseudocode does not
mean lrint. The disassembly is read instead:

  truncating  FLDCW + FIST(P), CVTTSS2SI/CVTTSD2SI, call __ftol2 (0x00452c90 in grimrock2.exe)
  nearest     FIST(P) with the control word left alone, CVTSS2SI/CVTSD2SI, call lrint(f)

usage: roundcheck.py [--all]

The first game is disassembled with objdump from Grimrock.bin.x86(.orig), the second
through the Ghidra bridge. A lrint in a function whose original only truncates is
reported; one without an address comment is our own code and is listed with --all."""
import json, re, subprocess, sys, urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
everything = '--all' in sys.argv
STEAM = Path.home() / '.local/share/Steam/steamapps/common/Legend of Grimrock'
LOG1 = STEAM / 'Grimrock.bin.x86.orig'
if not LOG1.exists():
    LOG1 = STEAM / 'Grimrock.bin.x86'
GHIDRA = 'http://127.0.0.1:8089'

sizes = {}
for line in open(ROOT / 'reverse/symtab.txt'):
    parts = line.split()
    if len(parts) >= 8 and parts[3] == 'FUNC':
        sizes[parts[1].lower()] = int(parts[2])


def classify(lines):
    trunc = nearest = 0
    pending_cw = False
    for text in lines:
        t = text.lower()
        if 'fldcw' in t:
            pending_cw = not pending_cw  # set before the store, restored after
            continue
        if re.search(r'\bfist', t):
            if pending_cw:
                trunc += 1
            else:
                nearest += 1
        elif 'cvtts' in t:
            trunc += 1
        elif re.search(r'cvts[sd]2si', t):
            nearest += 1
        elif 'call' in t and ('452c90' in t or 'ftol' in t):
            trunc += 1
        elif 'call' in t and 'lrint' in t:
            nearest += 1
    return trunc, nearest


def log1(address):
    size = sizes.get(address)
    if not size or not LOG1.exists():
        return None
    start = int(address, 16)
    out = subprocess.run(['objdump', '-d', '--start-address=%#x' % start,
                          '--stop-address=%#x' % (start + size), str(LOG1)],
                         capture_output=True, text=True).stdout
    return classify(out.split('\n'))


def log2(address):
    try:
        with urllib.request.urlopen('%s/disassemble_function?address=%s' % (GHIDRA, address),
                                    timeout=20) as r:
            data = json.load(r)
    except Exception:
        return None
    ins = data.get('instructions') or data.get('disassembly') or []
    return classify([i.get('instruction', '') if isinstance(i, dict) else i for i in ins])


findings = 0
for directory in ('src', 'src2'):
    for path in sorted((ROOT / directory).rglob('*.cpp')):
        lines = path.read_text().split('\n')
        address = None
        for i, line in enumerate(lines):
            m = re.match(r'\s*// 0x(0?8[0-9a-f]{6}|00[0-9a-f]{6})', line)
            if m:
                address = m.group(1).zfill(8)
            elif line.startswith('}'):
                pass
            if not re.search(r'\bll?rintf?\(', line) or line.strip().startswith('//'):
                continue
            where = '%s:%d' % (path.relative_to(ROOT), i + 1)
            if not address:
                if everything:
                    print('%-48s ours (no address)' % where)
                continue
            result = log1(address) if address.startswith('08') else log2(address)
            if result is None:
                if everything:
                    print('%-48s %s not disassembled' % (where, address))
                continue
            trunc, nearest = result
            if nearest == 0 and trunc > 0:
                findings += 1
                print('%-48s %s truncates only (%d)  %s' % (where, address, trunc, line.strip()[:60]))
            elif everything:
                print('%-48s %s trunc %d nearest %d' % (where, address, trunc, nearest))
print('%d lrint calls where the original truncates' % findings)
