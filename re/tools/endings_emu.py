"""Build docs/spec/07e-endings.txt by running the state-10 code of Chaves.exe.

Usage: python3 endings_emu.py Chaves.exe > docs/spec/07e-endings.txt
Needs pefile and capstone (development only; the port never uses this).

The ending block of DXTimer1Timer (0x4ab8fe..0x4adafc) is straight-line code
keyed on the character (g_ciclo2 high byte) and the counter c (word at
[0x4b0fb0]).  Instead of pairing a decompiled listing with the assembly (which
lost and mis-paired calls) this walks the real instructions for every
character and every c in 0..2200 with a tiny x86 interpreter: memory comes
from the image, the helper calls (desenhalutador, carregarlutador,
carregarcenario, barra, Draw, DrawAlpha, spawn 0x476a04) are logged with their
register/stack arguments instead of being executed, Random(n) returns 0 (and,
in a second pass, n-1, to tell which calls sit behind a Random(n)==0 test).
Every logged call becomes one row, merged over consecutive ticks and ordered by
call-site address (= drawing order).
"""
import collections, re, struct, sys
import pefile, capstone
from capstone.x86 import *
pe = pefile.PE(sys.argv[1] if len(sys.argv) > 1 else '/mnt/project-files/street-chaves/StreetChaves/Chaves.exe')
BASE = 0x400000
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); md.detail = True
LO, HI = 0x4ab8fe, 0x4adafc
code = {i.address: i for i in md.disasm(pe.get_data(LO - BASE, 0x4ae000 - LO), LO)}
def pe_read(addr, n):
    try: d = pe.get_data(addr - BASE, n)
    except Exception: d = b''
    return d + b'\0' * (n - len(d))
REG32 = {X86_REG_EAX:'a',X86_REG_EBX:'b',X86_REG_ECX:'c',X86_REG_EDX:'d',X86_REG_ESI:'si',X86_REG_EDI:'di',X86_REG_EBP:'bp',X86_REG_ESP:'sp'}
SUB = {X86_REG_AX:('a',16),X86_REG_BX:('b',16),X86_REG_CX:('c',16),X86_REG_DX:('d',16),
       X86_REG_AL:('a',8),X86_REG_BL:('b',8),X86_REG_CL:('c',8),X86_REG_DL:('d',8),
       X86_REG_AH:('a',-8),X86_REG_BH:('b',-8),X86_REG_CH:('c',-8),X86_REG_DH:('d',-8)}
