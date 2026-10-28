"""List every Delphi long-string constant in the CODE section of Chaves.exe.

Usage: python3 textconst.py re/game/Chaves.exe > re/out/text_constants.txt
Rule (mirrored exactly by src/exetext.c): walking the CODE section in
4-byte steps, a constant starts where the 8 bytes before the text are
refcount -1 and a length 1..512, the text has no control characters other
than tab/CR/LF, and a NUL follows it. The output order is the index the
port uses at runtime.
"""
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(__file__))
from pe import PE  # noqa: E402

pe = PE(sys.argv[1])
name, va, vsize, raw, rsize = pe.sections[0]
d = pe.data
p = raw + 8
end = raw + rsize
out = []
while p + 1 < end:
    if d[p - 8:p - 4] == b'\xff\xff\xff\xff':
        ln = struct.unpack_from('<I', d, p - 4)[0]
        if 0 < ln <= 512 and p + ln < end and d[p + ln] == 0:
            s = d[p:p + ln]
            if all(c >= 0x20 or c in (9, 10, 13) for c in s):
                out.append((pe.off2va(p), s.decode('cp1252', 'replace')))
                p = p + ln + 1
                p += (-p) % 4
                continue
    p += 4
for a, s in out:
    print('%08x\t%s' % (a, s.replace('\t', '\\t').replace('\n', '\\n').replace('\r', '\\r')))
print(len(out), 'constants', file=sys.stderr)
