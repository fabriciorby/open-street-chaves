"""Extract per-move setup from the move-start routine (0x4775fc).

Usage: python3 movesetup.py sub_4775fc_setMove.c
For each `if ((cVar2 == M) && <conditions>) { ... }` block: move id(s),
allowed previous states, new state, duration, meter gain, sound, flags.
"""
import re, sys
text = open(sys.argv[1]).read()
CT = {'\\a': 7, '\\b': 8, '\\t': 9, '\\n': 10, '\\v': 11, '\\f': 12, '\\r': 13, '\\\\': 92, "\\'": 39, '\\"': 34}
def lit(s):
    if s in CT: return CT[s]
    if s.startswith('\\x'): return int(s[2:], 16)
    return ord(s)
# split into top-level char blocks
parts = re.split(r"\n  if \(\*\(char \*\)\(\(param_1 & 0xff\) \* 0xb0 \+ 0x4b4448\) == '([^']+)'\) \{\n", text)
blocks = [(0, parts[0])] + [(lit(parts[i]), parts[i+1]) for i in range(1, len(parts), 2)]
for cid, blk in blocks:
    print('\n## %s' % ('common' if cid == 0 else 'char %d' % cid))
    # each move block starts with "if ((cVar2 == 'M')" or "if (((cVar2 == 'M') || (cVar2 == 'N'))" or SUB81(dVar7,0) == 'M'
    for m in re.finditer(r"\n(\s*)if \(\(+(?:cVar2|SUB81\(dVar7,0\)) == '([^']+)'\)(?: \|\| \((?:cVar2|SUB81\(dVar7,0\)) == '([^']+)'\))?(.*?)\n\1\}", blk, re.S):
        ind, a, b, body = m.groups()
        ids = str(lit(a)) + ('/%d' % lit(b) if b else '')
        froms = sorted(set(lit(x) for x in re.findall(r"\(&DAT_004b44a3\)\[[^\]]*\] == '([^']+)'", body.split('{', 1)[0])))
        setst = re.search(r"\(&DAT_004b44a3\)\[[^\]]*\] = (0x[0-9a-f]+|\d+);", body)
        dur = re.search(r"0x4b44ae\) = (0x[0-9a-f]+|\d+);", body)
        meter = re.search(r"_DAT_004b5890 \* ([0-9.]+)", body)
        snd = re.search(r"g_sndIndexA = (0x[0-9a-f]+|\d+);", body)
        chk = re.search(r"func_0x00476454\(CONCAT31\([^,]*,(0x[0-9a-f]+|\d+)\)", body)
        extra = []
        if re.search(r"&DAT_004b44b0\)\[[^\]]*\] == '\\0'", body.split('{',1)[0]): extra.append('onGround(+0x78==0)')
        if '0x4b44a2) == ' in body.split('{',1)[0]: extra.append('flag+0x6a==0')
        if 'DAT_004b4478' in body.split('{',1)[0]: extra.append('health/meter cond')
        if 'DAT_004b4490' in body: extra.append('uses +0x58')
        if 'DAT_004b57f8 = ' in body: extra.append('superPause=' + re.search(r"DAT_004b57f8 = (0x[0-9a-f]+|\d+)", body).group(1))
        if 'sub_47648c' in body: extra.append('moveX')
        if 'FUN_00476a04' in body: extra.append('spawnEffect')
        print('  move %-5s from %-22s -> state %-4s dur %-4s meter x%-3s snd %-4s %s %s' % (
            ids, ','.join(map(str, froms)) or '-', setst.group(1) if setst else '-', dur.group(1) if dur else '-',
            meter.group(1) if meter else '-', snd.group(1) if snd else '-', 'needs%d' % int(chk.group(1), 0) if chk else '', ' '.join(extra)))
