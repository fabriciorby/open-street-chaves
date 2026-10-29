# 06 — Effects and projectiles

Legend: **[decomp]** Ghidra pseudocode (not source), **[infer]**
interpretation, **UNKNOWN** not established. Generated detail:
`06a-effects.txt` (regenerate with `tools/re/effects.py`): per effect type
the `(phase) → sprite` table, per-phase actions, and the spawn
initialisation (life, speed, damage, hit box) of every type.

## Table [decomp]

80 slots of 56 bytes at `0x4b4648` (layout in 02). A slot is free when
`type == 0`. `FUN_00476a04(type, variant, owner, y, x)` takes the first
free slot, zeroes it, stores `x`, `y`, `owner`, `dir := owner.facing` and
applies the per-type defaults (`06a`, second part). Variants `"A"`/`"B"`
select the weak/strong version (speed 8 vs 11 for most fireballs, bigger
hit boxes for some).

Sprites come from `DXImageListEfeitos` (`Efeitos/Efeito-NNN.bmp`, index =
NNN − 1) or `DXImageListEfeitosInvert` when `dir == 2`; the index per
`(type, phase)` is in `06a`. Effect 0 (`Efeito-001`) is the fighter shadow.

## Life cycle (`desenhaefeitos` `0x48edfc`) [decomp]

Every tick, for each live slot:

1. Pick the sprite for `(type, phase)`; some types loop (`phase := 0` at
   the end), some set `done` when `phase >= life − k`.
2. Type-specific motion (examples): type 2 (dust) drifts up by a constant;
   type 5 (floating text, e.g. the "+HP"/score popup, drawn with
   `MemoTexto[5]`) rises; projectiles move `x += ±speed` by `dir`; some
   spawn child effects (type 27 → three type-28 at the end of its life;
   type 19 → spark type 1).
3. **Effect vs effect** (not gated by pause or owner check up front): two
   effects with hit boxes that overlap. If one has `reflect` (+0x32, type
   30) it takes the other over: owner, `dir` flipped. Same owner → skip.
   Otherwise every non-persistent one (`persist` +0x2a clear) gets its own
   spark (type 1) and is removed.
4. **Effect vs player** (has hit box, `damage > 0`, not the owner, no pause
   gate): rectangles `effect(x+l, y+t, x+r, y+b)` and the player's hurt
   box must overlap (strict). Then:
   - victim blocking → `animTimer := 1`, sound 10, block spark (type 14),
     push ±10, chip `param` (+0x23 = Round(damage/4), or /15 for types 8,
     31, 32) × damageLevel, KO if health ≤ 0 (sound 9, move 10); removed
     unless persistent; hit box off; `hitStop = 2`.
   - else (`invuln == 0`): victim faces the projectile, sound 9, hit spark
     (type 1), red flash 4 with config line 8 (1/3), blood with config
     "sangue" (1/3), attacker score +10, combo timer 50, damage
     `damage × damageLevel`, the **victim** gains 1 meter unit, move 10
     when `knock` (+0x29) or vy ≠ 0 or health ≤ 0, else move 9, with
     `hitstun = 8`; first-hit bonus; drawn on top; `hitStop = 5`. Type 8
     and 20 instead force state 19 (`vy = −60` / `−20`, `rotStep = ±20`,
     hitstun 10 / 6, type 20 also clears invuln). Removed unless
     persistent; hit box off; frame selector re-run for the victim.
   - `knock` at spawn: 1 in 5 (`Random(5) == 0`) for types 7, 9, 12, 13,
     15, 18, 19, 22, 26, 29, 33; always for 8, 20, 23, 30, 31, 32, 34.
   - types 3 and 4 (stage pickups, from the passing object on stages 1–2)
     after phase 13, victim alive, not during the entrance: **heal** the
     player who touches them (`health += k × damageLevel`, none in the
     challenge mode), sound 11, popup type 5, removed.
