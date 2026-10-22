# 07b — Layout transcription (HUD, screens, stage animation, cutscenes)

Legend: **[decomp]** Ghidra pseudocode (not source), **[infer]**
interpretation, **UNKNOWN** not established.

The folder `07b-draw-calls/` holds one file per screen with **every drawing
call** of that region of `DXTimer1Timer` (or of `desenhaanimacoes`), in
program order, prefixed with the `if` conditions that guard it and with
the arguments expanded from the preceding assignments (`{…}`). They were
produced by `re/tools/drawcalls.py` from the decompiled C; numbers are
pixels in the 400×300 back buffer (origin top-left), colours are
`RGB(r,g,b)`. Where the decompiler lost an operand the text shows `?`,
`Round()` or an unexpanded `iVarN` — those spots need the assembly or a
runtime check (listed in 99).

Helper signatures (from `TForm1`): `barra(x1, y1, colour, y2, x2)` filled
rectangle; `retangulo(x1, y1, colour, y2, x2)` outline; `barraredonda(x1,
y1, fill, border, radius, y2, x2)` rounded rectangle; `circulo` /
`circulopintado(x, y, …)`; `texto(x, y, colour, size, …, text)` (`size` 9
and 12 are used; the `x` is the centre for HUD texts); `Draw(item, surface,
x, y)`; `func_0x004616c0` = `DrawRect`/stretch of an item into a rect
`(l, t, r, b)` built by `FUN_0040d760`.

## Fight HUD (`state7_hud.txt`) [decomp]

All HUD y values are relative to `hudY = 0x4b44d2` (0 during the fight,
animated to −120 at round end) — the second block uses `0x4b44d4`.

| element | call | geometry |
|---|---|---|
| Round text | `texto(200, hudY+3, …)` | centred at x = 200 |
| Timer frame | 3 nested `barraredonda`: (170, hudY+15) → (230, hudY+54) white, (172, hudY+17) → (228, hudY+52) grey `RGB(170,170,170)`, (174, hudY+19) → (226, hudY+50) black | radius 15 |
| Timer digits | `texto(200, hudY+10, …)` two sizes; red blinking when `timerA < 10`; hidden when the round time is "99" (`g_cfg1_mode == 4`) | |
| Health bar frame (per player p = 1 → sign −1, p = 2 → +1) | `barraredonda(200 + s·157, hudY+30, white, …, 8, hudY+50, 200 + s·17)`; inner grey `(200 + s·155, hudY+32) → (200 + s·19, hudY+48)`; black `(200 + s·153, hudY+34) → (200 + s·21, hudY+46)`; red when `health ≤ 15` (blinks) | the bar runs from x = 200 ± 21 to 200 ± 153, i.e. 132 px for 125 HP |
| Health fill | `barra(200 + round(k), hudY+35..39, …)` several stripes (y+35, +36, +38, +39) for the 3-D look; a `retangulo` at `(…+40)·s + 200, hudY+35)` marks the 15-HP "danger" threshold | fill width ∝ health: `k = health × 132 / 125` [infer from the frame] |
| Round-win lamps | `barraredonda(200 + s·(195 − 17·(i−1)), hudY+70, …, 5, hudY+85, 200 + s·(180 − 17·(i−1)))` for i = 1..`Rounds`; filled red-ish when won, blinking when the lamp is the deciding one | 15×15 px lamps, 17 px apart, starting 195 px from centre |
| Portraits | `Draw(Retrato[(id−1)·5 + 0/2], …)` at the outer ends of the bars, P2 uses `+1/+3` (mirrored faces) | exact x in the dump (`state7_hud.txt`, items 0x304) |
| Names | `texto(200 + s·145, hudY+45, …, MemoNomes[id])` | |
| "INSIRA FICHAS / ENTRE AGORA" | `texto(200 + s·100, 0, …)` blinking (`g_ciclo < 15`) above an empty side | |
| Combo counter | "n Acertos." / "M A X" via `MemoTexto[2]/[3]` at the player's side, `hudY+0x2d` | |
| Super stocks | 3 `circulopintado` at `(200 + s·(30 + 10·(i−1)), 0x4b44d4 + 0x11e)` i = 1..3, lit (`RGB(…)` pulsing with `0x4b4494`) for owned stocks | 10 px apart |
| Super bar | `barraredonda(200 + s·60, hudY2+0x11d) → (…, hudY2+0x120)` frame, inner bars at +0x11c/+0x11b; fill width ∝ `meter/102`; when full the bar flashes and "MAX"/`MemoTexto[3]` is drawn | at the bottom of the screen (y ≈ 283) |
| Level / credits | "NIVEL-n" (`MemoTexto[0x12]`) and "CREDITOS n" (`[0x13]`) at the bottom when the options are on | |
| Challenger box | `barraredonda(80, 100, …, 10, 170, 320)` with `MemoTexto[10..12]` | centred 240×70 box |

