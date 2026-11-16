# A Clareira da Luna

Jogo de **automação + aventura + exploração**, escrito em **C (C99)** com a biblioteca **raylib**.

> Mindustry/Factorio (logística e fábricas) + Pixel Dungeon (cavernas e exploração) + exploração espacial + a história da Luna e do Gabriel.

Este repositório contém, para já, **um único setor**: *A Clareira da Luna*, inspirado na Floresta dos Encantados da história original. Os setores são a unidade do mundo (como no Mindustry) e cada um pode recriar um local da história.

Este documento serve para que qualquer pessoa (ou IA) que pegue no projeto perceba **o que existe, porquê, e o que falta fazer**. Se mudares o código, atualiza-o (secção 1, roadmap e decisões técnicas).

---

## 0. Ficheiros do projeto

| Ficheiro | Para que serve |
|---|---|
| `main.c` | Teclado, rato, câmara e o ciclo do jogo. Ver secção 4. |
| `items.c` / `items.h` | **Os itens:** tipos, volume e densidade (tabela `ITEM[]`). Sem raylib (só o tipo `Color`). |
| `world.c` / `world.h` | **O mundo:** terreno, jazidas (o que há no chão), ruído e geração do setor. |
| `logic.c` / `logic.h` | **A lógica:** contentores (mochila, baús), objetos construídos, receitas, máquinas, esteiras, brocas, simulação, extração, carga da Luna, colisões. **Sem raylib.** |
| `render.c` / `render.h` | **Tudo o que desenha:** terreno, objetos, Luna, HUD. |
| `menu.c` / `menu.h` | **Os menus:** construção (`Q`) e mochila + estrutura (`E`): cliques para passar itens. |
| `Makefile` | `make` (jogo), `make test` (testes), `make clean`. Compila os 6 `.c` juntos. |
| `README.md` | Este documento. |
| `tests/test_logica.c` | Testes automáticos da **lógica** (sem janela; compilam só `world.c` + `logic.c` + `items.c`). Ver "Como correr os testes". |
| `tests/test_menu.c` | Testes dos **menus** com **rato falso**: simulam cliques (largar no chão, meter ingredientes, tirar produtos, escolher no menu de construção). Compilam também `menu.c` + `render.c`. |
| `tests/fake/raylib.h` | Cabeçalho **falso** da raylib (funções que não fazem nada; o rato e o Shift são controláveis pelos testes), só para compilar e testar onde a raylib não está instalada. **Só compila com `-DCLAREIRA_TEST`** (dá erro de propósito sem isso). **Nunca** o deixar ao lado do `main.c` nem pôr `-Itests/fake` no comando do jogo (o programa compilaria mas sairia logo, sem janela). |
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
- **Baús e contentores por massa e volume** (cada item tem massa e volume; o contentor tem limites para os dois). *(feito no Marco 4; ver secção 2)*
- **Comboios.**

**Aventura**
- **NPCs** (pixel art estilo Terraria) que se tornam teus amigos e exploram as dungeons contigo.
- Combate **estilo Factorio** (defesas, torres, ondas), não um hack and slash.

---

## 2. Estado atual

**Versão: Marco 3c implementado** (jogabilidade: controlos novos, menu de construção, modos criativo/sobrevivência, **obras com custos realistas e tempo de construção**, forno rupestre, menus de todas as estruturas, itens no chão e **interface ajustável**), em cima do **Marco 4** (mochila, massa e volume, que outra IA fez e o autor testou). Compila com `-std=c99 -Wall -Wextra` **sem avisos** e passa nos testes automáticos (`make test`: lógica + menus com cliques falsos; também com `-fsanitize=address,undefined`). **O 3c e o Marco 4 foram testados a jogar pelo autor e estão aprovados.** Depois do teste fez-se uma ronda de acertos: **mais recursos** por árvore e por jazida, e **obras com menu de depósito** (secção abaixo). Por fim, as **demolições progressivas** (secção Demolições): a estrutura desfaz-se aos poucos e o material sai degradado para amontoados no chão, com sucata e terra. Estas rondas ainda não foram testadas a jogar.

> **Nota sobre os nomes dos marcos.** Uma IA anterior chamou "Marco 3c" ao trabalho da mochila e da massa/volume; o autor chama a isso **Marco 4**. O **Marco 3c do autor** (jogabilidade) é o desta versão. A secção 8 tem a lista certa.

| Funciona | Ainda não existe |
|---|---|
| Geração do setor (160×120 tiles, sempre igual com a mesma seed) | Máquinas e brocas com buffers em litros (Marco 4b) |
| Terreno com quadrados coloridos (placeholder) | Combate, NPCs, dungeons, camadas verticais |
| Luna (placeholder): **WASD**, **Shift** correr (só com carga leve), colisões | Pixel art real, sons, guardar jogo |
| Câmara que segue a Luna, zoom, câmara livre (`TAB`) | História/diário da Luna dentro do jogo |
| Jazidas finitas: hematite, argila, enxofre, quartzo e areia; árvores mináveis | Nitrato, fibras, figos, granito como recursos |
| **Mão** (sem estrutura selecionada): botão esquerdo minera jazidas e árvores | Vidro, papel, pólvora, glicerina (receitas) |
| **Menu de construção (`Q`)**: lista com ícone, descrição e custo; a escolha fica na **hotbar** | Forno em 3 níveis (a história tem 3 versões) |
| **Modo criativo** (instantâneo e grátis) e **sobrevivência** (`M` alterna): **obras**: **depositas o material na obra pelo menu (`E`)** e a Luna constrói-a aos poucos, com **tempo** conforme a complexidade; sem material depositado a obra **fica a meio e não funciona** | Atalhos numéricos na hotbar / várias ranhuras |
| **Forno rupestre** (só argila, só tijolos, parte ao fim de 20) | Um `Esc` que abra um menu de pausa/sair (agora fecha-se na janela) |
| **Menu de cada estrutura (`E` com o rato em cima)**: baú, contentor, amontoado, **obras** (depositar material), **máquinas** (meter ingredientes a mão, tirar produtos), broca, esteira | Rodar máquinas (só as esteiras têm direção) |
| **Largar itens no chão (`E` sem estrutura)**: cria um **amontoado** do item | Esteiras/brocas a largar em amontoados (de propósito) |
| Broca mecânica 2×2 (também corta árvores), esteiras, baú 50 L, contentor 2000 L, carvoeira, forno | |
| Itens com volume e massa; mochila 20 L / 50 kg; carga da Luna (15/30/50 kg) | |
| Simulação em passo fixo (60 passos/s, independente do FPS) | |
| HUD: modo, o que está selecionado, info do objeto sob o rato, resumo da mochila, hotbar | |
| **Interface ajustável** (100% a 300%, `F6`/`F7` ou `Ctrl`+roda), ajuda com todos os controlos (`H`); texto que quebra linhas; nunca fica cortado em janelas pequenas | Fonte maior/mais legível, modo de alto contraste |

### Itens, volume e massa (Marco 4)

Escala: **1 tile = 50 cm**. A **unidade mínima** de um item é **0,1 L (100 mL)** para pó, areia, rocha partida e lenha; o **tijolo** é uma peça inteira (20×10×5 cm = 1 L). A massa de 1 unidade = volume × densidade. Tabela `ITEM[]` em `items.c` (fácil de ajustar):

