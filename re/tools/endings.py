"""Build docs/spec/07e-endings.txt: the state-10 ending scripts as numbers.

Usage: python3 endings.py <objdump of 0x4ab903..0x4ad760> docs/spec/07b-draw-calls/state10_endings.txt Chaves.exe
Pairs the k-th call of each helper in the decompiled dump (which keeps the
conditions) with the k-th call in the assembly (which keeps the arguments).
"""
import re, struct, sys

asm = open(sys.argv[1]).read()
asm = re.sub(r'<CODE\+0x[0-9a-f]+>', '', asm)
asm = re.sub(r'\s+# imm = 0x[0-9A-F]+', '', asm)
dump = open(sys.argv[2], encoding='utf-8').read().splitlines()
exe = open(sys.argv[3], 'rb').read()
pe = struct.unpack_from('<I', exe, 0x3c)[0]
nsec = struct.unpack_from('<H', exe, pe + 6)[0]; opt = struct.unpack_from('<H', exe, pe + 20)[0]
base = struct.unpack_from('<I', exe, pe + 24 + 28)[0]
secs = []
for i in range(nsec):
    o = pe + 24 + opt + i * 40
    vs, va, rs, ro = struct.unpack_from('<IIII', exe, o + 8)
    secs.append((base + va, vs, rs, ro))
def off(va):
    for s, vs, rs, ro in secs:
        if s <= va < s + max(vs, rs): return va - s + ro
def dstr(va):
    o = off(va); ln = struct.unpack_from('<I', exe, o - 4)[0]
    return exe[o:o + ln].decode('latin1')

# ---- assembly: ordered list of (func, args)
lines = [l for l in asm.splitlines() if re.match(r'\s*[0-9a-f]+:', l)]
ins = []
for l in lines:
    m = re.match(r'\s*([0-9a-f]+):\s+(\S+)\s*(.*)', l)
    ins.append((m.group(1), m.group(2), m.group(3).strip()))
def imm(s):
    m = re.match(r'\$(-?0x[0-9a-f]+|-?\d+)', s)
    return int(m.group(1), 0) if m else None
regs = {}; pushes = []; acalls = []
for addr, op, args in ins:
    if op in ('movl', 'movw', 'movb') and ',' in args:
        src, dst = [a.strip() for a in args.rsplit(',', 1)]
        if dst in ('%eax', '%ax', '%al'): regs['a'] = src
        elif dst in ('%edx', '%dx', '%dl'): regs['d'] = src
        elif dst in ('%ecx', '%cx', '%cl'): regs['c'] = src
    elif op == 'xorl' and args in ('%eax, %eax', '%edx, %edx', '%ecx, %ecx'): regs[args[1]] = '$0x0'
    elif op.startswith('push'): pushes.append(args)
    elif op == 'calll':
        t = args.split()[0]
        a = dict(regs); p = list(pushes); pushes = []
        if t == '0x490bb8': acalls.append(('FIGHTER', dict(p=imm(a['d']), x=imm(a['c']), y=imm(p[0]), state=imm(p[1]), timer=imm(p[2]))))
        elif t == '0x4908a8': acalls.append(('LOADSTAGE', dict(name=dstr(imm(a['d'])))))
        elif t == '0x48ea84': acalls.append(('LOADCHAR', dict(ch=imm(a['c']), slot=imm(p[0]))))
        elif t == '0x40d760': acalls.append(('RECT', dict(l=imm(a['a']), t=imm(a['d']), r=imm(a['c']), b=imm(p[0]))))
        elif t == '0x4616c0': acalls.append(('STRETCH', dict(alpha=imm(p[1]) if len(p) > 1 else 255)))
        elif t == '0x461dcc': acalls.append(('GETITEM', dict(idx=imm(a['d']) if a.get('d') and a['d'].startswith('$') else None, push=[imm(x) for x in p])))
        elif t == '0x46161c': acalls.append(('DRAW', dict(x=imm(a['c']))))
        elif t == '0x48e444': acalls.append(('BARRA', dict(x=imm(a['d']), y=imm(a['c']), push=[imm(x) for x in p], color=a.get('a'))))
        elif t == '0x406de4': acalls.append(('RGB', dict(r=a.get('a'), g=a.get('d'), b=a.get('c'))))
        elif t == '0x476a04': acalls.append(('SPAWN', dict(type=imm(a['a']))))
        elif t == '0x402a1c': acalls.append(('RANDOM', dict(n=imm(a['a']))))
# fold GETITEM+DRAW into DRAW(list idx x y)
folded = []
for i, (f, a) in enumerate(acalls):
    if f == 'DRAW':
        g = acalls[i - 1]
        folded.append(('DRAW', dict(idx=g[1]['idx'], y=g[1]['push'][0] if g[1]['push'] else None, x=a['x'])))
    elif f == 'GETITEM':
        continue
    else:
        folded.append((f, a))
acalls = folded

# ---- dump: ordered list of (func, cond)
FUNC = {'TForm1_desenhalutador': 'FIGHTER', 'TForm1_carregarcenario': 'LOADSTAGE', 'TForm1_carregarlutador': 'LOADCHAR',
        'DXImageListItem_Draw': 'DRAW', 'func_0x004616c0': 'STRETCH', 'TForm1_barra': 'BARRA', 'FUN_00476a04': 'SPAWN'}
