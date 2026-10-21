"""Minimal PE32 reader: sections, VA<->offset, resources. No external deps."""
import struct


class PE:
    def __init__(self, path):
        self.data = open(path, 'rb').read()
        d = self.data
        pe = struct.unpack_from('<I', d, 0x3C)[0]
        assert d[pe:pe + 4] == b'PE\0\0'
        nsec = struct.unpack_from('<H', d, pe + 6)[0]
        optsz = struct.unpack_from('<H', d, pe + 20)[0]
        opt = pe + 24
        self.image_base = struct.unpack_from('<I', d, opt + 28)[0]
        self.entry = self.image_base + struct.unpack_from('<I', d, opt + 16)[0]
        ndirs = struct.unpack_from('<I', d, opt + 92)[0]
        self.dirs = [struct.unpack_from('<II', d, opt + 96 + 8 * i) for i in range(ndirs)]
        self.sections = []
        s = opt + optsz
        for i in range(nsec):
            name = d[s:s + 8].rstrip(b'\0').decode('latin1')
            vsize, va, rsize, raw = struct.unpack_from('<IIII', d, s + 8)
            self.sections.append((name, va, vsize, raw, rsize))
            s += 40

    def rva2off(self, rva):
        for name, va, vsize, raw, rsize in self.sections:
            if va <= rva < va + max(vsize, rsize):
                if rva - va >= rsize:
                    return None
                return raw + rva - va
        return None

    def va2off(self, va):
        return self.rva2off(va - self.image_base)

    def off2va(self, off):
        for name, va, vsize, raw, rsize in self.sections:
            if raw <= off < raw + rsize:
                return self.image_base + va + off - raw
        return None

    def u32(self, va):
        o = self.va2off(va)
        return None if o is None else struct.unpack_from('<I', self.data, o)[0]

    def bytes_at(self, va, n):
        o = self.va2off(va)
        return None if o is None else self.data[o:o + n]

    def pstr(self, va):
        o = self.va2off(va)
        if o is None:
            return None
        n = self.data[o]
        return self.data[o + 1:o + 1 + n].decode('latin1')

    def resources(self):
        """Yield (type, name, lang, bytes). Names are str or int."""
        rva = self.dirs[2][0]
        base = self.rva2off(rva)
        d = self.data

        def name_of(entry_name):
            if entry_name & 0x80000000:
                o = base + (entry_name & 0x7FFFFFFF)
                n = struct.unpack_from('<H', d, o)[0]
                return d[o + 2:o + 2 + 2 * n].decode('utf-16le')
            return entry_name

        def walk(off, path):
            nn, ni = struct.unpack_from('<HH', d, off + 12)
            for i in range(nn + ni):
                en, eo = struct.unpack_from('<II', d, off + 16 + 8 * i)
                p = path + [name_of(en)]
                if eo & 0x80000000:
                    yield from walk(base + (eo & 0x7FFFFFFF), p)
                else:
                    drva, dsz = struct.unpack_from('<II', d, base + eo)
                    o = self.rva2off(drva)
                    yield (*p, d[o:o + dsz])

        yield from walk(base, [])