| Item | Volume/un. | Densidade (g/L) | Massa/un. | Porquê |
|---|---|---|---|---|
| Madeira | 0,1 L | 400 | 40 g | lenha empilhada (há ar entre os paus) |
| Carvão vegetal | 0,1 L | 300 | 30 g | leve e poroso (valor do autor) |
| Enxofre | 0,1 L | 1000 | 100 g | pó e pedacinhos |
| Quartzo | 0,1 L | 1500 | 150 g | rocha partida |
| Areia | 0,1 L | 1600 | 160 g | areia húmida |
| Argila | 0,1 L | 1700 | 170 g | argila húmida |
| Hematite | 0,1 L | 2500 | 250 g | minério de ferro partido |
| Tijolo | 1 L | 1800 | 1,8 kg | peça de 20×10×5 cm |
| Ferro (barra) | 0,1 L | 7800 | 780 g | barra pequena e maciça |
| Sucata (ferro enferrujado) | 0,1 L | 7800 | 780 g | a mesma barra, enferrujada: sai de demolir, volta a fundir-se (secção Demolições) |
| Terra | 0,1 L | 1000 | 100 g | terra solta: a massa que se perde ao demolir; desaparece do chão passados 2 min |

A densidade é "a granel" (como se carrega, com ar entre as pedras), não a da rocha maciça. Por isso o minério pesa muito mais do que a madeira para o mesmo volume: 20 L de madeira são 8 kg; 20 L de hematite são 50 kg.

**Contentores** (todos usam a mesma estrutura `Container`, com limite de volume e, opcionalmente, de massa; só entram **unidades inteiras** e basta **um** dos limites chegar para não caber mais):

| Contentor | Volume | Massa |
|---|---|---|
| Mochila da Luna | 20 L | 50 kg |
| Baú (1×1) | 50 L | ilimitada (fica no chão) |
| Contentor (2×2) | 2000 L | ilimitada |

**Carga da Luna** (peso da mochila; constantes em `logic.h`):

| Peso | O que acontece |
|---|---|
| até 15 kg | anda normalmente e **pode correr** |
| 15 a 30 kg | anda **devagar** (60 %) e não corre |
| 30 a 50 kg | consegue levar, mas **não anda** (pode esvaziar a mochila no menu) |
| mais de 50 kg | não consegue apanhar (é o limite da mochila) |

*(A Luna será uma NPC de fim de jogo e não a personagem jogável; por agora fica como está, para já a termos desenhada. O sistema de mochila e carga não depende dela: `backpack` é um `Container` à parte.)*

### Receitas (realistas: a massa do produto nunca passa a que entrou)

Tabela `RECIPES[]` em `logic.c`. Há um **teste automático** que falha se uma receita criar massa do nada. Tempos e quantidades **provisórios**: ajustar depois de jogar.

| Máquina | Receita | Entra | Sai | Tempo | Porquê |
|---|---|---|---|---|---|
| Carvoeira | Carvão vegetal | 4 madeira (160 g) | 1 carvão (30 g) | 3 s | ~19 % de rendimento em massa, o normal numa carvoeira de terra |
| Forno | Tijolo | 8 argila + 4 areia + 12 madeira (2,48 kg) | 1 tijolo (1,8 kg) | 8 s | 2 : 1 argila:areia (a história); perde água; a lenha é combustível |
| Forno | Ferro | 7 hematite + 40 carvão (2,95 kg) | 1 barra (780 g) | 15 s | 1,75 kg de minério (~60 % de ferro, ~74 % recuperado); ~1,5 kg de carvão por kg de ferro |
| Forno | Ferro (sucata) | 1 sucata + 5 carvão (930 g) | 1 barra (780 g) | 6 s | refundir ferro enferrujado de uma demolição; muito mais barato do que reduzir minério |

(Antes eram 3 tijolos por 4 unidades e 2 ferros por 4: a massa do produto era muito maior do que a que entrava.)

Como funciona (igual para todas as máquinas):
- A esteira **entra** na máquina: `accept_item()` só aceita itens que são ingredientes de alguma receita dessa máquina, até **2 lotes** do que a receita mais pede (`machine_in_cap()`). Quando está cheio, a esteira fica bloqueada à espera.
- A máquina escolhe sozinha a **primeira receita** para a qual já tem tudo, gasta os ingredientes no início e, passado o tempo, guarda o produto.
- Os produtos saem para qualquer **esteira, baú, contentor ou outra máquina encostada** a ela (reparte por todas as saídas). Pode guardar até 16 produtos (`MACH_OUT_CAP`); cheia → pára (luz amarela). **Baú ou contentor cheio de volume recusa itens**, e a esteira ou a broca ficam à espera.
- **Atenção, é de propósito:** uma esteira com itens misturados pode **bloquear** uma máquina (se o item da frente é um que a máquina já tem cheio, os de trás não passam, mesmo que ela precise deles). Resolve-se com uma esteira por ingrediente, como no Factorio. Uma carvoeira **encostada** ao forno entrega-lhe o carvão diretamente.
- Luz da máquina: verde trabalha, vermelho parada (faltam ingredientes), amarelo saída cheia. Passar o rato por cima mostra a receita, o progresso, o que tem dentro e o que está pronto.
- **Demolir** (botão direito) já **não é instantâneo nem vai para a mochila**: ver a secção **Demolições** abaixo. O que a máquina tinha dentro (incluindo a receita em curso) sai tal e qual para amontoados no chão.

### Modos de jogo, obras e custos (Marco 3c)

`M` alterna entre **criativo** e **sobrevivência**. **O jogo arranca em sobrevivência** (`DEFAULT_MODE` no `main.c`; a lógica em si arranca em criativo, para os testes).

- **Criativo:** as estruturas ficam **logo prontas e grátis**. Se mudares para criativo com obras a meio, acabam logo, sem gastar mais.
- **Sobrevivência:** podes **sempre pousar** a estrutura (mesmo sem material nenhum): fica uma **obra**. Para a construir, **chega-te ao pé (2,5 tiles), carrega `E` com o rato em cima da obra e deposita lá o material**: clica num item da mochila (esquerda) e ele passa para a obra (direita): clique = 10 un, **Shift+clique** = tudo o que a obra ainda pede, botão direito = 1. A obra **nunca aceita mais do que o custo** e só aceita os itens do seu custo.
- **A Luna constrói com o que lá está depositado**, ao ritmo da obra (o tempo da tabela). Se o material depositado acaba, **a obra pára a meio** (contorno vermelho; `PARADA: falta material`) e **não funciona** (não aceita itens, não anda, não trabalha): deposita mais e ela continua sozinha. Com vários materiais, o que **menos chegou** é que manda (ex.: broca com metade da madeira pára aos 50 %). A mochila **não é mexida** pela obra: só sai de lá o que tu depositas.
- **Tirar de volta:** no painel da obra, clicar no material tira o que **ainda não foi usado** pela construção (o que já entrou na obra só volta demolindo). Assim não se deposita, constrói e tira o material de volta.
- A Luna constrói **uma obra de cada vez** (a mais antiga que tenha material) e **só se estiver ao pé** (`BUILD_REACH_PX`, 10 tiles); longe, a obra pára (contorno cinzento; o material fica lá). Uma obra sem material não trava as outras.
- **Demolir** uma obra desfaz-a (ver **Demolições**): o material depositado e ainda **não usado** sai tal e qual; o já usado sai degradado como numa estrutura acabada. O que se construiu em criativo não dá material nenhum.
- No desenho, uma obra mostra o ícone da estrutura que vai ficando a descoberto de baixo para cima, com uma barra de progresso. Passa o rato por cima ou carrega `E` para ver o material (depositado / total) e quanto tens na mochila.

**Custos e tempos** (tabela `BUILD[]` em `logic.c`, com a conta feita no comentário). Escala: **1 tile = 50 cm**, logo uma estrutura 2×2 ocupa 1 m × 1 m; 1 unidade = 0,1 L (tijolo = 1 L). Os custos pensam-se em **volume de material de verdade**; **provisórios**: afinar depois de jogar.