5. `phase++` when not paused (unless `life == 99` = infinite); the slot is
   freed when `phase > life`. A second counter (`c4`/`c5`, only for types
   with `c5` ≠ 0: 27, 28, 30, 31, 32) runs right after the life test, also
   when not paused (decomp `0x48edfc` lines 889–915):
   ```
   if c5 != 0 and not paused:
       c4++
       if type == 27:                       # type read after the life test
           if c4 >= c5 - 2: hasHitbox = 1   # only type 27 arms here
           if c4 >= c5: spawn(28,"A",owner, x-30,y+20); spawn(28,"A",owner, x+30,y+20);
                        spawn(28,"A",owner, x,y-40)
       if c4 >= c5: type = 0                # slot freed
   ```
   so 27 lives 35 ticks and then bursts into three type-28 effects, which
   are freed after `c5` = 20 ticks (30: 25, 31/32: 70) even though 28 and 30
   loop their phase and are never freed by `life`. If the life test already
   freed the slot the burst does not happen. The same function adds extra
   `c4++` steps earlier in the slot (not gated by the pauses): type 28 at
   c4 = 3, 6, 9 (hit box re-armed) and the orbiting 31/32 every 10th tick
   (rule 4 below); a type 28 therefore hits again every ~3 ticks of c4 and a
   31/32 every 10.

The FPU literals that the decompiler shows as `_DAT_0049056x` are 32-bit
floats read from the binary: `0x490568` = 1.0, `0x49056c` = 3.0, `0x490570`
= 70.0, `0x490574` = 80.0, `0x490578` = 10.0, `0x49057c` = 2.0, `0x490580`
= 7.0, `0x490584` = 5.0, `0x490588` = 0.0, `0x49058c` = 102.0, `0x490590` =
20.0, `0x490594` = 30.0, `0x4905a4` = 40.0. So: dust (type 2) rises 1.0 px
per tick, floating text (type 5) rises 5.0 px per tick, pickups heal **20 ×
damageLevel** (type 3) and **30 × damageLevel** (type 4), a projectile hit
gives the victim `1.0 × g_meterUnit` of meter, type 27 spawns its three
type-28 bursts 20 px up and ±30 px sideways. Per-type motion statements
are in `06a` with these values to substitute.

## Motion rules [decomp, generic part of `desenhaefeitos`]

Applied every tick to every live slot, after the per-type sprite/phase
table and before drawing, unless `g_superPause`/`g_hitStop` is set:

1. **Horizontal**: `dir == 1 → x += speed`, `dir == 2 → x −= speed`
   (`speed` = field +0x21, e.g. 8 "A" / 11 "B" for fireballs, 10/13 for
   Bad Stone, 0 for stationary effects). Fireballs loop their phase and
   have `life` 5 but are kept alive by the loop `phase := 0`; they fly until
   they hit something, the owner state ends, or the **off-screen cull** (rule 5) removes them.
2. **Vertical specials**: type 19 (Bruxa air Chaos Flare) also `y +=
   speed` (diagonal down); type 20 `y −= 7 + Random(13)` (rises
   erratically); type 2 dust `y −= 1`; type 5 text `y −= 5`; type 24 wobbles
   `x` with `cos(phase)` and `y −= 1`.
3. **Gravity** (`f9` flag set at spawn for types 3, 4, 6, 21, 22): `y +=
   vy; vy += 4` (`vy` = i16 at rec+0x26, initial −22 for pickups/debris,
   −13 "A" / −17 "B" for Bad Stone), until `y > groundY − 5`: then `y =
   groundY − 5`, `speed = 0`, `vy = 0` (the flag stays set, the clamp just repeats); type 22 (Bad Stone) is flagged done
   and loses its hit box on landing.
