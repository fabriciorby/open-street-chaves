# 99 — Open questions and verification plan

Things the spec still marks UNKNOWN or [infer], in priority order for a
faithful port.

## Blocking for a first playable build

1. ~~FPU literal constants~~ — resolved (values in 06, 05, 02; menu step
   0.1).
2. ~~Difficulty → probability~~ — resolved: `k` comes from the fight
   number, "Nível do Jogo" is unused (05). **Verify at runtime** that the
   menu level really has no effect (search for other readers of
   `0x4b44d9` through aliases if a test contradicts this).
3. ~~Per-type projectile motion~~ — resolved (06 "Motion rules"); still
   open: whether an off-screen cull exists for looping fireballs.
4. **HUD coordinates** — transcribed statically into `07b-draw-calls/
   state7_hud.txt` and summarised in `07b-layout.md`; a few operands are
   still `Round()`/`?` (health-fill width, portrait x) → confirm from the
   assembly (`objdump -d --start-address=…`) or at runtime.
5. **Select / VS / continue / records layouts** — same status (dumps +
   summary in 07b); exact pixel values for the parts shown as `{…}`
   expressions should be evaluated once when porting.
6. **Intro cinematic** (state 1) and the 15 **ending choreographies**
   (state 10): dumped with conditions (`state1_attract.txt`,
   `state10_endings.txt`); turning them into per-tick scripts is a
   port-time task. `desenhaanimacoes` turned out to be the **stage
   animation** routine (dump: `stage_animations.txt`).
7. `healthShown` (+0x48), +0x50, +0x14, +0x7c, +0xac..+0xaf of the player
   record; `Energeticos.dat`, `Mentecapto.dat`; `0x4b449d` (round-end
   transition style 1..4); `0x4b58a2/a3` (sound/music counts).
8. Language 2 fill order of the memos (verify that constant indices pick
   the right language).
9. ~~Per-character CPU special choices~~ — resolved: table in `05d`
   (constants read from the float literals of `cpu`).

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
