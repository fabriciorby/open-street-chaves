"""Build docs/spec/06b-spawn-offsets.txt: where each effect spawns relative to its owner.

Usage: python3 spawnoffsets.py <objdump -d of Chaves.exe> Chaves.exe src/gen/tables_gen.c
Every FUN_00476a04(type, variant, owner, y, x) call in the frame selector computes
x = owner.x +/- (dx + Random(rx)) and y = owner.y + dy + Random(ry); the sign follows the
facing (the code has one call per facing). Combos are assigned to the characters that
spawn that type, in order.
"""
import re, struct, sys
asm = open(sys.argv[1]).read(); asm = re.sub(r'<CODE\+0x[0-9a-f]+>', '', asm); asm = re.sub(r'\s+# imm = 0x[0-9A-F]+', '', asm)
exe = open(sys.argv[2], 'rb').read()
pe = struct.unpack_from('<I', exe, 0x3c)[0]; nsec = struct.unpack_from('<H', exe, pe + 6)[0]; opt = struct.unpack_from('<H', exe, pe + 20)[0]
base = struct.unpack_from('<I', exe, pe + 24 + 28)[0]; secs = []
for i in range(nsec):
    o = pe + 24 + opt + i * 40; vs, va, rs, ro = struct.unpack_from('<IIII', exe, o + 8); secs.append((base + va, vs, rs, ro))
def off(va):
    for s, vs, rs, ro in secs:
        if s <= va < s + max(vs, rs): return va - s + ro
def f32(va): return struct.unpack_from('<f', exe, off(va))[0]
def imm(s):
    m = re.match(r'\$(-?0x[0-9a-f]+|-?\d+)', s); return int(m.group(1), 0) if m else None
ins = []
for l in asm.splitlines():
    m = re.match(r'\s*([0-9a-f]+):\s+(\S+)\s*(.*)', l)
    if m: ins.append((int(m.group(1), 16), m.group(2), m.group(3).strip()))
sites = [i for i, (a, op, ar) in enumerate(ins) if op == 'calll' and ar.startswith('0x476a04') and 0x477000 <= a < 0x48e000]
def parse(expr):
    """tokens (in execution order) -> (base 'X'/'Y', const, random)"""
    const = 0.0; rnd = 0; basev = None; sign = 1
    for t in expr:
        if t in ('X', 'Y'): basev = t
        elif t[0] in '+-' and t[1:].replace('.', '').isdigit(): const += float(t)
        elif t.startswith('R'): rnd = max(rnd, int(t[1:]))
    return basev, const, rnd
rows = []
for si in sites:
    j = si - 1; typ = None; exprs = []; cur = []
    while j > 0 and len(exprs) < 2 and si - j < 70:
        a, op, ar = ins[j]
        if op == 'calll' and not ar.startswith('0x402a1c') and not ar.startswith('0x402894'): break
        if op == 'movb' and ar.endswith('%al') and typ is None: typ = imm(ar.split(',')[0])
        if op == 'fstpl' and '(%esp)' in ar:
            if cur: exprs.append(cur)
            cur = []
        elif op == 'fldl' and ('-0xb0(' in ar or '-0xa8(' in ar): cur.append('X' if '-0xb0' in ar else 'Y')
        elif op in ('fadds', 'fsubs') and ar.startswith('0x'): cur.append(('+' if op == 'fadds' else '-') + ('%g' % f32(int(ar, 16))))
        elif op in ('faddl', 'fsubl') and ('-0xb0(' in ar or '-0xa8(' in ar): cur.append(('+' if op == 'faddl' else '-') + ('X' if '-0xb0' in ar else 'Y'))
        elif op == 'movl' and re.match(r'\$0x[0-9a-f]+, %eax', ar) and ins[j + 1][1] == 'calll' and ins[j + 1][2].startswith('0x402a1c'): cur.append('R%d' % imm(ar.split(',')[0]))
        j -= 1
    if cur and len(exprs) < 2: exprs.append(cur)
    exprs = [list(reversed(e)) for e in exprs]
    x = y = None
    for e in exprs:
        b, c, r = parse(e)
        if b == 'X': x = (c, r)
        elif b == 'Y': y = (c, r)
    rows.append((ins[si][0], typ, x or (0.0, 0), y or (0.0, 0)))
# collapse facing pairs: consecutive same type with mirrored x
combos = {}
prev = None
for addr, typ, x, y in rows:
    key = (typ, abs(x[0]), x[1], y[0], y[1])
    if prev and prev[0] == typ and abs(prev[1]) == abs(x[0]) and prev[2] == y:
        prev = None; continue
    combos.setdefault(typ, [])
    if not combos[typ] or combos[typ][-1] != key[1:]: combos[typ].append(key[1:])
    prev = (typ, x[0], y)
# characters per type, from the generated spawn table
gen = open(sys.argv[3]).read()
tbl = re.search(r'osc_fx_spawns\[\d+\] = \{(.*?)\};', gen, re.S).group(1)
spawns = [tuple(int(v) for v in m.groups()) for m in re.finditer(r'\{(\d+), (\d+), (-?\d+), (\d+), (\d+)\}', tbl)]
chars_by_type = {}
for ch, st, tm, typ, var in spawns:
    chars_by_type.setdefault(typ, [])
    if ch not in chars_by_type[typ]: chars_by_type[typ].append(ch)
print('# 06b — spawn offsets per (character, effect type), from the frame selector code [objdump].')
print('# x = owner.x +/- (dx + Random(rx)) towards the facing, y = owner.y + dy + Random(ry).')
print('# ch type dx rx dy ry')
for typ, chars in sorted(chars_by_type.items()):
    cs = combos.get(typ, [])
    for i, ch in enumerate(chars):
        c = cs[min(i, len(cs) - 1)] if cs else (0, 0, 0, 0)
        dx, rx, dy, ry = c
        print('%d %d %d %d %d %d' % (ch, typ, round(dx), rx, round(dy), ry))
print('# combos found: ' + '; '.join('%s:%d' % (k, len(v)) for k, v in sorted(combos.items())), file=sys.stderr)