FORM = 0x10000000
NST = {0x490bb8:3,0x48ea84:1,0x48e444:3,0x476a04:4,0x48e720:4,0x46161c:2,0x4616c0:2,0x40d760:2}
class M:
    def __init__(s, ch, c, rnd, init=None):
        s.mem = {}; s.r = collections.defaultdict(int); s.log = []; s.stack = 0x7f000000
        s.r['sp'] = s.stack; s.r['bp'] = s.stack + 0x2000; s.r['si'] = FORM
        s.rnd = rnd; s.flags = {}; s.wlog = []; s.last_rnd = s.test_rnd = 0
        s.setvar(0x4b111c, 1, 10); s.setvar(0x4b0dc4, 1, ch); s.setvar(0x4b0fb0, 2, c)
        s.wlog = []
        for k, (sz, v) in (init or {}).items(): s.setvar(k, sz, v)
    def ptr(s, slot): return struct.unpack('<I', pe_read(slot, 4))[0]
    def setvar(s, slot, sz, v): s.w(s.ptr(slot), sz, v)
    def getvar(s, slot, sz): return s.rd(s.ptr(slot), sz)
    def rd(s, a, sz):
        if FORM <= a < FORM + 0x2000 and sz == 4: return 0x20000000 + (a - FORM)
        if 0x20000000 <= a < 0x20001000 and sz == 4: return 0x30000000 + (a - 0x20000000 - 0x28) if (a - 0x20000000) & 0xffff >= 0x28 and False else 0x30000000 + (a - 0x28 - 0x20000000)
        v = 0
        for i in range(sz):
            b = s.mem.get(a + i)
            if b is None: b = pe_read(a + i, 1)[0] if 0x400000 <= a < 0x600000 else 0
            v |= b << (8 * i)
        return v
    def w(s, a, sz, v):
        v &= (1 << (8 * sz)) - 1
        for i in range(sz): s.mem[a + i] = (v >> (8 * i)) & 0xff
        if not (s.stack - 0x10000 <= a < s.stack + 0x10000): s.wlog.append((a, sz, v))
    def getreg(s, reg):
        if reg in REG32: return s.r[REG32[reg]] & 0xffffffff
        if reg in SUB:
            n, b = SUB[reg]; v = s.r[n]
            if b == 16: return v & 0xffff
            if b == 8: return v & 0xff
            return (v >> 8) & 0xff
        raise Exception('reg %s' % reg)
    def setreg(s, reg, v):
        if reg in REG32: s.r[REG32[reg]] = v & 0xffffffff; return
        n, b = SUB[reg]; o = s.r[n]
        if b == 16: s.r[n] = (o & 0xffff0000) | (v & 0xffff)
        elif b == 8: s.r[n] = (o & 0xffffff00) | (v & 0xff)
        else: s.r[n] = (o & 0xffff00ff) | ((v & 0xff) << 8)
    def regsize(s, reg):
        if reg in REG32: return 4
        n, b = SUB[reg]; return 2 if b == 16 else 1
    def ea(s, op):
        m = op.mem; a = m.disp
        if m.base: a += s.getreg(m.base)
        if m.index: a += s.getreg(m.index) * m.scale
        return a & 0xffffffff
    def val(s, op, sz=None):
        if op.type == X86_OP_IMM: return op.imm & 0xffffffff
        if op.type == X86_OP_REG: return s.getreg(op.reg)
        return s.rd(s.ea(op), op.size)
    def put(s, op, v):
        if op.type == X86_OP_REG: s.setreg(op.reg, v)
        else: s.w(s.ea(op), op.size, v)
    def opsize(s, op):
        return op.size
    def setf(s, res, sz, cf=0, of=0):
        mask = (1 << (8 * sz)) - 1; res &= mask
        s.flags = dict(zf=res == 0, sf=bool(res >> (8 * sz - 1)), cf=cf, of=of)
    def cond(s, mn):
        f = s.flags; zf, sf, cf, of = f['zf'], f['sf'], f['cf'], f['of']
        return {'je': zf, 'jne': not zf, 'jl': sf != of, 'jge': sf == of, 'jle': zf or sf != of, 'jg': (not zf) and sf == of,
                'jb': cf, 'jae': not cf, 'jbe': cf or zf, 'ja': not (cf or zf)}[mn]
    def sub(s, a, b, sz):
        mask = (1 << (8 * sz)) - 1; r = (a - b) & mask
        sa, sb, sr = a >> (8*sz-1) & 1, b >> (8*sz-1) & 1, r >> (8*sz-1) & 1
        s.flags = dict(zf=r == 0, sf=bool(sr), cf=(a & mask) < (b & mask), of=bool((sa != sb) and (sr != sa)))
        return r
    def add(s, a, b, sz):
        mask = (1 << (8 * sz)) - 1; r = (a + b) & mask
        sa, sb, sr = a >> (8*sz-1) & 1, b >> (8*sz-1) & 1, r >> (8*sz-1) & 1
        s.flags = dict(zf=r == 0, sf=bool(sr), cf=(a & mask) + (b & mask) > mask, of=bool((sa == sb) and (sr != sa)))
        return r
    def push(s, v): s.r['sp'] -= 4; s.w(s.r['sp'], 4, v)
    def pop(s): v = s.rd(s.r['sp'], 4); s.r['sp'] += 4; return v
    def sx(s, v, sz):
        return v - (1 << (8*sz)) if v >> (8*sz-1) else v
    def run(s):
        pc = LO; n = 0; fpu = []
        while LO <= pc < HI:
            n += 1
            if n > 20000: raise Exception('loop')
            i = code[pc]; mn = i.mnemonic; ops = i.operands; nxt = pc + i.size
            if mn == 'mov': s.put(ops[0], s.val(ops[1]))
            elif mn == 'push': s.push(s.val(ops[0]) if ops[0].type != X86_OP_IMM else ops[0].imm & 0xffffffff)
            elif mn == 'pop': s.put(ops[0], s.pop())
            elif mn == 'lea': s.setreg(ops[0].reg, s.ea(ops[1]))
            elif mn == 'xor':
                sz = ops[0].size; r = s.val(ops[0]) ^ s.val(ops[1]); s.setf(r, sz); s.put(ops[0], r)
            elif mn == 'cmp': s.sub(s.val(ops[0]), s.val(ops[1]) & ((1 << (8*ops[0].size))-1) if ops[1].type != X86_OP_IMM else ops[1].imm & ((1 << (8*ops[0].size))-1), ops[0].size)
            elif mn == 'test':
                sz = ops[0].size; r = s.val(ops[0]) & s.val(ops[1]); s.setf(r, sz)
                if ops[0].type == X86_OP_REG and ops[1].type == X86_OP_REG and ops[0].reg == X86_REG_EAX: s.test_rnd = s.last_rnd
            elif mn == 'add':
                sz = ops[0].size; b = s.val(ops[1]) if ops[1].type != X86_OP_IMM else ops[1].imm & ((1<<(8*sz))-1)
                s.put(ops[0], s.add(s.val(ops[0]), b, sz))
            elif mn == 'sub':
                sz = ops[0].size; b = s.val(ops[1]) if ops[1].type != X86_OP_IMM else ops[1].imm & ((1<<(8*sz))-1)
                s.put(ops[0], s.sub(s.val(ops[0]), b, sz))
            elif mn == 'inc': sz = ops[0].size; c = s.flags.get('cf', 0); s.put(ops[0], s.add(s.val(ops[0]), 1, sz)); s.flags['cf'] = c
            elif mn == 'shl': sz = ops[0].size; s.put(ops[0], s.val(ops[0]) << ops[1].imm)
            elif mn == 'imul':
                if len(ops) == 3: s.put(ops[0], s.sx(s.val(ops[1]), ops[1].size) * ops[2].imm)
                else: s.put(ops[0], s.sx(s.val(ops[0]), ops[0].size) * s.sx(s.val(ops[1]), ops[1].size))
            elif mn == 'movsx': s.put(ops[0], s.sx(s.val(ops[1]), ops[1].size))
            elif mn == 'movzx': s.put(ops[0], s.val(ops[1]))
            elif mn == 'fild': fpu.append(s.sx(s.val(ops[0]), 4))
            elif mn == 'fstp': s.w(s.ea(ops[0]), 4, 0); s.fst = fpu.pop(); s.log.append(('fstp', s.fst)); 
            elif mn == 'wait': pass
            elif mn.startswith('j'):
                tgt = ops[0].imm
                if mn == 'jmp' or s.cond(mn): nxt = tgt
            elif mn == 'call':
                if ops[0].type == X86_OP_IMM: t = ops[0].imm; nstack = NST.get(t, 0)
                else: t = None; nstack = 0
                a = dict(a=s.r['a'] & 0xffffffff, d=s.r['d'] & 0xffffffff, c=s.r['c'] & 0xffffffff)
                st = [s.rd(s.r['sp'] + 4 * k, 4) for k in range(nstack)]  # st[0] = last pushed
                s.r['sp'] += 4 * nstack
                ret = 0
                if t == 0x402a1c:
                    n_ = a['a']; ret = s.rnd(n_) if n_ else 0; s.last_rnd = n_
                elif t == 0x40d760:
                    p = st[0]; vals = (a['a'], a['d'], a['c'], st[1])
                    for k, v in enumerate(vals): s.w(p + 4*k, 4, v)
                    s.log.append(('rect', vals)); t = None
                elif t == 0x406de4:
                    ret = (a['a'] & 255) | ((a['d'] & 255) << 8) | ((a['c'] & 255) << 16)
                    t = None
                elif t == 0x40efcc: ret = 1
                elif t == 0x461dcc: ret = ('item', a['a'], a['d']); ret = (a['a'] << 8 | 0) if False else 0x40000000 + (((a['a'] - 0x30000000) & 0xffff) << 8) + (a['d'] & 0xff)
                if t not in (None, 0x402a1c, 0x40efcc, 0x461dcc, 0x403d60, 0x408190, 0x403c9c) or t == 0x403d60:
                    pass
                if t is not None or True:
                    s.log.append(('call', pc, t, a, st, ret, getattr(s, 'test_rnd', 0)))
                s.r['a'] = ret & 0xffffffff
                if t == 0x403d60: s.flags = dict(zf=False, sf=False, cf=0, of=0)
            else: raise Exception('unhandled %x %s %s' % (pc, mn, i.op_str))
            pc = nxt
        s.exit_pc = pc
        return s

