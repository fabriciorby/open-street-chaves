"""Find Delphi 5 class VMTs and dump published methods/fields.

Usage: python3 vmt.py Chaves.exe [ClassFilter...]
Prints every class (VMT address, name, parent, instance size) and, for
classes matching a filter, its published method table (event handlers with
their code addresses) and published field table (component fields with
their instance offsets).
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from pe import PE  # noqa: E402

SELF, INTF, AUTO, INIT, TINFO, FIELD, METHOD, DYN, NAME, ISIZE, PARENT = range(-76, -32, 4)


def find_vmts(pe):
    d = pe.data
    vmts = {}
    for name, va, vsize, raw, rsize in pe.sections:
        for off in range(raw, raw + rsize - 4, 4):
            v = struct.unpack_from('<I', d, off)[0]
            here = pe.off2va(off)
            if v == here + 76:
                nameptr = pe.u32(v + NAME)
                cname = pe.pstr(nameptr) if nameptr else None
                if cname and cname.isprintable() and 0 < len(cname) < 64:
                    vmts[v] = cname
    return vmts


def parent_name(pe, vmts, vmt):
    p = pe.u32(vmt + PARENT)
    if not p:
        return None
    pv = pe.u32(p)
    return vmts.get(pv, hex(pv) if pv else None)


def methods(pe, vmt):
    mt = pe.u32(vmt + METHOD)
    if not mt:
        return []
    n = struct.unpack('<H', pe.bytes_at(mt, 2))[0]
    p, out = mt + 2, []
    for _ in range(n):
        size, addr = struct.unpack('<HI', pe.bytes_at(p, 6))
        out.append((addr, pe.pstr(p + 6)))
        p += size
    return out


def fields(pe, vmt):
    ft = pe.u32(vmt + FIELD)
    if not ft:
        return []
    n, ctab = struct.unpack('<HI', pe.bytes_at(ft, 6))
    p, out = ft + 6, []
    classes = []
    if ctab:
        cn = struct.unpack('<H', pe.bytes_at(ctab, 2))[0]
        for i in range(cn):
            cref = pe.u32(ctab + 2 + 4 * i)
            cv = pe.u32(cref) if cref else None
            classes.append(pe.pstr(pe.u32(cv + NAME)) if cv else '?')
    for _ in range(n):
        off, idx = struct.unpack('<IH', pe.bytes_at(p, 6))
        nm = pe.pstr(p + 6)
        out.append((off, nm, classes[idx] if idx < len(classes) else '?'))
        p += 7 + len(nm)
    return out


def main():
    pe = PE(sys.argv[1])
    filt = sys.argv[2:]
    vmts = find_vmts(pe)
    for v, n in sorted(vmts.items()):
        isz = pe.u32(v + ISIZE)
        print('VMT %08x %-28s parent=%-20s size=%d' % (v, n, parent_name(pe, vmts, v), isz))
        if any(f.lower() == n.lower() for f in filt):
            for off, fn, cn in fields(pe, v):
                print('    field +0x%03x %-28s : %s' % (off, fn, cn))
            for a, mn in methods(pe, v):
                print('    method %08x %s' % (a, mn))


if __name__ == '__main__':
    main()
