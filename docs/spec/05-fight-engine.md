# 05 — Fight engine: timing, states, moves, physics, hits

Legend: **[decomp]** read from Ghidra pseudocode (analysis artifact, not
source), **[infer]** interpretation, **UNKNOWN** not established. Generated
tables (regenerate with the scripts in `re/tools/`):

- `05a-anim-tables.txt` — per character: `(state, animTimer) → frame`,
  hurt boxes per frame, hit boxes per `(state, timer)`, and other actions
  done by the frame selector (position changes, effect spawns).
- `05b-move-setup.txt` — per move id: allowed previous states, new state,
  duration, meter gain, sound.
- `05c-frame-offsets.txt` — per character and frame: horizontal draw offset.

## Tick and timing [decomp]

- The whole game runs from `DXTimer1.OnTimer`. At the top of every tick
  `DXTimer1Timer` does `DXTimer1.Interval := g_speedDelay` (`0x4b44d0`,
  default **28 ms**, `-`/`+` keys change it 0..80). So the nominal tick is
  28 ms ≈ **35.7 ticks/s**, and "speed" is just the timer interval. The
  `FPS=` debug line prints `DXTimer1.FrameRate`.
- Mash counters: every button press adds 6 to `mashP` (punch buttons) or
  `mashK` (kick buttons) of that player; both decay by 1 per unpaused tick
  and are capped at 15. Mash specials need > 5 (see 04).
- Game time: `g_timerA` (`0x4b44e3`) is the on-screen seconds; `g_timerB`
  (`0x4b44e4`) counts down from **25** every tick and decrements `g_timerA`
  when it reaches 0. So one game "second" = 25 ticks (0.7 s at 28 ms).
- Round length comes from config line 1 (`g_cfg1_mode` 1..4 → 30 / 60 /
  90 / 99 seconds).
- Pauses: `g_hitStop` (`0x4b57f9`, "GOLPE PAUSA") and `g_superPause`
  (`0x4b57f8`, "PAUSA ESPECIAL") freeze movement, animation timers and
  input while non-zero; `g_hitStop` decrements once per tick at the end of
  the player loop. `g_noPush` (`0x4b44e0`) disables input, push and AI
  during round intro/outro.
- Random numbers: `System.Random` (LCG `seed*0x8088405+1`, time-seeded).

## Player state machine [decomp]

`players[p].state` (+0x6b), `animTimer` (+0x74), `animDur` (+0x76),
`frame` (+0x6c). Every tick, for each player: if `animDur != 99` and no
pause, `animTimer++`; when `animTimer >= animDur` the move ends: state := 1,
`attacking` := 0, `animTimer` := 0, several flags cleared; if `y < groundY`
the player is put in state 12 (jump) with `inAir` = 1; facing is
recomputed from the x order; `y` is clamped to the ground. Duration 99
means "lasts until something else changes the state" (walk, crouch, jump).

| state | name | entered by (move id → see below) | duration |
|---|---|---|---|
| 1 | idle | end of any move; release of walk/crouch | — |
| 2 | walk | move 1 (left) / 2 (right): `x += ±speed` with push | 99 |
| 3 / 4 / 5 / 6 | stand LP / LK / HP / HK | moves 3–6 from 1, 2 (also from 21/22 block, and from an earlier normal once `animTimer > 1`: **chain cancels**) | 6 / 6 / 11 / 11 |
| 7 | crouch | move 7 (down held) | 99 |
| 8 / 9 / 10 / 11 | crouch LP / LK / HP / HK | moves 3–6 from 7 (and chain from 8, 9) | 6 / 6 / 11 / 11 |
| 12 | jump | up pressed in 1, 2, 21, 22 with `vy == 0` and `inAir == 0`: `vy = -32`, `inAir = 1`, `dir` = 1 neutral / 2 left / 3 right, `animDur = 99`; also the CPU "jump away" | 99 |
| 13 / 14 / 15 / 16 | jump LP / LK / HP / HK | moves 3–6 from 12 | 15 / 15 / 11 / 11 |
| 17 | hit (light) | move 8, only when `vy == 0` | 8 |
| 18 | hit (heavy) | move 9 | 8 |
| 19 | knocked down (airborne) | move 10: `vy = -19`, `inAir = 1`, `invuln = 1`, `attacking = 0` | 50 |
| 20 | lying on the ground | move 11 on landing from 19: `vy = -13` (small bounce), `invuln = 1` | 25 |
| 21 / 22 | stand block / crouch block | moves 12 / 13 from 1, 2, 7, 21, 22, or from 20 in its last 3 ticks; sets `isBlocking` = 1 and frame 31 / 32 | 11 |
| 23 | taunt / special stance | move 14 (char-specific: from 1, 2, 7) | 250 |
| 24 | throw? [infer] | move 15 from 1, 2 | 20 |
| 25 … 30+ | specials and super (per character) | moves 16–26, see 04 and `05b` | per move |

