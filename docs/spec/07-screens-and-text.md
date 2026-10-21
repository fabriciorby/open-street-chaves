# 07 — Screens, menus and text

Legend: **[decomp]** Ghidra pseudocode (not source), **[infer]**
interpretation, **UNKNOWN** not established. All in-game text lives in
`07a-texts.txt` (Portuguese and Spanish), extracted from the routine at
`0x4625a8` that fills the hidden memos at start-up and when the language
option changes.

> `07a-texts.txt` and the `07b-draw-calls/` dumps are **not committed** to
> the public repository (they contain CyberGamba's creative text and raw
> decompiler output). Regenerate them locally from your own `Chaves.exe`
> with `re/tools/` (see 00); the port extracts the game text from the
> executable at runtime instead of shipping it.

## Text tables [decomp]

| Memo | Lines (per language) | Content |
|---|---|---|
| `MemoNomes` | 16 | `-` + 15 fighter names (ES: EL CHAVO, DON RAMÓN, CHILINDRINA, …) |
| `MemoTexto` | 32 | HUD/system strings: 1 "Primeiro Ataque", 2 "Acertos.", 3 "M A X", 4 "VITALIDADE", 5 "INSIRA FICHAS", 6 "ENTRE AGORA", 7 "SELECIONE SEU LUTADOR", 8 "TEMPO", 9 "ESPERE ...", 10–11 "ALGUÉM ENTROU / NO COMBATE !", 12 "VENCEDORES NÃO USAM DROGAS", 13 "VENCEU", 14 "LUTEM", 15 "MÁXIMO DE ACERTOS", 16 "PONTOS", 17 "NIVEL-", 18 "CREDITOS", 19 "PRÓXIMA BATALHA", 20 "OS 10 MELHORES LUTADORES", 21 "O LUTADOR DA VILA", 22 "FIM DO JOGO", 23 "CONTINUE ?", 24 "OBRIGADO POR JOGAR", 25 "CRIADO POR CYBERGAMBA 2003/2004", 26 "FIM DO TEMPO", 27 "EMPATE", 28 "BATALHA", 29 "PERFEITO", 30 Spanish-version credit, 31 "COMPLETO" |
| `MemoCenarios` | 12 | stage names 1..12: Vila do Chaves, Casa do Seu Madruga, Casa da Dona Florinda, Sala de Aula, Entrada da Vila, Terreno Baldio, Salão de Cabeleireiro, Restaurante da Dona Florinda, Segundo Pátio da Vila, O Parque, Casa do Senhor Barriga, Parque de Diversões |
| `MemoConfig` | 33 | F1/F2 menu labels (below) |
| `MemoFrases` | 60 | win quotes: `[(char−1)*4 + k]`, k = 1..3 (slot 0 of each group is `-`) |
| `MemoFinais` | 180 | ending dialogue: 12 lines per character |
| `MemoDialogo` | 650 | pre-fight dialogues: runs of a speaker marker line (`A` = player, `B` = opponent) followed by 3 text lines |

The code indexes all memos with constants, e.g. `MemoTexto[15]` is "ROUND"
only in the sense that line 15 is "MÁXIMO DE ACERTOS" — the exact mapping of
each read site to a line is in `mainloop.c` (`(piVar4 + 0xc))(piVar4, N,
…)` = `Memo.Lines[N]`).

## F1 configuration menu (state 2) [decomp]

