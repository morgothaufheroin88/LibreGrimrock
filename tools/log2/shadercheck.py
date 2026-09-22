#!/usr/bin/env python3
"""Check that the reconstruction feeds the shaders of grimrock2.dat: every shader the
sources load is read out of the archive, its uniforms are collected (including the ones of
the files it includes) and compared with the uniform names the sources mention.

usage: shadercheck.py [archive] [--all]

A uniform no source names is either dead in that shader or an input left at its default,
which is how an effect ends up rendering but looking wrong. The built-in uniforms of the
mesh shaders are set through ShaderProgramGL's cached slots, whose names are in a table in
RenderContextGL.cpp, so they count as named."""
import re, subprocess, sys, tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
args = [a for a in sys.argv[1:] if not a.startswith('--')]
everything = '--all' in sys.argv
archive = Path(args[0]) if args else ROOT / 'run2/grimrock2.dat'
tool = ROOT / 'build-release/grimrock_archive'
if not tool.exists() or not archive.exists():
    raise SystemExit('need %s and %s' % (tool, archive))

# the sources as the game-2 build sees them: a file in src2 replaces the one in src
overlaid = {p.name for p in (ROOT / 'src2').rglob('*') if p.suffix in ('.cpp', '.h')}
sources = [p for d in ('src', 'src2') for p in (ROOT / d).rglob('*')
           if p.suffix in ('.cpp', '.h') and not (d == 'src' and p.name in overlaid)]
text = '\n'.join(p.read_text() for p in sources)
named = set(re.findall(r'"(g_\w+)"', text)) | set(re.findall(r'"(\w+)"', text))
shaders = sorted(set(re.findall(r'"((?:assets/)?shaders/gl/[\w/]+\.(?:vsh|fsh|h))"', text)))

UNIFORM = re.compile(r'^\s*uniform\s+\w+\s+(\w+)\s*(?:\[[^\]]*\])?\s*;', re.M)
INCLUDE = re.compile(r'^\s*#include\s+"([^"]+)"', re.M)
cache = {}


def read(path):
    if path in cache:
        return cache[path]
    out = Path(tempfile.gettempdir()) / ('shader_' + path.replace('/', '_'))
    result = subprocess.run([str(tool), str(archive), 'read', path, str(out)],
                            capture_output=True, text=True)
    cache[path] = out.read_text() if result.returncode == 0 and out.exists() else None
    return cache[path]


def uniforms(path, seen):
    body = read(path)
    if body is None:
        return None
    found = set(UNIFORM.findall(body))
    for include in INCLUDE.findall(body):
        candidate = path.rsplit('/', 1)[0] + '/' + include
        if candidate in seen:
            continue
        seen.add(candidate)
        found |= uniforms(candidate, seen) or set()
    return found


missing_files = []
findings = 0
for path in shaders:
    found = uniforms(path, {path})
    if found is None:
        missing_files.append(path)
        continue
    unused = sorted(u for u in found if u not in named)
    if unused:
        findings += 1
        print('%-44s %s' % (path, ', '.join(unused)))
    elif everything:
        print('%-44s ok (%d uniforms)' % (path, len(found)))
if missing_files:
    print('not in the archive: %s' % ', '.join(missing_files))
print('%d of %d shaders have a uniform no source names' % (findings, len(shaders)))