| Estrutura | Material | ~Volume | ~Massa | Obra |
|---|---|---|---|---|
| Forno rupestre | 800 argila *(cúpula de 1 m, paredes de ~10 cm)* | 80 L | 136 kg | 45 s |
| Carvoeira | 150 madeira + 400 argila *(lenha coberta de terra)* | 55 L | 74 kg | 30 s |
| Forno | 50 tijolos + 150 argila *(câmara de tijolo com argamassa)* | 65 L | 115 kg | 90 s |
| Broca mecânica | 8 ferro + 10 tijolos + 60 madeira | 17 L | 27 kg | 60 s |
| Esteira (1 tile) | 15 madeira | 1,5 L | 0,6 kg | 1,5 s |
| Baú | 100 madeira | 10 L | 4 kg | 6 s |
| Contentor | 40 ferro + 200 madeira | 24 L | 39 kg | 120 s |

Para afinar de uma vez, **dois botões** em `logic.h`: **`COST_PERCENT`** (escala todos os custos: 50 = metade) e **`TIME_PERCENT`** (escala todos os tempos de obra). A progressão pensada: **mão** (argila, areia, madeira) → **forno rupestre** (tijolos, a mão) → **forno** e **carvoeira** → **ferro** (hematite + carvão) → **brocas**, esteiras, baús. Como o forno pede 50 tijolos e cada rupestre só faz 20, são precisos ~3 rupestres: é o arranque lento que se pede a um jogo realista; **se for aborrecido, baixa `COST_PERCENT` antes de mexer nas receitas.**

### Demolições (inverso das construções)

Botão direito numa estrutura **marca-a para demolir**. A Luna desfaz-a **ao pé** (`BUILD_REACH_PX`, 10 tiles), **uma de cada vez** (a mais antiga marcada), em **metade do tempo da obra** (`DEMOLISH_TIME_PERCENT` = 50 em `logic.h`; uma obra a meio demora só a fração já construída). A estrutura **vai desaparecendo de cima para baixo** (contorno laranja a trabalhar, cinzento se a Luna está longe ou na fila, vermelho se não há chão livre) e, à medida que desaparece, o que tinha **sai para amontoados no chão à volta** (nunca para a mochila). Enquanto está a ser demolida **não funciona** e o menu (`E`) só mostra o que ainda falta sair. Os amontoados (`B_AMONTOADO`) continuam a apanhar-se na hora com o botão direito. Em **criativo** a demolição é instantânea e só larga o que a estrutura tinha dentro (o que se constrói em criativo não tem material).

O que sai (função `demolish_manifest()`):

| Origem | O que sai |
|---|---|
| O que estava **dentro** (esteira, baú, máquina, broca, receita em curso) | tal e qual |
| **Argila** do custo | 100 % |
| **Madeira** do custo | 50 % (`salvage_pct` em `ITEM[]`): apodrece e racha |
| **Tijolos** do custo | 60 %: partem-se ao desmontar a argamassa |
| **Ferro** do custo | 100 %, mas como **Sucata** (ferro enferrujado): só serve depois de **fundida no forno** (receita *Ferro (sucata)*) |
| **Massa que falta** (madeira e tijolos perdidos) | vira **Terra** (100 g/un) |
| **Forno rupestre** | **só terra** (a cúpula é argila cozida que se desfaz): 136 kg = 1360 un |

**A massa conserva-se:** massa da construção = massa do que sai, a menos de 1 unidade de terra (< 100 g, porque as unidades não se partem). Há um teste para todas as estruturas. Exemplos: esteira 600 g → 7 madeira + 3 terra; broca 26,6 kg → 8 sucata + 6 tijolos + 30 madeira + 84 terra.

A **terra** apanha-se como qualquer item mas **ainda não serve para nada**: o amontoado de terra **desaparece 2 minutos** (`TERRA_LIFE_S`) depois de lá chegar a última (cada vez que chega mais terra o relógio recomeça). Se **não houver chão livre** num raio de 6 tiles (água, árvores, estruturas), a demolição pára (vermelho) sem perder nada e continua quando houver espaço. Os itens saem a conta-gotas ao ritmo da demolição, e o que sai fica sempre em amontoados de **um só item**.

### Forno rupestre

2×2, **só de argila** (800: ver a tabela de custos), **só cozinha tijolos** (mesma receita do forno: 8 argila + 4 areia + 12 madeira → 1 tijolo, mas em **12 s** em vez de 8) e **parte ao fim de 20 tijolos** (`RUPESTRE_LIFE`, campo `life` de `BUILD[]`; qualquer máquina pode ter `life`). Ao partir, **o que tinha dentro (ingredientes e tijolos por entregar) fica no chão em amontoados** (nenhum item se perde). Racha-se no desenho a partir dos 15 lotes. Não rodar: tem as quatro saídas como as outras máquinas.

### Amontoados (itens no chão)

Largar itens num tile livre (terreno onde se anda; não água, não árvore, não estrutura) cria um **amontoado desse item** (200 L, massa ilimitada). As demolições também os criam (ver acima). **Um amontoado é de um só item**: para outro item, outro tile. Não bloqueia a Luna, mas não se constrói por cima. **Esteiras e brocas não largam itens neles** (de propósito, para não haver surpresas). Quando fica vazio **desaparece**. Demolir (botão direito) um amontoado **apanha o que couber** na mochila e deixa o resto. É também o que acontece ao conteúdo de uma máquina que parte.

### Menus e controlos

Todos os menus mostram a **mochila à esquerda** e, à direita, o que está sob o rato:

| Alvo | O que o painel mostra e faz |
|---|---|
| **Chão** (sem estrutura) | Larga itens da mochila (cria o amontoado). Usa o tile sob o rato, se estiver ao alcance, senão o tile da Luna |
| **Baú / contentor** | Conteúdo, barras de volume e peso. Passa itens nos dois sentidos |
| **Amontoado** | O item e a quantidade. Passa itens nos dois sentidos (só do mesmo item) |
| **Máquina** | Estado e progresso, lotes feitos (se parte), **receitas**, **Dentro** (ingredientes: clica para tirar; clicar um item da mochila **mete-o**, até 2 lotes) e **Pronto** (produtos: clica para tirar) |
| **Broca** | Estado e o que guarda (clica para tirar) |
| **Esteira** | Direção e itens que leva (só informação) |

Clique esquerdo numa linha: 10 unidades; **Shift+clique**: tudo o que couber; **botão direito**: 1. O menu **fecha sozinho** se a Luna se afasta (2,5 tiles) ou se a estrutura desaparece (se for um amontoado que se esvazia, passa a mostrar o chão). O **`Esc` fecha o menu**; sem menu, **larga a estrutura selecionada**. O `Esc` **não fecha o jogo** (`SetExitKey(KEY_NULL)`): fecha-se na janela.

**Hotbar.** A meio, em baixo: a estrutura selecionada (ou a "Mão"), o custo (a verde se a mochila o tem, a vermelho se não) e as dicas. A "caixa de construção" é o **fantasma** sob o rato: verde = pode, vermelho = não pode (terreno, ocupado, longe **ou sem recursos**).

### Como testar o Marco 3c (a jogar)

1. **Interface.** O jogo arranca a **150%**. `F6`/`F7` mudam o tamanho (100% a 300%; também `Ctrl` + roda do rato). `H` mostra a ajuda. Escolhe o tamanho em que consegues ler tudo e diz-me qual ficou melhor (para o pôr como predefinição). Numa janela pequena, o tamanho desce sozinho para nada ficar cortado (aparece um aviso); `F11` ou maximizar dá mais espaço.
2. **Arranca em sobrevivência** com a mochila vazia. Abre `Q`: o forno rupestre custa 800 argila, 136 kg, 45 s de obra. Escolhe-o, **pousa-o junto à argila** do ribeiro (pode-se pousar já). Minera argila à mão, carrega `E` em cima da obra e **deposita a argila**: a obra vai-se construindo; quando o depositado acaba, **pára a meio** (contorno vermelho) até depositares mais.
3. Põe-te ao pé, **rato em cima, `E`**: vês o material depositado/total e o que tens na mochila. Quando acabar, o `E` mostra o menu da máquina: minera areia e madeira e **clica nos itens da mochila** para os meter. **Ao 20.º tijolo parte**: o que sobrar fica em amontoados.
4. Afasta-te mais de 10 tiles de uma obra: **pára** (cinzento). Volta: continua.
5. **`E` sem estrutura**: a mochila com o "Chão" ao lado. Clica num item: larga 10 e cria o amontoado.
6. Constrói **carvoeira**, **forno** (50 tijolos!), e faz **ferro**. Depois **broca**, esteiras (15 madeira cada) e um **baú**.
7. `M` para **criativo**: tudo logo pronto e grátis. Volta a sobrevivência e demole: o que construíste em criativo **não devolve** recursos; uma obra a meio devolve o que já gastou.
8. **O que me interessa saber:** o tamanho da interface está bom (e a predefinição)? O arranque é demasiado lento (se sim, qual `COST_PERCENT`)? As obras fazem sentido (parar a meio, só ao pé, uma de cada vez)? O que falta para a obra ser mais clara?

