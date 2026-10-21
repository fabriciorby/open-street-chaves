#!/usr/bin/env python3
"""Generate src/tables_gen.c/.h from the spec tables in docs/spec.

Inputs (all derived from the original executable's code, no assets/text):
  05a-anim-tables.txt   (state, timer) -> frame, hurt boxes, hit boxes
  05b-move-setup.txt    move id -> new state, duration, meter, sound, gates
  05c-frame-offsets.txt horizontal draw offset per frame
  04a-move-tables.txt   command-parser transitions and button -> move map

Run from the repository root:  python3 tools/gen_tables.py
"""
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SPEC = os.path.join(ROOT, 'docs', 'spec')
OUT_C = os.path.join(ROOT, 'src', 'tables_gen.c')
OUT_H = os.path.join(ROOT, 'src', 'tables_gen.h')


def read(name):
    with open(os.path.join(SPEC, name), encoding='utf-8') as f:
        return f.read()


def num(s):
    return int(s, 0)


# ---------------------------------------------------------------- 05c offsets
offsets = {}
for m in re.finditer(r'^char\s+(\d+):\s+(.*)$', read('05c-frame-offsets.txt'), re.M):
    offsets[int(m.group(1))] = {int(k): int(v) for k, v in re.findall(r'(\d+):(-?\d+)', m.group(2))}

# ---------------------------------------------------------------- 05a anim
anim, hurt, hit = [], [], []
char = None
for line in read('05a-anim-tables.txt').split('\n'):
    m = re.match(r'## char (\d+)', line)
    if m:
        char = int(m.group(1))
        continue
    m = re.match(r'  state\s+(\d+): (.*)$', line)
    if m:
        st = int(m.group(1))
        for t, f in re.findall(r'(\*|\d+)->(\d+)', m.group(2)):
            anim.append((char, st, -1 if t == '*' else int(t), int(f)))
        continue
    m = re.match(r'  hurt frame\s+(\d+): (-?\d+),(-?\d+),(-?\d+),(-?\d+)$', line)
    if m:
        hurt.append((char,) + tuple(int(x) for x in m.groups()))
        continue
    m = re.match(r'  hit\s+(-?\d+(?:,-?\d+){9})$', line)
    if m:
        hit.append((char,) + tuple(int(x) for x in m.group(1).split(',')))
        continue

# ---------------------------------------------------------------- 05b moves
moves = []
char = 0
for line in read('05b-move-setup.txt').split('\n'):
    m = re.match(r'## (common|char (\d+))', line)
    if m:
        char = int(m.group(2)) if m.group(2) else 0
        continue
    m = re.match(r'  move (\d+)(?:/(\d+))?\s+from (\S+)\s+-> state (\S+)\s+dur (\S+)\s+meter x(\S+)\s+snd (\S+)\s*(.*)$', line)
    if not m:
        continue
    ids = [int(m.group(1))] + ([int(m.group(2))] if m.group(2) else [])
    froms = 0
    if m.group(3) != '-':
        for s in m.group(3).split(','):
            froms |= 1 << num(s)
    state = num(m.group(4))
    dur = num(m.group(5))
    meter = 0 if m.group(6) == '-' else int(round(float(m.group(6)) * 10))
    snd = -1 if m.group(7) == '-' else num(m.group(7))
    extra = m.group(8)
    flags = 0
    if 'onGround' in extra:
        flags |= 1
    if 'flag+0x6a==0' in extra:
        flags |= 2
    if 'health/meter cond' in extra:
        flags |= 4   # super: needs stock or health <= 15
    if 'moveX' in extra:
        flags |= 8
    needs = re.search(r'needs(\d+)', extra)
    needs = int(needs.group(1)) if needs else 0
    for i, mid in enumerate(ids):
        strength = (i + 1) if len(ids) == 2 else 0
        moves.append((char, mid, froms, state, dur, meter, snd, flags, needs, strength))

# ---------------------------------------------------------------- 04a sequences + golpe
DIRS = {'BAIXO': 0, 'FRENTE': 1, 'TRAS': 2, 'CIMA': 3}
seqs, golpe = [], []
text = read('04a-move-tables.txt')
seq_part, golpe_part = text.split('# golpe', 1)
char = None
for line in seq_part.split('\n'):
    m = re.match(r'## char (\d+)', line)
    if m:
        char = int(m.group(1))
        continue
    m = re.match(r'  (.*?)\s+(\d+) -> (\d+)$', line)
    if not m or char is None:
        continue
    req = forb = 0
    down_hold = 0
    on_ground = 0
    for tok in m.group(1).split():
        if tok in DIRS:
            req |= 1 << DIRS[tok]
        elif tok.startswith('!') and tok[1:] in DIRS:
            forb |= 1 << DIRS[tok[1:]]
        elif tok.startswith('downHold>'):
            down_hold = int(tok[9:])
        elif tok == 'onGround':
            on_ground = 1
    seqs.append((char, req, forb, down_hold, on_ground, int(m.group(2)), int(m.group(3))))
