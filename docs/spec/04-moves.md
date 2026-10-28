# 04 — Moves: special-move list, input notation, command parser, AI

Legend: **[static]** bytes/metadata, **[decomp]** Ghidra pseudocode (not
source), **[infer]** interpretation, **UNKNOWN** not established.

Generated detail tables (regenerate with `tools/re/seqtables.py`):
`04a-move-tables.txt` — per character, every `sequencia(from → to)`
transition with its direction conditions, and every `golpe` mapping
`(button, sequence state) → move id`.

## Move ids (argument of the move-start routine `0x4775fc`) [decomp]

`0x4775fc(player, moveId)` is called 298 times from the game code; it is the
single entry point that puts a fighter into a move/state. It was not a
Ghidra function in the first export and is being decompiled now, so the
per-move frame data is still UNKNOWN. Ids observed so far:

| id | meaning (from call sites) |
|---|---|
| 1, 2 | walk (AI: toward/away depending on facing) |
| 3–7 | basic actions picked randomly by the AI at close range (jump/crouch/normals) [infer] |
| 12 (0x0c) | stand block (`defendergolpe`, "TRAS" held, not "BAIXO") |
| 13 (0x0d) | crouch block ("TRAS" + "BAIXO") |
| 16, 17 | special 1 (weak / strong button) |
| 18, 19 | special 2 |
| 20, 21 | special 3 |
| 22, 23 | special 4 |
| 24 | super (same id for both buttons) |
| 25, 26 | extra specials, Jaiminho only |

Normal attacks (punch/kick without a sequence) are **not** started in
`golpe`; they must be in `DXTimer1Timer` (UNKNOWN until it is read).

## Input notation used by the in-game move list (`mostrasequencia` `0x491b5c`) [decomp]

Each character of the notation string is drawn as an icon from
`DXImageListIcone` or as text:

| char | drawn as | meaning |
|---|---|---|
| `C` | icon 0 "Cima" (up) | up / **(AR)** = in the air |
| `B` | icon 1 "Baixo" | down |
| `D` | icon 2 "Direita" | right = **forward** (list assumes facing right) |
| `E` | icon 3 "Esquerda" | left = **back** |
| `Y` | icon 4 "Diagonal1" | down-back diagonal [infer from Zan Ei Ken] |
| `Z` | icon 5 "Diagonal2" | other diagonal (unused in current lists) |
| `P` | text `+S` | + soco (punch) |
| `K` | text `+C` | + chute (kick) |
| `R` | text `rápido` | mash |
| `?` | text `+S/C` | punch or kick |

Super moves are prefixed with `*` and drawn in a different colour.

## Per-character special moves [decomp: strings in `mostragolpe` `0x491edc`]

The list is drawn per player (`players[p].charId`, 200 px columns). The
original fighters are reskins of SNK/Capcom characters; the source is
listed because the real move properties will be modelled on them [infer].

