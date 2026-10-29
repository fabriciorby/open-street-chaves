# 00 — Code map of Chaves.exe (Street Chaves 1.5A)

Spec index: 00 code map · 01 flow, keys, config · 02 player record and
globals · 03 assets · 04 moves and command parser · 05 fight engine (+05a
animation/hit-box tables, 05b move setup, 05c frame offsets) · 06 effects
(+06a) · 07 screens, menus and text (+07a all strings PT/ES) · 99 open
questions.

Status: first pass. Addresses are virtual addresses in the original
`Chaves.exe` (image base `0x400000`). Each claim below says how it was
obtained:

- **[static]**: read from the binary's bytes or Delphi metadata.
- **[decomp]**: read from Ghidra 12.1.4 pseudocode, which is an analysis
  artifact, not original source.
- **[infer]**: an interpretation that has not been confirmed yet.
- **UNKNOWN**: not established.

## Binary identity

| Item | Value | Source |
|---|---|---|
| File | `Chaves.exe`, 1,894,400 bytes, SHA-1 `d96d9dc7520600ba36da9baded8c88eb3d1bb616`, SHA-256 `bf3a9f361ea506056a189fc3f453bc5a9a9de08a6f1af4c53f57151e6792bee8` | [static] |
| Format | PE32 GUI, i386, image base `0x400000`, entry `0x4af6f0` | [static] |
| Toolchain | Borland Delphi (VMT layout matches Delphi 5) + VCL + **DelphiX** (`TDXDraw`, `TDXTimer`, `TDXInput`, `TDXSound`, `TDXImageList`, `TDXWaveList`, `TDIB`) | [static] VMT scan, 205 classes (`re/out/vmt.txt`) |
| Sections | CODE `0x401000` (0xae738), DATA `0x4b0000` (0x1278), BSS `0x4b2000` (0x38cd), .idata, .tls, .rdata, .reloc, .rsrc (1.1 MB) | [static] |
| Version string | `"V1.5A"` assigned in FormCreate (`0x49cf32`) | [decomp] |

## Program structure

The game is **one form, `TForm1`** (VMT `0x48de1c`, instance size 836).
Every piece of game logic is a *published* method of TForm1, so the
original Portuguese method names survive in the published method table
[static]. Game state lives in **unit globals in BSS**. Code reaches them
through a pointer table in DATA (`PTR_DAT_004b0dxx..004b12xx` → BSS).
`tools/re/globals_pp.py` rewrites those accesses as `g_<addr>`.

### Form components (from the DFM resource `TFORM1`) [static]

Full decoded form: `re/out/forms/TFORM1.dfm.txt`.

| Component | Class | Notes |
|---|---|---|
| Form1 | TForm1 | `BorderStyle=bsNone`, client 674×441, events OnCreate/OnClose/OnKeyDown |
| DXDraw1 | TDXDraw | **Display 400×300, 16 bpp**, `doWaitVBlank`, `doCenter`, `doHardware`, `doRetainedMode`, `doSelectDriver`, `doAllowReboot`; OnMouseMove |
| DXTimer1 | TDXTimer | `Interval = 1`, `ActiveOnly`, **OnTimer = DXTimer1Timer (main loop)** |
| DXInput1 | TDXInput | DirectInput keyboard + joystick 0 (deadzone 50, range 1000); `Keyboard.Assigns` blob holds the default key bindings |
| DXSound1 | TDXSound | `AutoInitialize = False` |
| DXImageListLutador / …LutadorInvert | TDXImageList | fighter frames, normal and mirrored (filled at runtime) |
| DXImageListRetrato | TDXImageList | portraits (filled at runtime) |
| DXImageListEfeitos / …EfeitosInvert | TDXImageList | effects (filled at runtime) |
| DXImageListCenario | TDXImageList | one item `Cenario`, opaque (stage background) |
| DXImageListAnimacao | TDXImageList | animations (filled at runtime) |
| DXImageListIcone | TDXImageList | 14 embedded items: direction arrows (12×12), "Mesa", "ESPECIAL", select cursor, secret face, X, logos, "Teste de forca"; transparent color **white** |
| DXImageListAbertura | TDXImageList | 16 embedded intro images (buildings, people, Chaves/Madruga, logos PT/ES, "Vencedores", "Cybergamba") |
| DXImageListFinal | TDXImageList | 11 embedded ending images |
| DXWaveListSnd / …Musica / …Vitoria | TDXWaveList | sounds; `Musica` has one embedded looped wave (5,551 bytes) |
| ImageLutador, ImageRetrato, ImageRetratoInvert, ImageLutadorFrame | TImage (hidden) | scratch bitmaps for loading and slicing BMPs |
| MemoNomes, MemoTexto, MemoConfig, MemoFrases, MemoDialogo, MemoFinais, MemoCenarios, MemoConfigSalvo | TMemo (hidden) | text tables (names, texts, config, quotes, dialogue, endings, stages) and saved config |

