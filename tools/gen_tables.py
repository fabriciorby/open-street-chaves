#!/usr/bin/env python3
"""Generate src/tables_gen.c/.h from the spec tables in docs/spec.

Inputs (all derived from the original executable's code, no assets/text):
  05a-anim-tables.txt   (state, timer) -> frame, hurt boxes, hit boxes
  05b-move-setup.txt    move id -> new state, duration, meter, sound, gates
  05c-frame-offsets.txt horizontal draw offset per frame
  05d-cpu-table.txt     CPU special-move decision table
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
def mid_first_fix(ids, state):
    return ids[0] in (3, 6) and state in (3, 6)


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
    # The first standing LP/HK rows lost their state list in the dump; the
    # pseudocode (05) shows them gated on states 1, 2 and 21 like the others.
    if char == 0 and froms == 0 and mid_first_fix(ids, state):
        froms = (1 << 1) | (1 << 2) | (1 << 21)
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

# ---------------------------------------------------------------- 06a effects
fx_init = {}   # type -> dict
fx_sprites = []  # (type, phase or -1, sprite)
fx_loop = {}   # type -> phase at which phase resets to 0
fx_done = {}   # type -> phase threshold at which done=1 (approx, '*' ignored)
part_sprites, part_init = read('06a-effects.txt').split('# FUN_00476a04', 1)
cur = None
for line in part_sprites.split('\n'):
    m = re.match(r'## effect type (\d+)', line)
    if m:
        cur = int(m.group(1))
        continue
    m = re.match(r'  phase (\*|\d+|[<>]\d+)\s+(.*)$', line)
    if not m or cur is None:
        continue
    ph, what = m.groups()
    phase = -1 if ph == '*' or ph[0] in '<>' else int(ph)
    s = re.match(r'sprite=(\d+)$', what)
    if s:
        fx_sprites.append((cur, phase, int(s.group(1))))
    elif what == 'phase=0' and phase >= 0:
        fx_loop[cur] = phase
cur_types = []
for line in part_init.split('\n'):
    m = re.match(r'## type ([\d/]+)', line)
    if m:
        cur_types = [int(x) for x in m.group(1).split('/')]
        for t in cur_types:
            fx_init.setdefault(t, {'life': 0, 'speedA': 0, 'speedB': 0, 'damage': 0, 'hitbox': (0, 0, 0, 0),
                                   'flags': 0, 'vy': 0, 'c5': 0})
        variant = None
        continue
    if not cur_types:
        continue
    s = line.strip()
    m = re.match(r'LStrCmp\(local_c,\(uint \*\)"([AB])"\)', s)
    if m:
        variant = m.group(1)
        continue
    hb = re.match(r'hitbox l=(-?\d+) t=(-?\d+) r=(-?\d+) b=(-?\d+)', s)
    if hb:
        for t in cur_types:
            fx_init[t]['hitbox'] = tuple(int(x) for x in hb.groups())
        continue
    m = re.match(r'(\w+) = (.*)$', s)
    if not m:
        continue
    key, val = m.groups()
    v = 0
    rnd = re.match(r'rand\+(\d+)', val)
    if rnd:
        v = int(rnd.group(1))
    elif re.match(r'-?\d+$', val):
        v = int(val)
    for t in cur_types:
        d = fx_init[t]
        if key == 'life':
            d['life'] = v
        elif key == 'speed':
            if variant == 'B':
                d['speedB'] = v
            else:
                d['speedA'] = v
                if variant is None:
                    d['speedB'] = v
        elif key == 'damage':
            d['damage'] = v
        elif key == 'hasHitbox' and v:
            d['flags'] |= 1
        elif key == 'f8' and v:
            d['flags'] |= 2      # anchored at the top
        elif key == 'f9' and v:
            d['flags'] |= 4      # falls with gravity
        elif key == 'flagA' and v:
            d['flags'] |= 8
        elif key == 'flagB' and v:
            d['flags'] |= 16     # unbreakable / persistent
        elif key == 'hb_dx':
            d['vy'] = v
        elif key == 'c5':
            d['c5'] = v
    hb = re.match(r'hitbox l=(-?\d+) t=(-?\d+) r=(-?\d+) b=(-?\d+)', s)
    if hb:
        for t in cur_types:
            fx_init[t]['hitbox'] = tuple(int(x) for x in hb.groups())
for t in fx_init:
    if fx_init[t]['speedB'] == 0:
        fx_init[t]['speedB'] = fx_init[t]['speedA']

# spawns from the frame selector: (char, state, timer, type, variant)
CT = {'\\a': 7, '\\b': 8, '\\t': 9, '\\n': 10, '\\v': 11, '\\f': 12, '\\r': 13}
spawns = set()
char = None
for line in read('05a-anim-tables.txt').split('\n'):
    m = re.match(r'## char (\d+)', line)
    if m:
        char = int(m.group(1))
        continue
    m = re.match(r"  state\s+(\d+) timer\s+(\*|\d+)\s*: FUN_00476a04\('([^']+)',\(uint \*\)(\"([AB])\"|0x0),", line)
    if not m:
        continue
    lit = m.group(3)
    typ = CT.get(lit, int(lit[2:], 16) if lit.startswith('\\x') else ord(lit[-1]))
    variant = m.group(5)
    spawns.add((char, int(m.group(1)), -1 if m.group(2) == '*' else int(m.group(2)), typ,
                0 if variant is None else (1 if variant == 'A' else 2)))
spawns = sorted(spawns)
fx_types = sorted(fx_init)

# ---------------------------------------------------------------- 07c stage animations
stage_anim = []
for line in read('07c-stage-animations.txt').split('\n'):
    m = re.match(r'(\d+) (\d+) (\d+) ([AE]) (\d+) (-?\d+) (-?\d+) (\d+)$', line)
    if m:
        stage_anim.append((int(m.group(1)), int(m.group(2)), int(m.group(3)), 1 if m.group(4) == 'E' else 0,
                           int(m.group(5)), int(m.group(6)), int(m.group(7)), int(m.group(8))))

# ---------------------------------------------------------------- 07d text index
text_index = []
for line in read('07d-text-index.txt').split('\n'):
    m = re.match(r'(\d+) (\d+) (-?\d+)$', line)
    if m:
        text_index.append((int(m.group(1)), int(m.group(2)), int(m.group(3))))

# ---------------------------------------------------------------- 05d cpu table
cpu_rows = []
COND = {'NOPROJ': 1, 'PROJ': 2, 'ANY': 0, 'OPPAIR': 4, 'SELFAIR100': 8, 'SELFAIR50': 16}
for line in read('05d-cpu-table.txt').split('\n'):
    m = re.match(r'(\d+)\s+([A-Z0-9+]+)\s+(dist|pd)([<>])(\d+)\s+(\d+)\s+(\d+)\s+(\d+)\s*$', line)
    if not m:
        continue
    cond = 0
    for w in m.group(2).split('+'):
        cond |= COND[w]
    cpu_rows.append((int(m.group(1)), cond, 1 if m.group(4) == '>' else 0, int(m.group(5)),
                     int(m.group(6)), int(m.group(7)), int(m.group(8))))

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
typedef struct { uint8_t ch, cond, greater; int16_t thr; uint8_t c, move, n; } osc_cpu_row;  /* cond bits: 1 NOPROJ 2 PROJ(pd) 4 OPPAIR 8 SELFAIR100 16 SELFAIR50 */

enum { OSC_MOVE_ONGROUND = 1, OSC_MOVE_NOT_INAIR = 2, OSC_MOVE_SUPER = 4, OSC_MOVE_WALK = 8 };

/* effects (06): per type init, (phase -> sprite) rows, spawns from the frame selector */
typedef struct { uint8_t type, life, speedA, speedB, damage, flags, c5, loop_at; int8_t vy; int16_t l, t, r, b; } osc_fx_type;
typedef struct { uint8_t type; int8_t phase; uint8_t sprite; } osc_fx_sprite_row;   /* phase -1 = any */
typedef struct { uint8_t ch, state; int8_t timer; uint8_t type, variant; } osc_fx_spawn_row; /* variant 0 none, 1 A, 2 B */
enum { OSC_FX_HITBOX = 1, OSC_FX_TOP = 2, OSC_FX_GRAVITY = 4, OSC_FX_FLAGA = 8, OSC_FX_PERSIST = 16 };

/* text index (07d): memo line -> index of the string constant inside Chaves.exe */
typedef struct { uint8_t memo; uint16_t line; int16_t index; } osc_text_index_row;
extern const osc_text_index_row osc_text_index[%d];
#define OSC_N_TEXT_INDEX %d

/* stage animations (07c): drawn each fight tick after the background */
typedef struct { uint16_t stage_mask; uint8_t ciclo_lo, ciclo_hi, is_effect, item; int16_t x, y; uint8_t not_char; } osc_stage_anim_row;
extern const osc_stage_anim_row osc_stage_anim[%d];
#define OSC_N_STAGE_ANIM %d

extern const osc_fx_type osc_fx_types[%d];
extern const osc_fx_sprite_row osc_fx_sprites[%d];
extern const osc_fx_spawn_row osc_fx_spawns[%d];
#define OSC_N_FX_TYPES %d
#define OSC_N_FX_SPRITES %d
#define OSC_N_FX_SPAWNS %d

extern const int8_t osc_frame_offset[16][64];
extern const osc_anim_row osc_anim[%d];
extern const osc_hurt_row osc_hurt[%d];
extern const osc_hit_row osc_hit[%d];
extern const osc_move_row osc_moves[%d];
extern const osc_seq_row osc_seqs[%d];
extern const osc_golpe_row osc_golpe[%d];
extern const osc_cpu_row osc_cpu[%d];
#define OSC_N_ANIM %d
#define OSC_N_HURT %d
#define OSC_N_HIT %d
#define OSC_N_MOVES %d
#define OSC_N_SEQS %d
#define OSC_N_GOLPE %d
#define OSC_N_CPU %d
#endif
''' % (len(text_index), len(text_index), len(stage_anim), len(stage_anim), len(fx_types), len(fx_sprites), len(spawns), len(fx_types), len(fx_sprites), len(spawns),
       len(anim), len(hurt), len(hit), len(moves), len(seqs), len(golpe), len(cpu_rows),
       len(anim), len(hurt), len(hit), len(moves), len(seqs), len(golpe), len(cpu_rows))

fx_type_rows = []
for t in fx_types:
    d = fx_init[t]
    fx_type_rows.append((t, d['life'] & 0xff, d['speedA'], d['speedB'], d['damage'], d['flags'], d['c5'],
                         fx_loop.get(t, 0), d['vy'], d['hitbox'][0], d['hitbox'][1], d['hitbox'][2], d['hitbox'][3]))

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

const osc_cpu_row osc_cpu[%d] = {
%s
};

const osc_fx_type osc_fx_types[%d] = {
%s
};

const osc_fx_sprite_row osc_fx_sprites[%d] = {
%s
};

const osc_fx_spawn_row osc_fx_spawns[%d] = {
%s
};

const osc_stage_anim_row osc_stage_anim[%d] = {
%s
};

const osc_text_index_row osc_text_index[%d] = {
%s
};
''' % (',\n'.join(off_rows), len(anim), rows(anim), len(hurt), rows(hurt), len(hit), rows(hit),
       len(moves), rows(moves), len(seqs), rows(seqs), len(golpe), rows(golpe), len(cpu_rows), rows(cpu_rows),
       len(fx_type_rows), rows(fx_type_rows), len(fx_sprites), rows(fx_sprites), len(spawns), rows(spawns),
       len(stage_anim), rows(stage_anim), len(text_index), rows(text_index))

with open(OUT_H, 'w') as f:
    f.write(h)
with open(OUT_C, 'w') as f:
    f.write(c)
print('cpu %d' % len(cpu_rows))
print('anim %d hurt %d hit %d moves %d seqs %d golpe %d fxtypes %d fxsprites %d spawns %d' % (len(anim), len(hurt), len(hit), len(moves), len(seqs), len(golpe), len(fx_types), len(fx_sprites), len(spawns)))
