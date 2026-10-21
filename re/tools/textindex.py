"""Build docs/spec/07d-text-index.txt: memo line -> index of the Delphi string
constant inside Chaves.exe's CODE section.

Usage: python3 textindex.py re/out/text_constants.txt docs/spec/07a-texts.txt > docs/spec/07d-text-index.txt
The port scans the executable for the same constants at runtime (see
src/exetext.c), so the repository only stores numbers, never the text.
"""
import sys

consts = []  # (addr, text) in file order
for line in open(sys.argv[1], encoding='utf-8'):
    a, _, t = line.rstrip('\n').partition('\t')
    consts.append((int(a, 16), t))
first = {}
for i, (a, t) in enumerate(consts):
    first.setdefault(t, i)

MEMOS = {'Nomes': 0, 'Texto': 1, 'Cenarios': 2, 'Dialogo': 3, 'Finais': 4, 'Frases': 5, 'Config': 6}
rows = []
missing = 0
for line in open(sys.argv[2], encoding='utf-8'):
    if '\t' not in line or line.startswith('#'):
        continue
    key, text = line.rstrip('\n').split('\t', 1)
    memo, idx = key[:-1].split('[')
    if memo not in MEMOS:
        continue
    ci = first.get(text)
    if ci is None:
        missing += 1
        ci = -1
    rows.append((MEMOS[memo], int(idx), ci))

print('# memo line -> string-constant index (0-based, in CODE order). memo: 0 Nomes 1 Texto 2 Cenarios 3 Dialogo 4 Finais 5 Frases 6 Config')
print('# constants: %d, first at 0x%08x, last at 0x%08x' % (len(consts), consts[0][0], consts[-1][0]))
for r in rows:
    print('%d %d %d' % r)
print('%d rows, %d without a constant' % (len(rows), missing), file=sys.stderr)
