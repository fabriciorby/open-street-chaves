"""Decode Delphi binary form resources (TPF0) into readable text.

Usage: python3 dfm.py Chaves.exe [outdir]
Writes one <FORM>.dfm.txt per RCDATA form resource. Large binary
properties (bitmaps, DelphiX image lists) are dumped as separate files
so their contents can be inspected later.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from pe import PE  # noqa: E402

RT_RCDATA = 10


class Reader:
    def __init__(self, b):
        self.b, self.p = b, 0

    def u8(self):
        v = self.b[self.p]
        self.p += 1
        return v

    def take(self, n):
        v = self.b[self.p:self.p + n]
        self.p += n
        return v

    def sstr(self):
        return self.take(self.u8()).decode('latin1')

    def i(self, fmt):
        v = struct.unpack_from(fmt, self.b, self.p)[0]
        self.p += struct.calcsize(fmt)
        return v


def read_value(r, blobs, path):
    t = r.u8()
    if t == 0:
        return 'Null'
    if t == 1:  # list
        items = []
        while r.b[r.p] != 0:
            items.append(read_value(r, blobs, path))
        r.u8()
        return '(' + ' '.join(items) + ')'
    if t == 2:
        return str(r.i('<b'))
    if t == 3:
        return str(r.i('<h'))
    if t == 4:
        return str(r.i('<i'))
    if t == 5:  # extended (10 bytes)
        raw = r.take(10)
        mant = int.from_bytes(raw[:8], 'little')
        se = int.from_bytes(raw[8:], 'little')
        sign = -1 if se & 0x8000 else 1
        exp = (se & 0x7FFF) - 16383
        return repr(sign * mant / (1 << 63) * 2.0 ** exp) if mant else '0'
    if t in (6, 7):  # string, ident
        return repr(r.sstr()) if t == 6 else r.sstr()
    if t == 8:
        return 'False'
    if t == 9:
        return 'True'
    if t == 10:  # binary
        n = r.i('<I')
        data = r.take(n)
        name = path.replace('.', '_') + '.bin'
        blobs[name] = data
        return '{binary %d bytes -> %s}' % (n, name)
    if t == 11:  # set
        items = []
        while True:
            s = r.sstr()
            if not s:
                break
            items.append(s)
        return '[' + ', '.join(items) + ']'
    if t == 12:  # long string
        return repr(r.take(r.i('<I')).decode('latin1'))
    if t == 13:
        return 'nil'
    if t == 14:  # collection
        items = []
        while r.b[r.p] != 0:
            if r.b[r.p] in (2, 3, 4):
                read_value(r, blobs, path)
            assert r.u8() == 1  # vaList
            props = []
            ipath = '%s.%02d' % (path, len(items))
            while r.b[r.p] != 0:
                pn = r.sstr()
                props.append('%s = %s' % (pn, read_value(r, blobs, ipath + '.' + pn)))
            r.u8()
            items.append('item ' + '; '.join(props) + ' end')
        r.u8()
        return '<' + ' '.join(items) + '>'
    if t == 15:
        return repr(r.i('<f'))
    if t == 16:
        return str(r.i('<q')) + ' {currency*1e4}'
    if t == 17:
        return str(r.i('<d')) + ' {date}'
    if t == 18:  # wstring
        return repr(r.take(2 * r.i('<I')).decode('utf-16le'))
    if t == 19:
        return str(r.i('<q'))
    raise ValueError('unknown value type %d at %d' % (t, r.p))


def read_component(r, blobs, out, depth, path):
    flags = r.b[r.p]
    if flags & 0xF0 == 0xF0:
        r.u8()
        if flags & 2:
            read_value(r, blobs, path)
    cls = r.sstr()
    name = r.sstr()
    ind = '  ' * depth
    out.append('%sobject %s: %s' % (ind, name, cls))
    cpath = path + '.' + (name or cls)
    while r.b[r.p] != 0:
        pn = r.sstr()
        out.append('%s  %s = %s' % (ind, pn, read_value(r, blobs, cpath + '.' + pn)))
    r.u8()
    while r.b[r.p] != 0:
        read_component(r, blobs, out, depth + 1, cpath)
    r.u8()
    out.append(ind + 'end')


def main():
    pe = PE(sys.argv[1])
    outdir = sys.argv[2] if len(sys.argv) > 2 else '.'
    os.makedirs(outdir, exist_ok=True)
    for typ, name, lang, data in pe.resources():
        if typ != RT_RCDATA or data[:4] != b'TPF0':
            continue
        r = Reader(data)
        r.take(4)
        out, blobs = [], {}
        read_component(r, blobs, out, 0, str(name))
        open(os.path.join(outdir, '%s.dfm.txt' % name), 'w').write('\n'.join(out) + '\n')
        bdir = os.path.join(outdir, '%s.blobs' % name)
        if blobs:
            os.makedirs(bdir, exist_ok=True)
        for bn, bd in blobs.items():
            open(os.path.join(bdir, bn), 'wb').write(bd)
        print(name, len(out), 'lines,', len(blobs), 'blobs')


if __name__ == '__main__':
    main()
