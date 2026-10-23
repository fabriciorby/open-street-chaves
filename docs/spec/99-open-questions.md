# 99 — Open questions and verification plan

Things the spec still marks UNKNOWN or [infer], in priority order for a
faithful port.

## Blocking for a first playable build

1. ~~FPU literal constants~~ — resolved (values in 06, 05, 02; menu step
   0.1).
2. ~~Difficulty → probability~~ — resolved: `k` uses both "Nível do Jogo"
   (`g_cfg0`) and the fight number (05, "AI difficulty").
3. ~~Per-type projectile motion~~ — resolved (06 "Motion rules"); still
   open: whether an off-screen cull exists for looping fireballs.
4. **HUD coordinates** — transcribed statically into `07b-draw-calls/
   state7_hud.txt` and summarised in `07b-layout.md`; a few operands are
   still `Round()`/`?` (health-fill width, portrait x) → confirm from the
   assembly (`objdump -d --start-address=…`) or at runtime.
5. **Select / VS / continue / records layouts** — same status (dumps +
   summary in 07b); exact pixel values for the parts shown as `{…}`
   expressions should be evaluated once when porting.
6. ~~Intro cinematic and the 15 ending choreographies~~ — transcribed
   (07b "Attract", `07e-endings.txt` via `re/tools/endings.py`). Open:
   whether the ending fighters animate (the port shows the scripted
   state/timer frame), and the exact spawn boxes of the fireworks.
7. `healthShown` (+0x48), +0x50, +0x14, +0x7c, +0xac..+0xaf of the player
   record; `Energeticos.dat`, `Mentecapto.dat`; `0x4b449d` (round-end
   transition style 1..4); `0x4b58a2/a3` (sound/music counts).
8. Language 2 fill order of the memos (verify that constant indices pick
   the right language).
9. ~~Per-character CPU special choices~~ — resolved: table in `05d`
   (constants read from the float literals of `cpu`).
10. ~~Attract timeline~~ — transcribed from the code (07b). Still
    approximate in the port: the records-screen gradient and row sizes, the
    text sizes of the join/challenger prompts, the spark (type 1) spawn
    spread.
11. ~~Effect spawn offsets~~ — resolved: `06e-selector-spawns.txt`: every
    `FUN_00476a04` call of the frame selector with its guards and offsets. ~~Stage 8 moving
    elements~~ — the passing car and the flickering mice/lights are in 07c
    (`xmul`, `yjit` columns).
12. ~~Special-move sounds and motion~~ — resolved: `06c-special-sounds.txt`
    (every `g_sndIndexA` of the frame selector) and `06d-special-motion.txt`
    (all 196 `sub_47648c` walk calls with their pushed steps, plus the direct
    x/y and `inAir`/`vy` writes). Still open from the same function: the throw
    set-ups that place and launch the opponent (decomp lines 871–886, 973–986,
    1357–1373, 2663–2676; endings 1424, 2715), the `knockdown` flag (+0x69)
    that special states set every tick (~40 sites, so specials do not knock
    down yet) and the KO fix near line 302.
13. Port checks after 06d: a fighter can sit up to 12 px under the floor for a
    tick or two in a few states (Furtado 27, Madruga 27, Glória 28,
    Dona Neves 27), and a long CPU-vs-CPU run logged x outside the camera
    clamps in states 1/19/20 — confirm against the original whether either
    happens there.

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