Hit/knockdown reactions also set `hitstun` (+0x72) = 4 by default, with
larger values (13, 18, 20, 25) for specific heavy specials, and may set
`vy` to −30 / −26 (launchers) and `rotStep` = ±20 (spinning fall).

### Move start (`0x4775fc`) [decomp]

`setMove(p, moveId)` is the only writer of `state` for moves. Common part
(all characters): ids 1–13 as in the table above. Each normal attack adds
special meter: `meter += g_meterUnit × 1.0` (light) or `× 2.0` (heavy),
capped below 102, and queues sound 5 (light) / 6 (heavy). Per-character
part: ids 14–26 (`05b-move-setup.txt`). Specials add `× 5.0`–`× 7.0`.
Projectile specials are refused while the player already owns an effect of
that type (`sub_476454`), unless `Bruxa.dat` is present (`0x4b58b0`).
The super (id 24) requires `health ≤ 15` **or** `stocks > 0`; it consumes a
stock (`+0x58`), sets `g_superPause = 22`, sound 19.

### Special meter and stocks [decomp]

- `meter` (+0x60, double) fills to 102; at 102 with `stocks < 3`:
  `stocks++`, meter := 0, sound 13. At 3 stocks the bar is shown full.
- Config line 2 (`g_cfg2`): 2 or 4 → P1 always has 3 stocks; 3 or 4 → P2
  (handicap / infinite super option) [infer on the label].
- On a clean hit the attacker gains `× 2.0`, the victim `× 1.0`.

## Movement and physics [decomp]

- World: x in pixels (double), y = feet (double), `groundY = 300`. Camera
  `g_camX`/`g_camY` is subtracted when drawing; the playable band is
  `camX + 30 … camX + 370`.
- Walk: `sub_47648c(p, dx)` adds `dx` (±`speed`, +0x80: 8.0, 8.5 or 7.5 by
  character) and resolves body overlap: if the hurt boxes of both players
  intersect (and neither is in state 20/23), the other player is pushed 5
  px and the mover is placed at the edge of the other's box; both are kept
  inside the band while alive.
- Jump: while `inAir`, each unpaused tick `y += vy; vy += g_gravity`
  (`0x4b449c`, FormCreate sets **4**), and x moves by **±8.0** (a constant,
  not the walk speed) for `jumpDir` 2/3. Landing (`y > groundY`) resets `y`,
  `vy`, `inAir`, `rot`, `rotStep`, `jumpDir`, `attacking`, `damage`,
  `knockdown`, state := 1, `animTimer` := 0, faces the opponent and drops
  five dust puffs (type 2 at `x − 20 + Random(40)`, `y + Random(10)`).
  `animDur` is left alone. [0x4a95a5]
- Round 1 entrance [0x4a44cb]: while `roundNo == 1 && roundStart == 0`,
  P1 is put at x 0 with `jumpDir` 3 and P2 at x 600 with `jumpDir` 2, both
  in the air with `vy = −25`, state 12, frame 21: they jump in from the
  stage edges (clamped to the band).
- Walk (moves 1/2, `setMove` 0x477634): only from states 1/2; calls
  `sub_47648c(∓speed)`, state 2, `animDur` 99, keeps `animTimer`. Every
  `setMove` first clears `blocking`.
- Knockdown (state 19): x moves by `hitstun` (+0x72) px per tick, toward
  the back of the victim's facing, written directly (no push). On landing
  → move 11 (state 20), `y = groundY − 10`, sound 12, five dust puffs;
  characters 7 and 10 also set the camera shake (`0x4b449a = 20`, the
  camera jumps by `−10 + Random(20)` each tick) and 20 more puffs at
  `x − 30 + Random(60)`.
