# A Clareira da Luna

Jogo de **automação + aventura + exploração**, escrito em **C (C99)** com a biblioteca **raylib**.

> Mindustry/Factorio (logística e fábricas) + Pixel Dungeon (cavernas e exploração) + a história da Luna.

Este repositório contém, para já, **um único setor**: *A Clareira da Luna*, inspirado na Floresta dos Encantados da história original. A ideia a longo prazo é um mundo dividido em vários setores (como no Mindustry), cada um recriando um local da história.

Este documento serve para que qualquer pessoa (ou IA) que pegue no projeto perceba **o que existe, porquê, e o que falta fazer**.

---

## 1. Estado atual

**Versão: Marco 3a implementado** (extração automática). Compila com `-std=c99 -Wall -Wextra` sem avisos e a lógica passou em testes automáticos (jazida → broca → esteira com curva → baú, minérios misturados, conservação de unidades, baú cheio, demolição, árvores). **Ainda não foi testado a jogar** pelo autor do design: é o primeiro passo a fazer (ver "Como testar o Marco 3a" abaixo).

| Funciona | Ainda não existe |
|---|---|
| Geração do setor (160×120 tiles, sempre igual com a mesma seed) | Forno, receitas, carvão, areia (Marco 3b) |
| Terreno com quadrados coloridos (placeholder) | Inventário real (massa/volume), custo de construção, combate |
| Luna (placeholder) com WASD/setas, correr com Shift, colisões | NPCs, dungeons, camadas verticais |
| Câmara que segue a Luna, zoom, câmara livre (TAB) | Pixel art real, sons, menus, guardar jogo |
| Trilhos e vaus (água rasa) que ligam a clareira aos recursos | História/diário da Luna dentro do jogo |
| Jazidas finitas de hematite, argila, enxofre e quartzo | |
| **Árvores mináveis:** dão madeira (20 a 30 cada); cortadas, viram chão da floresta | |
| Extração a mão com o rato (ferramenta 1) | |
| **Broca mecânica 2×2**, **esteiras** (itens misturados) e **baú** | |
| Simulação em passo fixo (60 passos/s, independente do FPS) | |
| Mochila provisória (só contagens) no canto do ecrã | |

### Como testar o Marco 3a

1. Corre o jogo. Com a ferramenta `1` (mão), corta uma árvore (clique esquerdo, segurar): a copa encolhe, ganhas madeira, e no fim o tile fica livre.
2. Vai à encosta de hematite (trilho para sudeste). Carrega em `2` e põe uma broca sobre a jazida (o "fantasma" fica verde se pode; precisa de pelo menos 1 tile com minério por baixo).
3. Carrega em `3`, e arrasta com o botão esquerdo a partir de um tile ao lado da broca: faz uma linha de esteira (as curvas ligam-se sozinhas). Acaba-a num baú (`4`).
4. Vê os itens a andar na esteira e o baú a encher. A luz da broca: verde trabalha, amarelo saída cheia, vermelho sem minério. Passa o rato sobre o baú para ver o conteúdo e carrega em `F` (perto) para o esvaziar para a mochila.
5. Compara: a broca tira ~3× mais depressa do que as tuas mãos e esgota a jazida.

---
|---|
| Geração do setor (160×120 tiles, sempre igual com a mesma seed) | Esteiras, máquinas, baús |
| Terreno com quadrados coloridos (placeholder) | Inventário real (massa/volume), construção, combate |
| Luna (placeholder) com WASD/setas, correr com Shift, colisões | NPCs, dungeons, camadas verticais |
| Câmara que segue a Luna, zoom, câmara livre (TAB) | Pixel art real, sons, menus, guardar jogo |
| **Trilhos** e **vaus** (água rasa) que ligam a clareira aos recursos | História/diário da Luna dentro do jogo |
| **Jazidas finitas** de hematite, argila, enxofre e quartzo | |
| **Extração a mão** com o rato: a jazida perde pedrinhas e esgota-se | |
| Mochila provisória (só contagens) no canto do ecrã | |

---

## 2. Como compilar e correr

Precisas de um compilador de C (gcc) e da raylib.

