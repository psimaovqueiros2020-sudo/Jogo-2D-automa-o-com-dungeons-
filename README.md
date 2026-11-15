# A Clareira da Luna

Jogo de **automação + aventura + exploração**, escrito em **C (C99)** com a biblioteca **raylib**.

> Mindustry/Factorio (logística e fábricas) + Pixel Dungeon (cavernas e exploração) + exploração espacial + a história da Luna e do Gabriel.

Este repositório contém, para já, **um único setor**: *A Clareira da Luna*, inspirado na Floresta dos Encantados da história original. Os setores são a unidade do mundo (como no Mindustry) e cada um pode recriar um local da história.

Este documento serve para que qualquer pessoa (ou IA) que pegue no projeto perceba **o que existe, porquê, e o que falta fazer**. Se mudares o código, atualiza-o (secção 1, roadmap e decisões técnicas).

---

## 0. Ficheiros do projeto

| Ficheiro | Para que serve |
|---|---|
| `main.c` | Teclado, rato, câmara e o ciclo do jogo (≈310 linhas). Ver secção 4. |
| `world.c` / `world.h` | **O mundo:** terreno, minérios (tabelas), ruído e geração do setor. |
| `logic.c` / `logic.h` | **A lógica:** objetos construídos, receitas, máquinas, esteiras, brocas, simulação, extração, mochila, colisões da Luna. **Sem raylib.** |
| `render.c` / `render.h` | **Tudo o que desenha:** terreno, objetos, Luna, HUD. |
| `Makefile` | `make` (jogo), `make test` (testes), `make clean`. |
| `README.md` | Este documento. |
| `tests/test_logica.c` | Testes automáticos da **lógica** (sem janela; compilam só `world.c` + `logic.c`). Ver "Como correr os testes". |
| `tests/fake/raylib.h` | Cabeçalho **falso** da raylib (funções que não fazem nada), só para compilar e testar onde a raylib não está instalada. **Nunca** o deixar ao lado do `main.c` nem pôr `-Itests/fake` no comando do jogo (o programa compilaria mas sairia logo, sem janela). |
| `0O_Livros_versão_1/2/3.docx` | A história original (autor do jogo). A versão 3 é a mais completa. Ver secção 6. |

---

## 1. Visão do jogo (ideia mãe)

Tudo o que se segue é o **design original**. Só uma parte está implementada (secção 2), mas qualquer mecânica nova deve respeitar isto.

**Mundo**
- Mundo dividido em **setores** (estilo Mindustry). Alguns setores recriam locais da história da Luna (com **folhas do diário da Luna** como coleccionáveis que contam a história e desbloqueiam receitas). Há também **exploração espacial** (detalhes ainda por desenhar).
- **Camadas verticais:** superfície + cavernas/dungeons por baixo. **Cada quadrado da superfície corresponde ao mesmo (x, y) nas camadas inferiores.** Camadas ligam-se por **esteiras verticais** (sobem e descem) e **elevadores** que extraem recursos de umas camadas para as outras e para a superfície.
- **Cavernas** espalhadas pelo mapa, estilo **Pixel Dungeon**, com **minérios mais raros**.
- **Casas com interior maior que o exterior:** a casa ocupa pouco no mapa, mas ao entrar pela porta o espaço é maior.
- Muralhas, torres de defesa, **vários reinos**.

**Recursos e logística**
- **Jazidas estilo Factorio:** manchas no chão com quantidade **finita** de minério.
- Máquinas e logística como no Mindustry/Factorio, **mas sem inserters**: a esteira entra dentro da máquina e a máquina tem uma saída.
- **Baús e contentores por massa e volume** (cada item tem massa e volume; o contentor tem limites para os dois).
- **Comboios.**

**Aventura**
- **NPCs** (pixel art estilo Terraria) que se tornam teus amigos e exploram as dungeons contigo.
- Combate **estilo Factorio** (defesas, torres, ondas), não um hack and slash.

---

## 2. Estado atual

**Versão: Marco 3b implementado** (o forno), com o código **dividido em módulos** (sem mudar o comportamento). Compila com `-std=c99 -Wall -Wextra` **sem avisos** e a lógica passou nos testes automáticos (`tests/test_logica.c`). Os Marcos 3a e 3b foram **testados a jogar pelo autor e funcionam**. Os números das receitas continuam provisórios (ver "Como testar o Marco 3b" para o ritmo).