The embedded images have been extracted to `re/out/embedded/*.bmp` by
`tools/re/dib2bmp.py`.

### TForm1 published methods [static: names and addresses; decomp: role]

| Address | Name | Size (bytes) | Role |
|---|---|---|---|
| `0x49ed44` | **DXTimer1Timer** | 65,078 | Main loop / whole game state machine, called every DXTimer tick |
| `0x49cd34` | FormCreate | 4,573 | Init: Randomize, unlock-file checks, defaults, load `Chaves.dat` config |
| `0x49e2d4` | FormKeyDown | 2,576 | Keyboard handling (F1/F2 menus, pause, speed, …) [infer] |
| `0x49e27c` | FormClose | 88 | Cleanup |
| `0x4915cc` | DXDraw1MouseMove | 27 | Mouse handling (trivial) |
| `0x4931ac` | cpu | 6,575 | CPU opponent AI [infer from name] |
| `0x4951ec` | golpe | 5,825 | Attack ("golpe" = blow) |
| `0x494bd8` | defendergolpe | 1,400 | Blocking ("defender golpe") |
| `0x4968b0` | sequenciagolpe | 13,746 | Move/command sequences [infer] |
| `0x4951b8` | sequencia | 51 | Input sequence helper [infer] |
| `0x491b5c` | mostrasequencia | 805 | Show move list (G key) [infer] |
| `0x491edc` | mostragolpe | 2,676 | Show move [infer] |
| `0x499ea4` | criaadversario | 881 | Pick next opponent (arcade ladder) [infer] |
| `0x49a218` | zerardados | 465 | Reset round/match data |
| `0x490bb8` | desenhalutador | 2,299 | Draw fighter |
| `0x48edfc` | desenhaefeitos | 5,982 | Draw effects |
| `0x49a3f4` | desenhaanimacoes | 10,545 | Draw the animated stage elements (`Animacoes/Anim-NN.bmp`) chosen by stage id and blink counter; called every fight tick after the background |
| `0x4905a8` | seta | 685 | Draw arrow (direction icon) |
| `0x48ea84` | carregarlutador | 857 | Load fighter BMP and slice it into frames (see below) |
| `0x4908a8` | carregarcenario | 290 | Load stage |
| `0x4909f4` | carregarosto | 409 | Load face/portrait |
| `0x4915e8` | carregarfala | 146 | Load voice WAV |
| `0x491688` | carregaranimacao | 214 | Load animation BMP |
| `0x491780` | carregareftsonoro | 146 | Load sound-effect WAV |
| `0x491820` | carregarmusica | 214 | Load music WAV |
| `0x491948` | carregarefeitos | 500 | Load effect BMPs |
| `0x4914bc` | criarecorde | 132 | Create high-score record |
| `0x491550` | ordenarecordista | 124 | Sort high scores |
| `0x48e444` `0x48e4c0` `0x48e570` `0x48e5fc` `0x48e69c` `0x48e720` `0x48e8d0` | barra, retangulo, barraredonda, circulo, circulopintado, texto, copiabmp | small | Drawing helpers: bar, rectangle, rounded bar, circle, filled circle, text, copy bitmap region |

### Identified library routines (for reading the pseudocode) [decomp]

| Address | Routine | Evidence |
|---|---|---|
| `0x402a1c` | `System.Random(n)` | `RandSeed = RandSeed*0x8088405+1; return (n*RandSeed)>>32`, with RandSeed at `0x4b2040` |
| `0x402830` | `System.Randomize` | seeds RandSeed from GetSystemTime |
| `0x402894` | `System.Round` | FPU round of ST0 |
| `0x40286c` | `System.Cos` | `fcos` |
| `0x406de4` | `Windows.RGB` | packs r, g, b |
| `0x408490` | `SysUtils.FileSearch` | iterates a `;`-separated directory list |
| `0x408190` / `0x4081f4` | `IntToStr` / `StrToInt` | |
| `0x403d10` `0x403d60` `0x403a24` `0x4039d0` `0x403c50` | LStrCatN, LStrCmp, LStrAsg, LStrClr, LStrLen | Delphi long-string RTL |
| `0x461dcc` | `TPictureCollection.Items[i]` (DXImageList item) | |
| `0x449f54` | `TWaveCollection.Items[i]` | |
| `0x45cd9c` | `TDirectDrawSurface.Canvas` | |

