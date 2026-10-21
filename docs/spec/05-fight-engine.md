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
  (`0x4b449c`, "GRAV."; default UNKNOWN until the config/init that sets it
  is read — FormCreate sets `0x4b449c` to **4**), and x moves by ±`speed`
  for `dir` 2/3. Landing (`y >= groundY`) resets `y`, `vy`, `inAir`, `rot`,
  `rotStep`, state := 1, `animTimer` := 0.
- Knockdown (state 19): x moves by `hitstun` (+0x72) px per tick away
  from the attacker; on landing → move 11 (state 20), `y = groundY − 10`,
  sound 12, dust effects (type 2); heavy characters (7 Barriga, 10 Nhonho)
  also shake the screen (`0x4b449a = 20`) with extra dust.
- Facing is recomputed each tick when not attacking/blocking so players
  always face each other.

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
60 — the most common are 40, 30, 20, 10) and `k` depends only on the
**arcade fight number** `g_fightNo` (`0x4b44dd`, 1..8, 8 = the rival):

- `cpu()`: `k = (8 − fightNo) × 0.6 + 1.3 − fightNo × 0.1` = **6.1 − 0.7·fightNo**
  (5.4 in fight 1 … 0.5 in fight 8).
- `defendergolpe()` (blocking / jumping away): `k = (8 − fightNo) × 0.9 +
  1.2 − fightNo × 0.1` = **8.4 − fightNo** (7.4 … 0.4; with `C = 0.5` and
  `2.5` for the two block checks, `5` and `45` for others).

So in fight 8 almost every check fires (`round(C·0.5)` is small), while in
fight 1 the CPU acts about ten times less often. The F1 option "Nível do
Jogo" (`g_cfg0`, 1..8) is **not read anywhere in the gameplay code**: it is
only clamped, saved and displayed as "NIVEL-n" — a cosmetic setting (or an
unfinished feature) in 1.5A. A runtime test should confirm this (see 99).

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
13 stock gained, 19 super start, 21/23/24 character specials. Voices:
`DXWaveListVitoria[2(N−1) + {0: a, 1: b}]` = `Falas/Fala-NN-a/b.wav`,
N = 1..15. Music: `Musicas/Musica-0N.wav`, N chosen at round init.

## Stage objects [decomp]

- Stages 1 and 2: a moving object (`0x4b57c8` record: x, y = 146, speed 7,
  type 1/2) crosses from `camX − 80` with probability `1/Random(k)` per
  tick while `40 < camX < 160`; it can drop an effect (type 3 or 4, which
  has a hit box) between `camX + 150` and `camX + 250`.
- Stage 2: a breakable object (rect from `g_4b57d8`, icon
  `DXImageListIcone[0x4b57f0]`) that a player knocked down (state 19)
  into destroys: 30 debris effects (type 21), sound 9.
