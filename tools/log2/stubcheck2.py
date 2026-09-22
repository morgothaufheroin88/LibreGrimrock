#!/usr/bin/env python3
"""Compare every reconstructed function with the pseudocode of the address it claims and
report what the original mentions and ours does not: string literals (uniform names,
shader files, messages) and the GL entry points it calls.

usage: stubcheck2.py [--calls] [--quiet]

The strings are the sharp end: a uniform the original sets and the reconstruction does not
is a shader input left at its default. A string that appears elsewhere in the same source
counts as present, because the reconstruction factors helpers and registration tables out
of the function. GL calls are noisier, because the reconstruction goes through
RenderContextGL for the state it sets, so they are only listed with --calls."""
import json, re, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
show_calls = '--calls' in sys.argv
quiet = '--quiet' in sys.argv
names = json.load(open(ROOT / 'reverse/log2/names.json'))
STRING = re.compile(r'"((?:[^"\\\n]|\\.){2,60})"')
# the back ends the reconstruction does not have
OUT_OF_SCOPE = re.compile(r'XAudio|D3D|d3d9|Direct3D|CreateSourceVoice|IXAudio')
# strings of the original that the reconstruction has no reason to carry: the keys of the
# luax registry (they live in luax.cpp), the GL entry point names (GLEW resolves them),
# the assert texts of the MSVC runtime and the zlib version the library checks itself
IGNORE = re.compile(r'^(rapid\.|gl[A-Z]|al[A-Z]|wgl|c:\\|i >= 0|\d+\.\d+\.\d+$)')
GLCALL = re.compile(r'\b(gl[A-Z]\w+)\s*\(')
# format strings, single characters and the like carry no meaning for the comparison
BORING = re.compile(r'^(%[sdfxu]|\\n|[ .,:/*+-]|<[^>]*>)*$')
# the state the reconstruction sets through RenderContextGL rather than by hand
WRAPPED = {
    'glUseProgram': 'useProgram', 'glActiveTexture': 'setUniformTexture',
    'glBindTexture': 'setUniformTexture', 'glUniform1i': 'setUniformTexture',
    'glBlendFunc': 'setBlendMode', 'glBlendFuncSeparate': 'setBlendMode',
    'glBindFramebuffer': 'setRenderTarget', 'glFramebufferTexture2D': 'setRenderTarget',
    'glFramebufferRenderbuffer': 'setRenderTarget', 'glDrawBuffers': 'setRenderTarget',
    'glTexParameteri': 'setUniformTexture', 'glDrawArrays': 'drawRect',
    'glDrawElements': 'drawRect', 'glBindBuffer': 'streamWrite', 'glMapBufferRange': 'streamWrite',
    'glUnmapBuffer': 'streamWrite', 'glBufferData': 'streamWrite',
}


def body(lines, start):
    depth, out = 0, []
    for i in range(start, min(start + 600, len(lines))):
        line = lines[i]
        if depth == 0:
            if line.startswith('{'):
                depth = 1
            elif line.rstrip().endswith(';'):
                return ''            # a declaration
            continue
        out.append(line)
        depth += line.count('{') - line.count('}')
        if depth <= 0:
            break
    return '\n'.join(out)


findings = 0
for directory in ('src', 'src2'):
    for path in sorted((ROOT / directory).rglob('*.cpp')):
        lines = path.read_text().split('\n')
        for i, line in enumerate(lines):
            m = re.match(r'// 0x00([0-9a-f]{6})', line.strip())
            if not m:
                continue
            address = '00' + m.group(1)
            native = ROOT / 'reverse/log2/native' / (address + '.c')
            if not native.exists():
                continue
            code = native.read_text()
            ours = body(lines, i + 1)
            if not ours:
                continue
            whole = '\n'.join(lines)   # a helper or a table the unit factored out
            # the pseudocode escapes the quotes inside a string, our sources do not
            unescape = lambda text: text.replace("\\'", "'").replace('\\"', '"')
            theirs_strings = {unescape(s) for s in STRING.findall(code) if not BORING.match(s)}
            our_strings = {unescape(s) for s in STRING.findall(ours)}
            # a type name the binding checks (luax::checkObject<Image>) is spelled as an
            # identifier in the reconstruction, not as a string
            missing = sorted(s for s in theirs_strings
                             if s not in our_strings
                             and not re.search(r'\b%s\b' % re.escape(s), ours)
                             and ('"%s"' % s) not in whole
                             and not OUT_OF_SCOPE.search(s) and not IGNORE.match(s))
            missing_calls = []
            if show_calls:
                our_calls = set(GLCALL.findall(ours))
                for call in sorted(set(GLCALL.findall(code))):
                    if call in our_calls:
                        continue
                    wrapper = WRAPPED.get(call)
                    if wrapper and wrapper in ours:
                        continue
                    missing_calls.append(call)
            if not missing and not missing_calls:
                continue
            findings += 1
            name = names.get(address, {}).get('name', '')
            print('%s %s:%d %s' % (address, path.relative_to(ROOT), i + 1, name[:40]))
            if missing:
                print('    strings: %s' % ', '.join(missing[:8]))
            if missing_calls:
                print('    calls:   %s' % ', '.join(missing_calls[:8]))
if not quiet:
    print('%d functions mention something the reconstruction does not' % findings)
