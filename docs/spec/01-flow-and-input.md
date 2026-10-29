# 01 — Game flow, keys, config and saves

Legend: **[decomp]** Ghidra pseudocode (not source), **[infer]**
interpretation, **UNKNOWN** not established.

## Game state `g_state` (`0x4b44d6`, u8) [decomp]

| value | screen | notes |
|---|---|---|
| 0 | resolution menu | 400x300 / TELA CHEIA (1) / TELA CHEIA (2) → 1 |
| 1 | attract / title | intro cinematic, demo fight after 650 ticks |
| 2 | F1 configuration menu | see 07 |
| 3 | F2 key configuration | see 07 |
| 4 | character select | 30 s timer |
| 5 | VS / ladder screen | stage choice, dialogue setup |
| 6 | round init | one tick: loads fighters/stage/music → 7 |
| 7 | fight | see 05 |
| 8 | game over + top-10 records | → 1 |
| 9 | continue countdown | → 4/5 or 8 |
| 10 | ending cutscene | → 11 |
| 11 | credits | → 9 |
| 12 | name entry for the records | → 9 or 10 |

Transitions and timers are in 07. Shared counters: `g_ciclo` (`0x4b44c0`,
u8 blink counter), `g_ciclo2` (`0x4b44c4`, i16 screen counter), `g_camY`
(`0x4b44cc`, also used as a scroll offset by the menus), `g_timerA`/
`g_timerB` (seconds / 25-tick sub-counter), `g_noPush` (`0x4b44e0`,
input/physics lock).

## Fixed keys (FormKeyDown `0x49e2d4`) [decomp]

| key | action |
|---|---|
| Esc, Left Win | quit |
| `-` / `+` | `g_speedDelay` ±1 (DXTimer interval in ms, 0..80, default 28); shows "VELOCIDADE" for 30 ticks |
| `,` / `.` | `DXDraw1.Left` -1 / +1 while > -40 / < 40 (picture slides, black form behind it; not saved) |
| `P` | pause toggle (see below) |
| `G` | only in state 7 (fight): pause + move list (`mostragolpe`), or resume. Port rule: ignored while `G` is bound to either player's controls in `g->keymap` (P2's default strong punch is `G`), so a P2 keyboard player does not pause on every punch |
| `F1` | attract ↔ configuration menu; leaving saves `Chaves.dat` |
| `F2` | attract ↔ key configuration |
| `1` / `2` | start or join as P1 / P2 (needs a credit) |
| `5` | insert coin (`g_credits++`, max 99, sound 0) |
| letters, digits, arrows, numpad | in the F2 menu: assign to the current action |

Gameplay input goes through DelphiX `TDXInput`: P1 = keyboard (default
arrows + Z X C V = Button1..4 = soco fraco, chute fraco, soco forte, chute
forte), P2 = joystick 0 (buttons 1..4 the same), as the README says. Both
state bytes are read each tick (`DXInput1.Keyboard/Joystick.States`): bit 0
up, 1 down, 2 left, 3 right, 4..7 buttons 1..4. Button presses are
edge-detected with `btnLatch[]`; presses also add 6 to the mash counters
`mashP`/`mashK` (+0x9c/+0x9d), which decay by 1 per tick and cap at 15 —
`golpe()` requires > 5 for mash moves.

### Speed, shift, pause and the move list [decomp]

FormKeyDown `0x49e2e8`. VK `0xBD` (`-`): `g_speedDelay++` while < 80; VK `0xBB`
(`=`/`+` key): `g_speedDelay--` while >= 1; either sets `g_speedOverlayTimer = 30`.
`g_speedDelay` is only used as `DXTimer1.Interval` (set at the top of every
`DXTimer1Timer`, `0x49eda5`): a bigger value is a slower game, 0 is "as fast as
possible". Default 28 (`FormCreate` `0x49d062`). `FPS=` debug line prints it as `VEL=`.

Overlay (end of `DXTimer1Timer`, `0x4ae1bd`, every state except 2 and 3): while the
timer is > 0, `texto(200, 260, "VELOCIDADE", 'c', 8, white)` (constant index 1642),
a white bar `barra(160, 278, 240, 280)` and a red `barra(v, 275, v + 2, 283)`
(RGB 255,50,50) with `v = 238 - Round(g_speedDelay / 1.05)`; then the timer drops by 1
(so 30 ticks).

`,` (VK `0xBC`) / `.` (VK `0xBE`): `DXDraw1.Left` (`TControl.SetLeft 0x425b20`) -1 while
`Left > -40`, +1 while `Left < 40`. FormCreate and the video-mode menu reset it
(0 / 62); it is not part of `Chaves.dat`. The port starts at 0 and slides the
400x300 picture inside the window, clipped to the picture, black behind.

`P`: `g_paused == 0` -> `g_paused = 1`; otherwise `DXTimer1.Enabled = True` (`0x446c98`),
`g_paused = 0`, `g_showMoves = 0`. `G` does the same but only when `g_state == 7`,
and its pause also sets `g_showMoves = 1`. Pausing does not stop anything by itself:
the next `DXTimer1Timer` runs completely (including drawing) and, at its end
(`0x4ae4df`), `if g_paused == 1 then DXTimer1.Enabled = False`. The frozen picture is
therefore that last tick, and the key handler keeps working while the timer is off.