4. **Orbiting** (types 31, 32 — Neves' Phoenix Arrow / Shining Crystal
   Bit, `0x48f76f..0x48f8f1`, not gated by the pauses): the position is recomputed
   from the owner every tick, with `k = c4 / 3.0` (`0x49056c`), `r = Round(70.0 · Cos(k))`
   (`fcos` at `0x40286c`, 70.0 at `0x490570`): type 31 `x = owner.x + r`,
   type 32 `x = owner.x − r`, both `y = owner.y − 80.0 + r` (`0x490574`; the
   y term uses the **same** cosine, so each effect sweeps a diagonal through
   the owner's chest, 31 up-left/down-right, 32 mirrored, ±70 px in x and y,
   period 2π·3 ≈ 19 ticks of c4, which advances ~1.1 per tick). Whenever
   `c4 % 10 == 0` (`c4/10` equals its truncation, `0x40287c`): `hasHitbox := 1`,
   `c4++` and, only then, the slot is freed if the owner's state is not 29 (so the
   effects can outlive the state by up to 10 ticks). Type 30 is freed on every tick the owner
   is not in state 26. Type 34 loses its hit box from phase 14, not gated.
   Type 24 (`0x48f920`, not gated): `x += Round(2.0 · Cos(phase))` (`0x49057c`), `y −= 1.0`.
   Type 8 (`0x48f6fb`, not gated): at phase 2 `FUN_004769c4(slot, −45, −150, 0, 45)` and at phase 5
   `(−45, −220, 0, 45)` — it stores `l, t, r, b` = (−45, −150 / −220, 45, 0) in record `+0x10..+0x1c`
   (the stdcall pushes `0x2d` then `0`, which land in `r` and `b`); at phase 6 and 12
   `hasHitbox := 1` and `phase++` (the pillar skips phases 7 and 13 for display and counts
   them for `life`). Type 28 re-arms at c4 = 3, 6, 9 and steps c4.
5. **Off-screen cull** [decomp, `0x48edfc`, decomp lines 917-934, last statements of the slot, not gated by
   `g_hitStop`/`g_superPause`]: after the life/`c5` handling every live slot is freed (`type := 0`) when
   `x < camX − 50`, `x > camX + 450` (0x1c2) or `y < camY − 50` (`g_counterE`). There is no lower
   bound other than rule 3's landing (a stray fireball can sink below the screen, it is only culled by
   x). Type 19 (Chaos Flare) is additionally freed, with a hit spark (type 1) at its position, as soon
   as `y + hitBoxBottom > groundY` (the ground check uses the box bottom, +0x1c). Camera: `camX` and
   `camY` are clamped to 0..200 and 0..20 (`0x4a47f1`); the port used the stage image size, now the same fixed limits.
6. Debug: with `g_debugBoxes` the effect's box is drawn (`barra`).
7. Drawing: sprite `Efeitos[sprite]` at `(x − camX − w/2, y − camY − h/2)`, or `y − camY − h` when `f8`
   (centred horizontally; centred on y by default, `f8` flag = bottom-anchored, standing on y),
   mirrored list when `dir == 2`; with `done == 1` the effect is drawn on even `g_ciclo` only (the
   blink counter at `0x4b44c0`, the byte the decompiler calls `g_counterD`; not the tick counter).
   The draw is the statement right after the generic motion, **before** the type-5 rise,
   the collisions, the life test and the cull, so an effect that is removed in that same slot pass
   (hit, cancel, last life tick) is still drawn once, and one removed by the owner-state tests above
   is too. The port snapshots the draw data at that point (`fx_snapshot`) and `osc_fx_render` replays it.
   Type 5 draws no sprite: `texto(Round(x) − camX, Round(y) − camY, MemoTexto.Get(5), size 10, 'c',
   RGB(80, 255, 140))` ("VITALIDADE"), blinking with `done`, and rises 5 px per tick when not
   super-paused (a hit stop does not stop it).

## Who spawns what [decomp, from `05a`]

