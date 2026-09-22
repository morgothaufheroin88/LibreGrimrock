#!/usr/bin/env python3
"""Write the window icon of grimrock2.exe out as a PNG, the way the game expects it on
Linux: the Windows build calls LoadIcon on resource 0x65 of its own executable, the
reconstruction reads grimrock2.png next to the binary (see src/rapid/main.cpp).

usage: icon.py [grimrock2.exe] [grimrock2.png]

Only the standard library is used: the resource directory of the PE file is walked for
the icon group, the largest icon in it is taken, and a 32 bit DIB is turned into a PNG
(an icon that is already a PNG is written as it is)."""
import struct, sys, zlib
from pathlib import Path

RT_ICON, RT_GROUP_ICON = 3, 14
ICON_GROUP_ID = 0x65


def sections(data):
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    if data[pe:pe + 4] != b'PE\0\0':
        raise SystemExit('not a PE file')
    count, = struct.unpack_from('<H', data, pe + 6)
    optional, = struct.unpack_from('<H', data, pe + 20)
    table = pe + 24 + optional
    out = []
    for i in range(count):
        offset = table + i * 40
        name = data[offset:offset + 8].rstrip(b'\0').decode('latin1')
        size, address, raw_size, raw = struct.unpack_from('<IIII', data, offset + 8)
        out.append((name, address, size, raw, raw_size))
    return out


class Resources:
    """the .rsrc directory tree, addressed by (type, name/id)"""

    def __init__(self, data):
        for name, address, size, raw, raw_size in sections(data):
            if name == '.rsrc':
                self.data = data
                self.base = raw
                self.address = address
                return
        raise SystemExit('no resource section')

    def entries(self, offset):
        named, ids = struct.unpack_from('<HH', self.data, self.base + offset + 12)
        out = []
        for i in range(named + ids):
            entry = self.base + offset + 16 + i * 8
            key, value = struct.unpack_from('<II', self.data, entry)
            out.append((key & 0x7fffffff if key & 0x80000000 else key, value))
        return out

    def leaf(self, offset):
        address, size = struct.unpack_from('<II', self.data, self.base + offset)
        start = self.base + address - self.address
        return self.data[start:start + size]

    def find(self, type_id, resource_id):
        for key, value in self.entries(0):
            if key != type_id or not value & 0x80000000:
                continue
            for name, sub in self.entries(value & 0x7fffffff):
                if resource_id is not None and name != resource_id:
                    continue
                if not sub & 0x80000000:
                    return self.leaf(sub)
                language = self.entries(sub & 0x7fffffff)[0][1]
                return self.leaf(language)
        return None


def png(width, height, rgba):
    """an RGBA image as a PNG file"""
    raw = b''.join(b'\0' + rgba[y * width * 4:(y + 1) * width * 4] for y in range(height))

    def chunk(tag, payload):
        return (struct.pack('>I', len(payload)) + tag + payload
                + struct.pack('>I', zlib.crc32(tag + payload) & 0xffffffff))

    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def icon_to_png(blob):
    if blob[:8] == b'\x89PNG\r\n\x1a\n':
        return blob
    size, width, height, planes, bits = struct.unpack_from('<IiiHH', blob, 0)
    height //= 2  # the colour and the mask image are stacked
    if bits != 32:
        raise SystemExit('icon is %d bits per pixel, only 32 are handled' % bits)
    pixels = blob[size:size + width * height * 4]
    rows = []
    for y in range(height - 1, -1, -1):  # a DIB is stored bottom up
        row = bytearray(pixels[y * width * 4:(y + 1) * width * 4])
        row[0::4], row[2::4] = row[2::4], row[0::4]  # BGRA to RGBA
        rows.append(bytes(row))
    return png(width, height, b''.join(rows))


def main():
    default = Path.home() / '.local/share/Steam/steamapps/common/Legend of Grimrock 2/grimrock2.exe'
    exe = Path(sys.argv[1]) if len(sys.argv) > 1 else default
    out = Path(sys.argv[2]) if len(sys.argv) > 2 else Path('grimrock2.png')
    resources = Resources(exe.read_bytes())
    group = resources.find(RT_GROUP_ICON, ICON_GROUP_ID)
    if group is None:
        raise SystemExit('no icon group 0x%x in %s' % (ICON_GROUP_ID, exe))
    count, = struct.unpack_from('<H', group, 4)
    best = None
    for i in range(count):
        entry = 6 + i * 14
        width, height, colours, _, planes, bits, size, ident = struct.unpack_from(
            '<BBBBHHIH', group, entry)
        width = width or 256
        height = height or 256
        if best is None or (width, bits) > (best[0], best[2]):
            best = (width, height, bits, ident)
    width, height, bits, ident = best
    blob = resources.find(RT_ICON, ident)
    if blob is None:
        raise SystemExit('icon %d of the group is missing' % ident)
    out.write_bytes(icon_to_png(blob))
    print('%s: %dx%d, %d bits -> %s' % (exe.name, width, height, bits, out))


main()