- State 20 slides by `hitstun` through `sub_47648c`; with blood on
  (config 4) `Random(4) == 0` and `animTimer == 5` sprays blood (type 10)
  30 px behind. States 17/18 slide the same way with dust 1 tick in 2.
- Facing is set in `desenhalutador` for states 1/2/7 while `vy == 0`, at
  the end of a move, on landing and when hit.

## Order of a fight tick (state 7) [decomp]

1. Pending sound `g_sndIndexA` from the last tick is played and cleared
   (the last writer of a tick wins).
2. Round-1 entrance, last-fight dialogue check, `g_superPause−−` (while it
   runs the stage is replaced by `Icone[7]` tiled 100 px and shifted by
   20/40/60/80 when the counter divides by 2/3/4/5), camera (midpoint,
   then shake, then clamps), HUD and clock.
3. Input of the humans (walk, crouch, jump, then all `golpe` buttons, then
   all `setMove 3..6`), `sequenciagolpe` P1, P2, then `cpu` for CPU
   players.
4. Stage objects; fighters drawn (frame selector, effect spawns, spin)
   with the one whose +0x91 flag is set on top; `defendergolpe`;
   `desenhaefeitos` (effects and projectile hits).
5. Time over, round intro (held while the dialogue runs), round end.
6. Body hits for attacker P1 then P2, `g_hitStop−−`, move clocks, ground
   clamp, physics.
7. Dialogue box, `g_ciclo++` (not while paused), red flash
   (`0x4b449b`, full-screen red on even `g_ciclo`).

## Body hits [decomp, 0x4a85ec]

Hit box of the attacker against the hurt box of the victim, both rounded
to integers, strict overlap, attacker `attacking`, victim not `invuln`, no
pause. A low attack (state 9/11) opens a standing guard (21), a jump attack
(13..16) a crouching one (22). Blocked: victim `animTimer = 1`, sound 10,
spark 14 at the hit-box centre, victim pushed 10 px back, chip × damage
level, KO → sound 9 + move 10, hit stop 2+Random(4) (level 1) or
4+Random(4) (level 2). If the guard is down (also after a chip KO): victim
faces the attacker, +5 score, meter +2/+1 units, combo (reset when the
timer was ≤ 35, timer 44, 50 for states 11/16 and some specials), damage,
red flash (config 7) 1 in 5 (level 1) or 1 in 2 (level 2), KO sound 8, spin
±20 for the throw specials, spark 1, blood 1 in 5 on level 2, move 9 for
states 4/6/8/9/10 else 8, move 10 if the attack knocks down or the victim
is airborne, slide `hitstun` 4 (bigger for some specials), hit sound 7/8/9
by level, first-hit bonus, attacker drawn on top, both `attacking` cleared,
multi-hit specials chain (char 2: 28→29, 30→31; char 3: 29→30; char 7:
29→30), hit stop 5+Random(4) / 7+Random(4), frame selector for the victim.
Afterwards a victim at the band edge pushes the attacker 12 px back.

## Hit boxes [decomp]

Defined by the frame selector `0x47d21c` through two helpers:

- `sub_476870(p, frameIndex, left, bottom, right, top)` — **hurt box**
  (+0x20..+0x2c) when the player's current frame equals `frameIndex`;
  mirrored when facing left (`left = -right`, `right = -left`).
- `FUN_004768f8(p, state, timer, p4, damage, level, attacking, bottom,
  right, top, left)` — **hit box** (+0x30..+0x3c) when `(state, animTimer)`
  match; also sets `attacking` (+0x68) = p7, `hitLevel` (+0x6f) = level
  (1 light, 2 heavy; 0 keeps the previous), `damage` (+0x70), and +0x71 =
  p4 (chip/guard damage, see below). A row with all zeros and `attacking =
  0` ends the active window.

Table completeness: the decompiled listing dropped the hurt-box calls whose
frame is `base + N` with the arguments held in locals (about 120 of 305) and
all of character 1's hit boxes (21). `05a-anim-tables.txt` now carries every
`sub_476870` / `FUN_004768f8` call site of the frame selector (15 per-character
blocks, 305 hurt and 379 hit rows). Frames without a row keep the previous hurt
box, so a missing standing-frame row left a stale box after any jump or guard.