| type | spawned by | meaning |
|---|---|---|
| 1 | hits, cancels | hit spark (3 phases) |
| 2 | landings, dashes | dust puff (15 ticks) |
| 3 / 4 | stage object | dropped pickup (80 ticks, hit box, heals) |
| 5 | pickups | floating text (20 ticks) |
| 6 | ? | 99-tick looping sprite, speed 5 |
| 7 "A"/"B" | Chaves state 25 | Power Wave (life 5 looped, speed 8/11, damage 12, box −25..25 × −38..0) |
| 8 | Chaves state 29 | Power Geyser pillar (23 ticks, damage 40, knockdown) |
| 9 "A"/"B" | Madruga / Chiquinha state 25 | fireball (damage 12) |
| 10 | hits | blood |
| 11 | Quico | ? (15 ticks) |
| 12 "A"/"B" | Quico | Hi Sho Ken |
| 13 "A"/"B" | Florinda | Souhakkei |
| 14 | blocks | block spark (6 ticks) |
| 15 "A"/"B" | Girafales | Tiger Shot (high) |
| 16 | Chaves | ? |
| 17 | Bruxa / Jaiminho | ? (10 ticks) |
| 18 "A"/"B" | Bruxa | Chaos Flare |
| 19 "A"/"B" | Bruxa | air Chaos Flare (spawns spark) |
| 20 "A" | Bruxa | Demon Horde? (knockdown) |
| 21 | breakable object | debris (23 ticks, random dir) |
| 22 "A"/"B" | Godines | Bad Stone (speed 10/13, arc) |
| 23 "A"/"B" | Godines | ? |
| 24 | Bruxa | ? |
| 25 | ending cutscene | ? |
| 26 "A"/"B" | Jaiminho | Psycho Shot |
| 27 "A"/"B" | Jaiminho | Psycho Field (spawns 3 × 28) |
| 28 "A" | (child of 27) | Psycho Field burst (damage 2, box ±40) |
| 29 "A"/"B" | Neves | Psycho Ball |
| 30 "A" | Neves | Psycho Reflector (damage 25) |
| 31 "A" | Neves | Phoenix Arrow? |
| 32 | Neves | Shining Crystal Bit (life 6, damage 35) |
| 33 "A"/"B" | Glória | KaChoSen (fan) |
| 34 "A" | Glória | ? (life 20, damage 18, big box) |
| 35 "A" | Glória | ? |
| 36 "A" | Glória | ? |

The "?" rows need the move names matched against `04-moves.md` by reading
the spawning state in `05a` — pending.

## Spawn calls of the frame selector (`06e-selector-spawns.txt`) [objdump]