| Funciona | Ainda não existe |
|---|---|
| Geração do setor (160×120 tiles, sempre igual com a mesma seed) | Massa/volume nos itens e baús (Marco 4) |
| Terreno com quadrados coloridos (placeholder) | Custo de construção, inventário real (Marco 5) |
| Luna (placeholder) com WASD/setas, correr com Shift, colisões | Combate, NPCs, dungeons, camadas verticais |
| Câmara que segue a Luna, zoom, câmara livre (TAB) | Pixel art real, sons, menus, guardar jogo |
| Trilhos e vaus (água rasa) que ligam a clareira aos recursos | História/diário da Luna dentro do jogo |
| Jazidas finitas: hematite, argila, enxofre, quartzo e **areia** | Nitrato, fibras, figos, granito como recursos |
| Árvores mináveis (madeira, 20 a 30 cada) | Vidro, papel, pólvora, glicerina (receitas) |
| Extração a mão com o rato (ferramenta 1) | Forno em 3 níveis (a história tem 3 versões) |
| **Broca mecânica 2×2** (também **corta árvores**), **esteiras** (itens misturados) e **baú** | Meter itens da mochila em máquinas/baús à mão |
| **Carvoeira 2×2**: madeira → carvão vegetal | |
| **Forno 2×2**: tijolos e ferro | |
| Simulação em passo fixo (60 passos/s, independente do FPS) | |
| Mochila provisória (só contagens) no canto do ecrã | |

### Receitas (Marco 3b)

Estão na tabela `RECIPES[]` do `logic.c` (fácil de ajustar ou ampliar). **Números provisórios**: ajustar depois de jogar.

| Máquina | Receita | Entra | Sai | Tempo |
|---|---|---|---|---|
| Carvoeira | Carvão vegetal (pirólise) | 4 madeira | 1 carvão | 3 s |
| Forno | Tijolos | 2 argila + 1 areia + 1 madeira (combustível) | 3 tijolos | 2 s |
| Forno | Ferro | 3 hematite + 1 carvão | 2 ferro | 4 s |

Como funciona (igual para todas as máquinas):
- A esteira **entra** na máquina: `accept_item()` só aceita itens que são ingredientes de alguma receita dessa máquina, até **12 de cada** (`MACH_IN_CAP`). Quando o ingrediente está cheio, a esteira fica bloqueada à espera.
- A máquina escolhe sozinha a **primeira receita** para a qual já tem tudo, gasta os ingredientes no início e, passado o tempo, guarda o produto.
- Os produtos saem para qualquer **esteira, baú ou outra máquina encostada** a ela (reparte por todas as saídas). Pode guardar até 16 produtos (`MACH_OUT_CAP`); cheia → pára (luz amarela).
- **Atenção, é de propósito:** uma esteira com itens misturados pode **bloquear** uma máquina (se o item da frente é um que a máquina já tem cheio, os de trás não passam, mesmo que ela precise deles). Resolve-se com uma esteira por ingrediente, como no Factorio. Uma carvoeira **encostada** ao forno entrega-lhe o carvão diretamente.
- Luz da máquina: verde trabalha, vermelho parada (faltam ingredientes), amarelo saída cheia. Passar o rato por cima mostra a receita, o progresso, o que tem dentro e o que está pronto.

### Como testar o Marco 3b

1. **Carvão.** Tecla `2` e põe uma broca em cima de árvores (agora pode; serve de serra, e quando a árvore acaba o tile vira chão da floresta). Tecla `3`: faz uma esteira da broca até uma carvoeira (`5`); a esteira tem de apontar **para** a máquina. Do outro lado, uma esteira a sair (a apontar para longe) até a um baú (`4`). Vê a luz verde, a barra de progresso e o carvão a chegar ao baú.
2. **Tijolos.** Broca sobre argila, outra sobre areia (margem norte do ribeiro: as manchas alternam), mais madeira, tudo a entrar num forno (`6`). O forno despeja tijolos num baú. Se a esteira bloquear, usa uma esteira por ingrediente.
3. **Ferro.** Broca sobre hematite (encosta a sudeste) → forno. Põe a carvoeira **encostada** ao forno (ela entrega carvão diretamente) ou liga-a por esteira. O ferro sai para um baú.
4. Demolir (botão direito) uma máquina devolve à mochila tudo o que tinha dentro (incluindo a receita em curso).
5. **O que me interessa saber depois de jogares:** o ritmo está bom ou lento? (carvoeira: 1 carvão por 3 s e por 4 madeira; forno: ver tabela). Ajustar os números em `RECIPES[]`, `DRILL_PERIOD` e `ORE[].richest`.