Cursor `g_menuCursor` 1..12, moved with up/down (both players' devices),
values changed with left/right, buttons act on the test items. Labels are
`MemoConfig` lines; saved to `Chaves.dat` on exit (line order = list order):

| cursor | label | variable | values | default |
|---|---|---|---|---|
| 1 | Nível do Jogo | `g_cfg0` → file line 0 | 1..8 (slider ×10 px) | 4 |
| 2 | Tempo do Round | `g_cfg1_mode` → line 1 | 1..4 = 30 / 60 / 90 / 99 s | 3 |
| 3 | Poder | `g_cfg2` → line 2 | 1 NORMAL, 2 1P MAX, 3 2P MAX, 4 MAX (permanent 3 super stocks) | 1 |
| 4 | Nível de Dano | `g_damageLevel` → line 3 | 0.3 .. 2.0 step (`_DAT_004aeeb8`, 0.1 [infer]) | 0.9 |
| 5 | Sangue | `g_cfg4_bool` → line 4 | SIM / NÃO (blood effects) | SIM |
| 6 | Rounds | `g_cfg5` → line 5 | 1..3 rounds to win | 2 |
| 7 | Linguagem | `g_cfg6` → line 6 | 1 Português, 2 Espanhol (refills the memos) | 1 |
| 8 | Teste de Som | `g_sndIndexA` | index 1..`0x4b58a2`; button plays `Sons/Som-NN` | — |
| 9 | Teste de Música | `g_sndFlagB` | index 1..`0x4b58a3`; button loads/plays `Musicas/Musica-0N` | — |
| 10 | Flash | `g_cfg7_bool` → line 7 | screen flash on hits | SIM |
| 11 | Mostrar Nível | `g_cfg8_bool` → line 8 | show "NIVEL-n" (difficulty) on screen | SIM |
| 12 | Mostrar Creditos | `g_cfg9_bool` → line 9 | show "CREDITOS n" | SIM |

"Nível do Jogo" (`g_cfg0`, 1..8) is the CPU difficulty: the `cpu()` and
`defendergolpe()` probabilities are `Random(round(k)) == 0` where `k`
derives from it [infer: the FPU operand could not be recovered textually; it
is the only 1..8 setting].

## F2 key configuration (state 3) [decomp]

Labels `MemoConfig[23..32]` ("Configurações de Teclas", Cima, Baixo,
Esquerda, Direita, Soco Fraco, Chute Fraco, Soco Forte, Chute Forte,
"F2-SAIR"). Cursor `g_menuCursor` 1..8; any letter/digit/arrow/numpad key
pressed in FormKeyDown is stored into DXInput's keyboard assign slot for
that action (P1 only) and the cursor advances. Not persisted in
`Chaves.dat` (UNKNOWN whether DelphiX saves it elsewhere; assume not).

## Screen flow [decomp]

- **State 0 — resolution menu**: "SELECIONE A RESOLUÇÃO": 400x300 /
  TELA CHEIA (1) / TELA CHEIA (2); "- SELECIONA", "Z - CONFIRMA". Chooses
  windowed vs two full-screen modes for DXDraw, then state 1.
- **State 1 — attract**: counter `g_ciclo2` 0..650: intro cinematic drawn
  inline from `DXImageListAbertura` (buildings, people, Chaves vs Madruga,
  logo PT/ES by language; call list in `07b-draw-calls/state1_attract.txt`), "INSIRA FICHAS"/"ENTRE AGORA" blinking after tick 440,
  sound 9 at tick 200; at tick 650 with no credits it starts a **demo
  fight** (state 7 with `g_demo = 600`, both players CPU, random stage) and
  returns to attract afterwards; a coin jumps to tick 500 (title).
- **State 4 — character select** ("SELECIONE SEU LUTADOR", timer 30 s
  shown with "TEMPO"): 15 portraits (`DXImageListRetrato[(id−1)*5]`, 50×60)
  in two rows of 8/7 at x = (i)*49 − 402 + scroll, y = 163 and 223; cursor
  per player (P1 starts on 1, P2 on 2; Gentalha unlock starts on 8); left
  /right move ±1 with wrap 1..15, up/down jump ±8 rows, button 1 confirms
  (sound 2); the name (`MemoNomes[id]`) and big portrait (`+4`) are shown;
  select sound 1 on move. Both confirmed (or time out) → state 5 (CPU
  opponent chosen first) or 6 (two humans).
- **State 5 — VS / ladder**: shows the arcade ladder faces (8 slots,
  `DXImageListRetrato[(id)*5 − 4]`), "PRÓXIMA BATALHA"; the stage is chosen
  by the opponent's character (1→1, 2→2, 3→2, 4→3, 5→3, 6→4, 7→11, 8→1,
  9→1, 10→11, 11→6, 12→1, 13→1, 14→2, 15→1) with a 40 % chance of an
  alternative stage (2→7, 3→10, 4→10, 5→8, 6→8, 7→5, 8→9, 9→10, 10→10,
  11→5, 12→9, 13→5, 14→1, 15→6) and a 1/3 chance of stage 12 for 3, 4, 9,
  10, 11, 14; `SCxx.dat` overrides. The stage name (`MemoCenarios`) is
  shown; a button speeds the counter; at 150 → state 6. `Fadiga.dat` skips
  straight to the ending (state 10).
- **State 6 → 7 — fight**: see 05. Before round 1 against the rival
  (ladder slot 8) a dialogue (`g_dialogo = 600`, `MemoDialogo`) plays.
