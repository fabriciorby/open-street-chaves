"""Extract the animation table from the frame selector (0x47d21c).

Usage: python3 animtables.py sub_47d21c.c
Walks the pseudocode line by line, tracking nested `if` conditions, and
reports per character:
  state S: timer->frame ...               players[p].frame = base + F
  hurt frame F: left,bottom,right,top     (sub_476870)
  hit  state,timer,p4,dmg,level,atk,bottom,right,top,left  (FUN_004768f8)
  other statements executed under a (state, timer) condition (raw)
"""
import re
import sys

text = open(sys.argv[1]).read()
CT = {'\\a': 7, '\\b': 8, '\\t': 9, '\\n': 10, '\\v': 11, '\\f': 12, '\\r': 13, '\\\\': 92, "\\'": 39, '\\"': 34}


def lit(s):
    if s in CT:
        return CT[s]
    if s.startswith('\\x'):
        return int(s[2:], 16)
    return ord(s)


def num(s):
    s = s.strip()
    if s.startswith("'"):
        return lit(s[1:-1])
    if s.startswith('-'):
        return -num(s[1:])
    v = int(s, 0)
    return v - (1 << 32) if v >= 0x80000000 else v


# Join continuation lines so each statement / if-header is one logical line.
lines = []
buf = ''
for raw in text.split('\n'):
    s = raw.strip()
    if not s:
        continue
    buf = (buf + ' ' + s) if buf else s
    if buf.startswith('if (') and not buf.endswith('{'):
        continue
    if not buf.startswith('if (') and not buf.endswith((';', '{', '}', ':')):
        continue
    lines.append(buf)
    buf = ''

stack = []  # one dict per open brace: {char, state, timer}
frames, boxes, other = {}, {}, {}


def ctx():
    c = {'char': 0, 'state': None, 'timer': None}
    for e in stack:
        for k in ('char', 'state', 'timer'):
            if e.get(k) is not None:
                c[k] = e[k]
    return c


for ln in lines:
    if ln.endswith('{'):
        e = {}
        if ln.startswith('if ('):
            m = re.search(r"0x4b4448\) == '([^']+)'", ln)
            if m:
                e['char'] = lit(m.group(1))
            m = re.search(r"\(&DAT_004b44a3\)\[[^\]]*\] == '([^']+)'", ln)
            if m:
                e['state'] = lit(m.group(1))
            m = re.search(r"\(&DAT_004b44ac \+ [^=]*?\) == (0x[0-9a-f]+|\d+)", ln)
            if m:
                e['timer'] = int(m.group(1), 0)
        stack.append(e)
        continue
    if ln.startswith('}'):
        if stack:
            stack.pop()
        if ln.endswith('{'):
            stack.append({})
        continue
    c = ctx()
    m = re.match(r"\(&DAT_004b44a4\)\[[^\]]*\] = \w+ \+ (?:'((?:\\.|[^'\\])+)'|(0x[0-9a-f]+|\d+));", ln)
    if m and c['state'] is not None:
        val = lit(m.group(1)) if m.group(1) is not None else int(m.group(2), 0)
        frames.setdefault(c['char'], {}).setdefault(c['state'], {})[c['timer'] if c['timer'] is not None else '*'] = val
        continue
    m = re.match(r"func_0x00476870\(param_1 & 0xff,\([^)]*\) \+ (0x[0-9a-f]+|\d+),([^;]*)\);", ln)
    if m:
        boxes.setdefault(c['char'], []).append('hurt frame %2d: %s' % (int(m.group(1), 0), ','.join(str(num(x)) for x in m.group(2).split(','))))
        continue
    m = re.match(r"FUN_004768f8\(param_1 & 0xff,([^;]*)\);", ln)
    if m:
        boxes.setdefault(c['char'], []).append('hit  %s' % ','.join(str(num(x)) for x in m.group(1).split(',')))
        continue
    if c['state'] is not None and not ln.startswith(('uVar', 'cVar', 'bVar', 'iVar', 'dVar', 'in_stack', 'return', 'cStack', 'uStack')):
        other.setdefault(c['char'], []).append('state %2d timer %-3s: %s' % (c['state'], c['timer'] if c['timer'] is not None else '*', ln[:160]))

for ch in sorted(set(frames) | set(boxes) | set(other)):
    print('\n## char %d' % ch)
    for st in sorted(frames.get(ch, {})):
        d = frames[ch][st]
        items = sorted(((k, v) for k, v in d.items() if k != '*'))
        if '*' in d:
            items.append(('*', d['*']))
        print('  state %2d: %s' % (st, ' '.join('%s->%d' % (k, v) for k, v in items)))
    for b in boxes.get(ch, []):
        print('  ' + b)
    for o in other.get(ch, []):
        print('  ' + o)
