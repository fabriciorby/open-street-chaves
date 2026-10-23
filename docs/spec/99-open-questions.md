# 99 — Open questions and verification plan

Things the spec still marks UNKNOWN or [infer], in priority order for a
faithful port.

## Blocking for a first playable build

1. ~~FPU literal constants~~ — resolved (values in 06, 05, 02; menu step
   0.1).
2. ~~Difficulty → probability~~ — resolved: `k` uses both "Nível do Jogo"
   (`g_cfg0`) and the fight number (05, "AI difficulty").
3. ~~Per-type projectile motion~~ — resolved (06 "Motion rules"); the
   off-screen cull exists and is ported (06 rule 5: x outside camX−50..camX+450, y above camY−50).
   The last gaps of `desenhaefeitos` are closed too (Neves' orbiting 31/32, Power Geyser box growth
   and re-arm, Psycho Field re-arm, type 24 wobble, range sprites of 24/25/30, the start-of-tick loop
   rewind, type-5 popup text, draw-at-slot-time): inventory in 06 "desenhaefeitos vs port inventory".
4. ~~HUD coordinates~~ — resolved: `draw_hud_top` / `draw_hud_bottom` follow
   DXTimer1Timer 0x4a49ac..0x4a5a61 call by call with the operands read from
   the assembly (07b-layout "full transcription"); the join prompts (size 10)
   and the challenger box (0x4ae384: rounded 80,100..320,170 r 10, Get(10..12)
   at y 100/120/140 size 15 RGB(250,140,140), noPush := 1) checked again.
5. ~~Select / VS / continue / records layouts~~ — resolved: transcribed call by
   call (07b "Select, next battle, continue, records, name entry: full
   transcription"), records gradient and row sizes included.
6. ~~Intro cinematic and the 15 ending choreographies~~ — transcribed
   (07b "Attract", `07e-endings.txt` via `re/tools/endings_emu.py`, which runs the
   code). Answered in 07b "Endings reviewed against the binary": the ending
   fighters do not animate by themselves (desenhalutador is given a fixed
   state/timer each tick and runs the frame selector on it), and the firework
   spawn boxes are type 1 x 130..279 y 100..199, 24 x 150..249 y 60..89,
   25 x 250..309 y 110..129.
7. ~~Remaining record fields~~ — +0x14, +0x1a, +0x8c, +0x94, +0x9f are never
   referenced by the code; +0x4c/+0x50 are only written (reset per round /
   by throws, decomp all.pp.c 87393–87400, 95115, 95184), so they have no
   visible effect; `healthShown` (+0x48) is the lagging red bar (ported);
   `0x4b449d` (round-end transition style 1..4) is ported (`g->transition`). ~~`0x4b58a2/a3`~~ — resolved: FormCreate counts the
   `Sons/Som-NN.wav` / `Musicas/Musica-NN.wav` files (07b "Menus reviewed").
8. ~~Language 2 fill order~~ — checked with `--es` on the fight HUD, title,
   credits and endings: every constant index picks the Spanish line.
9. ~~Per-character CPU special choices~~ — resolved: table in `05d`
   (constants read from the float literals of `cpu`).
10. ~~Attract timeline~~ — transcribed from the code and counted call by call
    against the binary (07b "Attract reviewed against the binary"). The records
    gradient and row sizes were already exact; the join prompt is size 10, the
    challenger box is a rounded box with size-15 text (another change), and the
    hit spark (type 1) is spawned at the exact hit-box centre / effect position
    with no random spread (0x4a8f2e, 0x48feb2, 0x48fed1; `FUN_00476a04` only sets
    its life to 3).
11. ~~Effect spawn offsets~~ — resolved: `06e-selector-spawns.txt`: every
    `FUN_00476a04` call of the frame selector with its guards and offsets. ~~Stage 8 moving
    elements~~ — the passing car and the flickering mice/lights are in 07c
    (`xmul`, `yjit` columns).
12. ~~Special-move sounds and motion~~ — resolved: `06c-special-sounds.txt`
    (every `g_sndIndexA` of the frame selector) and `06d-special-motion.txt`
    (all 196 `sub_47648c` walk calls with their pushed steps, plus the direct
    x/y and `inAir`/`vy` writes). The rest of the function (throw set-ups and
    endings, the `knockdown` flag stores, the KO landing) is ported too: see
    the frame-selector inventory in 05 (`06f`, `06g`, `sel_code`, `sel_throws`).
13. ~~Port checks after 06d~~ — resolved by tracing a 3000-frame CPU fight for 12 pairs with a
    per-tick log. (a) Floor: the 2-12 px under the floor seen in specials (Furtado 27, Madruga 27,
    Glória 28/29, Dona Neves 27) only existed in a log taken before the move clock and gravity
    block; at the end of the tick `y <= groundY` always holds. The original does it the same way:
    a non-airborne special moves y freely and `y := groundY` is applied by the move clock when the
    move ends (DXTimer1Timer ~5625) or by the airborne landing check (~5750). (b) x outside
    `camX+30..camX+370` only happens for fighters with `health <= 0`: the knockdown flight (state 19,
    which adds `hitstun` to x without `sub_47648c`, DXTimer1Timer ~5660) and the bounce (state 20 via
    `sub_47648c`, whose clamp is guarded by `0.0 < health`, 0x4b4478) are unclamped in the original too,
    and a killing throw has `hitstun` 28..30, so a corpse flies 300-600 px (x −610..1150 seen).
    Alive fighters never leave the window (the round-1 jump-in from x 0 / 600 is original, 0x4a44cb).
    (c) The only real divergence was the camera clamp (port: stage image size, original: x 0..200,
    y 0..20), now fixed; effects at such x are removed by the cull of item 3.

14. ~~F1/F2 menus~~ — reviewed call by call against the binary (07b "Menus reviewed against the
    binary"): layout, colours, wrap, button rules, file format and the F1 exit were wrong or missing
    in the port and are fixed. Open: the F1-leave stop of the sound-effect item (no per-sound stop in
    the port), the original's default `Chaves.dat` locale (pt-BR writes "0,9"; the port reads both),
    and `StrToInt` on a malformed line (the original raises, the port keeps the default).

## Verification plan (needs Wine or a Windows VM)

- Run the original with `g_debugOverlay`-style instrumentation: since the
  overlay exists in the binary, find what toggles `0x4b58a4`/`0x4b58a5`
  (UNKNOWN — possibly a key combination in FormKeyDown not yet mapped, or
  a `.dat` unlock) and record a fight to compare positions/frames with the
  port tick by tick.
- Capture `Sons/*.wav` usage by index to label every sound.
- Compare the sliced frames of `re/out/` against the port's slicing of the
  same strips (identical separators → identical frame counts per fighter).

## Already settled (do not re-investigate)

Tick = DXTimer interval (28 ms default); 25 ticks per game second; RNG =
Delphi `Random`; transparent colour = white; frame slicing by dark
separator columns; hit/hurt boxes relative to sprite top-left; damage ×
`g_damageLevel`; meter 102 → stock; super needs stock or health ≤ 15; block
breaking rules (low attacks vs stand block, jump attacks vs crouch block);
projectile cancel rules; text tables PT/ES; config file layout.

- **First-attack headline** — on the first hit of a round the attacker gets
  `msgIndex = 1`, and the HUD draws `MemoTexto.Get(0)` big in green; line 0
  is "-", so nothing visible appears in our dump. We skip "-"; only the small
  "Primeiro Ataque" line shows. A build with a different memo may differ.

