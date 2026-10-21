"""Extract the input-sequence state machine from TForm1_sequenciagolpe.

Usage: python3 seqtables.py tform1.snap.c
For every `TForm1_sequencia(p, from, to)` call, reports the seta() direction
tests and extra conditions that guard it, grouped by character id.
"""
import re
import sys

text = open(sys.argv[1]).read()
m = re.search(r'// ===== 004968b0.*?(?=// ===== 00499ea4)', text, re.S)
body = m.group(0)
lines = body.split('\n')

CTRL = {'\\a': 7, '\\b': 8, '\\t': 9, '\\n': 10, '\\v': 11, '\\f': 12, '\\r': 13}


def lit(s):
    if s in CTRL:
        return CTRL[s]
    if s.startswith('\\x'):
        return int(s[2:], 16)
    return ord(s)


cur = None
rows = {}
for i, line in enumerate(lines):
    g = re.search(r"\* 0xb0 \+ -0xa0\] == '([^']+)'\) \{\s*$", line)
    if g and line.startswith('  if ('):
        cur = lit(g.group(1))
        continue
    c = re.search(r'TForm1_sequencia\([^,]*,[^,]*,(\d+),(\d+)\);', line)
    if not c:
        continue
    # Walk back to the start of this if-chain: stop at a line with lower
    # indentation than the first `if` above the call.
    j = i - 1
    conds = []
    last_dir = None
    while j > 0:
        l = lines[j]
        ind = len(l) - len(l.lstrip())
        if l.strip() == '}' and ind <= 4:
            break
        # Walking backwards: the `if (cVar2 ...)` test comes first, then the
        # seta("DIR") call that produced cVar2.
        t = re.search(r"cVar2 ([!=]=) '([^']*)'", l)
        if t:
            last_dir = (t.group(1) == '!=' and t.group(2) == '\\0') or (t.group(1) == '==' and t.group(2) == '\\x01')
        s = re.search(r'TForm1_seta\([^"]*"(\w+)"\)', l)
        if s and last_dir is not None:
            conds.append(('' if last_dir else '!') + s.group(1))
            last_dir = None
        if '-0x16]' in l and '<' in l:
            conds.append('downHold>%d' % int(re.search(r'\((\d+|0x[0-9a-f]+) <', l).group(1), 0))
        if '-0x15]' in l and '<' in l:
            conds.append('backHold>%d' % int(re.search(r'\((\d+|0x[0-9a-f]+) <', l).group(1), 0))
        if 'g_4b4498' in l:
            conds.append('onGround')
        if '-0x38]' in l:
            conds.append('airborne' if "!= '\\0'" in l else 'grounded')
        if s and ind <= 4:
            break
        j -= 1
    rows.setdefault(cur, []).append((' '.join(reversed(conds)), int(c.group(1)), int(c.group(2))))

for cid in sorted(rows, key=lambda k: (k is None, k)):
    print('\n## char %s' % cid)
    seen = set()
    for cond, a, b in rows[cid]:
        key = (cond, a, b)
        if key in seen:
            continue
        seen.add(key)
        print('  %-40s %2d -> %d' % (cond, a, b))

# --- golpe: (button, condition) -> move id ---------------------------------
g = re.search(r'// ===== 004951ec.*?(?=// ===== 004968b0)', text, re.S).group(0)
glines = g.split('\n')
cur = None
print('\n# golpe: button 1=LP 2=LK 3=HP 4=HK; seq = players[p].+0x98 (sequence state)')
for i, line in enumerate(glines):
    gg = re.search(r"\* 0xb0 \+ -0xa0\] == '([^']+)'\) \{\s*$", line)
    if gg:
        cur = lit(gg.group(1))
        print('\n## char %d' % cur)
        continue
    c = re.search(r'func_0x004775fc\(param_2,(0x[0-9a-f]+)\);', line)
    if not c:
        continue
    cond = glines[i - 1]
    if 'if (' not in cond:
        cond = glines[i - 2] + cond
    btn = re.search(r"param_3 == '\\x0(\d)'", cond)
    st = re.search(r"-0x18\] == '([^']+)'", cond)
    parts = []
    if st:
        parts.append('seq=%d' % lit(st.group(1)))
    if '-0x13]' in cond:
        parts.append('backHold>%d' % int(re.search(r'\((\d+) < \(byte\)[^\n]*-0x13', cond).group(1)))
    if '-0x14]' in cond:
        parts.append('downHold>%d' % int(re.search(r'\((\d+) < \(byte\)[^\n]*-0x14', cond).group(1)))
    if '-0x38]' in cond:
        parts.append('airborne' if "!= '\\0'" in cond else 'grounded')
    print('  btn%s %-24s -> move %d' % (btn.group(1) if btn else '?', ' '.join(parts), int(c.group(1), 16)))