char = None
for line in golpe_part.split('\n'):
    m = re.match(r'## char (\d+)', line)
    if m:
        char = int(m.group(1))
        continue
    m = re.match(r'  btn(\d)\s+(.*?)\s*-> move (\d+)$', line)
    if not m or char is None:
        continue
    cond = m.group(2)
    seq = -1
    extra = 0
    s = re.search(r'seq=(\d+)', cond)
    if s:
        seq = int(s.group(1))
    if 'backHold' in cond:
        extra = 1
    if 'downHold' in cond:
        extra = 2
    if 'airborne' in cond:
        extra |= 4
    golpe.append((char, int(m.group(1)), seq, extra, int(m.group(3))))

# ---------------------------------------------------------------- emit
def rows(items):
    return ',\n'.join('    {%s}' % ', '.join(str(v) for v in it) for it in items)


h = '''/* Generated by tools/gen_tables.py from docs/spec — do not edit. */
#ifndef OSC_TABLES_GEN_H
#define OSC_TABLES_GEN_H
#include <stdint.h>

typedef struct { uint8_t ch, state; int8_t timer; uint8_t frame; } osc_anim_row;     /* timer -1 = any */
typedef struct { uint8_t ch, frame; int16_t l, b, r, t; } osc_hurt_row;             /* from sprite top-left */
typedef struct { uint8_t ch, state, timer, p4, dmg, level, atk; int16_t b, r, t, l; } osc_hit_row;
typedef struct { uint8_t ch, move; uint32_t from_mask; uint8_t state, dur, meter10; int8_t snd; uint8_t flags, needs_fx, strength; } osc_move_row;
typedef struct { uint8_t ch, req, forb, down_hold, on_ground, from, to; } osc_seq_row;  /* bit0 BAIXO 1 FRENTE 2 TRAS 3 CIMA */
typedef struct { uint8_t ch, btn; int8_t seq; uint8_t extra, move; } osc_golpe_row;     /* extra: 1 backHold>5 2 downHold>5 4 airborne */

enum { OSC_MOVE_ONGROUND = 1, OSC_MOVE_NOT_INAIR = 2, OSC_MOVE_SUPER = 4, OSC_MOVE_WALK = 8 };

extern const int8_t osc_frame_offset[16][64];
extern const osc_anim_row osc_anim[%d];
extern const osc_hurt_row osc_hurt[%d];
extern const osc_hit_row osc_hit[%d];
extern const osc_move_row osc_moves[%d];
extern const osc_seq_row osc_seqs[%d];
extern const osc_golpe_row osc_golpe[%d];
#define OSC_N_ANIM %d
#define OSC_N_HURT %d
#define OSC_N_HIT %d
#define OSC_N_MOVES %d
#define OSC_N_SEQS %d
#define OSC_N_GOLPE %d
#endif
''' % (len(anim), len(hurt), len(hit), len(moves), len(seqs), len(golpe),
       len(anim), len(hurt), len(hit), len(moves), len(seqs), len(golpe))

off_rows = []
for c in range(16):
    row = [0] * 64
    for k, v in offsets.get(c, {}).items():
        if 0 <= k < 64:
            row[k] = v
    off_rows.append('    {' + ', '.join(str(v) for v in row) + '}')

c = '''/* Generated by tools/gen_tables.py from docs/spec — do not edit. */
#include "tables_gen.h"

const int8_t osc_frame_offset[16][64] = {
%s
};

const osc_anim_row osc_anim[%d] = {
%s
};

const osc_hurt_row osc_hurt[%d] = {
%s
};

const osc_hit_row osc_hit[%d] = {
%s
};

const osc_move_row osc_moves[%d] = {
%s
};

const osc_seq_row osc_seqs[%d] = {
%s
};

const osc_golpe_row osc_golpe[%d] = {
%s
};
''' % (',\n'.join(off_rows), len(anim), rows(anim), len(hurt), rows(hurt), len(hit), rows(hit),
       len(moves), rows(moves), len(seqs), rows(seqs), len(golpe), rows(golpe))

with open(OUT_H, 'w') as f:
    f.write(h)
with open(OUT_C, 'w') as f:
    f.write(c)
print('anim %d hurt %d hit %d moves %d seqs %d golpe %d' % (len(anim), len(hurt), len(hit), len(moves), len(seqs), len(golpe)))