## First findings

### Startup (FormCreate `0x49cd34`) [decomp]

- Calls `Randomize`, so the RNG is time-seeded. A port with seedable RNG must
  still reproduce `Random(n) = (n * seed) >> 32` with
  `seed = seed*0x8088405+1` if exact AI behaviour matters.
- **Unlock files**: it checks with `FileSearch(name, ".")` whether these
  files exist in the game folder and sets a flag for each: `Churros.dat`,
  `Gentalha.dat`, `Energeticos.dat` (sets a double to 0.15:
  `0x3fc3333333333333`), `Satanas.dat`, `Fadiga.dat`, `Mentecapto.dat`,
  `Bruxa.dat`, and `SC01.dat` … `SC12.dat` (stores the highest N found).
  What each unlock does: UNKNOWN (to trace).
- **Config**: if `Chaves.dat` exists, it is loaded into `MemoConfigSalvo`
  and read line by line: line 0 → int, line 1 → one of "1".."4", line 2 → int,
  line 3 → float (default 0.9), lines 4, 7, 8… → "0"/"1" booleans.
  Field meanings: UNKNOWN (to map against the F1 menu).
- Many defaults are set here, for example `30` (0x1e), `25` (0x19), `0x14a`
  (330), a double `2.0`, a double `0.9`, `4`, `1`, `2`, `3`.

### Players [decomp + infer]

- Two player records, 1-based, at `0x4b44e8 + (p-1)*0xb0`. Each record is
  0xb0 = 176 bytes.
- `+0x10` (u8) is the character id 1..15.
- `+0x7a` (i16) is a per-character constant set in carregarlutador: Chaves
  156, Madruga 162, Chiquinha 147, Quico 149, Florinda 144, Girafales 190,
  Barriga 160, Bruxa 160, Paty 150, Nhonho 155, Godines 155, Furtado 165,
  Jaiminho 155, Neves 160, Glória 165. It looks like the sprite/standing
  height [infer].
- FormCreate initialises per player: `+0x60` double = 0, `+0x58` i16 = 0,
  `+0x88` = 0, `+0x8c` = 0, and `+0xa0..+0xa7` eight bytes = 1.

### Fighter sprite slicing (carregarlutador `0x48ea84`) [decomp]

- `Lutadores/Cxx.bmp` is one horizontal strip with all frames side by side
  (C01 is 4639×152, 24 bpp).
- The loader walks the strip column by column. A column whose **top pixel
  and bottom pixel both have R, G and B < 101** counts as a separator. The
  area since the previous separator is copied (`copiabmp`) into image-list
  slot `n` (P1 uses slots 1.., P2 uses 51..).
- The transparency colour and frame offsets are still to be traced in
  `copiabmp` and `desenhalutador`.

## Tooling

- `tools/re/dfm.py`: decode Delphi DFM resources.
- `tools/re/dib2bmp.py`: extract DelphiX embedded images.
- `tools/re/vmt.py`: Delphi VMT, published-method and field scan.
- `tools/re/Improve.java`: Ghidra pass that defines Delphi string literals,
  marks pointer-table entries constant and applies global names.
- `tools/re/ExportAll.java`: Ghidra pass that applies function names and
  exports all decompiled C and the string/global tables.
- `tools/re/export.sh`: runs both Ghidra passes on the saved project.
- `tools/re/globals_pp.py`: rewrites `PTR_DAT_` accesses as named globals.
- Outputs: `re/out/decomp/{all.c,tform1.c,tform1.pp.c,functions.tsv,strings.tsv,globals.tsv}`.

REA (`rea` CLI 6.7.0 + Ghidra) is used for spot checks with Evidence IDs.
The bulk export uses Ghidra headless directly, because each `rea` CLI call
re-runs a full analysis (~36 s). Overview Evidence:
`ev_43816063cabc732cc7536abfba9e36a11b90966b44a93e60942e691b20483359`
(2,849 procedures, 2,108 strings, 9 segments).
