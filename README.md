# open-street-chaves

[Português](#português) · [English](#english)

---

## English

**open-street-chaves** is a faithful, open-source reimplementation in **C17 +
SDL3** of *Street Chaves — O Lutador da Vila* (version 1.5A, 2004), the
Brazilian freeware fighting game by **CyberGamba** based on the TV show
*El Chavo del Ocho*. The original is a Windows / Delphi 5 + DelphiX
executable; this project rebuilds its engine so the game runs on Windows,
macOS, Linux and in the browser, frame-exact where possible.

The game logic was recovered by reverse engineering `Chaves.exe`; the
findings live in [`docs/spec/`](docs/spec/) (tick timing, state machine,
move tables, hit boxes, effects, screens). Those documents are the
contract the port follows.

### You need the original game

This repository contains **no game assets**: no sprites, sounds, music,
stage art or in-game text. Those belong to CyberGamba and must come from
your own copy of **Street Chaves 1.5A** (`StreetChaves_1.5A.rar` /
`Chaves.exe` with the folders `Animacoes`, `Cenarios`, `Efeitos`, `Falas`,
`Lutadores`, `Musicas`, `Rostos`, `Sons`). The port reads them at runtime:

```
open-street-chaves --data /path/to/StreetChaves
```

### Status

Playable fight prototype: any two of the 15 fighters (`--p1 N --p2 N`),
walking, jumping, crouching, the four normals standing/crouching/jumping,
blocking (high/low rules), hit, knockdown and lie-down states, hit boxes,
damage, hit-stop, combos, super meter and stocks, command parser for the
special moves (quarter circles, charges, mash) with their frames, hit boxes
and projectiles/effects, a CPU opponent with the original's probability
model, round timer and KO/time-over flow, sounds and music. All numeric
data is generated from `docs/spec` by `tools/gen_tables.py`.

Not there yet: menus and screens (title, select, VS, continue, records,
endings), stage animations, exact per-move projectile offsets, the
per-character AI special choices (approximated), text rendering with the
original font, joystick input, and the web build's data loading.

### Building

Requirements: CMake ≥ 3.24, a C17 compiler, Git (SDL3 is fetched and built
automatically; on Linux install the usual SDL build dependencies first —
see `.github/workflows/build.yml` for the list).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/open-street-chaves --data /path/to/StreetChaves
```

Web (Emscripten): `emcmake cmake -S . -B build-web && cmake --build build-web`
produces `open-street-chaves.html`. The browser build still needs the data
folder to be provided (loading from the user's disk is not wired yet).

Pre-built binaries for every platform are produced by GitHub Actions on each
push (see the *Actions* tab → artifacts).

### Controls (current)

P1: arrows + `Z` `X` `C` `V` (weak punch, weak kick, strong punch, strong
kick — the original layout). P2 (`--2p`): `W` `A` `S` `D` + `F` `G` `H` `J`.
`P` pauses, `B` shows hit/hurt boxes, `Esc` quits. Without `--2p` the
second fighter is the CPU.

### Credits and license

- *Street Chaves* © 2003–2004 **CyberGamba** (freeware, "proibido ser
  comercializado"); Spanish version by Carlos "C32" Varela. All characters
  belong to their respective owners. This project is a fan reimplementation
  and is not affiliated with CyberGamba or Televisa.
- The source code of open-street-chaves is licensed under the
  **GNU GPL v3.0** (see [LICENSE](LICENSE)). The license covers only this
  code and documentation, never the original game's assets or text.

---

## Português

**open-street-chaves** é uma reimplementação fiel e de código aberto, em
**C17 + SDL3**, de *Street Chaves — O Lutador da Vila* (versão 1.5A, 2004),
o jogo de luta freeware brasileiro do **CyberGamba** baseado no seriado
*Chaves*. O original é um executável Windows feito em Delphi 5 + DelphiX;
este projeto reconstrói o motor para que o jogo rode em Windows, macOS,
Linux e no navegador, com fidelidade de frame sempre que possível.

A lógica do jogo foi recuperada por engenharia reversa do `Chaves.exe`; o
resultado está em [`docs/spec/`](docs/spec/) (tempo de tick, máquina de
estados, tabelas de golpes, hit boxes, efeitos, telas). Esses documentos são
o contrato que o port segue.

### Você precisa do jogo original

Este repositório **não contém nenhum asset do jogo**: nem sprites, sons,
músicas, cenários ou textos. Eles pertencem ao CyberGamba e devem vir da
sua própria cópia do **Street Chaves 1.5A** (`StreetChaves_1.5A.rar` /
`Chaves.exe` com as pastas `Animacoes`, `Cenarios`, `Efeitos`, `Falas`,
`Lutadores`, `Musicas`, `Rostos`, `Sons`). O port lê tudo em tempo de
execução:

```
open-street-chaves --data /caminho/para/StreetChaves
```

### Estado atual

Esqueleto inicial (marco 1): abre, carrega `Cenarios/Cenario-01.bmp` e a
tira `Lutadores/C01.bmp`, fatia a tira exatamente como o original (colunas
separadoras escuras, branco transparente), roda um tick determinístico de
28 ms e desenha o Chaves parado / andando no cenário com a música original.
O restante está especificado em `docs/spec` e vai sendo portado.

### Como compilar

Requisitos: CMake ≥ 3.24, um compilador C17 e Git (o SDL3 é baixado e
compilado automaticamente; no Linux instale antes as dependências usuais do
SDL — a lista está em `.github/workflows/build.yml`).

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/open-street-chaves --data /caminho/para/StreetChaves
```

Web (Emscripten): `emcmake cmake -S . -B build-web && cmake --build build-web`
gera `open-street-chaves.html`. A versão para navegador ainda precisa receber
a pasta de dados (o carregamento a partir do disco do usuário ainda não está
ligado).

Binários prontos para todas as plataformas são gerados pelo GitHub Actions a
cada push (aba *Actions* → artifacts).

### Controles (por enquanto)

Setas andam, `P` pausa, `Esc` sai. O layout original (setas + Z X C V para
soco fraco / chute fraco / soco forte / chute forte, `5` ficha, `1`/`2`
start) volta conforme o motor de luta for portado.

### Créditos e licença

- *Street Chaves* © 2003–2004 **CyberGamba** (freeware, proibido ser
  comercializado); versão em espanhol de Carlos "C32" Varela. Os personagens
  pertencem aos seus respectivos donos. Este projeto é uma reimplementação
  feita por fãs, sem ligação com o CyberGamba ou com a Televisa.
- O código-fonte do open-street-chaves é licenciado sob a **GNU GPL v3.0**
  (veja [LICENSE](LICENSE)). A licença cobre só este código e documentação,
  nunca os assets ou textos do jogo original.
