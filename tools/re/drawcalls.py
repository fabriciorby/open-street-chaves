"""Dump drawing calls of a pseudocode region with their guarding conditions.

Usage: python3 drawcalls.py <file.c> <first_line> <last_line> [--all]
Prints one line per drawing/helper call: the stack of enclosing `if`
conditions (compressed) followed by the call with its arguments. Numeric
arguments are kept verbatim; decompiler noise (CONCAT31, extraout_*) is
stripped. With --all every statement is printed, not only drawing calls.
"""
import re
import sys

DRAW = ('TForm1_barra(', 'TForm1_barraredonda(', 'TForm1_retangulo(', 'TForm1_circulo(',
        'TForm1_circulopintado(', 'TForm1_texto(', 'DXImageListItem_Draw(', 'func_0x004616c0(',
        'func_0x00461670(', 'func_0x0046161c(', 'TForm1_desenhalutador(', 'TForm1_mostrasequencia(',
        'TForm1_desenhaefeitos(', 'TForm1_desenhaanimacoes(', 'TForm1_seta(', 'FUN_00476a04(',
        'TForm1_carregar', 'FUN_00449cac(', 'FUN_00449e68(', 'TForm1_zerardados(', 'sub_4775fc_setMove(')

path, a, b = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
show_all = '--all' in sys.argv
lines = open(path).read().split('\n')[a - 1:b]

# join continuation lines
logical = []
buf = ''
for raw in lines:
    s = raw.strip()
    if not s:
        continue
    buf = (buf + ' ' + s) if buf else s
    if buf.startswith(('if (', 'else if (', '} else if (', 'while (', 'do {', '} while')) and not buf.endswith(('{', ';')):
        continue
    if not buf.startswith(('if (', 'while', 'do', '}')) and not buf.endswith((';', '{', '}')):
        continue
    logical.append(re.sub(r'((?:TForm1_|DXImageListItem_|func_0x|FUN_|sub_)\w+)\s+\(', r'\1(', buf))
    buf = ''


def clean(s):
    s = re.sub(r'CONCAT31\(\(int3\)\([^)]*\) >> 8\),', '', s)
    s = re.sub(r'CONCAT31\(0x4b[0-9a-f]{2},', '', s)
    s = re.sub(r'\(int3\)\(\(uint\)\w+ >> 8\)', '', s)
    s = re.sub(r'extraout_\w+', '?', s)
    s = re.sub(r'\*\(undefined4 \*\)\(\*\(int \*\)\(param_1 \+ 0x2d0\) \+ 0x270\)', 'SURFACE', s)
    s = re.sub(r'\*\(int \*\)\(\*\(int \*\)\(param_1 \+ (0x[0-9a-f]+)\) \+ 0x28\)', r'LIST[\1]', s)
    s = re.sub(r'TPictureCollection_GetItem\(LIST\[(0x[0-9a-f]+)\],([^)]*)\)', r'item(\1,\2)', s)
    s = re.sub(r'\(undefined4 \*\)|\(int \*\)|\(uint \*\)|\(char \*\)|\(byte \*\)|\(undefined \*\)', '', s)
    s = re.sub(r'\s+', ' ', s)
    return s


def cond(s):
    s = clean(s)
    s = s.replace('if (', '').rstrip('{ ').strip()
    if s.endswith(')') and s.startswith('('):
        s = s[1:-1]
    return s


stack = []
pending_item = None
assign = {}  # var -> last assigned expression (for expanding call arguments)


def expand(txt, depth=2):
    def rep(m):
        v = m.group(0)
        e = assign.get(v)
        return '{%s}' % e if e and depth > 0 else v
    out = re.sub(r'\b(?:[iudsp]Var\d+|[iudsp]Stack(?:Y)?_[0-9a-f]+|local_[0-9a-f]+)\b', rep, txt)
    return expand(out, depth - 1) if depth > 1 and out != txt else out


for ln in logical:
    if ln.endswith('{'):
        if ln.startswith(('if (', '} else if (', 'else if (')):
            stack.append(cond(ln))
        elif ln.startswith(('} else', 'else')):
            if stack:
                stack[-1] = '!(' + stack[-1] + ')'
        else:
            stack.append('')
        continue
    if ln.startswith('}'):
        if stack:
            stack.pop()
        if ln.endswith('{'):
            stack.append('loop')
        continue
    am = re.match(r'([iudsp]Var\d+|[iudsp]Stack(?:Y)?_[0-9a-f]+|local_[0-9a-f]+) = (.*);$', ln)
    if am and not any(d in ln for d in DRAW):
        assign[am.group(1)] = clean(am.group(2))[:80]
    if 'TPictureCollection_GetItem(' in ln and '=' in ln and not any(d in ln for d in DRAW):
        pending_item = clean(ln.split('=', 1)[1])
        continue
    if show_all or any(d in ln for d in DRAW):
        ctx = ' && '.join(c for c in stack if c)
        txt = clean(ln)
        if pending_item and ('DXImageListItem_Draw' in txt or 'func_0x004616' in txt):
            txt = txt.replace('uVar5,', 'ITEM{%s},' % pending_item, 1).replace('uVar7,', 'ITEM{%s},' % pending_item, 1)
        print('[%s] %s' % (ctx[-800:], expand(txt)[:330]))
        pending_item = None
