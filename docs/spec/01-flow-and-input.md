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
| `,` / `.` | move the DXDraw control ±1 px horizontally (±40) |
| `P` | pause toggle |
| `G` | in a fight: pause + move list (`mostragolpe`) |
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
| `Energeticos.dat` | sets a 0.15 factor (`0x4b5898`) — UNKNOWN use |
| `Satanas.dat` | blood on every hit |
| `Fadiga.dat` | VS screen jumps straight to the ending (state 10) |
| `Mentecapto.dat` | UNKNOWN (`0x4b58af`) |
| `Bruxa.dat` | no limit of one projectile per owner |
| `SC01.dat` … `SC12.dat` | force stage NN |

## Startup defaults (FormCreate) [decomp]

`g_speedDelay = 28`, `g_gravity = 4`, `g_meterUnit = 2.0`, `g_damageLevel =
0.9`, `0x4b57c8+10 = 330`, high-score defaults CHV/SMD/CHQ/QUI/DFL/PGR/SBR/
B71/PTY/NHO (`criarecorde`), all assets loaded (see 03), `V1.5A` version
string, then `DXTimer1` enabled.