# ---------------------------------------------------------------- the ending scripts
NAMES = {0x490bb8: 'FIGHTER', 0x48ea84: 'LOADCHAR', 0x4908a8: 'LOADSTAGE', 0x46161c: 'DRAW', 0x4616c0: 'DRAWA',
         0x48e444: 'BARRA', 0x476a04: 'SPAWN'}
LETTERBOX = {(0, 0, 400, 50), (0, 220, 400, 300), (0, 50, 50, 220), (350, 50, 400, 220)}
LIST_STAGE, LIST_EFFECTS, LIST_FINAL = 0x310, 0x308, 0x330   # Form1 field offsets of the image lists

def stage_no(va):
    ln = struct.unpack('<I', pe.get_data(va - BASE - 4, 4))[0]
    return int(re.search(r'(\d+)', pe.get_data(va - BASE, ln).decode('latin1')).group(1))

def events(ch, c, rnd):
    """(call site, row) for every drawing/loading call of tick c, in execution order."""
    m = M(ch, c, rnd, init={0x4b0f84: (4, 0x63)}).run()   # 0x63: no speaker, so no dialogue calls
    ev = []; fp = []; rect = None
    for e in m.log:
        if e[0] == 'fstp': fp.append(e[1]); continue
        if e[0] == 'rect': rect = e[1]; continue
        _, pc, t, a, st, ret, trnd = e
        n = NAMES.get(t)
        if n == 'FIGHTER': ev.append((pc, ('FIGHTER', a['d'] & 0xff, a['c'] & 0xffff, st[2], st[1], st[0])))
        elif n == 'LOADCHAR': ev.append((pc, ('CHAR', st[0], a['c'] & 0xff)))
        elif n == 'LOADSTAGE': ev.append((pc, ('STAGE', stage_no(a['d']))))
        elif n == 'DRAW':
            lst, idx = (a['a'] >> 8) & 0xffff, a['a'] & 0xff
            if lst == LIST_STAGE: continue                      # the stage picture, every tick
            assert lst == LIST_FINAL and st[0] == 0, (pc, lst, st)
            ev.append((pc, ('FINAL', idx, a['c'], st[1])))
        elif n == 'DRAWA':
            assert (a['a'] >> 8) & 0xffff == LIST_EFFECTS and a['a'] & 0xff == 0 and st[0] == 70 and st[1] == 0
            ev.append((pc, ('SHADOW',) + tuple(rect)))
        elif n == 'BARRA':
            box = (a['d'], a['c'], st[2], st[1])
            if box in LETTERBOX: continue
            ev.append((pc, ('BLACK',) if box == (0, 0, 400, 300) else ('FLASH',) if box == (50, 50, 350, 220) else ('BOX',) + box))
        elif n == 'SPAWN':
            ev.append((pc, ('SPAWN', a['a'] & 0xff, trnd, tuple(fp[-2:])))); fp = []
    return ev

