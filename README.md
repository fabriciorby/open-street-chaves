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

Complete arcade flow: attract/title cinematic (the pictures are read from
inside your `Chaves.exe`, like all the game text), demo fight, coins and
start buttons, character select, VS ladder, fights, continue, top-10
records, the 15 endings with their dialogues and the credits. The fight engine has walking, jumping, crouching, the four normals
standing/crouching/jumping, blocking (high/low rules), hit, knockdown and
lie-down states, hit boxes, damage, hit-stop, combos, super meter and
stocks, the command parser for the special moves (quarter circles, charges,
mash) with their frames, hit boxes and projectiles/effects, a CPU opponent
with the original's per-character decision table, round timer and KO/time-
over flow, stage animations, sounds and music. All numeric data is
generated from `docs/spec` by `tools/gen_tables.py`.

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
push (see the *Actions* tab → artifacts). The web build asks for your game
folder with a folder picker; the files stay in the page's memory.

### Controls (current)

Arcade keys as in the original: `5` inserts a coin, `1` / `2` start that
player (on the title screen the first button also inserts a coin and
starts). P1: arrows + `Z` `X` `C` `V` (weak punch, weak kick, strong punch,
strong kick). P2: `W` `A` `S` `D` + `F` `G` `H` `J`, or a gamepad (d-pad or
left stick, `X` `A` `Y` `B` for the four attacks; the first pad goes to P2
like the original's joystick, a second pad to P1). `F1` opens the configuration menu and `F2` the key
configuration on the title screen (saved in SDL's preferences folder, never
in your game folder). `P` pauses, `B` shows hit/hurt boxes, `Esc` quits. `--fight --p1 N --p2 N` jumps straight into a
fight (`--2p` makes P2 human).

### Credits and license

- *Street Chaves* © 2003–2004 **CyberGamba** (freeware, "proibido ser
  comercializado"); Spanish version by Carlos "C32" Varela. All characters
  belong to their respective owners. This project is a fan reimplementation
  and is not affiliated with CyberGamba or Televisa.
- The source code of open-street-chaves is licensed under the
  **GNU GPL v3.0** (see [LICENSE](LICENSE)). The license covers only this
  code and documentation, never the original game's assets or text.
- Text is drawn with the system's *Comic Sans MS* when present (the font
  the original uses; it is never shipped here). Otherwise the bundled
  [Comic Neue](https://github.com/crozynski/comicneue) by Craig Rozynski is
  used, under the SIL Open Font License (`assets/fonts/OFL.txt`).
  Rasterised with Sean Barrett's public-domain `stb_truetype`.

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

Fluxo de arcade completo: abertura/título (as imagens vêm de dentro do seu
`Chaves.exe`, como todos os textos), luta de demonstração, fichas e botões
de start, seleção de personagem, tela VS com a escada de oponentes, lutas,
continue, recordes, os 15 finais com seus diálogos e os créditos. O motor de luta tem andar, pular, agachar, os quatro
golpes normais em pé/agachado/no ar, defesa (regras alta/baixa), estados
de acerto, queda e levantar, hit boxes, dano, hit-stop, combos, barra de
especial e estoques, o leitor de comandos dos especiais (meia-lua, carga,
repetição) com seus quadros, hit boxes e projéteis/efeitos, CPU com a tabela
de decisões por personagem do original, relógio do round e fluxo de KO e
tempo esgotado, animações dos cenários, sons e música. Todos os dados
numéricos são gerados de `docs/spec` por `tools/gen_tables.py`.

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

Teclas de arcade como no original: `5` insere ficha, `1` / `2` dão start
naquele jogador (na tela de título o primeiro botão também insere a ficha e
dá start). P1: setas + `Z` `X` `C` `V` (soco fraco, chute fraco, soco forte,
chute forte). P2: `W` `A` `S` `D` + `F` `G` `H` `J`, ou um controle (direcional
ou analógico esquerdo, `X` `A` `Y` `B` para os quatro golpes; o primeiro
controle vai para o P2 como o joystick do original, um segundo para o P1).
`F1` abre o menu de configuração e `F2` a configuração de teclas na tela de
título (salvas na pasta de preferências do SDL, nunca na pasta do jogo).
`P` pausa, `B` mostra as hit boxes, `Esc` sai. `--fight --p1 N --p2 N` vai
direto para uma luta (`--2p` deixa o P2 humano).

### Créditos e licença

- *Street Chaves* © 2003–2004 **CyberGamba** (freeware, proibido ser
  comercializado); versão em espanhol de Carlos "C32" Varela. Os personagens
  pertencem aos seus respectivos donos. Este projeto é uma reimplementação
  feita por fãs, sem ligação com o CyberGamba ou com a Televisa.
- O código-fonte do open-street-chaves é licenciado sob a **GNU GPL v3.0**
  (veja [LICENSE](LICENSE)). A licença cobre só este código e documentação,
  nunca os assets ou textos do jogo original.