### Como testar o Marco 4 (mochila, massa e volume)

1. **Mochila.** `E` (sem estrutura sob o rato) abre a mochila. Minera um pouco à mão e vê os números subirem. Aos 20 L ou 50 kg **deixa de apanhar** (aviso "Mochila cheia").
2. **Peso.** 15 a 30 kg: andas devagar e não corres; mais de 30 kg: não andas (20 L de areia = 32 kg). Esvazia no menu.
3. **Baú e contentor.** Constrói um baú (`Q`) e abre-o com `E` em cima. Clique passa 10, Shift+clique tudo, botão direito 1. Se não couber: "Nao cabe".
4. **Demolir** um baú cheio com a mochila vazia: recusa se não couber tudo (aviso).

### Como correr os testes

Não precisam da raylib nem de janela. A partir da pasta do projeto:

```
make test
```

(equivale a compilar `tests/test_logica.c` com `world.c logic.c items.c` e `tests/test_menu.c` com `menu.c render.c world.c logic.c items.c`, ambos com `-Wall -Wextra -Wno-unused-function -DCLAREIRA_TEST -Itests/fake -lm`, e correr os dois). Devem aparecer `TODOS OS TESTES PASSARAM` e `TESTES DOS MENUS PASSARAM`.

**`tests/test_logica.c`** (lógica) cobre: areia em 20 seeds (e só em lama), carvoeira alimentada por broca e esteira (conservação exata), forno de ferro e de tijolos, limites de entrada, saída cheia, demolição, carvoeira encostada ao forno, broca a cortar árvores, **tabela de itens** (volume e massa), **limites da mochila** (volume, massa, qual chega primeiro), **escalões de carga**, **baú de 50 L e contentor de 2000 L**, **transferências** entre contentores, **minerar a mão** com a mochila cheia, **demolição** (tudo para a mochila, ou recusa sem mudar nada), **broca contra baú cheio**, **realismo das receitas** (a massa do produto nunca passa a que entrou) e, desde o 3c: **custos e tempos** (campos sem itens repetidos, o rupestre é só argila e pesa centenas de kg, o tempo cresce com a complexidade), **obras** (pousa-se sem material; sem material não avança; **depositar material** (só os itens do custo, nunca mais que o custo; a mochila não é usada sozinha), avança ao ritmo da obra até onde o depositado chega e pára; com vários materiais manda o que menos chegou; fica a meio sem funcionar; **tirar de volta só o que ainda não foi usado**; acaba com o custo **exato** na obra e **nem um segundo antes** do tempo da tabela, para todas as estruturas; só avança com a Luna ao pé; uma de cada vez, a mais antiga, sem uma obra parada travar as outras; demolir devolve exatamente o depositado e recusa se não couber; e as **quantidades de recursos** (árvores 400–600 un, cada jazida com material para centenas de construções); criativo acaba logo e não devolve nada), **forno rupestre** (só tijolos, parte aos 20, **conservação exata de todos os ingredientes** com o que sobra em amontoados), **amontoados** (largar, juntar, um item por amontoado, terreno inválido, apanhar, 200 L, não bloqueia a Luna, desaparece vazio) e os **menus das estruturas** (carregar ingredientes até 2 lotes, tirar produtos, mochila cheia não perde nada, broca) e as **demolições**: **massa conservada em todas as estruturas** (< 1 unidade de terra de diferença) com o que sai no chão igual ao manifesto, tempos (metade da obra), contas fechadas (esteira, broca, carvoeira, rupestre só terra, conteúdo intacto, obra a meio, criativo instantâneo e sem recursos de borla), **a terra desaparece aos 2 min** (e o relógio recomeça quando chega mais), a Luna só demole ao pé e uma de cada vez, **sem chão livre pára e não perde nada**, e **sucata → ferro** no forno.

**`tests/test_menu.c`** (menus, com rato e Shift falsos) simula os cliques: largar no chão (10, tudo, 1), apanhar de volta, chão inválido, meter ingredientes numa máquina e tirar os produtos, o menu fechar sozinho (Luna afasta-se, estrutura demolida), o menu de construção devolver a estrutura escolhida (só uma vez), **depositar material numa obra e tirá-lo de volta** (clique, Shift+clique, botão direito; obra com dois materiais; nunca mais que o custo; uma obra não recebe ingredientes), e a **interface ajustável**: degraus de 100% a 300%, o tamanho que cabe na janela (1280×720 só deixa ~144%) e **os cliques a acertar a 150% e com a interface limitada pela janela**. **Não verifica o aspeto** (cores, posições no ecrã, a hotbar, o fantasma) nem a sensação. **Corre sempre os testes e compila com a raylib verdadeira no teu PC antes de dares um marco por fechado.**

---

## 3. Como compilar e correr

Precisas de um compilador de C (gcc) e da raylib.