### Fight HUD, full transcription (2026-10-22) [decomp]

The table above was a first pass; `draw_hud_top`, `draw_hud_bottom` and
`draw_round_texts` in `src/game.c` now follow DXTimer1Timer
(0x4a49ac..0x4a83bf) call by call. Corrections to the table:

- Draw order: stage, stage animations, **top HUD**, shadows, fighters,
  effects, **bottom HUD** (messages, combo, stocks, meter), round texts,
  winner scene and wipe.
- Primitives: `barraredonda(x1,y1,x2,y2,d,pen,brush)` is GDI RoundRect
  (corner ellipse diameter `d`); `circulopintado` is Ellipse; `barra` and
  `retangulo` normalise their corners.
- Health fill is 1 px per HP: `200 + round(s·(25 + health))` to `200 + s·25`,
  stripes y+35..45 (150,150,0), +36..44 (220,220,70), +38..42 (250,250,100),
  +39..40 white. A red trail `(170,0,0)` follows `shown` (+0x48), which
  waits `+0x50` = 20 ticks after a hit and then falls 3 per tick.
- Portrait: `Rxx-a`, or `Rxx-b` shaking ±1 px while the red trail is above
  the health; P1 at (3, hudY+7), P2 mirrored at (347, hudY+7). A name plate
  `barra(200+s·198, hudY+40, 200+s·130, hudY+68)` pulses blue / red.
- Names at `200+s·145, hudY+45`; "1P"/"2P" and the score at `hudY+10`
  for human players.
- MemoTexto index: the code's `Lines.Get(n)` is our `osc_text(TEXTO, n−1)`.
- Super stocks sit in a white pill at `hudY2+280..288` with the count at
  `200+s·18`; the meter frame spans `200+s·60..170`, fill from `200+s·64`.
- Round texts: "ROUND n" at rs 6..39, LUTEM at 43..59, KO at re 1..29,
  winner name / VENCEU / VITALIDADEx5 / PERFEITO / COMPLETO at re 31..89,
  big `Rxx-c` slide + quote box (Frases `(id−1)·4 + quote`) at re 90..210,
  and one of four black wipes (`transition` 1..4) after re 95.

## Character select (`state4_select.txt`) [decomp]

- Background: `Cenario` item 0 scrolled by `g_camY`, and 4×2 big
  background tiles (`item(0x328, …)`, 145 px apart) by language.
- Title `MemoTexto[7]` ("SELECIONE SEU LUTADOR") and `TEMPO` + seconds.
- Portrait grid: 15 cells `(x_i, y) → (x_i + 50, y + 60)` with
  `x_i = round(scroll) + 49·(i mod 8) − 402` and `y = 163` for i = 0..7,
  `y = 223` for i = 8..14 (`FUN_0040d760(x, 0xa3/0xdf, x+0x32, …, 0xdf/0x11b)`),
  drawing `Retrato[i·5]` stretched into the cell.
- Cursor: a `retangulo` around the selected cell (P1 yellow-ish, P2 the
  other colour, blinking with `g_ciclo`), "1P"/"2P" labels under it.
- Big portrait `Retrato[(id−1)·5 + 4]` and name `MemoNomes[id]` on each
  player's side, with a `Cos()`-based bob.
- Scroll-in: `g_ciclo2` grows by 20 per tick up to 300 before input is
  accepted (`g_selectSub` 1 → 2).

