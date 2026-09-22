#!/usr/bin/env python3
"""Disassembly of a grimrock2.exe function through the Ghidra bridge, with the string
operands resolved (usage: dis.py ADDR...)."""
import json, re, sys, urllib.request

BRIDGE = 'http://127.0.0.1:8089'


def bridge(endpoint, **params):
    q = '&'.join('%s=%s' % kv for kv in params.items())
    with urllib.request.urlopen('%s/%s?%s' % (BRIDGE, endpoint, q), timeout=120) as r:
        return json.loads(r.read())


for arg in sys.argv[1:]:
    addr = int(arg, 16)
    res = bridge('disassemble_function', address='%08x' % addr)
    lines = res.get('instructions', res)
    for ins in lines:
        text = ins if isinstance(ins, str) else '%s  %s' % (ins.get('address', ''), ins.get('instruction', ins))
        print(text)