- **Round end → next**: when a player reaches `Rounds` wins: human loses →
  state 12 (name entry) if `score > lowest record` else state 9
  (continue); human wins → state 12 or 10/next fight; after the 8th fight →
  ending. A "challenger" joining during a fight sets `g_challengeTimer =
  100` ("ALGUÉM ENTROU NO COMBATE !") then state 4.
- **State 9 — continue**: "CONTINUE ?" with a 9-second countdown
  (`g_timerA`, 25 ticks per second); `1`/`2` with credits restarts at state
  4/5; at 0 → "FIM DO JOGO" then state 8.
- **State 8 — game over / records**: "OS 10 MELHORES LUTADORES" table from
  `g_records` (name, portrait, score), scrolls for 280 ticks, back to
  state 1.
- **State 12 — name entry**: 3 letters (`A`..`Z`, `0`..`9` via up/down,
  button to advance) into `players[p].name`; then `criarecorde(10, name)` +
  `ordenarecordista`; → state 9 or 10.
- **State 10 — ending**: per character cutscene (`g_ciclo` high byte =
  character) using stages/fighters/`DXImageListFinal` items and
  `MemoFinais` lines (12 per character), then state 11.
- **State 11 — credits**: "VENCEDORES NÃO USAM DROGAS", "OBRIGADO POR
  JOGAR", "CRIADO POR CYBERGAMBA 2003/2004" for 200 ticks → state 9.

Ending cutscenes (assets per character, from the state-10 code):

| char | stages / fighters loaded | Final images |
|---|---|---|
| 1 Chaves | Cenario-02, C01 + C02 | — |
| 2 Madruga | Cenario-03, C02 + C05; then Cenario-01, C01 | Violao, Madruga-Violao |
| 3 Chiquinha | Cenario-01, C09 + C04; then C01 | Quico-Urso, Chaves-Urso, Paty-Urso |
| 4 Quico | Cenario-01, C04 + C01 | Chaves-Bola |
| 5 Florinda | Cenario-02, C05 + C02; then Cenario-01, C05 | Madruga-Machucado, Girafales-Flores |
| 6 Girafales | Cenario-11, C06 + C07; then Cenario-03, C05 | Girafales-Flores |
| 7 Barriga | Cenario-02, C07 + C02; then Cenario-01 | Madruga-Velho do saco |
| 8 Bruxa | Cenario-01, C08 + C02; then Cenario-02, C02 | Chaves-Gordo, Prato vazio |
| 9 Paty | Cenario-02, C09 + C03; then Cenario-01, C09 + C01 | Paty-Urso |
| 10 Nhonho | Cenario-01, C10 + C01; then Cenario-03, C10 + C04 | — |
| 11 Godines | Cenario-02, C11 + C03; then Cenario-05, C01 + C11 | — |
| 12 Furtado | Cenario-01, C01 + C02 | — |
| 13 Jaiminho | Cenario-01, C13 + C01 | — |
| 14 Neves | Cenario-01, C13 + C14 | — |
| 15 Glória | Cenario-02, C15 + C03; then Cenario-01, C02; then C02 + C04 | — |

The per-tick choreography of each ending (positions, which frames, effect
bursts such as fireworks type 1 / 24 / 25) is in `mainloop.c` lines
6807–7680 and is **not yet transcribed**.

## HUD (state 7) [decomp, partial]

- Health bars: rounded bars at y = `0x4b44d2 + 15..19`, player 1 from x =
  170 leftwards, player 2 mirrored (`x = p*0x9d + 200` …), length ∝
  `health` (125 = full), a second lagging bar from `healthShown` [infer].
- Timer: two digits centred, red when < 10.
- Portraits: `DXImageListRetrato[(id−1)*5 + 0/2]` (P1) and `+1/+3` (P2) —
  the `-a/-b/-c` faces are normal / hit / win [infer].
- Super meter: 3 round stock lamps per player (`circulopintado`, 10 px
  apart) and a bar 0..102 at y = `0x4b44d4 + 0x11c`; blinking when full.
- Combo counter "n Acertos." with `MemoTexto[2]`, "M A X" for the best.
- Round win markers: `roundsWon` lamps.
- Optional "NIVEL-n" and "CREDITOS n" lines (config 11/12).
- Debug overlay (`g_debugOverlay`) prints the labelled variables listed
  in 02.

Exact pixel coordinates of every HUD element are still to be transcribed
from the `barra`/`texto` calls (UNKNOWN in this revision).