| id | Fighter | Based on | Special 1 (16/17) | Special 2 (18/19) | Special 3 (20/21) | Special 4 (22/23) | Super (24) |
|---|---|---|---|---|---|---|---|
| 1 | Chaves | Terry Bogard | BDP Power Wave | BEP Burn Knuckle | BCP Rising Tackle | BEK Crack Shoot | BEBDP *Power Geyser |
| 2 | Seu Madruga | Robert Garcia | BDP Ryuu Geki Ken | DBDP RyuuGa | DEDK Genei Kyaku | CBEK Hien Ryuu Jin Kyaku | BDBEP *Ryuko Ranbu |
| 3 | Chiquinha | Ryo Sakazaki | BDP Ko Ou Ken | DBDP Kuu Ga | BEP Hien Senpuuken | BEK Hien Senpuukyaku | BDBEK *Hien Honou Kyaku |
| 4 | Quico | Andy Bogard | BEP Hi Sho Ken | YDP Zan Ei Ken | EBDK Ku Ha Dan | BDP Sho Ryu Dan | BEBDK *Cho Reppa Da |
| 5 | Dona Florinda | Chun-Li | BDP Souhakkei | BEK Sen en Shuu | BCK Spinning Bird Kick | KR Hyakuretsu Kyaku | BDBDK *Senretsu Kyaku |
| 6 | Prof. Girafales | Sagat | BDP Tiger Shot | BDK Low Tiger Shot | DBDP Tiger Blow | DBDK Tiger Crush | BDBDK *Tiger Cannon |
| 7 | Sr. Barriga | Chang Koehan | PR Tekkyu Dai Kaiten | EDP Tekkyu FunsaiGeki | BCK Tekkyu Hi En Zan | BCP Ricchi Futoi | BDBEP *Tekkyu Dai Bouson |
| 8 | Bruxa do 71 | Demitri | BDP Chaos Flare | CBDP Chaos Flare (AR) | DBDP Demon Cradle | BEK Bat Spin | BDBDK *Demon Horde |
| 9 | Paty | Cammy | BDK Cannon Drill | DBDK Cannon Spike | BDP Accel S. Knuckle | CBEK Cannon Strike (AR) | BDBDK *Spin D. Smasher |
| 10 | Nhonho | E. Honda | PR Hyakuretsu Harite | EDP Super Zutsuki | BCK Super Hyakkan Otoshi | BDK Biggu Hara | EDEDP *Onimussou |
| 11 | Godines | Cody | BDP Bad Stone | BEP Criminal Upper | BDK Raffian Kick | BEK Bushin Hurricane | BDBDK *Dead End Irony |
| 12 | Sr. Furtado | Dan Hibiki | BDP Gadouken | DBDP Kouryuken | BEK Dankuu Kyaku | CBEK Dankuu Kyaku (AR) | BDBDK *Kouryuu Rekka |
| 13 | Jaiminho | M. Bison (Vega) | BDP Psycho Shot | BDK Double Knee Press | BEP Psycho Field | DBD? Jaiminho Warp | EDEDP *Psycho Crusher (+ ids 25, 26) |
| 14 | Dona Neves | Athena | BEP Psycho Ball Attack | BEK Psycho Reflector | BEK Phoenix Arrow (AR) | DBDP Psycho Sword | BEBEP *Shining Crystal Bit |
| 15 | Glória | Mai Shiranui | BDP KaChoSen | BEP RyuEnBu | DBDK Hisho RyuEnJin | BDK Hissatsu Shinobibachi | BEBDK *Cho Hissatsu S. |

The slot → id mapping (special 1 = 16/17 …) is confirmed for Chaves by
`golpe` (QCF = state 2 → 16/17 with punch; QCB = state 3 → 18/19 punch and
22/23 kick; charge-down-up = state 4 → 20/21; QCB,QCF = state 6 → 24).
For the other characters the order in the list and the order of the
`golpe` entries match the same way [infer; verify per character against
`04a-move-tables.txt`].

## Command parser [decomp]

### Direction reading (`seta` `0x4905a8`)

`seta(player, "DIR")` tests the DelphiX input state of that player's
device (`DXInput1.Keyboard` for P1, `DXInput1.Joystick` for P2; both are
masked through the `Assigns` table so the keyboard can also drive P2) and
returns true when the named direction is held **relative to facing**:

| string | raw bit | condition |
|---|---|---|
| `"CIMA"` | isUp (bit 0) | |
| `"BAIXO"` | isDown (bit 1) | |
| `"TRAS"` | isLeft (bit 2) if facing == 1 (facing right); isRight (bit 3) if facing == 2 |
| `"FRENTE"` | isRight (bit 3) if facing == 1; isLeft (bit 2) if facing == 2 |

So `facing` (+0x6d) 1 = facing right, 2 = facing left.

### Sequence state (`sequenciagolpe` `0x4968b0`, `sequencia` `0x4951b8`)

Per player: `seqState` (+0x98, byte), `seqTimer` (+0x99), `downHold`
(+0x9a) and `backHold` (+0x9b).

- Every tick: `downHold++` while BAIXO held (cap 18), `downHold--` when
  released (floor 1); same for `backHold` with TRAS.