def main():
    rows = []
    for ch in range(1, 16):
        occ = collections.defaultdict(set); chance = {}
        for c in range(0, 2201):
            ev = events(ch, c, lambda n: 0)
            ev_hi = None
            for pc, row in ev:
                key = (pc, row[:2] if row[0] == 'SPAWN' else row)
                occ[key].add(c)
                if row[0] == 'SPAWN':
                    if ev_hi is None: ev_hi = {p for p, _ in events(ch, c, lambda n: n - 1)}
                    chance[key] = row[2] if pc not in ev_hi else 0
        for (pc, row), cs in occ.items():
            cs = sorted(cs); runs = []
            for c in cs:
                if runs and runs[-1][1] == c - 1: runs[-1][1] = c
                else: runs.append([c, c])
            for lo, hi in runs: rows.append((ch, lo, hi, pc, row, chance.get((pc, row), 0)))
    rows.sort(key=lambda r: (r[0], r[1] != r[2], r[3], r[1]))
    print(HEADER)
    for ch, lo, hi, pc, row, ch_n in rows:
        print('%d %d %d %s%s' % (ch, lo, hi, ' '.join(str(x) for x in row), ' P1/%d' % ch_n if ch_n else ''))

HEADER = """# 07e — ending scripts (state 10), generated by re/tools/endings_emu.py from the x86 code (numbers only).
# Scene viewport (50,50)-(350,220) inside black bars; ground y = 210; the stage picture is drawn at (-80,-100).
# The counter c runs 0..2200 (one tick = one DXTimer1Timer call; the frame of tick c is drawn with c before the
# increment; after c = 2200 -> state 11). A held button 1 of a human player jumps c to the end of the current
# 100-tick block (c in (i*100+5, i*100+85) -> i*100+85). Dialogue: 11 segments of 200 ticks (c < 2200); segment k
# uses MemoFinais (+0x32c, 650 lines) line base+4k as speaker tag ("-" none, "A".."O" = character 1..15 ->
# Retrato[(n-1)*5] at (20,222)) and lines base+4k+1..3 at (80,228) (80,245) (80,262), size 12, white, left.
# BASE per character: 0 44 88 125 166 210 254 298 342 386 430 474 518 562 606
# Rows are in call-site (drawing) order; `from == to` loads run before the draws of that tick.
# ops: STAGE n | CHAR slot n | FIGHTER slot x y state timer | SHADOW l t r b (Efeitos[0] stretched, alpha 70)
#      | FINAL idx x y (DXImageListFinal item, pattern 0) | BLACK (barra 0,0,400,300 RGB(0,0,0))
#      | FLASH (barra 50,50,350,220 RGB(255,rnd,rnd)) | SPAWN type: owner 0, 1 at x Random(150)+130,
#        y Random(100)+100; 24 at x Random(100)+150, y Random(30)+60; 25 at x Random(60)+250, y Random(20)+110
#      P1/n = only when Random(n)==0
# The fighters do not animate by themselves: desenhalutador stores the state and timer it is given in the
# player record and runs the frame selector (0x47d21c) on them every call.
#
# ch from to op ..."""

if __name__ == '__main__':
    main()
