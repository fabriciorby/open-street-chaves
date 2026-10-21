# 02 — Player record and fight globals

Legend: **[decomp]** Ghidra pseudocode (not source), **[infer]**
interpretation, **UNKNOWN** not established. Names in the *port name*
column are the ones used by the other spec files.

## `players[1..2]` — two 176-byte (0xb0) records at `0x4b44e8` [decomp]

Code indexes them 1-based (`0x4b4438 + p*0xb0` style, so `players[1]` is at
`0x4b44e8`). The debug overlay (`g_debugOverlay`) prints many of these with
labels, which fixed their meaning.

| off | type | port name | meaning / evidence |
|---|---|---|---|
| +0x00 | double | `x` | world X ("POS="). `zerardados`: P1 220, P2 380 |
| +0x08 | double | `y` | feet Y; `groundY` (300) on the ground; `< groundY` = airborne |
| +0x10 | u8 | `charId` | 1..15 ("JOG1=", "JOG2=") |
| +0x11 | shortstring[4] | `name` | 3-letter name for the records screen, initialised to `#3 'AAA'` on entering state 12 |
| +0x16 | i16 | `rot` | drawing rotation ("ROT="); `rot += rotStep` each tick outside states 12/19 it is zeroed |
| +0x18 | i16 | `rotStep` | "VEL ROT="; set ±20 by launchers |
| +0x1c | i32 | `aiPrefDist` | CPU preferred distance |
| +0x20 +0x24 +0x28 +0x2c | i32 | `hurtL hurtT hurtR hurtB` | hurt box, offsets from the sprite's top-left (set by `sub_476870`) |
| +0x30 +0x34 +0x38 +0x3c | i32 | `hitL hitT hitR hitB` | hit box (set by `FUN_004768f8`) |
| +0x40 | double | `health` | "HP="; 125 at round start |
| +0x48 | double | `healthShown`? | second 125.0 value — displayed/lagging bar [infer] |
| +0x50 | double | — | zeroed per round (knockback accumulator?) UNKNOWN |
| +0x58 | i16 | `stocks` | super stocks 0..3 |
| +0x60 | double | `meter` | special meter 0..102 ("ESPECIAL=") |
| +0x68 | u8 | `attacking` | hit box active |
| +0x69 | u8 | `knockdown` | current attack knocks down ("DERRUBA") |
| +0x6a | u8 | `inAir` | 1 while jumping / knocked up |
| +0x6b | u8 | `state` | see 05 |
| +0x6c | u8 | `frame` | sprite index (P1 1.., P2 51..) |
| +0x6d | u8 | `facing` | 1 right, 2 left ("DIR=") |
| +0x6e | u8 | `jumpDir` | 1 neutral, 2 left, 3 right |
| +0x6f | u8 | `hitLevel` | 1 light, 2 heavy (3 for some supers) |
| +0x70 | u8 | `damage` | of the current attack |
| +0x71 | u8 | `chipDamage` | applied when blocked |
| +0x72 | u8 | `hitstun`/`knockback` | px per tick pushed while in state 19; default 4 |
| +0x74 | i16 | `animTimer` | "DESENHO=" first value |
| +0x76 | i16 | `animDur` | "DESENHO=" second value; 99 = open-ended |
| +0x78 | i8 | `vy` | vertical velocity, −32 on jump, −19 knockdown, −13 bounce; gravity +4 per tick ("GRAV.=" is the global) |
| +0x7a | i16 | `spriteH` | per-character strip height constant |
| +0x80 | double | `speed` | 8.0 / 8.5 / 7.5 |
| +0x88 | i32 | `score` | +5 per hit, +300 first hit, compared with records |
| +0x8c | u8 | — | zeroed on join |
| +0x8f | u8 | `msgIndex` | `MemoTexto` line shown over the player |
| +0x90 | u8 | `msgTimer` | ticks the message stays |
| +0x91 | u8 | `roundsWon`? | P1 = 1, P2 = 0 after `zerardados` [infer] |
| +0x92 | u8 | `invuln` | cannot be hit (KO'd, knocked down, lying) |
| +0x93 | u8 | `isBlocking` | set in states 21/22 |
| +0x95 | u8 | `hits` | current combo ("HITS=") |
| +0x96 | u8 | `comboTimer` | 44/50 ticks ("HITS TEMPO=") |
| +0x97 | u8 | `maxCombo` | best combo |
| +0x98 | u8 | `seqState` | command parser ("SEQUENCIA=") |
| +0x99 | u8 | `seqTimer` | "SEQ. TEMPO=" |
| +0x9a | u8 | `downHold` | "AGACHAR TEMP=" |
| +0x9b | u8 | `backHold` | "TRAS TEMP=" |
| +0x9c | u8 | `mashP` | "SOCO REPET=" |
| +0x9d | u8 | `mashK` | "CHUTE REPET=" |
| +0x9e | u8 | `human` | 1 = controlled by a human (else `cpu()` drives it) |
| +0x9f | u8 | — | zeroed on join |
| +0xa0..+0xa6 | u8[7] | `ladder[]` | arcade opponents (`criaadversario`) |
| +0xa7 | u8 | `rival` | final opponent |
| +0xa8..+0xab | u8[4] | `btnLatch[]` | 1 while button 1..4 is held (edge detection) |

Remaining UNKNOWN: +0x14, +0x1a, +0x4c?, +0x50, +0x7c, +0x8c, +0x94, +0x9f,
+0xac..+0xaf.

### Rival and exclusions (`criaadversario` `0x499ea4`) [decomp]

`criaadversario(p, charId)` fills `ladder[0..6]` with 7 distinct random
characters (`Random(15)+1`), never the player's own, never the rival, never
the excluded ones; `rival` is the 8th fight.

| char | rival | excluded |
|---|---|---|
| 1 Chaves | 2 | 3, 9 |
| 2 Madruga | 5 | 3, 8, 9, 14, 15 |
| 3 Chiquinha | 9 | 1, 2, 14 |
| 4 Quico | 1 | 5, 6 |
| 5 Florinda | 2 | 4, 6 |
| 6 Girafales | 7 | 4, 5, 15 |
| 7 Barriga | 2 | 8, 10 |
| 8 Bruxa | 5 | 2, 7 |
| 9 Paty | 3 | 1, 2, 15 |
| 10 Nhonho | 1 | 7, 9 |
| 11 Godines | 3 | 1, 9 |
| 12 Furtado | 1 | 5, 8 |
| 13 Jaiminho | 5 | 1, 8, 14 |
| 14 Neves | 5 | 2, 3, 13 |
| 15 Glória | 3 | 1, 2, 9 |

## Round reset (`zerardados` `0x49a218`) [decomp]

health 125, state 1, flags cleared, `speed` per character, P1 x = 220 /
P2 x = 380, y = 300, facing P1→1 P2→2, camera `g_camX = round((p1.x +
p2.x) / 2) − 200`, `g_camY = round((p1.y + p2.y) / 8) − 55` (float constants
2.0 and 8.0 at `0x49a3ec/0x49a3f0`), round flags `0x4b44d2 = 0x4b44d4 = 0`,
`g_firstHit (0x4b57f4) = 0`, and the 80 effect slots cleared.

## Effects / projectiles table [decomp]

80 records × 0x38 (56) bytes at `0x4b4648`; the code walks them through
`rec = 0x4b4673 + i*0x38` (= record + 0x2b). Field offsets from the record
start:

| off | meaning |
|---|---|
| +0x00 | double `x` |
| +0x08 | double `y` |
| +0x10, +0x18 | doubles (velocity / target?) UNKNOWN |
| +0x20 | `dir` (copied from owner's facing) |
| +0x21 | `speed` |
| +0x22 | `damage`? (`[-9]`) |
| +0x23 | `param` (`[-8]`, from `Round()`) |
| +0x24 | `owner` player id |
| +0x25 | `hostile`/`active for blocking` (`[-6]`) |
| +0x26, +0x27 | signed bytes: hit-box offsets? (`-22, -1` for types 3/4/6) |
| +0x28 | `done` flag |
| +0x29 | flag set randomly (1/5) for type 7/9 |
| +0x2a | flag |
| +0x2b | **`type`** (0 = free slot) |
| +0x2c | `phase` (animation counter) |
| +0x2d | `life` |
| +0x2e | `sprite` index into `DXImageListEfeitos` chosen by `(type, phase)` in `desenhaefeitos` |
| +0x2f, +0x30 | counters |
| +0x31 | has hit box (`[6]`) |
| +0x33, +0x34 | flags (`[8]`, `[9]`) |

`FUN_00476a04(type, variant "A"/"B", owner, y, x)` spawns one; per-type
initial values and `FUN_004769c4(slot, l, t, r, b)` hit boxes are listed in
`05a`/the effects spec (pending).

## Other fight globals [decomp]

| addr | name | meaning |
|---|---|---|
| `0x4b4498` | `g_groundY` | 300 ("CHÃO") |
| `0x4b449a` | `g_shakeTimer` | 20 after heavy landings |
| `0x4b449b` | `g_shakeHit` | 6 on hits when config line 7 is on |
| `0x4b449c` | `g_gravity` | 4 (FormCreate), added to `vy` per tick |
| `0x4b44c0` | `g_ciclo` | tick counter (u8) used for blinking |
| `0x4b44c4` | `g_ciclo2` | i16 counter |
| `0x4b44c8` / `0x4b44cc` | `g_camX` / `g_camY` | camera ("TELAX=", "TELAY=") |
| `0x4b44d2` / `0x4b44d4` | HUD slide offsets | animate at round end |
| `0x4b44d8` | `g_stageId` | 1..12 |
| `0x4b44db` | `g_roundNo` | printed in "ROUND n" |
| `0x4b44dd` | `g_fightNo` | arcade fight number 1..8 (starts at 1, or 8 with `Gentalha.dat`); indexes `ladder[]`, drives the AI probabilities (05) and the VS screen |
| `0x4b44de` / `0x4b44df` | `g_roundStart` / `g_roundEnd` | intro/outro tick counters |
| `0x4b57f4` | `g_firstHit` | first hit bonus given |
| `0x4b57f6` | `g_dialogo` | pre-fight dialogue running |
| `0x4b57f8` / `0x4b57f9` | `g_superPause` / `g_hitStop` | freezes |
| `0x4b57fa` | `g_challengeTimer` | 100 when a challenger joins |
| `0x4b5810` | `g_records[10]` | 3-char name, char id, i32 score (defaults CHV, SMD, CHQ, QUI, DFL, PGR, SBR, B71, PTY, NHO) |
| `0x4b5884` | lowest record score | compared with `score` for name entry |
| `0x4b5888` | `g_damageLevel` | 0.9 (config line 3) |
| `0x4b5890` | `g_meterUnit` | 2.0 |
| `0x4b58a3` | number of music tracks available | random music upper bound |
| `0x4b58a4` / `0x4b58a5` | `g_debugOverlay` / `g_debugBoxes` | debug display switches (how they are toggled: UNKNOWN) |