`mostragolpe` (`0x491edc`, called at `0x4aa31c` right after the red flash `barra`, only
state 7 and `g_showMoves == 1`). For player s = 1..2 (both, human or CPU), with
`X = 200 (s - 1)`:

1. `barra(X + 3, 115, X + 197, 200, RGB(20,40,60))`, then `retangulo` of the same
   rectangle in white.
2. Five lines for the fighter's `charId` 1..15 (nothing for other ids), line k at
   `y = 120 + 15 k`, through `mostrasequencia(x = X + 6, y, name, inputs)` (`0x491b5c`).

`mostrasequencia`: `texto(x, y, name + " - ", '-', 9)`, white, or RGB(255,120,120)
when the name starts with `*` (the `*` is printed: the super move). `w` =
`Canvas.TextWidth(name + " - ")`. Then one cell of 12 px per character of `inputs`,
at `x + w + 12 i`: `C B D E Y Z` draw `DXImageListIcone[0..5]` (the 12x12 arrows up, down,
right, left, two diagonals) at `y + 3`; `P` `K` `R` `?` print the tags "+S", "+C",
" (rapido)", "+S/C" with `texto(.., '-', 9, white)` at `y`; any other character leaves
the cell empty. Names, inputs and tags are static Delphi string constants (one
language only, not in the memos): the port reads them from `Chaves.exe` by constant
index (`src/screens/movelist.c` holds the index table, no text). There is no demonstration
animation; the "3,500 lines" of the decompilation are the 15 x 5 inlined calls.

## Config file `Chaves.dat` [decomp]

Plain text, one value per line, written on leaving the F1 menu and read in
FormCreate (missing file → defaults):

| line | setting (F1 label) | variable | values | default |
|---|---|---|---|---|
| 0 | Nível do Jogo | `g_cfg0` | 1..8 — displayed only ("NIVEL-n"); no gameplay use found, the AI scales with the fight number (05) | 4 |
| 1 | Tempo do Round | `g_cfg1_mode` | 1..4 → 30/60/90/99 s | 3 |
| 2 | Poder | `g_cfg2` | 1 NORMAL, 2 1P MAX, 3 2P MAX, 4 MAX | 1 |
| 3 | Nível de Dano | `g_damageLevel` (double) | 0.3..2.0 | 0.9 |
| 4 | Sangue | `g_cfg4_bool` | 0/1 | 1 |
| 5 | Rounds | `g_cfg5` | 1..3 | 2 |
| 6 | Linguagem | `g_cfg6` | 1 PT, 2 ES | 1 |
| 7 | Flash | `g_cfg7_bool` | 0/1 | 1 |
| 8 | Mostrar Nível | `g_cfg8_bool` | 0/1 | 1 |
| 9 | Mostrar Creditos | `g_cfg9_bool` | 0/1 | 1 |

## Unlock files (FormCreate) [decomp]

| file in the game folder | effect |
|---|---|
| `Churros.dat` | VS screen shows the whole ladder's faces instead of only the fought ones [infer] |
| `Gentalha.dat` | select cursor starts at 8 and the ladder/boss logic uses `g_selectCursorStart ≥ 8` (hidden characters path) |
| `Energeticos.dat` | `g_energeticos` (`0x4b5898`) = 0.15, else 0. Every tick of the fight screen, in the player loop right after the round-timer drawing of `DXTimer1Timer` (`all.pp.c` ~93632, no pause test of its own): for each player `if (p1.health > 0 && p2.health > 0) health += 0.15`, followed by the usual clamps (< 0 → 0, > 125 → 125) |
| `Satanas.dat` (`0x4b58ad`) | blood on every hit: each blood spawn (effect 10) is `(cfg_blood && Random(n) == 0 [&& extra]) \|\| satanas` (`all.pp.c` 82468 `Random(3)`, 95231 hit level 2 && `Random(5)`, 95534 `Random(4)` && field +10 == 5); with blood off no `Random` is drawn |
| `Fadiga.dat` (`0x4b58ae`) | VS / next-battle screen (state 5): right after its own transition test (`0x96 < counterC` → state 6), `state = 10; counterD = 0; counterC = 0` every tick, so it goes straight to the ending of the player's character |
| `Mentecapto.dat` (`0x4b58af`) | at the top of every `DXTimer1Timer` tick `hudY (0x4b44d2) = -90; hudY2 (0x4b44d4) = 40`: the HUD is pushed off screen |
| `Bruxa.dat` (`0x4b58b0`) | `0x476454(type, owner)` ("an effect of that type of that owner is alive") returns false when the flag is set, so the one-projectile-at-a-time rule of the move setup (`needsN` in `05b-move-setup.txt`) never refuses |
| `SC01.dat` … `SC12.dat` (`0x4b58b1`) | the highest NN found (scan 1..12) forces the stage: 1P fights `all.pp.c` ~93209 `if (sc != 0 && selectCursorStart < 8) stage = clamp(sc, 1, 12)`; 2P fights ~93375 `if (sc != 0) stage = clamp(sc, 1, 12)` unconditionally |

## Startup defaults (FormCreate) [decomp]

`g_speedDelay = 28`, `g_gravity = 4`, `g_meterUnit = 2.0`, `g_damageLevel =
0.9`, `0x4b57c8+10 = 330`, high-score defaults CHV/SMD/CHQ/QUI/DFL/PGR/SBR/
B71/PTY/NHO (`criarecorde`), all assets loaded (see 03), `V1.5A` version
string, then `DXTimer1` enabled.
