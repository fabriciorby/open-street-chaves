"""Turn the desenhaanimacoes draw-call dump into a numeric table.

Usage: python3 stageanim.py re/out/draw/stage_animations.txt > docs/spec/07c-stage-animations.txt
Each output row: stage_mask ciclo_lo ciclo_hi list item x y not_char
  stage_mask  bit n set = drawn on stage n (1..12)
  ciclo_lo/hi inclusive range of g_ciclo (blink counter) or 0 255
  list        A = Animacoes item (Anim-(item+1)), E = Efeitos item (Efeito-(item+1))
  item        0-based image-list index
  x, y        screen position before camera: x - camX, y - camY
  not_char    character id that suppresses the element (0 = none)
Only rows whose coordinates are plain constants are emitted; the rest are
reported on stderr for manual transcription.
"""
import re
import sys

CT = {'\\a': 7, '\\b': 8, '\\t': 9, '\\n': 10, '\\v': 11, '\\f': 12, '\\r': 13, '\\0': 0}


def lit(s):
    if s in CT:
        return CT[s]
    if s.startswith('\\x'):
        return int(s[2:], 16)
    return ord(s[-1])


rows = []
skipped = 0
for line in open(sys.argv[1]):
    m = re.match(r'\[(.*)\] (.*)$', line.rstrip('\n'))
    if not m:
        continue
    cond, call = m.groups()
    stages = [lit(x) for x in re.findall(r"DAT_004b44d8 == '([^']+)'", cond)]
    if not stages:
        continue
    mask = 0
    for s in stages:
        mask |= 1 << s
    lo, hi = 0, 255
    # bounds are numeric or char literals ('\b', '\t', '\x0f' ...)
    B = r"(0x[0-9a-f]+|\d+|'[^']+')"
    bnd = lambda v: lit(v[1:-1]) if v.startswith("'") else int(v, 0)
    for a, b in re.findall(B + r" < \(byte\)g_counterD|\(byte\)g_counterD < " + B, cond):
        if a:
            lo = max(lo, bnd(a) + 1)
        elif b:
            hi = min(hi, bnd(b) - 1)
    notc = re.search(r"DAT_004b4(4f8|5a8) != '([^']+)'", cond)
    notc = lit(notc.group(2)) if notc else 0
    c = re.search(r'item\((0x308|0x338),(0x[0-9a-f]+|\d+)\)\},SURFACE,(.*)\);$', call)
    if not c:
        skipped += 1
        print('SKIP', line.strip()[:160], file=sys.stderr)
        continue
    lst = 'E' if c.group(1) == '0x308' else 'A'
    item = int(c.group(2), 0)
    args = c.group(3).replace(' ', '')
    x = re.match(r'\{?(-?\d+|0x[0-9a-f]+)-g_camX\}?,\{0\},\{(-?\d+|0x[0-9a-f]+)-g_counterE\}$', args)
    if not x:
        skipped += 1
        print('SKIP', line.strip()[:200], file=sys.stderr)
        continue
    rows.append((mask, lo, hi, lst, item, int(x.group(1), 0), int(x.group(2), 0), notc))

print('# stage animations (desenhaanimacoes): stage_mask ciclo_lo ciclo_hi list item x y not_char')
for r in rows:
    print('%d %d %d %s %d %d %d %d' % r)
print('%d rows, %d skipped' % (len(rows), skipped), file=sys.stderr)
