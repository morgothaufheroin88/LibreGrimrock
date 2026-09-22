#!/usr/bin/env python3
"""For every grimrock2.exe Lua binding that has a LoG1 counterpart (reverse/log2/names.json),
compares a signature of the two pseudocodes: the ordered list of Lua API calls with
their literal arguments, luaL_check*/lua_to*/lua_push* calls, string constants and float
constants.  Prints the bindings whose signatures differ, so the game-2 differences can be
reviewed in bulk instead of one runtime error at a time."""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
names = json.load(open(ROOT / 'reverse/log2/names.json'))
bindings = json.load(open(ROOT / 'reverse/log2/bindings.json'))
CLASS_NAMES = set(c['name'] for classes in bindings.values() for c in classes)
API = re.compile(r'\b(luaL?_[a-zA-Z_]+)\s*\(([^;]*?)\)\s*;')
STR = re.compile(r'"((?:[^"\\]|\\.)*)"')
FLT = re.compile(r'\b\d+\.\d+\b')


# MSVC and GCC inline different helpers: normalise the API names first
NORMALISE = {'luaL_checklstring': 'luaL_checkstring', 'luaL_optlstring': 'luaL_optstring',
             'lua_tolstring': 'lua_tostring', 'lua_settop': 'lua_pop',
             'lua_pushlstring': 'lua_pushstring', 'luaL_checknumber': 'luaL_checknumber',
             'lua_objlen': 'lua_objlen', 'lua_createtable': 'lua_newtable'}
IGNORE = {'lua_pop', 'lua_getfield', 'lua_rawget', 'lua_rawset', 'lua_pushlightuserdata',
          'lua_remove', 'lua_pushvalue', 'lua_setmetatable', 'lua_newuserdata', 'lua_insert',
          'lua_type', 'lua_gettop', 'lua_rawgeti', 'lua_rawseti', 'lua_setfield',
          'lua_newtable', 'lua_next', 'lua_pushnil', 'luaL_error', 'lua_isnil', 'luaL_typerror',
          'luaL_argerror'}


def signature(text):
    sig = []
    for m in API.finditer(text):
        fn = NORMALISE.get(m.group(1), m.group(1))
        if fn in IGNORE:
            continue
        sig.append(fn)
    # the class name of luaL_typerror is a helper in LoG1 and inlined in LoG2
    strings = [s for s in STR.findall(text) if not s.startswith('rapid.') and s not in
               ('vec', 'table', 'string', 'number') and s not in CLASS_NAMES and ' ' not in s]
    sig += sorted('S:' + s for s in strings)
    sig += sorted('F:' + f for f in FLT.findall(text))
    return sig


verbose = '-v' in sys.argv
only = [a for a in sys.argv[1:] if not a.startswith('-')]
differ = 0
for addr, info in sorted(names.items(), key=lambda kv: kv[1]['name']):
    if info.get('how') != 'binding' or not info.get('log1'):
        continue
    if only and not any(info['name'].startswith(o) for o in only):
        continue
    p2 = ROOT / 'reverse/log2/native' / (addr + '.c')
    p1 = ROOT / 'reverse/native' / (info['log1'] + '.c')
    if not p2.exists() or not p1.exists():
        continue
    s2, s1 = signature(p2.read_text()), signature(p1.read_text())
    if s1 == s2:
        continue
    differ += 1
    print('%-45s %s  (LoG1 %s)' % (info['name'], addr, info['log1']))
    if verbose:
        import difflib
        for line in difflib.unified_diff(s1, s2, 'log1', 'log2', lineterm='', n=0):
            print('    ' + line)
print('%d bindings differ' % differ)
