# tools/re

Reverse-engineering scripts used to recover the game logic documented in
[`docs/spec/`](../../docs/spec/). Development only: neither the build nor the
port uses them. They need your own `Chaves.exe` (Street Chaves 1.5A), which
this repository does not ship.

Ferramentas de engenharia reversa usadas para gerar `docs/spec/`. Somente para
desenvolvimento; exigem o seu próprio `Chaves.exe`, que não está no repositório.

```
pip install -r tools/re/requirements.txt   # only endings_emu.py needs it
```

Local workspace (gitignored): `re/game/` (the exe), `re/out/`, `re/ghidra/`.
Name lists for the Ghidra passes are in [`data/`](data/).

## Python (run with `python3`)

| Script | What it does |
| --- | --- |
| `pe.py` | Minimal PE32 reader (sections, VA/offset, resources); helper for the others |
| `dfm.py` | Decode Delphi binary form resources into text |
| `dib2bmp.py` | Convert DelphiX picture blobs into `.bmp` files |
| `vmt.py` | Find Delphi VMTs and dump published methods and fields |
| `globals_pp.py` | Rewrite `PTR_DAT_` accesses in decompiled C as named globals |
| `textconst.py` | List the Delphi string constants in the CODE section |
| `textindex.py` | Map memo lines to string-constant indices (`07d-text-index.txt`) |
| `animtables.py` | Extract the animation table from the frame selector |
| `movesetup.py` | Extract per-move setup from the move-start routine |
| `movetables.py` | Extract input-sequence and attack tables |
| `seqtables.py` | Extract the input-sequence state machine |
| `effects.py` | Extract the effects table and spawn initialisation |
| `spawnoffsets.py` | Build effect spawn offsets (`06b-spawn-offsets.txt`) |
| `drawcalls.py` | Dump drawing calls of a pseudocode region with their guards |
| `stageanim.py` | Turn the stage-animation draw dump into a numeric table |
| `endings_emu.py` | Emulate the ending code to build `07e-endings.txt` (needs pefile, capstone) |

Each script's docstring has its usage line.

## Ghidra

Run inside Ghidra (script manager or `analyzeHeadless`), not with Python.

| Script | What it does |
| --- | --- |
| `Improve.java` | Define Delphi string literals, mark the global pointer table, apply `data/globals.txt` |
| `ExportAll.java` | Apply function names and export decompiled C for every function |
| `DecompileList.java` | Decompile a list of addresses, creating functions where missing |
| `export.sh` | Run `Improve.java` and `ExportAll.java` headless on the saved project (macOS/zsh paths) |