dcalls = []
for l in dump:
    m = re.match(r'\[(.*)\]\s+(\w+)\(', l)
    if not m: continue
    cond, fn = m.groups()
    if fn not in FUNC: continue
    if 'g_state == 10' not in cond: continue
    ch = re.search(r"g_counterD\._2_1_ == '(.*?)'", cond)
    if not ch: continue
    t = ch.group(1)
    ch = {'\\a': 7, '\\b': 8, '\\t': 9, '\\n': 10, '\\v': 11, '\\f': 12, '\\r': 13}.get(t) or (int(t[2:], 16) if t.startswith('\\x') else ord(t))
    rest = cond.split("g_counterD._2_1_ == ")[1]
    rest = rest[rest.find(')') + 1:]
    dcalls.append((FUNC[fn], ch, rest.strip(' &')))
# pair per function kind
by = {}
for f, a in acalls: by.setdefault(f, []).append(a)
used = {k: 0 for k in by}
rows = []
for f, ch, cond in dcalls:
    if f not in by or used[f] >= len(by[f]): rows.append((ch, cond, f, None)); continue
    rows.append((ch, cond, f, by[f][used[f]])); used[f] += 1

def rng(cond):
    lo, hi = 0, 2200
    exact = re.findall(r"\(short\)g_counterC == (0x[0-9a-f]+|\d+)", cond)
    if exact: v = int(exact[0], 0); return v, v, ''
    for m in re.finditer(r"(0x[0-9a-f]+|\d+) < \(short\)g_counterC", cond): lo = max(lo, int(m.group(1), 0) + 1)
    for m in re.finditer(r"\(short\)g_counterC < (0x[0-9a-f]+|\d+)", cond): hi = min(hi, int(m.group(1), 0) - 1)
    extra = ''
    if 'Random(' in cond:
        m = re.search(r'Random\((0x[0-9a-f]+|\d+)\)', cond); extra = 'P1/%d' % int(m.group(1), 0)
    if '||' in cond: extra += ' OR'
    return lo, hi, extra

out = ['# 07e — ending scripts (state 10), transcribed from the code: numbers only.',
       '# Scene viewport (50,50)-(350,220) inside black bars; ground y = 210; stage drawn at (-80,-100);',
       '# camX = 0. The counter c runs 0..2200; a button jumps c to the end of the current 100-tick block',
       '# (c in (i*100+5, i*100+85) -> i*100+85). Dialogue: 11 segments of 200 ticks; segment k uses',
       '# MemoDialogo line base+4k (speaker tag "-" none, "A".."O" = character 1..15 -> Retrato[(n-1)*5]',
       '# at (20,222)) and lines base+4k+1..3 at (80,228) (80,245) (80,262), size 12, white, left.',
       '# BASE per character: ' + ' '.join(str(b) for b in [0,44,88,125,166,210,254,298,342,386,430,474,518,562,606]),
       '# ops: STAGE n | CHAR slot n | FIGHTER slot x y state timer | SHADOW l t r b (Efeitos[0] alpha 70)',
       '#      | FINAL idx x y | BLACK (whole screen) | FLASH (viewport filled with RGB(255,rnd,rnd)) | SPAWN type',
       '# cond: from to [P1/n = only when Random(n)==0] [OR = two ranges, see dump]',
       '#', '# ch from to op ...']
for ch, cond, f, a in rows:
    lo, hi, extra = rng(cond)
    if a is None: s = f + ' ?'
    elif f == 'FIGHTER': s = 'FIGHTER %d %d %d %d %d' % (a['p'], a['x'], a['y'], a['state'], a['timer'])
    elif f == 'LOADSTAGE': s = 'STAGE %d' % int(re.search(r'(\d+)', a['name']).group(1))
    elif f == 'LOADCHAR': s = 'CHAR %d %d' % (a['slot'], a['ch'])
    elif f == 'DRAW': s = 'FINAL %s %s %s' % (a['idx'], a['x'], a['y'])
    elif f == 'STRETCH':
        # the RECT right before this stretch in the assembly order
        s = 'SHADOW ?'
    elif f == 'BARRA':
        s = 'BLACK' if a['x'] == 0 and a['y'] == 0 else 'FLASH'
    elif f == 'SPAWN': s = 'SPAWN %d' % a['type']
    out.append('%d %d %d %s %s' % (ch, lo, hi, s, extra))
# shadows: attach rects in order
rects = [a for f, a in acalls if f == 'RECT']
ri = 0
for i, l in enumerate(out):
    if 'SHADOW ?' in l and ri < len(rects):
        r = rects[ri]; ri += 1
        out[i] = l.replace('SHADOW ?', 'SHADOW %d %d %d %d' % (r['l'], r['t'], r['r'], r['b']))
print('\n'.join(out))
print('# asm calls used: ' + ', '.join('%s %d/%d' % (k, used[k], len(v)) for k, v in by.items()), file=sys.stderr)