### Como correr os testes

Não precisam da raylib nem de janela. A partir da pasta do projeto:

```
gcc -std=c99 -Wall -Wextra -Wno-unused-function -Itests/fake tests/test_logica.c world.c logic.c -o test_logica -lm
./test_logica
```

(ou simplesmente `make test`)

Devem aparecer as linhas de resumo e `TODOS OS TESTES PASSARAM`. Cobrem: areia em 20 seeds (e só em lama), carvoeira alimentada por broca e esteira (conservação exata de unidades), forno de ferro e de tijolos, limites de entrada (rejeita produtos e minérios errados), saída cheia (a máquina pára e não perde itens), demolição (devolve tudo), carvoeira encostada ao forno, e a broca a cortar árvores (a parede do bordo continua proibida). **Corre sempre os testes e compila com a raylib verdadeira no teu PC antes de dares um marco por fechado** (o cabeçalho falso não verifica o desenho nem o rato).

---

## 3. Como compilar e correr

Precisas de um compilador de C (gcc) e da raylib.

**Windows**
1. Instala a raylib com o instalador oficial (https://www.raylib.com), que já traz o compilador MinGW.
2. Na pasta do projeto: `make`
   (ou `gcc -std=c99 -Wall -Wextra main.c world.c logic.c render.c -o clareira.exe -lraylib -lopengl32 -lgdi32 -lwinmm`)
3. Corre `clareira.exe`.

**Linux**
```
sudo apt install libraylib-dev     # ou compila a raylib a partir do GitHub
make && ./clareira
```

**macOS**
```
brew install raylib
make && ./clareira
```

Os quatro ficheiros `.c` compilam sempre **juntos** com um único comando (é o que o `make` faz). O código compila com `-std=c99 -Wall -Wextra` **sem avisos**. Mantém assim.

### Controlos

| Tecla | Ação |
|---|---|
| `W A S D` / setas | Mover a Luna (em câmara livre: mover a câmara) |
| `Shift` | Correr |
| `1` | Ferramenta **mão**: botão esquerdo (segurar) minera a jazida ou a árvore sob o rato, se estiver ao alcance (2.5 tiles) |
| `2` `3` `4` `5` `6` | Ferramenta **broca** / **esteira** / **baú** / **carvoeira** / **forno**: botão esquerdo constrói (alcance 10 tiles); segurar e arrastar faz uma linha de esteiras |
| `Q` / `E` | Rodar a esteira (anti-horário / horário) |
| Botão direito | Demolir o objeto sob o rato (devolve os itens à mochila). Em câmara livre serve para arrastar o mapa |
| `F` | Pegar tudo do baú sob o rato (ao alcance das mãos) |
| Roda do rato | Zoom (0.5x a 8x) |
| `TAB` | Alternar câmara livre / seguir a Luna |
| Botão direito ou do meio | Arrastar o mapa (só em câmara livre) |
| `G` | Mostrar/esconder grelha (só com zoom ≥ 2x) |
| `R` | Gerar outro mapa (muda a seed; esvazia a mochila e os objetos) |
| `F11` | Ecrã inteiro |

---

## 4. Estrutura do código

O código está dividido em **4 módulos** (cada um com o seu `.c`/`.h`), por camadas. Cada módulo só conhece os que estão **abaixo** dele:

```
main.c  ──►  render.c  ──►  logic.c  ──►  world.c
(input)      (desenho)      (sem raylib)   (mundo)
```

Única exceção, de propósito: `generate_world()` (em `world.c`) chama `reset_buildings()` (de `logic.c`) para limpar os objetos ao gerar outro mapa.

**`world.c` / `world.h` – o mundo**

| Secção | O que contém |
|---|---|
| Configuração | `MAP_W`, `MAP_H`, `MAP_D`, `TILE_SIZE` e as posições dos locais importantes |
| Tipos de terreno | `enum Terrain` e a tabela `TERRAIN[]` (nome, cor, se dá para andar) |
| Minérios e itens | `enum OreType` e a tabela `ORE[]` (nome, cor, quantidade máxima). **O `OreType` serve também de tipo de item**: depois de `ORE_AREIA` vêm os produtos (`ORE_CARVAO`, `ORE_TIJOLO`, `ORE_FERRO`), que não existem no chão (`richest = 0`) |
| Mundo | `Tile` (terreno + jazida), `world[z][y][x]`, `tile_at()`, `in_bounds()`, fila `mark_dirty()` de tiles a redesenhar, `ore_dots()` |
| Ruído | `hash2()`, `rnd01()`, `dist2d()`, `clampf()` (públicos); `value_noise()` e `fbm()` (internos) |
| Geração | `generate_tile()` decide o terreno; `carve_trails()` abre trilhos e vaus; `generate_ores()` e `place_patch()` colocam as jazidas; `remove_unreachable_ore()` apaga minério inacessível; `generate_world()` junta tudo |

**`logic.c` / `logic.h` – a lógica (sem raylib)**

| Secção | O que contém |
|---|---|
| Objetos construídos | `Building` (broca, esteira, baú, carvoeira, forno), lista `buildings[]`, grelha `bgrid[][]`, `building_at()`, tabela `BUILD[]` |
| Jogador (só colisões) | `Player`, `tile_blocked()`, `box_blocked()`, `player_move()`, `player_spawn()` |
| Extração e mochila | `extract_from_tile()` (usada por mãos e brocas), `mine_tile()` (mãos), `inventory[]`, `tile_in_reach()`, `tile_within()` |
| Receitas e máquinas | `RECIPES[]`, `machine_wants()`, `recipe_ready()`, `machine_step()`, `machine_status()` |
| Construção e logística | `can_place()`, `place_building()`, `remove_building_at()`, `accept_item()` (entregar itens), `dump_item()` (despejar para os vizinhos), `belt_step()`, `drill_step()`, `sim_step()` (passo fixo) |

**`render.c` / `render.h` – o desenho (raylib)**

| Secção | O que contém |
|---|---|
| Terreno | `render_init()`/`render_shutdown()` (textura do terreno), `draw_tile()`, `build_terrain_texture()`, `flush_dirty()`, `draw_world()` |
| Luna e objetos | `draw_player()`, `draw_buildings()` (`draw_belt`, `draw_drill`, `draw_chest`, `draw_machine`), `draw_build_preview()`, `draw_hover()`, `mouse_tile()` |
| HUD | `draw_hud()` (informação no canto do ecrã) |

**`main.c` – input e ciclo do jogo**

| Secção | O que contém |
|---|---|
| Estado da interface | `tool`, `build_dir`, `is_mining`… (só existem aqui; quem desenha recebe-os por parâmetro) |
| Input | `update_player()` (teclado), `update_mining()` e `update_building()` (rato) |
| Câmara | `update_camera()` (seguir, câmara livre, zoom) |
| Principal | `main()` e o ciclo do jogo |

### Decisões técnicas importantes (não mudar sem pensar)

- **Mundo em `world[z][y][x]`.** Hoje `MAP_D = 1` (só superfície). As cavernas serão `z = 1, 2, …`. Cada quadrado da superfície corresponde ao mesmo `(x, y)` nas camadas inferiores.
- **Geração determinística.** O mapa depende só da seed (`DEFAULT_SEED = 1234`). Não usar `rand()` na geração; usar `hash2()`/`rnd01()` para o resultado ser sempre igual.
- **Terreno em textura.** O terreno é desenhado uma vez para `terrain_tex` e só é redesenhado quando o mapa muda. A lógica chama `mark_dirty(x, y)`; o `main()` chama `flush_dirty()` (em `render.c`) uma vez por frame. (Não se chama `redraw_tile()` diretamente: foi removida.)
- **Colisão por eixos separados.** `player_move()` move em X e em Y em separado para a Luna deslizar pelas paredes. Caixa de colisão 8×8 px (`PLAYER_HALF = 4`).
- **Ordem da geração:** terreno base → trilhos e vaus → jazidas → remoção de minério inalcançável. Cada jazida só se coloca no terreno certo (hematite e quartzo em pedra, enxofre em pântano, argila e areia em lama).
- **Tudo o que é minério tem de ser alcançável a pé.** O ribeiro corta o mapa e a floresta é densa, por isso os trilhos abrem caminho da clareira até cada zona de recursos e, onde cruzam água, criam um **vau** (`T_VAU`, água rasa onde se anda). No fim, `remove_unreachable_ore()` apaga qualquer minério que mesmo assim fique isolado.
- **Jazidas finitas.** Cada tile guarda `ore` e `ore_amount`. Mais perto do centro da mancha, mais minério. Ao chegar a 0, `ore` volta a `ORE_NONE`. O aspeto (1 a 4 pedrinhas) depende da quantidade que resta.
- **Árvores.** Cada `T_ARVORE` do interior do mapa tem `ore = ORE_MADEIRA` (20 a 30). Ao esgotar, `extract_from_tile()` muda o terreno para `T_FLORESTA`. A parede de árvores do bordo (3 tiles) **não** dá madeira, para o setor continuar fechado. Cortam-se a mão do tile ao lado ou com uma **broca assente em cima** (`can_place()` abre essa exceção só para a broca e só em árvores com madeira): é assim que a madeira entra nas esteiras, porque ainda não se pode meter itens da mochila em máquinas.
- **Lógica separada do rato e do desenho.** `extract_from_tile()`, `accept_item()`, `sim_step()`, `place_building()`… **não usam raylib nem input**: por isso se testam sem janela. Os testes compilam só `world.c` + `logic.c`. (A única coisa da raylib que `world.h` usa é o **tipo** `Color`, nas tabelas de cores.)
- **Módulos.** Dependências só "para baixo" (`main → render → logic → world`). As tabelas (`TERRAIN[]`, `ORE[]`, `BUILD[]`, `RECIPES[]`) são `extern const` no `.h` e definidas no `.c` (não `static const` num cabeçalho, que daria avisos). O estado da interface (`tool`, `build_dir`, `is_mining`) fica no `main.c` e passa-se por parâmetro a quem desenha.
- **Unidades:** posições do jogador e da câmara em **pixels do mundo**; posições do mapa em **tiles**. `tile = floor(pixel / TILE_SIZE)`. Tile: 16×16 px.
- **Objetos construídos separados do `Tile`.** Vivem em `buildings[]` (máx. `MAX_BUILDINGS` = 2048) e `bgrid[y][x]` guarda o índice + 1 (0 = livre). Brocas, carvoeiras e fornos ocupam 2×2 (a origem `x, y` é o canto superior esquerdo). Brocas, baús e máquinas bloqueiam a Luna; esteiras não.
- **Simulação em passo fixo.** `sim_step(SIM_DT)` corre 60 vezes por segundo (acumulador no `main()`), por isso as máquinas andam à mesma velocidade com qualquer FPS.
- **Sem inserters.** Quem entrega chama `accept_item(tx, ty, dx, dy, item)` ao objeto à sua frente; devolve 0 se não aceita (cheio, esteira de frente, ingrediente que a máquina não usa…). A esteira entra no baú ou na máquina diretamente. Uma esteira também aceita itens do lado e de trás, nunca de quem está virado para ela (por isso uma esteira de entrada nunca recebe de volta o que a máquina despeja).
- **Itens misturados na esteira.** Cada tile leva até 4 itens (`BELT_CAP`), cada um com tipo e posição (0 a 1); espaçamento `ITEM_GAP`.
- **Despejar (`dump_item()`):** broca e máquinas entregam 1 item por passo a um dos tiles à volta (esquerda, direita, cima, baixo), a começar em `cursor_out` e a rodar; alternam entre tipos (`cursor_type`), por isso a esteira sai misturada.
- **Broca.** A cada `DRILL_PERIOD` (0,25 s) tira 1 unidade de **cada** tile com minério debaixo dela (até 4 → 16/s) para um buffer de 16. Buffer cheio → pára (luz amarela). Nada por baixo → luz vermelha.
- **Construir é grátis por agora** (sem custo em recursos). O Marco 5 liga a construção ao inventário (agora que já há tijolo e ferro para gastar).
- **Baú com limite provisório** de `CHEST_CAP` = 500 unidades no total; o Marco 4 troca-o por massa e volume.
- **Valores provisórios:** `ORE[].richest`, `DRILL_PERIOD`, `RECIPES[]`, `MACH_IN_CAP`, `MACH_OUT_CAP`. Ajustar depois de jogar.
- **Estilo de código:** C simples (arrays de tamanho fixo, `struct`s, poucos ponteiros, sem alocação dinâmica enquanto possível). Comentários em português, **sem acentos** nos comentários e nas strings do código (como o código já faz; manter).

---

## 5. O mapa e os recursos

O setor reproduz a clareira de Gabriel e Luna. Cada terreno tem origem na história:

| Terreno | Onde | Papel no jogo |
|---|---|---|
| Clareira | Centro (80, 55), raio ~14 | Base inicial, sem árvores |
| Casa na árvore | Centro da clareira | Ponto de partida; obstáculo |
| Ribeiro + lama | A sul, corre para leste | Água; margem com argila e areia |
| Pântano | Leste (143, 74) | Enxofre, resina, água ácida |
| Encosta rochosa | Sudeste (118, 102) | Hematite (minério de ferro) |
| Afloramento | Noroeste (28, 24) | Quartzo, granito |
| Floresta densa | Em redor, mais densa longe da clareira | Limite do setor; árvores bloqueiam; madeira |
| Trilho | Da clareira para hematite, quartzo, pântano e ribeiro | Terra batida; garante acesso aos recursos |
| Vau | Onde um trilho cruza o ribeiro | Água rasa; única forma de atravessar o rio |

### Jazidas (seed 1234, medido)

| Recurso | Terreno | Onde | Máx. por tile | Tiles | Unidades |
|---|---|---|---|---|---|
| Hematite | Pedra | Encosta sudeste (2 manchas) | 120 | 198 | ~11 700 |
| Argila | Lama | Margem norte do ribeiro (4 manchas) | 150 | 83 | ~7 200 |
| Areia | Lama | Margem norte do ribeiro, **entre** as de argila (4 manchas) | 100 | 62 | ~3 300 |
| Enxofre | Pântano | Pântano a leste (2 manchas) | 80 | 83 | ~3 300 |
| Quartzo | Pedra | Afloramento noroeste (2 manchas) | 60 | 84 | ~2 500 |
| Madeira | Árvore | Qualquer árvore do interior do mapa | 30 (por árvore) | 7340 | ~183 000 |

Ainda **não existem** nitrato, fibras, figos nem granito como recursos.

### Árvore tecnológica (inspirada no capítulo da clareira)

| # | Cadeia | Estado |
|---|---|---|
| 1 | Argila + areia → tijolos → forno (3 níveis na história) | **Feito** (um só forno; os 3 níveis faltam) |
| 2 | Madeira → carvão vegetal (pirólise) | **Feito** (carvoeira) |
| 3 | Hematite + carvão → ferro (fundição com cadinho; ferramentas) | **Feito** (receita simples; sem cadinho/escória/ferramentas) |
| 4 | Enxofre + nitrato + carvão → pólvora (partir granito e abrir cavernas) | Falta nitrato e granito |
| 5 | Fibras → papel; quartzo → vidro | Falta |
| 6 | Figos → fermentação → álcool → glicerina | Falta |

Os coleccionáveis "folhas do diário da Luna" desbloqueiam receitas e contam a história. Os **Apêndices Científicos** da história (ver abaixo) são matéria-prima natural para essas folhas.

---

## 6. A história

Escrita pelo autor do jogo. Ficheiros: `0O_Livros_versão_1/2/3.docx` (a v3 tem todos os capítulos; a v2 tem rascunhos dos últimos, como "As Lendas de Ur", que a v3 reescreveu; a v1 só vai até "Um Lugar de Morte"). **Lidos na íntegra** (a v3 completa, e o capítulo "As Lendas de Ur" da v2, que a v3 reescreveu).

### Resumo (v3)

1. **Luna descobre a Floresta.** Luna, jovem de cidade pequena (cabelo escuro, vestido azul-celeste com adornos cor de topázio, caderno de anotações), foge de casa depois de uma discussão com os pais e entra na **Floresta dos Encantados** (sempre de noite, luminescente). Atravessa um ribeiro para uma floresta congelada e morre de frio.
2. **Um sonho misterioso.** Gabriel, rapaz de outro mundo, adormece e acorda na floresta. Encontra Luna, reanima-a, e tornam-se amigos. (Luna tem orelhas de gato; Gabriel nota-o em diálogo.)
3. **O Culto.** Um cultista desfaz Gabriel em pó (ele acorda no mundo dele). Os cultistas (que mantêm o **Senhor das Estrelas** a dormir) obrigam Luna a beber uma poção: tem de cantar 3 vezes por dia durante um ano (**1095 cultos**) ou morre. Luna rouba um livro numa língua antiga. Gabriel regressa sempre ao mesmo sítio quando adormece. O tempo corre mais devagar no mundo de Luna.
4. **O Vale das Fadas.** As fadas (cidade avançada, escravizadas pelos cultistas no passado) atacam Luna por causa do manto de cultista. Oferecem-lhe uma fada escrava, que Luna liberta e baptiza **Estrelinha**. Luna e Gabriel constroem a **casa na árvore** na clareira (com o pirilampo gigante domesticado e a Estrelinha).
5. **Um Lugar de Morte.** O pântano é um mar de ácido; só os **sapos** o atravessam e fazem comércio com os cultistas. Luna infiltra-se no porto dos cultistas, rouba uma carroça, um burro e as **ervas que filtram o ar**. Luna e Estrelinha fazem fornos, carvão e **ferro**, ferramentas e um barco de madeira do pântano. O sapo **Gilberto** (mestre de poções) pede em troca da cura uma **flor de fogo**.
6. **O Inferno na Terra.** O pântano de fogo (vulcão adormecido). Colhem a flor, mas a **Hidra** morde Luna (veneno com necrose). Gilberto dá-lhes as duas poções que quebram a maldição de escravatura, mas diz que só um médico da Floresta Densa trata veneno de Hidra.
7. **O Rapaz que Construía para Não Pensar** (capítulo de Gabriel; **é o que o setor reproduz**). Com Luna ausente, Gabriel constrói para não pensar: tijolos, forno (3 versões), carvão, ferro, ferramentas, pólvora, papel, vidro, álcool e glicerina; amplia a clareira e constrói uma casa com um quarto para ela. Estrelinha chega a pedir-lhe ajuda.
8. **A Floresta que Engolia a Luz.** A Floresta Densa (escuridão total, cogumelos, risos dentro da cabeça). Falam de uma entidade (**Ur** na v2) que governava tudo o que não é humano entrando nas mentes e já não é o que era. Luna tem uma visão de grades de gelo. Vêem um coração num frasco à porta de uma cabana.
9. **O Médico.** Cabana maior por dentro, médico da mesma espécie de Luna, sorriso que não chega aos olhos, pagamento em espécie (Luna paga com uma escama da Hidra). A filha adoptiva **Maria**, deixada pelos cultistas, tem o antebraço sem músculo (só osso e pele). Os capítulos 7, 8 e 9 terminam cada um com um **Apêndice Científico**.

### Ligações história → jogo

- **Casa com interior maior que o exterior** (a cabana do médico) é a base da mecânica de casas do design.
- **A Floresta que Engolia a Luz / noite eterna.** A história passa-se numa floresta sempre de noite, com luz azulada e cogumelos luminescentes; o placeholder atual é um prado de dia. A arte final deve decidir o ambiente (ver secção 7). O pirilampo e a Estrelinha são candidatos a fonte de luz / companheiros.
- **Os dois protagonistas encaixam nas duas metades do jogo:** Gabriel é o construtor (Mindustry), Luna a exploradora (Pixel Dungeon). Hoje o jogador é a Luna a construir; é uma decisão de design **ainda em aberto** (quem se joga em cada setor).

### Notas de continuidade (fios soltos da história a decidir)

- **A tecnologia é descoberta duas vezes:** no capítulo 5 Luna e Estrelinha já fazem fornos, carvão e ferro; no 7, Gabriel descobre tudo de raiz e o primeiro machado "é dele". Decidir quem inventa o quê.
- **Regra da presença do Gabriel:** só está na floresta enquanto sonha (aparece e desaparece). No capítulo 7 vive semanas seguidas na clareira.
- **As duas poções do Gilberto** e a dívida dos 1095 cultos ainda não foram resolvidas.
- **Ciência dos apêndices:** a equação da redução do ferro está mal equilibrada (certa: `2 Fe₂O₃ + 3 C → 4 Fe + 3 CO₂`); a glicerina não pode vir da resina (resina não é gordura: usar gordura de animal). Corrigir antes de usar nas folhas do diário.

---

## 7. Pixel art

Atualmente tudo são **quadrados coloridos (placeholders)**. Especificações para a arte final, para manter consistência:

- **Tiles de 16×16 px**, vista de cima (top-down), estilo Terraria/Stardew.
- **Paleta limitada** (16 a 24 cores), partilhada por todos os sprites.
- Sem anti-aliasing; fundo transparente nos objetos; terrenos que encaixam sem costura.
- 2 a 4 variantes por terreno, para o mapa não parecer repetido.
- Entregar em **spritesheets** em grelha de 16×16. Confirmar sempre as dimensões exatas das imagens.

Precisam de arte: relva clara, relva normal, chão da floresta, árvore, água, lama, pântano, pedra, casa na árvore, a Luna (idealmente 4 direções e animação de andar), e os objetos: **broca, esteira (com direção), baú, carvoeira, forno** (com estado: a trabalhar/parado) e os itens (hematite, argila, areia, enxofre, quartzo, madeira, carvão, tijolo, ferro).

---

## 8. Roadmap

Regra de ouro: **cada marco tem de ser jogável e testável antes de começar o seguinte.**

- [x] **Marco 1 – Mapa:** terreno, câmara, zoom, Luna a andar com colisão.
- [x] **Marco 2 – Recursos:** jazidas finitas no chão, trilhos e vaus, extração a mão. *(testado no jogo)*
- [x] **Marco 3a – Extração automática:** broca, esteiras com itens misturados, baú, árvores mináveis. *(testado no jogo)*
- [x] **Divisão em módulos** (`world`, `logic`, `render`, `main`), sem mudar o comportamento.
- [x] **Marco 3b – O forno:** areia, carvoeira (pirólise), forno com receitas (tijolo, ferro), broca sobre árvores. *(testado a jogar)*
- [ ] **Marco 4 – Massa e volume:** itens com massa/volume e baús com limites.
- [ ] **Marco 5 – Personagem:** recolher à mão, inventário real, construir com custo (agora há tijolo e ferro), meter itens em máquinas à mão.
- [ ] **Marco 6 – Camadas:** primeira caverna (`z = 1`), escadas, esteiras verticais, elevadores.
- [ ] **Marco 7 – Mundo vivo:** NPCs, casas com interior, defesas.
- [ ] **Depois:** vários setores, comboios, reinos, exploração espacial, diário da Luna, música, menus, guardar/carregar jogo.

### Dívida técnica a ter em conta

- ~~Dividir `main.c` em módulos~~ **Feito** (world / logic / render / main). Verificado sem mudar o comportamento: mesmos testes, e um teste de regressão (30 mundos + 4 cenários com ~1400 construções) com resultado idêntico antes e depois.
- **`OreType` como tipo de item** (Marco 4): passar a um `ItemType` próprio com massa e volume; os produtos novos já o antecipam.
- Criar um sistema de **guardar/carregar** cedo (a geração determinística ajuda: basta guardar a seed + as alterações e os objetos).
- `sim_step()` percorre os 2048 objetos todos os passos; quando houver muitos, pensar numa lista só dos ativos.

---

## 9. Notas para quem continua o projeto

- **Quem desenhou o jogo não programa.** O código tem de ser fácil de ler e comentado. Evitar truques e otimizações prematuras.
- Ao acabar uma alteração: compilar com `-Wall -Wextra`, correr `tests/test_logica.c`, correr o jogo e **atualizar este README** (secção 2, roadmap, decisões).
- Qualquer nova mecânica deve respeitar a visão da secção 1.
- **Máquinas novas** (ex.: forno nível 2, fábrica de vidro): acrescentar um tipo em `BuildType`/`BUILD[]` (`logic.h`/`logic.c`), acrescentar a(s) receita(s) a `RECIPES[]` (`logic.c`) com esse `machine`, e adicionar o tipo a `is_machine()` (`logic.c`) e a `draw_machine()` (`render.c`). `accept_item()`, `machine_step()`, a demolição e o HUD já as tratam.
- **Itens novos:** acrescentar a `OreType` (antes de `ORE_COUNT`, em `world.h`) e a `ORE[]` (`world.c`). Se for um produto, `richest = 0`.
- A mochila (`inventory[]`) é provisória e só conta unidades; o Marco 4 troca-a por itens com massa e volume.
- No ambiente de desenvolvimento das IAs **não há raylib**: a verificação foi feita com `tests/fake/raylib.h`. Compila e testa sempre no PC do autor com a raylib verdadeira antes de dares o marco por fechado.
