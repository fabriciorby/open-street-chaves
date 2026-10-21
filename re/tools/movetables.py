"""Extract input-sequence and attack tables from the decompiled game code.

Usage: python3 movetables.py tform1.snap.c
Prints, per character id:
  * sequenciagolpe: condition on seta() results -> sequencia(from, to)
  * golpe: (button, sequence state) -> move id
The decompiled text is treated as data; this only pattern-matches it.
"""
import re
import sys

text = open(sys.argv[1]).read()


def section(addr_from, addr_to):
    m = re.search(r'// ===== %s.*?(?=// ===== %s)' % (addr_from, addr_to), text, re.S)
    return m.group(0)


def char_blocks(body):
    """Split a function body on `if (players[p].charId == N) {` guards."""
    parts = re.split(r"\n  if \([^\n]*\* 0xb0 \+ -0xa0\] == '([^']+)'\) \{\n", body)
    out = {}
    for i in range(1, len(parts), 2):
        out[decode_char(parts[i])] = parts[i + 1]
    return out


def decode_char(lit):
    table = {'\\a': 7, '\\b': 8, '\\t': 9, '\\n': 10, '\\v': 11, '\\f': 12, '\\r': 13}
    if lit in table:
        return table[lit]
    if lit.startswith('\\x'):
        return int(lit[2:], 16)
    return ord(lit)


# --- sequenciagolpe -------------------------------------------------------
seq = section('004968b0', '00499ea4')
print('# sequenciagolpe: direction counters')
print('# players[p].+0x9a (downHold) ++ while BAIXO held (max 18), -- otherwise')
print('# players[p].+0x9b (backHold) ++ while TRAS held (max 18), -- otherwise')
for cid, blk in sorted(char_blocks(seq).items()):
    print('\n## char %d' % cid)
    # Walk statements: collect seta() tests preceding each sequencia() call.
    for m in re.finditer(r'((?:\s*cVar2 = TForm1_seta\([^"]*"(\w+)"\);\n\s*if \(\(?\(?cVar2 ([!=]=) \'\\?x?0?\'?\)?[^\n]*\n)+)\s*TForm1_sequencia\([^,]*,[^,]*,(\d+),(\d+)\);', blk):
        conds = re.findall(r'"(\w+)"\);\n\s*if \(\(?\(?cVar2 ([!=]=) \'(\\?x?0?1?)\'', m.group(1))
        cs = []
        for d, op, val in conds:
            held = (op == '!=' and val in ('\\0', '0')) or (op == '==' and val in ('\\x01', '1'))
            cs.append(('' if held else '!') + d)
        extra = ''
        if '0x9a' in m.group(1) or '-0x16]' in m.group(1):
            extra += ' downHold>9'
        if 'g_4b4498' in m.group(1):
            extra += ' onGround'
        print('  %-28s %s -> %s%s' % (' '.join(cs), m.group(4), m.group(5), extra))

# --- golpe ---------------------------------------------------------------
golpe = section('004951ec', '004968b0')
print('\n# golpe: (button, seqState) -> move id   (button 1=LP 2=LK 3=HP 4=HK)')
for cid, blk in sorted(char_blocks(golpe).items()):
    rows = []
    for m in re.finditer(r"if \(\(\(?param_3 == '\\x0(\d)'\) && \(([^\n]*?)\)\)?(?: &&\s*\(([^\n]*)\))?\) \{\n\s*func_0x004775fc\(param_2,(0x[0-9a-f]+)\);", blk):
        btn, cond, cond2, mv = m.groups()
        st = re.search(r"-0x18\] == '([^']+)'", cond)
        if st:
            c = 'seq=%d' % decode_char(st.group(1))
        elif '-0x13]' in cond:
            c = 'backHold>5'
        elif '-0x14]' in cond:
            c = 'downHold>5'
        else:
            c = cond
        if cond2:
            c += ' & airborne' if '-0x38]' in cond2 else ' & ' + cond2
        rows.append((int(btn), c, int(mv, 16)))
    print('\n## char %d' % cid)
    for btn, c, mv in rows:
        print('  btn%d %-18s -> move %d' % (btn, c, mv))
