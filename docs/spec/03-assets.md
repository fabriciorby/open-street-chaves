# 03 — Assets: files, formats and how they are loaded

Legend: **[static]** read from files/metadata, **[decomp]** read from Ghidra
pseudocode (analysis artifact, not source), **[infer]** interpretation not yet
confirmed, **UNKNOWN** not established.

## Files on disk (StreetChaves 1.5A) [static]

| Folder | Count | Format | Naming |
|---|---|---|---|
| `Lutadores/` | 15 | BMP 24 bpp, one horizontal strip per fighter | `C01.bmp` … `C15.bmp` (C01 = 4639×152, C02 = 3979×160) |
| `Rostos/` | 45 | BMP | `Rxx-a.bmp`, `Rxx-b.bmp`, `Rxx-c.bmp` (3 faces per fighter) |
| `Animacoes/` | 78 | BMP 24 bpp | `Anim-01.bmp` … (Anim-01 = 56×136) |
| `Efeitos/` | 101 | BMP | `Efeito-001.bmp` … |
| `Cenarios/` | 12 | BMP | `Cenario-01.bmp` … `Cenario-12.bmp` |
| `Falas/` | 30 | WAV | voice lines |
| `Sons/` | 67 | WAV | sound effects |
| `Musicas/` | 6 | WAV | `Musica-01.wav` … `Musica-06.wav`, looped |
| `Thumbs.db` | — | Windows Explorer cache, not used by the game | |

Fighter ids 1..15 follow the README order: 1 Chaves, 2 Seu Madruga,
3 Chiquinha, 4 Quico, 5 Dona Florinda, 6 Professor Girafales, 7 Senhor
Barriga, 8 Bruxa do 71, 9 Paty, 10 Nhonho, 11 Godines, 12 Senhor Furtado,
13 Jaiminho, 14 Dona Neves, 15 Glória. [static README + decomp: the
per-character tables in `carregarlutador`, `zerardados` and `criaadversario`
index 1..15 in that order; which file maps to which id is confirmed by name
only for `Cxx` → id xx, [infer].]

### Embedded in the executable [static]

Decoded from the `TFORM1` RCDATA resource (`re/out/forms/TFORM1.dfm.txt`),
extracted as BMP to `re/out/embedded/`:

- `DXImageListAbertura` (intro, 16 images): Predio-01-a/b, Predio-02-a/b,
  Folhagem, Pessoa-01..03, Chaves (141×123), Seu Madruga (161×124), Chaves
  dando soco (251×152), Seu Madruga caindo (165×135), Logo PORTUGUES and
  Logo ESPANHOL (223×117), Vencedores (192×200), Cybergamba (141×21).
- `DXImageListFinal` (endings, 11 images): Violao, Madruga-Violao,
  Quico-Urso, Chaves-Urso, Paty-Urso, Chaves-Bola, Madruga-Machucado,
  Girafales-Flores, Madruga-Velho do saco, Chaves-Gordo, Prato vazio.
- `DXImageListIcone` (14): arrows Cima/Baixo/Direita/Esquerda/Diagonal1/
  Diagonal2 (12×12), Mesa (144×98), ESPECIAL (100×100), Select (15×8),
  Rosto Secreto (50×60, 8 bpp), X (44×42, 8 bpp), two "Logo Transparente"
  (113×61), "Teste de forca" (9×9).
- `DXWaveListMusica` item 0: a 5,551-byte looped WAV placeholder; the real
  music is loaded over it at runtime.
- All embedded images are stored as DelphiX `TDIB` (class name + DIB).
  Transparent colour for Icone/Final/Abertura items is **white**
  (`TransparentColor = clWhite`), except Predio-01-a/b (opaque).

## Loading [decomp]

All loaders take a file name, check it with `FileSearch(name, "")` (current
directory) and silently skip missing files. Images are loaded through hidden
`TImage` controls and then handed to DelphiX image lists:

| Function | Target list | Behaviour |
|---|---|---|
| `carregarlutador(name, charId, player)` `0x48ea84` | `DXImageListLutador` + `DXImageListLutadorInvert` | Loads the strip into `ImageLutador`, writes `players[p].charId`, sets `players[p].spriteHeight` (+0x7a) from a per-character table, then **slices** the strip (see below). Disables `DXTimer1` while loading? No: it only re-enables it at the end (`DXTimer1.Enabled := True`). |
| `copiabmp(slot, x1, yBottom, x2, yTop)` `0x48e8d0` | both fighter lists | Sizes `ImageLutadorFrame` to `(x2-x1) × (yBottom-yTop)`, `CopyRect`s the region from `ImageLutador`, and assigns it to list item `slot` with `Name='Lutador'`, `SystemMemory=False`, `Transparent=True`, **`TransparentColor = RGB(255,255,255)` (white)**. Then repeats with the source rectangle reversed horizontally (`x2-1 … x1`), which produces the mirrored frame in `DXImageListLutadorInvert`. |
| `carregarcenario(name)` `0x4908a8` | `DXImageListCenario[0]` | Disables `DXTimer1`, loads the picture, `Transparent=False`, re-enables the timer. If the stage id (`g_4b44d8`) is 2 it also fills a 7-value record at `g_4b57d8` = (395, 195, 25, 25, 110, 90, count 6) — stage-specific data, meaning UNKNOWN [infer: an animated element rect]. |
| `carregarosto(name, side)` `0x4909f4` | `DXImageListRetrato` (appends) | Loads into `ImageRetrato`, makes a horizontally flipped copy in `ImageRetratoInvert` (CopyRect with reversed x), appends an item `Name='Rosto'`, transparent white. Which of the two is kept depends on `side` (1 or 2). |
| `carregaranimacao(name)` `0x491688` | `DXImageListAnimacao` (appends) | Item `Name='Animacao'`, transparent white. |
| `carregarefeitos(name)` `0x491948` | `DXImageListEfeitos` + `DXImageListEfeitosInvert` (append) | Normal and mirrored copies, `Name='Efeitos'`, transparent white. |
| `carregarfala(name)` `0x4915e8` | `DXWaveListVitoria` (appends) | Loads a WAV into the new item. (Despite the component name, this list receives the *voice* files.) |
| `carregareftsonoro(name)` `0x491780` | `DXWaveListSnd` (appends) | Sound effects. |
| `carregarmusica(n)` `0x491820` | `DXWaveListMusica[0]` | Builds the file name from `n` (two branches, `< 10` and `>= 10`; the prefix strings were not recovered yet, expected `"Musicas\Musica-0" + n + ".wav"` [infer]), loads it into item 0 and (re)initialises `DXSound1`. Stores `n` in `g_4b580a`. |

Where the loaders are called from (file-name construction, which files are
loaded for which fighter/stage, and when `Rxx-a/b/c` are used) is in
`DXTimer1Timer` / `FormCreate` and is still **UNKNOWN**: the main loop has not
been decompiled yet.

### Fighter strip slicing (`carregarlutador`) [decomp]

```
slotBase = 0 for player 1, 50 for player 2
n = 1; prevX = 0
for x in 0 .. width-1:
    top    = pixel(x, 0)
    bottom = pixel(x, height-1)
    if top.r,g,b < 101 and bottom.r,g,b < 101:      # dark separator column
        copiabmp(slot = slotBase + n, x1 = prevX, yBottom = height-1, x2 = x-1, yTop = 0)
        n += 1
        prevX = x + 1
```

So frames are delimited by dark columns that span the full strip height,
and each frame is the full strip height. Frame index `n` is what the
animation code stores in `players[p].frame` (+0x6c) and passes to
`TPictureCollection.Items[]`. Player 1 frames occupy items 1..49 and
player 2 items 51..99 of the same list (item 0 is unused / reserved).
Frames beyond 49 per fighter would overlap: UNKNOWN whether any strip has
that many.

### Drawing a fighter (`desenhalutador`) [decomp, partial]

- Draws a shadow first: `DXImageListEfeitos[0]` at `(x - 35 - camX,
  groundY - 12 - … - camY)`.
- Facing: `players[p].facing` (+0x6d) = 1 if `p.x < opponent.x` else 2,
  updated only while in states 1, 2 or 7 and not airborne (+0x78 == 0).
- Uses `DXImageListLutador` when facing == 1 and `DXImageListLutadorInvert`
  when facing == 2. The horizontal offset comes from a helper at
  `0x473728` (not yet decompiled).
- Position: `(x + offset + shake - camX, y - spriteHeight - camY)`, where
  `camX = g_4b44c8`, `camY = g_4b44cc`.
- `shake` is `Random(10)-5` in states 17/18, `Random(14)-7` in state 19
  while `g_4b57f9` (hit-stop/shake counter) is set, `Random(20)-10` when
  that counter is > 15.
- When `players[p].rot` (+0x18, i16) is non-zero the frame is drawn with
  `DrawRotate` around its centre (0.5, 0.5) — used for spinning/falling
  animations [infer].
- Debug overlay: when `g_4b58a5 == 1` the hurt box (+0x20..+0x2c, blue) and
  the hit box (+0x30..+0x3c, red, only while +0x68 == 1) are drawn, plus
  the stage bounds. This is a built-in hitbox viewer [infer].

## Sound [decomp, partial]

- Coin (`5` key) plays `DXWaveListSnd[0]` → first sound-effect file loaded.
- After saving config (F1), it plays `DXWaveListSnd[g_4b5809]` and
  restarts music item 0.
- Which WAV each index refers to depends on the load order in the main
  loop: UNKNOWN until `DXTimer1Timer` is read.