Box coordinates are offsets from the sprite's top-left corner `(x, y −
spriteH)`, in sprite pixels (debug overlay: `g_debugBoxes`).

### Collision test (main loop) [decomp]

For attacker `a` and victim `v`, once per tick, when `a.attacking == 1`,
`v.invuln == 0`, no pause: world rectangles
`A = (a.x + hitL, a.y − a.spriteH + hitT, a.x + hitR, a.y − a.spriteH + hitB)`
and `V = (v.x + hurtL, v.y − v.spriteH + hurtT, …)` must overlap strictly
(`A.left < V.right && V.left < A.right && A.top < V.bottom && V.top <
A.bottom`).

- Crouching kicks (states 9, 11) break a **stand** block (21); jumping
  attacks (13–16) break a **crouch** block (22) — `isBlocking` is cleared
  before the resolution.
- **Blocked** (`v.isBlocking == 1`): victim `animTimer := 1` (block stays
  up), sound 10, block spark (effect 14), victim pushed back
  (`sub_47648c`), chip damage `+0x71 × g_damageLevel`, `hitStop =
  Random(4) + 2` (light) / `+ 4` (heavy). If health ≤ 0 → KO (sound 9,
  move 10).
- **Clean hit**: `attacking := 0` (one hit per attack), facing fixed,
  human attacker gets +5 score, meter gains, hit counter `+0x95++` (combo;
  reset when the combo timer `+0x96 < 36`) and combo timer := 44 (50 for
  states 11, 16 and some specials), damage `+0x70 × g_damageLevel`
  (`g_damageLevel` = `0x4b5888`, config line 3, default **0.9**, debug
  label "NIVEL DE DANO"), victim knockback velocity fields reset, hit
  spark (effect 1) at the box corner, blood (effect 10) with 1/5 chance on
  heavy hits when config line 4 ("sangue") is 1 or always with
  `Satanas.dat`, screen shake flag `0x4b449b = 6` with config line 7, sound
  7 / 8 / 9 by level, `hitStop = Random(4) + 5` (light) / `+ 7` (heavy).
  Victim reaction: move 9 (heavy hit) if the attacker is in state 4, 6, 8,
  9 or 10 else move 8; then move 10 (knockdown) if the attacker's
  `knockdown` flag (+0x69, "DERRUBA") is set or the victim is airborne.
  `hitstun` (+0x72) := 4 or a per-special value. If health ≤ 0 → KO.
- First hit of the round: message "FIRST HIT"-style flag `0x4b57f4` and
  +300 score for a human attacker.

## AI difficulty [decomp, from the x86 disassembly of `cpu` and `defendergolpe`]

Every CPU decision is `Random(round(C × k)) == 0`, i.e. probability
`1 / round(C·k)` per tick, where `C` is a per-decision float constant
(2, 3, 5, 6, 7, 8, 9, 10, 12, 15, 18, 20, 22, 25, 30, 35, 40, 45, 50, 55 or
60 — the most common are 40, 30, 20, 10) and `k` depends on the F1 option
"Nível do Jogo" (`g_cfg0`, 1..8, default 4) and the arcade fight number
`g_fightNo` (`0x4b44dd`, 1..8, 8 = the rival):

- `cpu()`: `k = (8 − level) × 0.6 + 1.3 − fightNo × 0.1`
  (level 4, fight 1: 3.6; level 8, fight 8: 0.5).
- `defendergolpe()`: `kb = (8 − level) × 0.9 + 1.2 − fightNo × 0.1`, set to
  0 when negative (a `Random(0)` check always fires).

Higher levels and later fights make every check fire more often.

### defendergolpe() [objdump, `0x494bd8`]

Runs for both players after the fighters are framed, unless paused, in the
challenge or during the entrance.

- Human: holding back (`TRAS`) while the opponent attacks gives move 12
  (standing guard), or 13 with `BAIXO`; the same for every live hostile
  effect with a hit box, attacking or not.
- CPU (`dist` between the fighters; `proj`/`pd` as in `cpu()`, plus that
  effect's `y` and whether it is persistent):
  1. Opponent attacking and `dist ≤ 160`: opponent `y ≤ groundY − 30` →
     move 12 with `C = 0.5`; `y ≥ groundY − 30` → move 13 with `C = 0.5`;
     own state 21 / 22 renews move 12 / 13.
  2. Standing or walking: jump (vy −32, state 12, toward the opponent, 1 in
     8 straight up) when a projectile is within 130 px and `my.y − 120 <
     effect.y` with `Random(Round(5·kb)) ≤ 3`; otherwise, when no
     projectile is live and not airborne, with `C = 45`.
  3. Projectile within 120 px and `my.y − 120 < effect.y`: move 12 with
     `C = 0.4`; within 100 px and `my.y − 120 ≥ effect.y`: move 13 with
     `C = 2.5`.

### cpu() decision order [objdump]

The full per-tick procedure (preferred distance, walking, close-range
normals, crouch, and the 86 per-character special/super checks with their
`C` constants, distance tests and projectile/airborne conditions) is
transcribed as a table in `05d-cpu-table.txt`. The projectile test looks for
any effect slot with `type != 0`, `owner != me` and "has hit box" (+0x31);
some characters counter an incoming projectile that is still more than 140 px
away, others only act when none is live.

## Round flow inside state 7 [decomp]

- Round init (state 6 → 7): timer per config, `g_camX = 100`,
  `zerardados(1)`, `zerardados(2)`, stocks/meter cleared, effect table
  cleared, both fighters' strips loaded (`Lutadores/Cxx.bmp`), stage
  `Cenarios/Cenario-NN.bmp` chosen randomly 1..12 in a 2-player game
  (`SCxx.dat` forces stage xx), music index chosen by character (Chaves →
  4, Quico → 2, Madruga → 3, stage 12 → 6, else random ≤ `0x4b58a3`),
  `carregarmusica(n)`.
- `g_roundStart` counts 0 → 72 (`g_noPush` while < 10): health bars fill
  from 1 to 125 (+3 per tick from tick 6 to 54), "ROUND n" text (`MemoTexto`
  line 15) flashes from 43 to 60, at tick 55 `g_noPush := 0` and health =
  125 ("FIGHT").
- KO or time over: `g_roundEnd` counts 1 → 230; first tick sets
  `hitStop = 30`; the loser gets `invuln = 1`; the HUD slides
  (`0x4b44d2 -= 5`, `0x4b44d4 += 5`); from tick 30 the winner plays move 14
  (win pose); at 35/45 "PERFECT"/"K.O." checks (health == 125 → perfect);
  time over (`g_timerA == 0`) → `g_roundEnd = 30`, sound 4, lower health
  loses (draw clears both). After the sequence: next round, or state 9
  (continue), 10 (victory/ending) or 12 (name entry) when a score beats
  the lowest record.

## Sound indices [decomp]

`DXWaveListSnd[k]` = `Sons/Som-NN.wav` with NN = k + 1 (loaded in order in
FormCreate, missing files skipped — so indices shift if a file is absent).
Observed uses: 0 coin, 4 time over, 5 light whiff, 6 heavy whiff, 7 light
hit, 8 heavy hit, 9 KO / level-3 hit, 10 block, 12 land after knockdown,
13 stock gained, 19 super start, 21/23/24 character specials. The per-character special-move sounds that the frame selector
sets (`g_sndIndexA = N` per character, state and animation timer, plus six
Random-gated rows) are listed in 06c-special-sounds.txt. Voices:
`DXWaveListVitoria[2(N−1) + {0: a, 1: b}]` = `Falas/Fala-NN-a/b.wav`,
N = 1..15, played once at `g_roundEnd == 95` (no hit stop or super
pause) for each fighter whose wins reached the rounds-to-win setting: the
match winner's victory line, b when `Random(200) > 100` (0x4a7c80). Music: `Musicas/Musica-0N.wav`, N chosen at round init.

## Stage objects [decomp]

- Stage 1, or stage 2 while `noPush` is off: when the record at `0x4b57c8`
  is idle, `40 < camX < 160` and `Random(330) == 0`, a passer-by starts at
  `x = camX − 80`, `y = 146`, speed 7, kind `Random(2) + 1`. It is drawn as
  `Efeitos[0]` at `(x + 10, y + 132)` under `Efeitos[9]` at `(x, y)`,
  moves by its speed when not paused and ends past `camX + 420`. Between
  `camX + 150` and `camX + 250`, 1 in 5 per tick (no pause, no `noPush`) it
  drops a pickup, effect 3 (kind 1) or 4 (kind 2), at `(x + 13, y − 5)`,
  once. The record is reset at every round.
- Stage 2: `carregarcenario` fills `0x4b57d8` = (395, 195, 25, 25, 110, 90,
  icon 6). `Icone[6]` is drawn at (395, 195) while the icon is set (not
  during a super pause). A player in state 19 whose hurt box reaches it
  (`x + 110 ≥ L`, `y + 90 ≥ T`, `x + 25 ≤ R`, `y + 25 ≤ B`) breaks it: icon
  0, sound 9, 30 debris effects (type 21) at `(x + 20 + Random(40), y + 20 +
  Random(20))`.

## Last-fight dialogue [decomp, 0x4a4591 and 0x4a99de]

At fight 8, round 1, `roundStart == 1`, with a CPU player: `g_dialogo =
600`, `roundStart = 2`, HUD offsets −120 / +60; the round intro waits until
`g_dialogo` (−1 per tick) is 0. Two rounded boxes, (5, 165)–(290, 215) and
(110, 220)–(395, 270), white pen, black fill; the human's big portrait
(`Retrato[(c − 1)·5 + 4]`) at (15, 85) and the CPU's at (288, 140), both
sliding in 10 px per tick during the first 20 ticks. Four exchanges of 150
ticks (human, CPU, human, CPU) show three lines of the dialogue memo (form
field 0x300) from line `(humanChar − 1)·12 + 0/3/6/9`, size 10, at (115,
168 + 14k) for the human and (120, 223 + 14k) for the CPU. The human's
first button takes 5 more ticks off while more than 10 are left.

Selector effects: `06e-selector-spawns.txt` holds all 106 spawn call sites of the
frame selector with their guards (see `06-effects.md`). The selector's
`g_sndIndexA` writes are in `06c-special-sounds.txt`, including the throw
supers' hit sounds (Madruga states 29 and 31, Chiquinha and Barriga state 30:
`Random(4 or 6) == 0` for sound 7, 8, 9 in turn, the last one wins). `setMove`
writes the move's sound into `g_sndIndexA` too (played at the start of the next
tick, not immediately).

### Inventory of the frame selector `0x47d21c..0x48dcbf` (asm vs port) [objdump]

| statement kind | in the asm | in the port |
|---|---|---|
| `FUN_00476a04` effect spawn | 106 sites (+1 in `setMove`) | all (06e) |
| `sub_476870` hurt box / `FUN_004768f8` hit box | 305 / 379 | all (`05a`) |
| frame store `+0x6c` | 421: 379-row anim table; 14 are guarded by hit level, 29 by an even-timer or current-frame test (alternating frames), 4 are random, 4 pick the victim's frame in a throw | the table only: the alternating, level-guarded and throw frames are missing |
| `g_sndIndexA` writes | 51 | all (`06c`, with the throw supers' hit sounds) |
| `sub_47648c` (move_x) | 196 | other session (06d-special-motion) |
| direct `x`/`y` stores (`fstp`) | 80 (35 self y, 10 self x, 8 + 8 on the opponent, throw and teleport lifts) | y rules by the other session; the throw ones (opponent lifted and shaken) not ported |
| `setMove` (`0x4775fc`) | 6, all in the throw supers (victim move 10, own move 0x13 / 0x15) | not ported |
| player fields `+0x68`, `+0x69` (knockdown), `+0x6a`, `+0x6b`, `+0x6d`, `+0x6e`, `+0x70`, `+0x72`, `+0x74`, `+0x76`, `+0x78` (vy), `+0x92` (invuln), `+0x93`, `+0x95`, `+0x96`, `+0x18` | 2, 38, 23, 4, 4, 7, 4, 9, 10, 5, 31, 20, 1, 4, 4, 6 | not ported (only `+0x78` partly by the other session) |
| health store `+0x40` (throw super hit) | 4 | not ported |
| globals `g_hitStop` `0x4b0e84`, `0x4b0fc8`, `0x4b0d68` | 5, 4, 1 | not ported |
| `Random` | 86 (spawn offsets, sound draws, random frames, throw shake) | spawn offsets and sound draws |
| `Round`, `0x40287c` (trunc, even-timer tests), `Cos` | 5, 18, 1 | the Round of Bruxa's dust base; the parity tests are the `mod` column of 06e; `Cos` (`0x48af09`) not ported |
