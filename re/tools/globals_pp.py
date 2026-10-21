"""Rewrite PTR_DAT_<ptr> accesses in decompiled C into named globals.

Delphi references unit globals through a pointer table in DATA, so Ghidra
shows `*PTR_DAT_004b1014` for what is really one global variable. This maps
each pointer to its target and prints `g_<target>` (or the name assigned in
globals.txt) instead.

Usage: python3 globals_pp.py Chaves.exe globals.txt in.c out.c
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(__file__))
from pe import PE  # noqa: E402


def main():
    exe, gfile, src, dst = sys.argv[1:5]
    pe = PE(exe)
    names = {}
    if os.path.exists(gfile):
        for line in open(gfile):
            line = line.split('#')[0].split()
            if len(line) >= 2:
                names[int(line[0], 16)] = line[1]

    def target(ptr_hex):
        t = pe.u32(int(ptr_hex, 16))
        return names.get(t, 'g_%06x' % (t & 0xFFFFFF)) if t else None

    text = open(src).read()

    def deref(m):
        n = target(m.group(2))
        return n if n else m.group(0)

    text = re.sub(r'\*(\([\w ]+\*\))?PTR_DAT_([0-9a-f]{8})\b', deref, text)

    def addr(m):
        n = target(m.group(1))
        return '&' + n if n else m.group(0)

    text = re.sub(r'\bPTR_DAT_([0-9a-f]{8})\b', addr, text)
    # Direct references to named globals (BSS/DATA accessed without the table).
    for a, n in names.items():
        text = re.sub(r'\b(?:DAT|_DAT)_%08x\b' % a, n, text)
    open(dst, 'w').write(text)


if __name__ == '__main__':
    main()