## VS / ladder (`state5_vs.txt`) [decomp]

- 60 rows of thin coloured `barra`s as a background gradient.
- Ladder: for slot i = 1..8, face `Retrato[(ladder[i]·5) − 4]` (the
  `-b` face) at `x = 48·i − 40` (y fixed), drawn until `g_ciclo2 ≥ 50`
  then shifted by 5 px; slots already beaten (`i < fightNo`) show the
  normal face, the current one blinks, future ones are hidden unless
  `Churros.dat`.
- Stage preview: `Cenario` item 0 stretched into `(80, 6) → (320, 134)` and
  the stage name `MemoCenarios[stage−1]` after `g_ciclo2 > 60`.
- `MemoTexto[0x14]` ("PRÓXIMA BATALHA") header, player names.

## Continue / game over / records / name entry [decomp]

- Continue (`state8_9.txt`): `MemoTexto[0x18]` ("CONTINUE ?") and the
  countdown digit at size `25·timerA` (grows as it counts down), then
  `MemoTexto[0x17]` ("FIM DO JOGO").
- Records (state 8): title line 20 ("OS 10 MELHORES LUTADORES") at (200,5)
  while `g_ciclo` < 18, line 5 ("INSIRA FICHAS") at (200,240) while < 10;
  10 rows at y = 60i − 350 − `g_camY`: `"i -"` at x 50, name at x 100,
  `Retrato[(char−1)·5]` at x 160, score at x 230 (row 1 blinks); scrolling
  up after tick 80 (−5 per tick, floor −320), black after tick 240, state 1
  after tick 280. Defaults (FormCreate): CHV 10000, SMD 8000, CHQ 7500,
  QUI 7000, DFL 6500, PGR 6000, SBR 5500, B71 5000, PTY 4500, NHO 4000,
  portraits 1..10. The gradient colours and the text sizes of the rows were
  not recovered from the code [approx].
- Name entry (`state11_12_tail.txt`): the 3 letters of `players[p].name`
  with the active one blinking, `MemoTexto[0x19/0x1a/0x1f]` credits lines
  in state 11.

## Attract / intro (`state1_attract.txt`) [objdump, verified against the x86 code]

