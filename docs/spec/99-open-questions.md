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
    x/y and `inAir`/`vy` writes). Still open from the same function: the throw
    set-ups that place and launch the opponent (decomp lines 871–886, 973–986,
    1357–1373, 2663–2676; endings 1424, 2715), the `knockdown` flag (+0x69)
    that special states set every tick (~40 sites, so specials do not knock
    down yet) and the KO fix near line 302.
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