**Windows**
1. Instala a raylib com o instalador oficial (https://www.raylib.com), que já traz o compilador MinGW.
2. Na pasta do projeto: `make`
   (ou `gcc main.c -o clareira.exe -lraylib -lopengl32 -lgdi32 -lwinmm`)
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

O código compila com `-std=c99 -Wall -Wextra` **sem avisos**. Mantém assim.

### Controlos

| Tecla | Ação |
|---|---|
| `W A S D` / setas | Mover a Luna (em câmara livre: mover a câmara) |
| `Shift` | Correr |
| `1` | Ferramenta **mão**: botão esquerdo (segurar) minera a jazida ou a árvore sob o rato, se estiver ao alcance (2.5 tiles) |
| `2` / `3` / `4` | Ferramenta **broca** / **esteira** / **baú**: botão esquerdo constrói (alcance 10 tiles); segurar e arrastar faz uma linha |
| `Q` / `E` | Rodar a esteira (anti-horário / horário) |
| Botão direito | Demolir o objeto sob o rato (devolve os itens à mochila). Em câmara livre serve para arrastar o mapa |
| `F` | Pegar tudo do baú sob o rato (ao alcance das mãos) |
| Roda do rato | Zoom (0.5x a 8x) |
| `TAB` | Alternar câmara livre / seguir a Luna |
| Botão direito ou do meio | Arrastar o mapa (só em câmara livre) |
| `G` | Mostrar/esconder grelha (só com zoom ≥ 2x) |
| `R` | Gerar outro mapa (muda a seed; esvazia a mochila) |
| `F11` | Ecrã inteiro |

---

## 3. Estrutura do código

Por agora tudo está em **um só ficheiro**, `main.c`, organizado por secções (separadas por comentários). Quando passar de ~1500 linhas, deve ser dividido em módulos (ver roadmap).

| Secção | O que contém |
|---|---|
| Configuração | `MAP_W`, `MAP_H`, `MAP_D`, `TILE_SIZE` e as posições dos locais importantes |
| Tipos de terreno | `enum Terrain` e a tabela `TERRAIN[]` (nome, cor, se dá para andar) |
| Minérios | `enum OreType` e a tabela `ORE[]` (nome, cor, quantidade máxima no centro de uma jazida) |
| Mundo | `Tile` (terreno + jazida), `world[z][y][x]`, `tile_at()`, `in_bounds()`, fila `mark_dirty()` de tiles a redesenhar |
| Objetos construídos | `Building` (broca, esteira, baú), lista `buildings[]`, grelha `bgrid[][]`, `building_at()`, tabela `BUILD[]` |
| Ruído | `value_noise()` e `fbm()`: ruído suave para o terreno parecer natural |
| Geração | `generate_tile()` decide o terreno; `carve_trails()` abre trilhos e vaus; `generate_ores()` e `place_patch()` colocam as jazidas; `remove_unreachable_ore()` apaga minério inacessível; `generate_world()` junta tudo |
| Desenho | `draw_tile()`, `ore_dots()`, `build_terrain_texture()`, `redraw_tile()`, `draw_world()` |
| Jogador | `Player`, `player_move()` (colisão), `update_player()`, `draw_player()` |
| Extração | `extract_from_tile()` (lógica pura, usada por mãos e brocas), `mine_tile()` (mãos), `update_mining()` (rato e temporizador), `draw_hover()`, `inventory[]` |
| Construção e logística | `can_place()`, `place_building()`, `remove_building_at()`, `accept_item()` (entregar itens), `belt_step()`, `drill_step()`, `sim_step()` (passo fixo), `update_building()` (rato), `draw_buildings()`, `draw_build_preview()` |
| Câmara | `update_camera()` (seguir, câmara livre, zoom) |
| HUD | `draw_hud()` (informação no canto do ecrã) |
| Principal | `main()` e o ciclo do jogo |

### Decisões técnicas importantes (não mudar sem pensar)

- **Mundo em `world[z][y][x]`.** Hoje `MAP_D = 1` (só superfície). As cavernas serão `z = 1, 2, …`. Cada quadrado da superfície corresponde ao mesmo `(x, y)` nas camadas inferiores, como no design original.
- **Geração determinística.** O mapa depende só da seed (`DEFAULT_SEED = 1234`). Não usar `rand()` na geração; usar `hash2()`/`rnd01()` para o resultado ser sempre igual.
- **Terreno em textura.** O terreno é desenhado uma vez para `terrain_tex` e só é redesenhado quando o mapa muda (`build_terrain_texture()`). Se um tile mudar durante o jogo (ex.: uma jazida esgota-se), é preciso redesenhar esse tile na textura, não o mapa todo.
- **Colisão por eixos separados.** `player_move()` move em X e em Y em separado para a Luna deslizar pelas paredes. A caixa de colisão tem 8×8 px (`PLAYER_HALF = 4`).
- **Ordem da geração:** terreno base → trilhos e vaus → jazidas → remoção de minério inalcançável. Cada jazida só se coloca no terreno certo (hematite e quartzo em pedra, enxofre em pântano, argila em lama).
- **Tudo o que é minério tem de ser alcançável a pé.** O ribeiro corta o mapa e a floresta é densa, por isso os trilhos abrem caminho da clareira até cada zona de recursos e, onde cruzam água, criam um **vau** (`T_VAU`, água rasa onde se anda). No fim, `remove_unreachable_ore()` apaga qualquer minério que mesmo assim fique isolado. Verificado em 60 seeds.
- **Jazidas finitas.** Cada tile guarda `ore` e `ore_amount`. Mais perto do centro da mancha, mais minério. Ao chegar a 0, `ore` volta a `ORE_NONE`. O aspeto (1 a 4 pedrinhas) depende da quantidade que resta.
- **Quando um tile muda durante o jogo** chama-se `redraw_tile(x, y)` para atualizar só esse tile na textura (não o mapa todo).
- **Lógica separada do rato.** `mine_tile()` não usa raylib nem input: assim pode ser testada sem abrir uma janela, e as máquinas do Marco 3 podem reutilizá-la.
- **Valores de minério baixos de propósito** (`ORE[].richest`: 60 a 150). Com o Marco 3a a jazida finita passa a importar: uma broca sobre 4 tiles ricos esgota-os em menos de um minuto (4 tiles x até 120 unidades, a 16/s). Ajustar `richest` (e `DRILL_PERIOD`) depois de jogar, conforme o ritmo que se quiser.
- **Unidades:** posições do jogador e da câmara em **pixels do mundo**; posições do mapa em **tiles**. Conversão: `tile = floor(pixel / TILE_SIZE)`.
- **Tamanho do tile: 16×16 px.**
- **Objetos construídos separados do `Tile`.** Vivem em `buildings[]` (máx. `MAX_BUILDINGS` = 2048) e `bgrid[y][x]` guarda o índice + 1 do objeto que ocupa cada tile (0 = livre). A broca ocupa 2×2 (a origem `x, y` é o canto superior esquerdo). Brocas e baús bloqueiam a Luna; esteiras não.
- **Simulação em passo fixo.** `sim_step(SIM_DT)` corre 60 vezes por segundo (acumulador no `main()`), por isso as máquinas andam à mesma velocidade com qualquer FPS. A lógica (`sim_step`, `accept_item`, `extract_from_tile`, `place_building`…) **não usa raylib** e foi testada sem janela.
- **Sem inserters.** Quem entrega chama `accept_item(tx, ty, dx, dy, item)` ao objeto à sua frente; devolve 0 se não aceita (cheio, esteira de frente, broca…). A esteira entra no baú (e, no Marco 3b, na máquina) diretamente. Uma esteira também aceita itens do lado e de trás, nunca de quem está virado para ela.
- **Itens misturados na esteira.** Cada tile de esteira leva até 4 itens (`BELT_CAP`), cada um com o seu tipo e posição (0 a 1); espaçamento `ITEM_GAP`. A broca alterna entre os tipos que extraiu, por isso a esteira sai misturada.
- **Broca.** A cada `DRILL_PERIOD` (0,25 s) tira 1 unidade de **cada** tile com minério debaixo dela (até 4 → 16/s) para um buffer de 16; entrega 1 item por passo a uma das 8 posições à volta (esteira ou baú), alternando. Buffer cheio → pára (luz amarela). Nada por baixo → luz vermelha.
- **Tiles a redesenhar.** A lógica chama `mark_dirty(x, y)`; o `main()` chama `flush_dirty()` uma vez por frame. Já não se chama `redraw_tile()` diretamente (foi removida).
- **Árvores.** Cada `T_ARVORE` do interior do mapa tem `ore = ORE_MADEIRA` (20 a 30). Ao esgotar, `extract_from_tile()` muda o terreno para `T_FLORESTA`. A parede de árvores do bordo (3 tiles) **não** dá madeira, para o setor continuar fechado. `remove_unreachable_ore()` ignora árvores (nunca são "alcançáveis a pé"; cortam-se do tile ao lado) e `carve_brush()` limpa a madeira das árvores que os trilhos substituem.
- **Construir é grátis por agora** (sem custo em recursos). O Marco 5 liga a construção ao inventário.
- **Baú com limite provisório** de `CHEST_CAP` = 500 unidades no total; o Marco 4 troca-o por massa e volume.
- **Estilo de código:** C simples (arrays de tamanho fixo, `struct`s, poucos ponteiros, sem alocação dinâmica enquanto possível). Comentários em português.

---

## 4. O mapa e a história

O setor reproduz a clareira de Gabriel e Luna (capítulo *"O Rapaz que Construía para Não Pensar"*). Cada terreno tem origem na história:

| Terreno | Onde | Papel no jogo (planeado) |
|---|---|---|
| Clareira | Centro (80, 55), raio ~14 | Base inicial, sem árvores |
| Casa na árvore | Centro da clareira | Ponto de partida; obstáculo |
| Ribeiro + lama | A sul, corre para leste | Água; margem com argila e areia |
| Pântano | Leste (143, 74) | Enxofre, resina, água ácida |
| Encosta rochosa | Sudeste (118, 102) | Hematite (minério de ferro) |
| Afloramento | Noroeste (28, 24) | Quartzo, granito |
| Floresta densa | Em redor, mais densa longe da clareira | Limite do setor; árvores bloqueiam |
| Trilho | Da clareira para hematite, quartzo, pântano e ribeiro | Terra batida; garante acesso aos recursos |
| Vau | Onde um trilho cruza o ribeiro | Água rasa; única forma de atravessar o rio |

### Jazidas (Marco 2)

| Minério | Terreno | Onde | Quantidade máxima por tile |
|---|---|---|---|
| Hematite | Pedra | Encosta sudeste (2 manchas) | 120 |
| Argila | Lama | Margem norte do ribeiro (4 manchas; seguem o ribeiro) | 150 |
| Enxofre | Pântano | Pântano a leste (2 manchas) | 80 |
| Quartzo | Pedra | Afloramento noroeste (2 manchas) | 60 |
| Madeira | Árvore | Qualquer árvore do interior do mapa | 30 (por árvore) |

Na seed 1234 há cerca de 200 tiles de hematite, 85 de argila, 85 de enxofre e 85 de quartzo. Na seed 1234 há ~7300 árvores mináveis (madeira, 20 a 30 cada). Ainda **não existem** nitrato, areia, fibras, figos nem granito como recursos.

### Árvore tecnológica (inspirada no capítulo da clareira)

Esta cadeia de produção é a base da progressão do setor:

1. **Argila + areia → tijolos → forno** (o forno tem 3 versões na história: 3 níveis da mesma máquina)
2. **Madeira → carvão vegetal** (pirólise)
3. **Hematite + carvão → ferro** (fundição com cadinho; ferramentas)
4. **Enxofre (pântano) + nitrato + carvão → pólvora** (partir granito e abrir cavernas)
5. **Fibras → papel**, **quartzo → vidro**
6. **Figos → fermentação → álcool → glicerina**

Os coleccionáveis "folhas do diário da Luna" desbloqueiam receitas e contam a história.

**Ficheiros da história** (`0O_Livros_versão_1/2/3.docx`): a versão 3 foi lida apenas em parte (início e capítulo da clareira). As versões 1 e 2 ainda não foram analisadas. Antes de recriar outros locais da história em novos setores, ler os restantes capítulos.

---

## 5. Visão completa do jogo (design original)

Isto **ainda não está implementado**, mas guia todas as decisões:

**Mundo**
- Mundo dividido em **setores** (estilo Mindustry); este repositório começa só com um.
- **Camadas verticais:** superfície + cavernas/dungeons por baixo, ligadas por esteiras verticais e elevadores que extraem recursos para cima.
- Cavernas espalhadas pelo mapa, estilo **Pixel Dungeon**, com minérios mais raros.
- Casas com **interior maior que o exterior** (entras pela porta e o espaço é maior).

**Recursos e logística**
- **Jazidas estilo Factorio:** manchas no chão com quantidade **finita** de minério.
- **Esteiras sem inserters:** a esteira entra na máquina e a máquina tem uma saída.
- **Baús e contentores por massa e volume** (cada item tem massa e volume; o contentor tem limites para os dois).
- **Comboios.**

**Aventura**
- NPCs (pixel art estilo Terraria) que se tornam amigos e exploram dungeons contigo.
- Vários reinos; muralhas e torres de defesa.
- Combate estilo Factorio (defesas, torres, ondas), não um hack and slash.

---

## 6. Roadmap

Regra de ouro: **cada marco tem de ser jogável e testável antes de começar o seguinte.**

- [x] **Marco 1 – Mapa:** terreno, câmara, zoom, Luna a andar com colisão.
- [x] **Marco 2 – Recursos:** jazidas finitas no chão (hematite, argila, enxofre, quartzo), trilhos e vaus, extração a mão que esgota a jazida. *(testado no jogo)*
- [x] **Marco 3a – Extração automática:** broca mecânica, esteiras com itens misturados, baú, árvores mináveis (madeira). *(implementado e testado por testes automáticos; falta testar a jogar)*
- [ ] **Marco 3b – O forno:** areia nas margens do ribeiro, carvão por pirólise (madeira → carvão), e o forno com receitas (tijolo; depois ferro). A esteira entra na máquina e sai por outro lado, sem inserters.
- [ ] **Marco 4 – Massa e volume:** itens com massa/volume e baús com limites.
- [ ] **Marco 5 – Personagem:** recolher à mão, inventário, construir (colocar esteiras e máquinas).
- [ ] **Marco 6 – Camadas:** primeira caverna (`z = 1`), escadas, esteiras verticais, elevadores.
- [ ] **Marco 7 – Mundo vivo:** NPCs, casas com interior, defesas.
- [ ] **Depois:** vários setores, comboios, reinos, diário da Luna, música, menus, guardar/carregar jogo.

### Dívida técnica a ter em conta

- Dividir `main.c` em módulos quando crescer (`world.c`, `player.c`, `render.c`, …).
- Criar um sistema de **guardar/carregar** cedo (a geração determinística ajuda: basta guardar a seed + as alterações).
- Quando existirem muitas entidades (itens em esteiras, máquinas), pensar em estruturas de dados próprias e **atualização em passo fixo** (simulação independente do FPS).

---

## 7. Pixel art

Atualmente tudo são **quadrados coloridos (placeholders)**. Especificações para a arte final, para manter consistência:

- **Tiles de 16×16 px**, vista de cima (top-down), estilo Terraria/Stardew.
- **Paleta limitada** (16 a 24 cores), partilhada por todos os sprites.
- Sem anti-aliasing; fundo transparente nos objetos; terrenos que encaixam sem costura.
- 2 a 4 variantes por terreno, para o mapa não parecer repetido.
- Entregar em **spritesheets** em grelha de 16×16. Confirmar sempre as dimensões exatas das imagens.

Terrenos que precisam de arte no Marco 1: relva clara, relva normal, chão da floresta, árvore, água, lama, pântano, pedra, casa na árvore, e a Luna (idealmente com 4 direções e animação de andar).

---

## 8. Notas para quem continua o projeto

- **Quem desenhou o jogo não programa.** O código tem de ser fácil de ler e comentado. Evitar truques e otimizações prematuras.
- Ao acabar uma alteração: compilar com `-Wall -Wextra`, correr o jogo e **atualizar este README** (secção 1 e roadmap).
- Qualquer nova mecânica deve respeitar as decisões da secção 5.
- A alteração mais provável a seguir é o Marco 3b (o forno). A base já existe: acrescentar um tipo em `BuildType`/`BUILD[]`, dar-lhe entrada em `accept_item()` e uma saída tal como `drill_dump()`. Os itens usam hoje o `OreType` como tipo; produtos novos (carvão, tijolo, ferro) precisam de entrar nessa lista (ou de uma `ItemType` própria, que será preciso de qualquer forma no Marco 4).
- A mochila (`inventory[]`) é provisória e só conta unidades; o Marco 4 troca-a por itens com massa e volume.
- Recursos que a história menciona e ainda não existem no jogo: nitrato, areia, fibras, figos, granito. (A madeira já existe.)
- Só existe `main.c` e não há raylib neste ambiente de desenvolvimento: a verificação foi feita com um cabeçalho falso. Compila sempre no teu PC com a raylib verdadeira antes de dares o marco por fechado.