- `sequencia(p, from, to)`: `if seqState == from then seqState := to;
  seqTimer := 9`. The timer countdown back to state 0 happens elsewhere
  (main loop, UNKNOWN exact rule; presumably `seqTimer--` per tick and
  `seqState := 0` at zero).
- The transitions are **hand-written per character** (15 blocks). For
  Chaves (char 1):

  | condition | from → to | reading |
  |---|---|---|
  | BAIXO, not FRENTE/TRAS | 0,1,2 → 1 | down |
  | FRENTE, not BAIXO | 1 → 2 | down, forward = **QCF** |
  | TRAS, not BAIXO | 1 → 3 | down, back = **QCB** |
  | CIMA, not BAIXO, downHold > 9, on ground | 0,1 → 4 | **charge down, up** |
  | BAIXO, not FRENTE/TRAS | 3 → 5 | QCB then down |
  | FRENTE (± BAIXO), not TRAS | 3,5 → 6 | QCB, QCF = **super** |

  Note the parser is loose: diagonals are not required (down then
  forward is enough for a QCF), and "charge" is a hold counter, not a
  frame count.

### Button → move (`golpe` `0x4951ec`)

`golpe(p, button)` is called with button 1 = soco fraco (LP), 2 = chute
fraco (LK), 3 = soco forte (HP), 4 = chute forte (HK), only when
`g_4b57f8 == 0` (no hit-stop). For each character it maps
`(button, seqState)` to a move id (full table in `04a-move-tables.txt`).
Extra conditions seen: Bruxa's air Chaos Flare needs `airborne`
(+0x78 ≠ 0); mash moves use `+0x9c > 5` (Barriga, Nhonho, punch) /
`+0x9d > 5` (Florinda, kick) — those two counters are incremented
elsewhere (UNKNOWN where; presumably per button press in the main loop).

### Blocking (`defendergolpe` `0x494bd8`)

- Human player (+0x9e == 1): if TRAS held and the opponent is attacking
  (`opponent.+0x68 == 1`) or any active projectile in the effects table
  belongs to the opponent, start move 12 (stand block) or 13 (crouch
  block if BAIXO also held).
- CPU player: computes the distance to the opponent (|p.x − o.x|) and to
  the nearest hostile projectile; blocks with probability
  `Random(round(difficultyVar)) == 0` under distance thresholds (0xa1,
  0x82, 0x79, 0x65 px) and, with a further random check, can instead
  start a **jump away** (state 12: `vy = −32`, `inAir = 1`, state 12,
  `animTimer = 0`, `animDur = 99`, `jumpDir` = 3 or 2 by facing, or 1 with
  1/8 chance). The projectile-distance threshold `_DAT_0049519c` is the
  float **120.0**; the probability operands are described in 05 ("AI
  difficulty").

## CPU opponent (`cpu` `0x4931ac`) [decomp]

- `dist = |p.x − o.x|`; scans the effects table for hostile projectiles
  (`projDist`).
- Preferred distance `+0x1c` (int): with chance 1/13 per tick it is
  re-rolled to one of {100, 150, 200, 250, 300, 350}; when within 50 px of
  the current distance it snaps to the current distance.
- If not blocking/stunned (+0x93 == 0) and no projectile: walk toward
  (move 1/2 by facing) when `dist > preferred`, away when closer.
- Within 80 px, each of moves 3, 4, 5, 6 fires with probability
  `Random(round(k)) == 0`; move 7 likewise and, if taken, re-rolls 3–6.
- Then a per-character block (15) fires specials by distance:
  typical pattern — projectile (16/17) when `dist > 120` or a hostile
  projectile is `> 140` px away; anti-air/rush (18/19) when `dist < 200`
  or `< 100`; 20/21 when the opponent is airborne (`o.y < groundY`) and
  `dist < 150`; 22/23 when `dist < 200`; super 24 when `dist < 150`
  and the opponent is airborne (Chaves) / `dist < 120` (others). Each with
  `Random(round(k)) == 0`, so `k` is the difficulty knob.
- Facing of `k`: the FPU value passed to `Round()` could not be recovered
  textually; it is one of the config doubles (0.9 default / "velocidade"
  / difficulty 1..4) — UNKNOWN which.