The frame selector `0x47d21c..0x48dcbf` calls `FUN_00476a04` at 106 sites
(the first decompiled listing showed only 61 and merged the rest). Register
convention: `al` type, `edx` variant string (`"A"`/`"B"`/nil), `cl` owner (0 =
none; dust, sparks and the Bruxa/Chaves effects 1, 2 spawn without owner),
stack: `x` then `y` as doubles. Every site sits under a guard chain of
character (`+0x10`), state (`+0x6b`), animation timer (`+0x74`, `==`, `>=`,
`>`, `<` or none), facing (`+0x6d`), hit level (`+0x6f`) and sometimes the
current frame (`+0x6c`) or an even-timer test. Facing and level pairs are
separate call sites, with x offsets of both signs; one site (Bruxa's second
variant "B") uses -60 where its mirror uses +40. Some sites sit inside loops
that spawn 3, 5 or 8 puffs. Jaiminho's super (state 28, timer 10) spawns five
type-17 effects, one on him and four at `g_camX + 0x50/0xa0/0xf0/0x140`,
`g_camY + 0x6e`. Paty's moves 20 and 21 spawn five dust puffs from `setMove`
(`0x47b0bb`). The old table keyed rows by (char, state, timer) with `*` meaning
timer 0, so lunges (Chaves' Burn Knuckle, type 16 from timer 9 on), dust
trails and the Jaiminho extras never fired, and both hit levels of the
projectile specials shared one offset.

Drawing (`desenhaefeitos` `0x48fb0e`): the sprite's left edge is
`Round(x) - Round(w / 2)` and its top `Round(y) - Round(h / 2)`; with the
spawn flag `f8` (types 2, 7, 8, 23, 34) the top is `Round(y) - h` (bottom
anchored on `y`).

## desenhaefeitos vs port inventory

Statement blocks of `desenhaefeitos` (`0x48edfc`, decomp
`re/out/decomp/funcs/0048edfc_TForm1_desenhaefeitos.c`, FPU operands read from the
disassembly) in execution order against `update_fx` (`src/fight/effects.c`) / `osc_fx_render` (`src/render/fight_draw.c`).
Status is that of the port **before** this audit: **ported** (matched), **missing**,
**different**; every missing or different block is now ported (column "now").
Pause gates: `P2` = `g_superPause` (`0x4b57f8`) and `g_hitStop` (`0x4b57f9`) both
(`frozen` in the port), `P1` = only `g_superPause`, `-` = not gated.

| # | decomp | block | gate | before | now |
|---|---|---|---|---|---|
| 1 | 51-53 | loop over the 80 slots, skip `type == 0` | - | ported | ported |
| 2 | 53-63 | type 1 sprites by phase 0 / 1 / 3 | - | ported | ported |
| 3 | 64-79 | type 2 sprites 0 / 3 / 6 / 9 / 12 | - | ported | ported |
| 4 | 80-83 | type 2 `y -= 1.0` | P2 | ported | ported |
| 5 | 84-91 | types 3 / 4: sprite 10 / 11, `done` when `life − 20 <= phase` | - | sprite ported, `done` **missing** | ported |
| 6 | 92-94 | type 5: `done` when `life − 10 <= phase` | - | **missing** | ported |
| 7 | 95-97 | type 6 sprite 12 | - | ported | ported |
| 8 | 98-109, 122-133, 182-193, 245-256 (types 7, 9, 12, 13, 15, 18, 19, 20) | sprites 0 / 1 / 2 and `phase == 3 → phase := 0` | - | sprites ported, loop **different** | ported |
| 9 | 110-121 | type 8 sprites 0 / 2 / 5, `done` at phase 5 | - | sprites ported, `done` **missing** | ported |
| 10 | 134-181, 194-232 (types 10, 11, 14, 16, 17) | sprite tables, no loop | - | ported | ported |
| 11 | 233-268 | types 18 / 19 / 20 sprites 0 / 1 / 2 (+ loop at 3) | - | see 8 | ported |
| 12 | 269-307 | type 23 sprites (alternating 57 / 58 every 2 phases, 79 at 22, 60 at 24) | - | ported | ported |
| 13 | 308-313 | type 24 sprite: phase `<` / `>=` life − 10 → 61 / 62 | - | **different** (always 62) | ported |
| 14 | 314-322 | type 25 sprite: `< 12` → 63, `12..23` → 64, `>= 24` → 65 | - | **different** (always 65) | ported |
| 15 | 323-346 | types 26 (66, 67, loop 6), 27 (68, 69, loop 6) | - | sprites ported, loop **different** | ported |
| 16 | 347-373 | type 28 (70, 71, loop 4), type 29 (72, 73, 74, loop 6) | - | sprites ported, loop **different** | ported |
| 17 | 374-394 | type 30 sprites 0 / 1 → 75, 4 → 76, loop 5, then `c4 >= c5 − 10 / − 7 / − 4` → 77 / 78 / 79 | - | phase rows ported, the c4 conditions **different** (always 79) | ported |
| 18 | 395-406 | type 31 sprites 2 / 4 / 5 → 72 / 73 / 74, loop 6 | - | ported (loop **different**) | ported |
| 19 | 407-421 | type 32: `phase < 2` → 72, 2 → 72, 4 / 5 → 73 / 74, loop 6 | - | **different** (an any-phase row also set 72 at phases 3 and 6) | ported |
| 20 | 422-439 | type 33 sprites 80..83, loop 6 | - | sprites ported, loop **different** | ported |
| 21 | 440-460 | type 34 sprites 84..90 every 3 phases | - | ported | ported |
| 22 | 461-475 | **type 35** sprites '[' `\` ']' '^' '_' = 91..95 at phases 0 / 2 / 4 / 6 / 8 | - | ported (checked) | ported |
| 23 | 476-490 | **type 36** sprites '`' 'a' 'b' 'c' 'd' = 96..100 at phases 0 / 2 / 4 / 6 / 8 | - | ported (checked) | ported |
| 24 | 491-496 | type 8 at phases 2 / 5: `FUN_004769c4(slot, −45, −150 / −220, 0, 45)` = box `l, t, r, b` := −45, −150 / −220, 45, 0 | - | **missing** | ported |
| 25 | 497-500 | type 8 at phases 6 and 12: `hasHitbox := 1`, `phase++` | - | **missing** | ported |
| 26 | 501-505 | type 28 at c4 = 3, 6, 9: `hasHitbox := 1`, `c4++` | - | **missing** | ported |
| 27 | 506-519 | type 31: `x = owner.x + Round(70·Cos(c4/3))`, `y = owner.y − 80 + Round(70·Cos(c4/3))` | - | **missing** | ported |
| 28 | 520-533 | type 32: `x = owner.x − Round(70·Cos(c4/3))`, same `y` | - | **missing** | ported |
| 29 | 534-546 | types 31 / 32 at `c4 % 10 == 0`: `hasHitbox := 1`, `c4++`, free if owner state ≠ 29 | - | **different** (freed on every tick the owner state ≠ 29, gated by P2) | ported |
| 30 | 547-549 | type 30 freed when owner state ≠ 26 | - | **different** (gated by P2) | ported |
| 31 | 550-552 | type 34 loses its box from phase 14 | - | **different** (gated by P2) | ported |
| 32 | 553-560 | type 24: `x += Round(2.0·Cos(phase))`, `y −= 1.0` | - | **missing** | ported |
| 33 | 561-577 | gravity (`f9`): `y += vy; vy += 4`; clamp at `groundY − 5` with `speed = vy = 0`; type 22 done + box off | P2 | ported (the port also cleared the `f9` flag; equivalent, y ends at the ground either way) | ported (flag kept) |
| 34 | 578-596 | `dir` motion, type 19 `y += speed`, type 20 `y += Random(13) − 7` | P2 | ported | ported |
| 35 | 597-606 | debug box (`DAT_004b58a5`) for every live slot | - | **different** (only armed slots) | ported |
| 36 | 607-656 | sprite draw (not for type 5): blink with `done` on even `g_ciclo`, `f8` bottom-anchored, mirrored list for `dir == 2` | - | **different**: drawn by the render pass after the whole update (a slot freed in the pass is never seen, type-5 rise already applied), blink with the tick counter | ported (`fx_snapshot`) |
| 37 | 657-689 | type 5 floating text "VITALIDADE" (`MemoTexto.Get(5)`, size 10, centred, RGB 80,255,140), blinking with `done` | - | **missing** | ported |
| 38 | 690-692 | type 5 `y −= 5.0` | P1 | **different** (also stopped by the hit stop) | ported |
| 39 | 694-735 | effect vs effect (overlap strict, both armed, both `damage != 0`, not self; reflector takes over; same owner skip; non-persistent leave a spark and are freed) | - | ported | ported |
| 40 | 736-888 | effect vs player blocks A (guard), B (hit), C (pickup 3 / 4) | - | ported | ported |
| 41 | 694-888 | the slot continues to its end after being freed by 25-31 or 39 (a freed effect with a box can still hit this tick; the draw still happens) | - | **different** (the port jumped to the next slot) | ported |
| 42 | 889-892 | `phase++` unless `life == 99`, freed when `life < phase` | P2 | ported (the loop wrap sat here, see 8) | ported |
| 43 | 893-916 | `c5` countdown: `c4++`, type 27 arms at `c5 − 2` and bursts three 28s at `c5`, freed at `c5` | P2 | ported | ported |
| 44 | 917-933 | cull (`x` outside camX − 50..camX + 450, `y < camY − 50`), type 19 box bottom below the ground → spark | - | ported | ported |
| 45 | `0x476a04` (spawn) | the sprite byte (+3) is **not** written by the spawn: it keeps the previous occupant's value; only type 31 shows it (phases 0 and 1 have no sprite row) | - | **different** (zeroed) | ported |

Totals of the 45 rows: 17 ported (33 and 42 only lose a no-op / the loop wrap that moved to row 8),
10 missing (5, 6, 9 in part, 24-28, 32, 37) and 18 different (8, 11, 13-20, 29-31, 35, 36, 38, 41,
45); all now ported. Details of the new rules are in "Life cycle" and "Motion rules" above; the
constants read from the disassembly are `0x49056c` = 3.0 (the Cos divisor), `0x490570` = 70.0 (radius),
`0x490574` = 80.0 (y offset), `0x490578` = 10.0 (the `c4 % 10` test), `0x49057c` = 2.0 (type 24),
`0x490584` = 5.0 (type 5 rise) and the `fcos` routine `0x40286c`.
