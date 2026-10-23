# 06 — Effects and projectiles

Legend: **[decomp]** Ghidra pseudocode (not source), **[infer]**
interpretation, **UNKNOWN** not established. Generated detail:
`06a-effects.txt` (regenerate with `re/tools/effects.py`): per effect type
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
   freed when `phase > life`. A second counter (`c4`/`c5`) drives delayed
   actions (e.g. type 27 arms its hit box 2 ticks before the end).

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
   Bad Stone, 0 for stationary effects). Nothing stops a projectile at the
   screen edge; it lives until `phase > life` (fireballs loop their phase
   and have `life` 5 but are kept alive by the loop `phase := 0` — their
   lifetime is the loop, so they fly until they hit something or the owner
   state ends; UNKNOWN whether an off-screen cull exists elsewhere).
2. **Vertical specials**: type 19 (Bruxa air Chaos Flare) also `y +=
   speed` (diagonal down); type 20 `y −= 7 + Random(13)` (rises
   erratically); type 2 dust `y −= 1`; type 5 text `y −= 5`; type 24 wobbles
   `x` with `cos(phase)` and `y −= 1`.
3. **Gravity** (`f9` flag set at spawn for types 3, 4, 6, 21, 22): `y +=
   vy; vy += 4` (`vy` = i16 at rec+0x26, initial −22 for pickups/debris,
   −13 "A" / −17 "B" for Bad Stone), until `y > groundY − 5`: then `y =
   groundY − 5`, `speed = 0`, `vy = 0`; type 22 (Bad Stone) is flagged done
   and loses its hit box on landing.
4. **Orbiting** (types 31, 32 — Neves' Phoenix Arrow / Shining Crystal
   Bit): position is recomputed from the owner each tick, `x = owner.x ±
   R·cos(angle)`, `y = owner.y − spriteH + …`, with `angle` = counter `c4`;
   every 10th tick the hit box is re-armed and the counter advances; the
   effect is removed when the owner leaves state 29 (and type 30 when the
   owner leaves state 26). Type 34 loses its hit box after phase 13.
5. Debug: with `g_debugBoxes` the effect's box is drawn (`barra`).
6. Drawing: sprite `Efeitos[sprite]` at `(x − camX − w/2, y − camY − h/2)`, or `y − camY − h` when `f8`
   (centred horizontally; centred on y by default, `f8` flag = bottom-anchored, standing on y),
   mirrored list when `dir == 2`; types with `done == 1` draw blinking.

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
