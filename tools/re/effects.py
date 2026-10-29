"""Extract the effects table from desenhaefeitos (0x48edfc) and the spawn
initialisation from FUN_00476a04.

Usage: python3 effects.py desenhaefeitos.c all.pp.c
Record fields are addressed relative to rec+0x2b (`pcVar9`/`pcVar4`):
  [0]=type [1]=phase [2]=life [3]=sprite [4],[5] counters [6]=hasHitbox
  [-0xb]=dir [-10]=speed [-9]=damage [-8]=param [-7]=owner [-6]=hostile
  [-3]=done [-2],[-1]=flags; doubles at [-0x2b]=x [-0x23]=y
"""
import re
import sys

CT = {'\\a': 7, '\\b': 8, '\\t': 9, '\\n': 10, '\\v': 11, '\\f': 12, '\\r': 13, '\\\\': 92, "\\'": 39, '\\"': 34, '\\0': 0}
FIELD = {'0': 'type', '1': 'phase', '2': 'life', '3': 'sprite', '4': 'c4', '5': 'c5', '6': 'hasHitbox', '7': 'c7', '8': 'f8', '9': 'f9',
         '-0xb': 'dir', '-10': 'speed', '-9': 'damage', '-8': 'param', '-7': 'owner', '-6': 'hostile', '-3': 'done', '-2': 'flagA', '-1': 'flagB',
         '-0xffffffff00000005': 'hb_dx', '-0xffffffff00000004': 'hb_dy'}


def lit(s):
    if s in CT:
        return CT[s]
    if s.startswith('\\x'):
        return int(s[2:], 16)
    return ord(s)


def val(s):
    s = s.strip()
    if s.startswith("'"):
        return lit(s[1:-1])
    m = re.match(r'\(char\)uVar\d+ \+ \'([^\']+)\'', s)
    if m:
        return 'rand+%d' % lit(m.group(1))
    if s.startswith('(&DAT_004b44a5)'):
        return 'ownerFacing'
    try:
        v = int(s, 0)
        return v - 256 if v > 127 and v < 256 else v
    except ValueError:
        return s


def logical_lines(text):
    out, buf = [], ''
    for raw in text.split('\n'):
        s = raw.strip()
        if not s:
            continue
        buf = (buf + ' ' + s) if buf else s
        if buf.startswith('if (') and not buf.endswith('{'):
            continue
        if not buf.startswith('if (') and not buf.endswith((';', '{', '}')):
            continue
        out.append(buf)
        buf = ''
    return out


def section(text, addr):
    m = re.search(r'// ===== %s.*?(?=// ===== |\Z)' % addr, text, re.S)
    return m.group(0)


# --- desenhaefeitos ---------------------------------------------------------
de = open(sys.argv[1]).read()
stack = []
rows = {}


def ctx():
    c = {}
    for e in stack:
        c.update({k: v for k, v in e.items() if v is not None})
    return c


for ln in logical_lines(de):
    if ln.endswith('{'):
        e = {}
        if ln.startswith('if ('):
            m = re.search(r"\*pcVar9 == '([^']+)'", ln)
            if m:
                e['type'] = lit(m.group(1))
            m = re.search(r"pcVar9\[1\] == '([^']+)'", ln)
            if m:
                e['phase'] = lit(m.group(1))
            m = re.search(r"\(byte\)pcVar9\[1\] < (0x[0-9a-f]+|\d+)", ln)
            if m:
                e['phase'] = '<%d' % int(m.group(1), 0)
            m = re.search(r"(0x[0-9a-f]+|\d+) < \(byte\)pcVar9\[1\]", ln)
            if m:
                e['phase'] = '>%d' % int(m.group(1), 0)
        stack.append(e)
        continue
    if ln.startswith('}'):
        if stack:
            stack.pop()
        if ln.endswith('{'):
            stack.append({})
        continue
    c = ctx()
    if 'type' not in c:
        continue
    key = (c['type'], c.get('phase', '*'))
    m = re.match(r"pcVar9\[3\] = '([^']+)';", ln)
    if m:
        rows.setdefault(c['type'], []).append((key[1], 'sprite=%d' % lit(m.group(1))))
        continue
    m = re.match(r"pcVar9\[([-0-9a-fx]+)\] = (.*);", ln)
    if m:
        rows.setdefault(c['type'], []).append((key[1], '%s=%s' % (FIELD.get(m.group(1), m.group(1)), val(m.group(2)))))
        continue
    if not ln.startswith(('uVar', 'cVar', 'bVar', 'iVar', 'dVar', 'puVar', 'piVar', 'pdVar', 'fVar', 'in_stack', 'cStack', 'uStack')):
        rows.setdefault(c['type'], []).append((key[1], ln[:140]))

print('# desenhaefeitos: per effect type, per phase: sprite index and actions')
for t in sorted(rows):
    print('\n## effect type %d' % t)
    for ph, what in rows[t]:
        print('  phase %-4s %s' % (ph, what))

# --- spawn init ----------------------------------------------------------------
allc = open(sys.argv[2]).read()
sp = section(allc, '00476a04')
print('\n\n# FUN_00476a04 spawn(type, variant, owner, y, x): per-type initial fields')
cur = None
for ln in logical_lines(sp):
    m = re.search(r"\*pcVar4 == '([^']+)'", ln)
    if m and ln.startswith('if ('):
        types = [lit(x) for x in re.findall(r"\*pcVar4 == '([^']+)'", ln)]
        cur = '/'.join(map(str, types))
        print('\n## type %s' % cur)
        continue
    if cur is None:
        continue
    m = re.match(r"pcVar4\[([-0-9a-fx]+)\] = (.*);", ln)
    if m:
        print('  %s = %s' % (FIELD.get(m.group(1), m.group(1)), val(m.group(2))))
        continue
    m = re.match(r"FUN_004769c4\(iVar3,(.*)\);", ln)
    if m:
        v = [val(x) for x in m.group(1).split(',')]
        v = [x - (1 << 32) if isinstance(x, int) and x > 0x7fffffff else x for x in v]
        print('  hitbox l=%s t=%s r=%s b=%s' % (v[0], v[1], v[3], v[2]))
        continue
    if 'LStrCmp' in ln or 'Random' in ln:
        print('  ' + ln[:100])