**Windows**
1. Instala a raylib com o instalador oficial (https://www.raylib.com), que já traz o compilador MinGW.
2. Na pasta do projeto: `make`
   (ou `gcc -std=c99 -Wall -Wextra main.c world.c logic.c render.c items.c menu.c -o clareira.exe -lraylib -lopengl32 -lgdi32 -lwinmm`)
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

Os seis ficheiros `.c` compilam sempre **juntos** com um único comando (é o que o `make` faz). O código compila com `-std=c99 -Wall -Wextra` **sem avisos**. Mantém assim.

### Controlos

| Tecla | Ação |
|---|---|
| `W A S D` / setas | Mover a Luna (em câmara livre: mover a câmara) |
| `Shift` | Correr (só com a mochila até 15 kg) |
| Botão esquerdo | **Sem estrutura selecionada:** minera a jazida ou árvore sob o rato (ao alcance, 2.5 tiles; não por baixo de estruturas). **Com uma estrutura selecionada:** pousa-a (alcance 10 tiles; em sobrevivência fica uma **obra**); segurar e arrastar faz uma linha de esteiras |
| Botão direito | **Marca para demolir** o objeto sob o rato: a Luna desfaz-a ao pé e o material sai para amontoados no chão (madeira e tijolos só em parte, ferro enferrujado, o resto terra). Num amontoado, apanha o que couber. Em câmara livre serve para arrastar o mapa |
| `Q` | **Menu de construção:** escolhe a estrutura (fica na hotbar). `Q` outra vez fecha |
| `R` | **Roda** a estrutura antes de a construir (`Shift+R` ao contrário). Só as esteiras têm direção |
| `E` | **Mochila.** Com o rato sobre uma estrutura ao alcance: abre o menu dela. Sem estrutura: abre a mochila com o chão ao lado (largar itens). `E` outra vez fecha |
| `Esc` | Fecha a ajuda ou o menu aberto; sem nada aberto, **larga a estrutura selecionada**. Nunca fecha o jogo |
| No menu | Clique: 10 un. · Shift+clique: tudo o que couber · botão direito: 1 un. · botão "Fechar" |
| `M` | Alternar **criativo / sobrevivência** |
| `F6` / `F7` | Interface **menor / maior** (100% a 300%; também `-`/`+` do teclado numérico e **`Ctrl` + roda do rato**) |
| `H` | **Ajuda** com todos os controlos (`H` ou `Esc` fecham) |
| Roda do rato | Zoom (0.5x a 8x) |
| `TAB` | Alternar câmara livre / seguir a Luna |
| Botão direito ou do meio | Arrastar o mapa (só em câmara livre) |
| `G` | Mostrar/esconder grelha (só com zoom ≥ 2x) |
| `F5` | Gerar outro mapa (muda a seed; esvazia a mochila e os objetos) |
| `F11` | Ecrã inteiro |

*(Antes do 3c: `1`–`7` escolhiam a ferramenta, `Q`/`E` rodavam, `I` abria a mochila, `F` abria baús e `R` gerava outro mapa. Tudo isso mudou.)*

---

## 4. Estrutura do código

O código está dividido em **6 módulos** (cada um com o seu `.c`/`.h`), por camadas. Cada módulo só conhece os que estão **abaixo** dele:

```
main.c  ──►  menu.c, render.c  ──►  logic.c  ──►  world.c  ──►  items.c
(input)      (desenho, raylib)      (sem raylib)   (mundo)       (itens)
```

Única exceção, de propósito: `generate_world()` (em `world.c`) chama `reset_buildings()` (de `logic.c`) para limpar os objetos ao gerar outro mapa. `menu.c` usa também duas peças de `render.c` (`draw_fill_bar`, `carry_label`).

**`items.c` / `items.h` – os itens**

| Secção | O que contém |
|---|---|
| Tipos | `enum ItemType` (hematite, argila, enxofre, quartzo, madeira, areia, os produtos carvão, tijolo, ferro, e o que sai de demolir: **sucata** (`ITEM_FERRO_ENF`) e **terra**) |
| Tabela | `ITEM[]`: nome, cor, **volume de 1 unidade (mL)**, **densidade (g/L)**, e para a demolição **`salvage_pct`** (% que se aproveita) e **`salvage_item`** (em que se transforma, `ITEM_NONE` = o mesmo); `item_mass_g()` calcula a massa |

**`world.c` / `world.h` – o mundo**

| Secção | O que contém |
|---|---|
| Configuração | `MAP_W`, `MAP_H`, `MAP_D`, `TILE_SIZE` e as posições dos locais importantes |
| Tipos de terreno | `enum Terrain` e a tabela `TERRAIN[]` (nome, cor, se dá para andar) |
| Jazidas | `enum OreType` (só o que existe **no chão**) e `ORE[]` (que `item` dá cada jazida e a quantidade máxima) |
| Mundo | `Tile` (terreno + jazida), `world[z][y][x]`, `tile_at()`, `in_bounds()`, fila `mark_dirty()` de tiles a redesenhar, `ore_dots()` |
| Ruído | `hash2()`, `rnd01()`, `dist2d()`, `clampf()` (públicos); `value_noise()` e `fbm()` (internos) |
| Geração | `generate_tile()` decide o terreno; `carve_trails()` abre trilhos e vaus; `generate_ores()` e `place_patch()` colocam as jazidas; `remove_unreachable_ore()` apaga minério inacessível; `generate_world()` junta tudo |

**`logic.c` / `logic.h` – a lógica (sem raylib)**

| Secção | O que contém |
|---|---|
| Contentores | `Container` (limite de volume e de massa, unidades de cada item), `container_room()`, `container_add()`, `container_take()`, `container_move()`, `counts_fit()`… |
| Objetos construídos | `Building` (broca, esteira, baú, contentor, carvoeira, forno, **forno rupestre**, **amontoado**), `buildings[]`, `bgrid[][]`, `building_at()`, tabela `BUILD[]` (nome, descrição, tamanho, volume que guarda, **vida**, **custo**), `BUILDABLE[]` (a ordem do menu), `is_storage()`, `is_pile()`, `has_store()` |
| **Modos, custos e obras** | `game_mode`, `cost_units()` / `build_seconds()` (com `COST_PERCENT`/`TIME_PERCENT`), `build_cost_vol_ml()` / `build_cost_mass_g()`, `can_afford()` (só avisos); `place_building()` pousa sempre (em sobrevivência cria uma obra), **`construction_step()`** (a Luna constrói: uma obra de cada vez, a mais antiga com material depositado, ao pé; não mexe na mochila), `site_active()`, `SiteState`; **`site_deposit()` / `site_take_back()` / `site_free()` / `site_need()` / `site_slot()`** (o material depositado na obra, `Building.deposit[]`); `remove_building_at()` (instantâneo, para a mochila: hoje só o usa o `main.c` nos amontoados; as estruturas demolem-se com `demolish_start()`) |
| **Amontoados** | `ground_ok()`, `ground_accepts()`, `drop_to_ground()`, `pile_take()`; `spill_and_remove()` (interna: o que sobra quando uma máquina parte); a terra desaparece em `sim_step()` (`Building.decay`, `TERRA_LIFE_S`) |
| **Demolição** | `demolish_manifest()` (o que sai, com a massa conservada), `demolish_start()` (marca), **`demolition_step()`** (a Luna demole uma de cada vez, ao pé), `demo_active()`, `demolish_seconds()`, `DemoState`; internas: `demo_release()`, `spill_item_around()` (amontoados em anéis à volta, `DEMO_SPILL_RINGS`). Campos `demo_*` do `Building` |
| **Menus das estruturas** | `machine_load()`, `machine_take_in()`, `machine_take_out()`, `drill_take()`: movem itens sem perder nenhum |
| Jogador | `Player`, colisões (`tile_blocked()`, `box_blocked()`, `player_move()`, `player_spawn()`) e **carga** (`carry_state()`, `carry_speed_factor()`, `carry_can_run()`) |
| Extração e mochila | `extract_from_tile()` (mãos e brocas), `mine_tile()` (mãos, só o que cabe), `mine_blocked()`, `backpack`, `tile_in_reach()`, `tile_within()` |
| Receitas e máquinas | `RECIPES[]`, `machine_in_cap()`, `recipe_ready()`, `machine_step()`, `machine_status()` |
| Construção e logística | `can_place()`, `place_building()`, `remove_building_at()` (mochila ou recusa), `accept_item()`, `dump_item()`, `belt_step()`, `drill_step()`, `sim_step()` |

**`render.c` / `render.h` – o desenho (raylib)**

| Secção | O que contém |
|---|---|
| Terreno | `render_init()`/`render_shutdown()` (textura do terreno), `draw_tile()`, `build_terrain_texture()`, `flush_dirty()`, `draw_world()` |
| Luna e objetos | `draw_player()`, `draw_buildings()` (`draw_belt`, `draw_drill`, `draw_chest`, `draw_container`, `draw_machine` (inclui o rupestre), `draw_pile` (a terra esvanece-se no fim), **`draw_site`** = a obra, **`draw_demo`** = a demolição), `draw_build_preview()`, `draw_hover()`, `mouse_tile()` |
| **Interface (escala)** | `ui_scale` (o tamanho pedido), `ui_scale_step()`, **`ui_fit()`** (o pedido ou o maior que cabe na janela), `draw_help()`. Tudo o que é interface desenha-se num **ecrã virtual** (`BeginMode2D` com zoom = fator) e o rato divide-se pelo mesmo fator |
| HUD e peças partilhadas | `draw_hud()` (painel de cima com **quebra de linhas**, modo, selecionado, info do objeto sob o rato incl. obras, resumo da mochila, avisos), **`draw_hotbar()`** (inclui tempo e peso da obra), `draw_fill_bar()`, `carry_label()`, `game_mode_label()`, **`draw_build_icon()`** (ícone de cada estrutura) e **`draw_cost()`** (o custo, a verde/vermelho) |

**`menu.c` / `menu.h` – os menus (raylib)**

| Secção | O que contém |
|---|---|
| Estado | `menu_toggle_build()` (Q), `menu_open_building()` / `menu_open_backpack()` (E), `menu_close()`, `menu_is_open()`, `menu_picked_build()` (a escolha do menu de construção, entregue uma só vez) |
| Linhas | `left_rows()` (mochila) e `right_rows()` (estrutura ou chão): cada painel é uma lista de linhas (texto, título, item clicável); o **mesmo** código das linhas serve para desenhar e para os cliques |
| Cliques | `menu_update()`: `put_item()` (mochila → estrutura/chão), `take_item()` (estrutura → mochila); fecha sozinho se a estrutura some ou a Luna se afasta. Uma **obra** mostra o material (depositado/total) e **recebe o que o custo pede** (`put_item()` → `site_deposit()`; `take_item()` → `site_take_back()`). A escala (`set_scale()`, `mouse()`) é a mesma do desenho |
| Desenho | `menu_draw()`: menu de construção (ícone, nome, descrição, custo) ou os dois painéis (mochila e estrutura/chão) com barras de volume e peso |

**`main.c` – input e ciclo do jogo**

| Secção | O que contém |
|---|---|
| Estado da interface | `tool` (0 = nada selecionado, senão um `BuildType`), `build_dir`, `is_mining`, `ignore_mouse`, o aviso curto no ecrã, `DEFAULT_MODE` |
| Input | `update_player()` (teclado, com a carga), `update_mining()` (não minera por baixo de estruturas), `update_building()` (rato: pousar obras, demolir, avisos), `open_menu_at_mouse()` (tecla E); teclas `F6`/`F7`/`H`, `Ctrl`+roda |
| Câmara | `update_camera()` (seguir, câmara livre, zoom) |
| Principal | `main()` e o ciclo do jogo: teclas (M, R, Q, E, H, F5, F6/F7, Esc...), menus, rato, `construction_step()` + simulação, desenho. `SetExitKey(KEY_NULL)` para o Esc não fechar o jogo |

### Decisões técnicas importantes (não mudar sem pensar)

- **Mundo em `world[z][y][x]`.** Hoje `MAP_D = 1` (só superfície). As cavernas serão `z = 1, 2, …`. Cada quadrado da superfície corresponde ao mesmo `(x, y)` nas camadas inferiores.
- **Geração determinística.** O mapa depende só da seed (`DEFAULT_SEED = 1234`). Não usar `rand()` na geração; usar `hash2()`/`rnd01()` para o resultado ser sempre igual.
- **Terreno em textura.** O terreno é desenhado uma vez para `terrain_tex` e só é redesenhado quando o mapa muda. A lógica chama `mark_dirty(x, y)`; o `main()` chama `flush_dirty()` (em `render.c`) uma vez por frame. (Não se chama `redraw_tile()` diretamente: foi removida.)
- **Colisão por eixos separados.** `player_move()` move em X e em Y em separado para a Luna deslizar pelas paredes. Caixa de colisão 8×8 px (`PLAYER_HALF = 4`).
- **Ordem da geração:** terreno base → trilhos e vaus → jazidas → remoção de minério inalcançável. Cada jazida só se coloca no terreno certo (hematite e quartzo em pedra, enxofre em pântano, argila e areia em lama).
- **Tudo o que é minério tem de ser alcançável a pé.** O ribeiro corta o mapa e a floresta é densa, por isso os trilhos abrem caminho da clareira até cada zona de recursos e, onde cruzam água, criam um **vau** (`T_VAU`, água rasa onde se anda). No fim, `remove_unreachable_ore()` apaga qualquer minério que mesmo assim fique isolado.
- **Jazidas finitas.** Cada tile guarda `ore` e `ore_amount`. Mais perto do centro da mancha, mais minério. Ao chegar a 0, `ore` volta a `ORE_NONE`. O aspeto (1 a 4 pedrinhas) depende da quantidade que resta.
- **Árvores.** Cada `T_ARVORE` do interior do mapa tem `ore = ORE_MADEIRA` (`TREE_WOOD_MIN` a `TREE_WOOD_MAX`, em `world.h`: 400 a 600 un = 40 a 60 L). Ao esgotar, `extract_from_tile()` muda o terreno para `T_FLORESTA`. A parede de árvores do bordo (3 tiles) **não** dá madeira, para o setor continuar fechado. Cortam-se a mão do tile ao lado ou com uma **broca assente em cima** (`can_place()` abre essa exceção só para a broca e só em árvores com madeira): é assim que a madeira entra nas esteiras automaticamente (à mão também se pode meter madeira nas máquinas pelo menu).
- **Lógica separada do rato e do desenho.** `extract_from_tile()`, `accept_item()`, `sim_step()`, `place_building()`… **não usam raylib nem input**: por isso se testam sem janela. Os testes compilam só `world.c` + `logic.c` + `items.c`. (A única coisa da raylib que a lógica usa é o **tipo** `Color`, nas tabelas de cores.)
- **Módulos.** Dependências só "para baixo" (`main → menu/render → logic → world → items`). As tabelas (`TERRAIN[]`, `ORE[]`, `BUILD[]`, `RECIPES[]`) são `extern const` no `.h` e definidas no `.c` (não `static const` num cabeçalho, que daria avisos). O estado da interface (`tool`, `build_dir`, `is_mining`) fica no `main.c` e passa-se por parâmetro a quem desenha.
- **Unidades:** posições do jogador e da câmara em **pixels do mundo**; posições do mapa em **tiles**. `tile = floor(pixel / TILE_SIZE)`. Tile: 16×16 px = **50 cm** (decisão do autor). Volumes em **mL** e massas em **gramas**, sempre inteiros.
- **Objetos construídos separados do `Tile`.** Vivem em `buildings[]` (máx. `MAX_BUILDINGS` = 2048) e `bgrid[y][x]` guarda o índice + 1 (0 = livre). Brocas, carvoeiras e fornos ocupam 2×2 (a origem `x, y` é o canto superior esquerdo). Brocas, baús e máquinas bloqueiam a Luna; esteiras não.
- **Simulação em passo fixo.** `sim_step(SIM_DT)` corre 60 vezes por segundo (acumulador no `main()`), por isso as máquinas andam à mesma velocidade com qualquer FPS.
- **Sem inserters.** Quem entrega chama `accept_item(tx, ty, dx, dy, item)` ao objeto à sua frente; devolve 0 se não aceita (cheio, esteira de frente, ingrediente que a máquina não usa…). A esteira entra no baú ou na máquina diretamente. Uma esteira também aceita itens do lado e de trás, nunca de quem está virado para ela (por isso uma esteira de entrada nunca recebe de volta o que a máquina despeja).
- **Itens misturados na esteira.** Cada tile leva até 4 itens (`BELT_CAP`), cada um com tipo e posição (0 a 1); espaçamento `ITEM_GAP`.
- **Despejar (`dump_item()`):** broca e máquinas entregam 1 item por passo a um dos tiles à volta (esquerda, direita, cima, baixo), a começar em `cursor_out` e a rodar; alternam entre tipos (`cursor_type`), por isso a esteira sai misturada.
- **Broca.** A cada `DRILL_PERIOD` (0,25 s) tira 1 unidade de **cada** tile com minério debaixo dela (até 4 → 16/s) para um buffer de 16. Buffer cheio → pára (luz amarela). Nada por baixo → luz vermelha.
- **Custos, modos e obras (3c, material depositado).** `game_mode` é global em `logic.c`. Em **criativo** `place_building()` cria a estrutura **acabada** (`Building.build = 1`) e de graça. Em **sobrevivência** cria uma **obra** (`build = 0`, `site_order` crescente): **nunca recusa por falta de material**. O jogador **deposita** o material na obra pelo menu (`site_deposit()`: só os itens do custo, nunca mais que o custo; guardado em `Building.deposit[]`). `construction_step(px, py, dt)` (chamado a cada passo da simulação pelo `main.c`, com a posição da Luna) escolhe **uma** obra: a mais antiga, ao alcance e com material; avança `dt / build_seconds()` e **limita o progresso ao que está depositado** (`site_material_limit()`: o campo do custo com menor `deposit / custo`). **Não mexe na mochila.** `site_take_back()` só deixa tirar o que o progresso ainda não usou (`site_free()` = depositado − `ceil(custo × progresso)`), para não dar para construir e recuperar o material. **Enquanto `build < 1` a estrutura não funciona:** `sim_step()` salta-a, `accept_item()` e `machine_load()` recusam, e o desenho/menus mostram a obra. **Demolir devolve `deposit[]`** (tudo o que foi depositado, usado ou não; por isso o que se fez em criativo não devolve nada). Os campos de um custo têm de ser itens **diferentes** (há teste). A lógica arranca em criativo (os testes antigos não precisam de recursos); **o jogo** escolhe o modo inicial (`DEFAULT_MODE`).
- **Interface ajustável (3c).** Nenhum tamanho de HUD/menu é em pixels reais: tudo se desenha num **ecrã virtual** = ecrã real ÷ fator, com `BeginMode2D(zoom = fator)`; o rato divide-se pelo mesmo fator (`mouse()` em `menu.c`; o rato do MUNDO no HUD continua real). O fator é `ui_fit(sw, sh, largura_precisa, altura_precisa)` = o pedido (`ui_scale`) ou o maior que cabe: assim **nunca fica nada cortado**. **Regra para código novo de interface:** desenhar sempre com as dimensões virtuais, e o texto comprido com quebra de linhas (`hud_add()`), nunca com larguras fixas em pixels do ecrã.
- **Máquinas que partem (3c).** `BUILD[].life` > 0 = lotes que a máquina faz antes de partir (forno rupestre: 20). Ao partir, `spill_and_remove()` tira o conteúdo (ingredientes, produtos, a receita em curso) e larga-o em **amontoados**, primeiro nos tiles onde ela estava e depois à volta: nada se perde.
- **Amontoado (3c).** É um `Building` do tipo `B_AMONTOADO` (1×1, 200 L, massa ilimitada, não sólido) com `pile_item`. Cria-se com `drop_to_ground()` (nunca com `place_building()`, que o recusa). Vazio desaparece (`pile_take()` e `sim_step()`).
- **Menus (3c).** Cada painel dos menus é uma lista de **linhas** gerada por `left_rows()`/`right_rows()`; o desenho e o teste do clique usam a mesma lista (por isso nunca desalinham). As linhas dos itens seguem a **ordem de `ItemType`** (hematite, argila, enxofre, quartzo, madeira, areia, carvão, tijolo, ferro). Os testes dos menus (`tests/test_menu.c`) dependem desta geometria: se mudares tamanhos ou ordem, atualiza-os.
- **O rato depois de um menu (3c).** Um clique que escolhe uma estrutura fecha o menu; para esse mesmo clique não construir logo, o `main.c` ignora o rato (`ignore_mouse`) até se largarem os botões.
- **Selecionar e largar (3c).** `tool` guarda a estrutura selecionada (0 = nada, a mão minera). `Esc` fecha menus e, sem menu, larga a selecção. `SetExitKey(KEY_NULL)` impede o Esc de fechar a janela.
- **`ItemType` ≠ `OreType`.** `OreType` é só o que existe **no chão** (`Tile.ore`); `ItemType` é o que anda em esteiras e se guarda (esteiras, baús, mochila, máquinas). Cada jazida diz que item dá (`ORE[].item`). Há itens que não existem no chão (carvão, tijolo, ferro). Os índices `store.n[]`, `in[]`, `out[]`, `buf[]` são de **ItemType**.
- **Unidade mínima de um item = 0,1 L** (pó, areia, rocha partida, lenha, barra de ferro) ou a peça inteira (tijolo = 1 L). Massa = volume × densidade, tabela `ITEM[]`. As quantidades que já existiam (jazidas, receitas) são em **unidades**; as receitas foram refeitas para conservar a massa.
- **`Container`:** uma estrutura única (limite de volume, limite de massa opcional, unidades de cada item) para a **mochila**, **baús** e **contentor**. Só entram unidades inteiras e basta um dos limites chegar para não caber mais (`container_room()`). **Baú** 50 L e **contentor** 2000 L, ambos com massa ilimitada (ficam no chão). **Mochila** 20 L e 50 kg.
- **Carga da Luna** (`carry_state()`): até 15 kg normal e corre; 15 a 30 kg devagar (60 %); 30 a 50 kg não anda; a mochila não passa de 50 kg. Os valores estão em `logic.h`.
- **Demolir (3c, demolição progressiva).** `demolish_start()` calcula **uma vez** o `demolish_manifest()` (tudo o que vai sair), guarda-o em `Building.demo_all[]` / `demo_left[]`, **esvazia a estrutura** (conteúdo e `deposit[]`, para nunca contar duas vezes) e põe `demo_on`. `demolition_step()` escolhe uma estrutura (a mais antiga, ao alcance) e baixa `demo_p` de 1 a 0; em cada passo larga em amontoados a diferença entre `demo_left` e `floor(demo_all × demo_p)` (`demo_release()`). **Se não houver chão, `demo_p` não avança** e o que ficou por largar continua em `demo_left`: nada se perde. A estrutura só é libertada quando tudo saiu. `deposit[]` é o **registo do que se pagou**: o que se construiu em criativo não tem `deposit` e por isso não dá material. Enquanto `demo_on`, `site_active()` é falso, e `sim_step()`, `accept_item()`, `machine_load()` e `construction_step()` ignoram a estrutura; o menu não deixa meter nem tirar itens. **Terra:** a massa perdida é `Σ(usado × massa) − Σ(aproveitado × massa)`, dividida pela massa de 1 unidade de terra (arredonda para baixo). `BUILD[].demo_earth` (rupestre) manda tudo para terra. O amontoado **não** se demole assim: `remove_building_at()` apanha o que couber para a mochila. *(O menu das máquinas passou a mostrar 18 linhas no painel da direita, para caberem as 3 receitas do forno sem esconder o "Pronto".)*
- **Limites das máquinas.** Ainda em unidades (`machine_in_cap()` = 2 lotes; `MACH_OUT_CAP` = 16; `DRILL_BUF` = 16). O Marco 4 passa-os a litros.
- **A Luna é uma NPC de fim de jogo** (decisão do autor); por agora é a personagem jogável. A mochila (`backpack`) e a carga não dependem dela, para se poder trocar de protagonista.
- **Valores provisórios:** `ORE[].richest`, `TREE_WOOD_MIN/MAX`, `DRILL_PERIOD`, `RECIPES[]`, **`BUILD[].cost` e `.build_s`** (ou de uma vez `COST_PERCENT`/`TIME_PERCENT`), `UI_SCALE_DEFAULT`, `RUPESTRE_LIFE`, `MACH_OUT_CAP`, `DRILL_BUF`, `CARRY_SLOW_FACTOR`, `PILE_VOL_ML`, tabela `ITEM[]`. Ajustar depois de jogar.
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
| Hematite | Pedra | Encosta sudeste (2 manchas) | 1500 | 198 | ~148 000 |
| Argila | Lama | Margem norte do ribeiro (4 manchas) | 2000 | 83 | ~96 000 |
| Areia | Lama | Margem norte do ribeiro, **entre** as de argila (4 manchas) | 1500 | 62 | ~50 000 |
| Enxofre | Pântano | Pântano a leste (2 manchas) | 800 | 83 | ~33 500 |
| Quartzo | Pedra | Afloramento noroeste (2 manchas) | 600 | 84 | ~25 000 |
| Madeira | Árvore | Qualquer árvore do interior do mapa | 400 a 600 (por árvore) | 7340 | ~3 670 000 |

Ainda **não existem** nitrato, fibras, figos nem granito como recursos.

### Árvore tecnológica (inspirada no capítulo da clareira)

| # | Cadeia | Estado |
|---|---|---|
| 1 | Argila + areia → tijolos → forno (3 níveis na história) | **Feito**: forno rupestre (parte aos 20) e forno (não parte); falta o terceiro nível |
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

Precisam de arte: relva clara, relva normal, chão da floresta, árvore, água, lama, pântano, pedra, casa na árvore, a Luna (idealmente 4 direções e animação de andar), e os objetos: **broca, esteira (com direção), baú, contentor, carvoeira, forno, forno rupestre** (com estado: a trabalhar/parado; o rupestre com 2 ou 3 fases de rachas), **amontoado** (pelo menos um por item, ou um genérico tingido) e os itens (hematite, argila, areia, enxofre, quartzo, madeira, carvão, tijolo, ferro).

---

## 8. Roadmap

Regra de ouro: **cada marco tem de ser jogável e testável antes de começar o seguinte.**

- [x] **Marco 1 – Mapa:** terreno, câmara, zoom, Luna a andar com colisão.
- [x] **Marco 2 – Recursos:** jazidas finitas no chão, trilhos e vaus, extração a mão. *(testado no jogo)*
- [x] **Marco 3a – Extração automática:** broca, esteiras com itens misturados, baú, árvores mináveis. *(testado no jogo)*
- [x] **Divisão em módulos** (`world`, `logic`, `render`, `main`), sem mudar o comportamento.
- [x] **Marco 3b – O forno:** areia, carvoeira (pirólise), forno com receitas (tijolo, ferro), broca sobre árvores. *(testado a jogar)*
- [x] **Marco 3c – Jogabilidade:** controlos novos (WASD, `R` roda, `E` mochila/menus, `Q` construção, `Esc`), menu de construção com hotbar, modos criativo e sobrevivência, **obras com material realista e tempo de construção** (o material deposita-se na obra pelo menu; ficam a meio sem material), forno rupestre, menu de **todas** as estruturas (meter ingredientes à mão, tirar produtos), itens no chão (amontoados) e **interface ajustável (100%–300%) com ajuda (`H`)**. *(implementado e com testes automáticos; falta testar a jogar)*
- [x] **Marco 4 – Massa e volume** *(chamado "3c" por uma IA anterior)*: itens com volume e massa (`ItemType`), mochila 20 L / 50 kg, carga da Luna, baú 50 L e contentor 2000 L, menu de inventário, receitas realistas. *(testado a jogar pelo autor)*
- [ ] **Marco 4b – Máquinas e brocas em litros:** buffers de entrada/saída das máquinas e das brocas com limite de volume (em vez de contar unidades), esteiras e brocas com velocidade padrão de coleta por volume, e reequilibrar o ritmo e os custos depois de jogar.
- [ ] **Marco 5 – Personagem:** NPC amigos, hotbar com várias ranhuras/atalhos, mais itens e ferramentas, inventário com mais estrutura. *(O que estava aqui — "recolher à mão, inventário real, construir com custo, meter itens em máquinas à mão" — ficou quase todo feito no 3c.)*
- [ ] **Marco 6 – Camadas:** primeira caverna (`z = 1`), escadas, esteiras verticais, elevadores.
- [ ] **Marco 7 – Mundo vivo:** NPCs, casas com interior, defesas.
- [ ] **Depois:** vários setores, comboios, reinos, exploração espacial, diário da Luna, música, menu de pausa (o `Esc` já não fecha o jogo), guardar/carregar jogo.

### Dívida técnica a ter em conta

- ~~Dividir `main.c` em módulos~~ **Feito** (world / logic / render / main). Verificado sem mudar o comportamento: mesmos testes, e um teste de regressão (30 mundos + 4 cenários com ~1400 construções) com resultado idêntico antes e depois.
- ~~`OreType` como tipo de item~~ **Feito** no 3c (`ItemType` em `items.h`).
- **Ritmo do arranque (3c):** a mão extrai 5 un/s e o rupestre faz 1 tijolo por 12 s a 24 un de matéria-prima cada: o início depende muito de minerar à mão. Se for aborrecido, os remédios são receitas/tempos e não os custos. À espera do teste do autor.
- ~~**Árvores pequenas** e jazidas curtas~~ **Resolvido** (a pedido do autor, depois de testar o 3c e o 4): cada árvore passou de 20–30 para **400–600 un (40–60 L, ~16–24 kg de lenha)** e as jazidas de ~10×: ver a tabela da secção 5. À espera de teste a jogar: se ainda faltar (ou sobrar), mexe em `ORE[]` (`world.c`) e em `TREE_WOOD_MIN/MAX` (`world.h`).
- **Obras de esteira em fila:** como cada esteira é uma obra e o material se deposita pelo menu, uma linha de 20 esteiras pede 20 depósitos (15 madeira cada). Se for aborrecido a jogar, remédios possíveis: esteiras instantâneas, custo muito menor, ou o menu depositar em várias obras de uma vez. À espera do teste do autor.
- **Demolir estruturas grandes em sobrevivência:** `remove_building_at()` exige que a mochila (20 L) aguente **todo** o material devolvido, e o forno (65 L), a carvoeira (55 L) e o contentor (24 L) custam mais do que isso: **nunca se conseguem demolir já prontas** (só obras a meio com pouco depositado). Já era assim antes. Solução possível: o que não cabe fica em amontoados no chão (como já se faz quando o forno rupestre parte, `spill_and_remove()`). Decisão do autor.
- Criar um sistema de **guardar/carregar** cedo (a geração determinística ajuda: basta guardar a seed + as alterações e os objetos).
- `sim_step()` percorre os 2048 objetos todos os passos; quando houver muitos, pensar numa lista só dos ativos.

---

## 9. Notas para quem continua o projeto

- **Quem desenhou o jogo não programa.** O código tem de ser fácil de ler e comentado. Evitar truques e otimizações prematuras.
- Ao acabar uma alteração: compilar com `-Wall -Wextra`, correr `make test`, correr o jogo e **atualizar este README** (secção 2, roadmap, decisões).
- Qualquer nova mecânica deve respeitar a visão da secção 1.
- **Máquinas novas** (ex.: forno nível 2, fábrica de vidro): acrescentar um tipo em `BuildType`/`BUILD[]` (`logic.h`/`logic.c`; **com descrição e custo**, e `life` se partir), a **`BUILDABLE[]`** (para aparecer no menu `Q`), acrescentar a(s) receita(s) a `RECIPES[]` (`logic.c`) com esse `machine`, e adicionar o tipo a `is_machine()` (`logic.c`), a `draw_machine()` e a `draw_build_icon()` (`render.c`). Os testes verificam o custo e o tempo da nova estrutura (e que acaba com gasto exato); os testes dos menus dependem de `BUILDABLE_COUNT`. `accept_item()`, `machine_step()`, a demolição e o HUD já as tratam.
- **Itens novos:** acrescentar a `ItemType` (antes de `ITEM_COUNT`, em `items.h`) e a `ITEM[]` (`items.c`, com volume e densidade). Se o item também existir no chão, acrescentar um `OreType` e uma linha em `ORE[]` (`world.c`) que aponte para o item.
- A mochila é `backpack` (um `Container`); constrói-se com ela (sobrevivência) e passam-se itens à mão para máquinas, baús e chão pelos menus. O sistema de volume e massa é o do Marco 4.
- No ambiente de desenvolvimento das IAs **não há raylib**: a verificação foi feita com `tests/fake/raylib.h` (só com `-DCLAREIRA_TEST`). Compila e testa sempre no PC do autor com a raylib verdadeira antes de dares o marco por fechado.