`texto(x, y, s, align, size, colour)` draws Comic Sans MS at `size` points
with a black shadow at (+1,+1); `align` `'c'` centres on x, `'e'` right-
aligns. Text line numbers below are the 0-based `MemoTexto` lines of
`07a`/`07d` (the code's `Lines.Get(n)` is one higher). Timeline on
`g_ciclo2` = c, scroll `camY` (0 at c = 100, −2 per tick after c > 235,
floor −200); `g_ciclo` wraps at 20 here:

| c | what |
|---|---|
| < 100 | screen filled RGB(120,120,250); `Abertura[14]` (Vencedores) at (110,10); line 12 ("VENCEDORES NÃO USAM DROGAS") centred at (200,210), size 16, RGB(250,200,0) |
| 100–370 | sky: 21 bars `(0, 15i)–(400, 15i+15)` RGB(120+2i, 120+2i, 200); `Abertura[2]` at (50, 50−camY/2), (350, 150−camY/2), (300, 180−camY/2); `[3]` at (50, 47j+73−camY/2), (350, 47j+173−camY/2), (300, 47j+203−camY/2) for j 0..4; `[0]` at (100, 80j−200−camY) and (305, 80j−200−camY) for j 0..6; `[1]` at (39i+113, 32j−200−camY) for i 0..4, j 0..15; black bar (147, −155−camY)–(270, −48−camY) with a white rectangle (149, −153−camY)–(268, −50−camY); `[4]` (Folhagem) at (50i, 220−camY) for i 0..7; people `[7]` at (50i, 245−camY+b), `[5]` at (50i−15, …), `[6]` at (50i+15, …) for i 0..8 with b = 4 while `g_ciclo` < 10; c < 200: `[8]` Chaves at (210, 185+b2−camY) and `[9]` Seu Madruga at (50, 178+b2−camY), b2 = 5 while `g_ciclo` > 10; c = 200: sound 9; c ≥ 200: `[11]` Seu Madruga caindo at (60−4(c−200), 180+2(c−200)−camY) and `[10]` Chaves dando soco at (100, 190−camY) |
| any | logo zoom factor k: 4 while c < 400, c−396 for 400..420, 440−c for 420..438, then 2; `Logo - PORTUGUES/ESPANHOL` (`[12]`/`[13]` by language) stretched to (205−round(223/k), −100−camY−round(117/k))–(205+…, −100−camY+…) |
| > 440 | the version string "V1.5A" left-aligned at (320,140), size 8, white; line 21 ("O LUTADOR DA VILA") size 12 RGB(90,90,255) sliding in: x = 200+4(500−c) on even ticks, 200−4(500−c) on odd ticks, fixed at 200 from c = 500 |
| any | `Abertura[15]` (Cybergamba) at (5,280); line 25 (CRIADO POR …) at (200,253) and line 30 (VERSÃO EM ESPANHOL …) at (200,267), size 8, white; while `g_ciclo` < 12: line 5 ("INSIRA FICHAS") or line 6 ("ENTRE AGORA" with credits) at (200,235) |
| tick | with credits: c := 500, camY := −200 (title held); c > 650: demo fight (state 7, `g_demo` = 600, both CPU, stage round(Random(120)/10)+1 clamped 1..12, characters round(Random(150)/10)+1 clamped 1..15) |

Demo fight: the clock stays at the configured time, both healths are reset to
125 every tick, "INSIRA FICHAS" blinks at (200,240) while `g_ciclo` < 20,
`Icone[11]`/`[12]` (Logo Transparente) sits at (390−w, 280−h) once `g_demo`
< 500, the screen is black for the last 40 ticks and at `g_demo` < 6 the
game goes to state 8 (records). Keys: `5` adds a credit (sound 0; during
the demo or on the records screen it also jumps to the title), `1`/`2`
with a credit start that player at state 4 (fight 1, select timer 30 s).

## Stage animations (`stage_animations.txt`, `desenhaanimacoes`) [decomp]

Called every fight tick after the background. Each stage draws a few
`Efeitos` base sprites and cycles `Animacoes/Anim-NN.bmp` frames on
`g_ciclo` (blink counter, period 25): e.g. stage 1/5/6 share a window with
4 frames (`Anim-01..04`, 8 ticks each) at `(240 − camX, 130 − camY)`;
stages 1/2/6 add a second element at `(385 − camX, 260 − camY)` unless
Chiquinha (char 3) is fighting (her sprite would overlap it [infer]).
The complete stage → sprite → position table is the dump itself; the
stage ↔ `Anim` index map printed by the extraction run is:

| stage | Anim-NN frames | Efeito-NNN statics |
|---|---|---|
| 1 | 12, 16, 28, 29, 57, 58, 59 | 001 |
| 2 | 05, 06, 10, 11, 60, 61 | 001 |
| 3 | 07, 08, 62, 63 | 001 |
| 4 | 37, 38, 64 | 001 |
| 5 | 01, 04, 13, 14, 15, 33 | 001 |
| 6 | 01, 02, 03, 04, 05, 06, 09, 35, 36, 65, 66, 67, 68, 69, 70 | 001 |
| 7 | 51, 52, 53, 54, 55, 56 | 001 |
| 8 | 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27 | — |
| 9 | 30, 71, 72, 73, 74, 75 | 001 |
| 10 | 39, 40 | — |
| 11 | 76, 77, 78 | — |
| 12 | 13, 14, 15, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50 | 001 |

Stages absent from the table draw no animation (or only the shared
elements guarded by several stage ids at once).

## Endings (`state10_endings.txt`) [decomp]

178 drawing calls, one block per character (`g_ciclo >> 16 == char`),
keyed on `g_ciclo2` ranges (0..~1700 ticks): fighters drawn with
`desenhalutador(p, y)` at scripted x positions, `Final` images stretched
into rects, fireworks (effects type 1/24/25 spawned at random positions
in ranges), and `MemoFinais[(char−1)·12 + k]` dialogue lines. The file is
the transcription; converting each block into a timeline table is left
as a port-time task (99).
