/*
 * logic.h  -  A LOGICA DO JOGO (implementada em logic.c).
 * Itens a andar, objetos construidos, custos e modos de jogo, receitas,
 * maquinas, amontoados no chao, simulacao, extracao, mochila e colisoes.
 *
 * REGRA: nada aqui usa funcoes da raylib nem input (para se testar sem janela).
 *
 * ESCALA: 1 tile = 50 cm. Volumes em mL e massas em gramas (inteiros).
 */
#ifndef LOGIC_H
#define LOGIC_H

#include "world.h"

/* ------------------------------------------------------------------ */
/* Contentores: guardam itens com limite de VOLUME e de MASSA          */
/* ------------------------------------------------------------------ */
/* A mochila, os baus e o contentor usam todos esta mesma estrutura.
 * Dentro so se guardam unidades inteiras de cada item (ver items.h). */

#define BACKPACK_VOL_ML    20000    /* mochila: 20 L                       */
#define BACKPACK_MASS_G    50000    /* mochila: 50 kg (acima nao se apanha) */
#define CHEST_VOL_ML       50000    /* bau: 50 L, massa ilimitada          */
#define CONTAINER_VOL_ML 2000000    /* contentor: 2000 L, massa ilimitada  */

typedef struct {
    int      cap_vol_ml;            /* limite de volume, em mL                    */
    int      cap_mass_g;            /* limite de massa, em g (0 = sem limite)     */
    uint16_t n[ITEM_COUNT];         /* unidades de cada item                      */
} Container;

int  container_vol_ml(const Container *c);      /* volume ocupado, em mL  */
int  container_mass_g(const Container *c);      /* massa total, em g      */
int  container_units(const Container *c);       /* unidades (todas somadas) */
int  container_room(const Container *c, int item);   /* quantas unidades MAIS deste item cabem */
int  container_add(Container *c, int item, int n);   /* mete ate n; devolve quantas meteu */
int  container_take(Container *c, int item, int n);  /* tira ate n; devolve quantas tirou */
int  container_move(Container *from, Container *to, int item, int n);  /* passa ate n; devolve quantas passou */
void container_clear(Container *c);             /* esvazia (mantem os limites) */
/* cabem TODAS estas unidades (add[item]) de uma vez em c? */
int  counts_fit(const Container *c, const int add[ITEM_COUNT]);

/* ------------------------------------------------------------------ */
/* Objetos construidos                                                 */
/* ------------------------------------------------------------------ */
/* Ficam SEPARADOS do Tile: uma lista de "Building" e uma grelha bgrid que
 * diz, para cada tile, qual e o objeto que o ocupa (indice + 1; 0 = livre).
 * Broca, carvoeira, forno e contentor ocupam 2x2 tiles; esteira e bau 1x1. */

typedef enum {
    B_NONE = 0,         /* posicao livre na lista                  */
    B_BROCA,            /* broca mecanica 2x2                      */
    B_ESTEIRA,          /* esteira 1x1, com direcao                */
    B_BAU,              /* bau 1x1                                 */
    B_CARVOEIRA,        /* carvoeira 2x2: madeira -> carvao        */
    B_FORNO,            /* forno 2x2: tijolos e ferro              */
    B_CONTENTOR,        /* contentor 2x2: 2000 L, massa ilimitada  */
    B_FORNO_RUPESTRE,   /* forno de argila 2x2: so tijolos, parte ao fim de 20 */
    B_AMONTOADO,        /* itens largados no chao (nao se constroi: cria-se a largar) */
    B_COUNT
} BuildType;

/* Custo em sobrevivencia: ate COST_SLOTS itens DIFERENTES (item 0 = campo livre).
 * COST_PERCENT e TIME_PERCENT sao dois botoes para afinar o jogo depois de
 * jogar: 100 = os valores da tabela BUILD[]; 50 = metade; 200 = o dobro. */
#define COST_SLOTS   3
#define COST_PERCENT 100        /* escala de TODOS os custos de construcao  */
#define TIME_PERCENT 100        /* escala de TODOS os tempos de obra        */
typedef struct {
    uint8_t  item;          /* ItemType                            */
    uint16_t n;             /* unidades                            */
} Cost;

typedef struct {
    const char *name;
    const char *desc;       /* uma frase para o menu de construcao  */
    int         w, h;       /* tamanho em tiles                    */
    int         solid;      /* 1 = a Luna nao passa por cima       */
    int         store_ml;   /* volume de armazenamento (0 = nao guarda) */
    int         life;       /* lotes que a maquina faz antes de partir (0 = nunca parte) */
    float       build_s;    /* segundos de obra em sobrevivencia (so a Luna a construir) */
    Cost        cost[COST_SLOTS];   /* material gasto, aos poucos, durante a obra */
    int         demo_earth;         /* 1 = ao demolir, TODO o material vira terra (forno rupestre) */
} BuildInfo;

extern const BuildInfo BUILD[B_COUNT];

/* O que aparece no menu de construcao (por ordem). O amontoado nao esta aqui:
 * cria-se a largar itens no chao. */
extern const int BUILDABLE[];
extern const int BUILDABLE_COUNT;

/* ------------------------------------------------------------------ */
/* Modos de jogo                                                       */
/* ------------------------------------------------------------------ */
/* CRIATIVO: construir nao custa nada. SOBREVIVENCIA: construir gasta itens da
 * mochila e demolir devolve-os. A logica arranca em CRIATIVO (para os testes);
 * quem escolhe o modo inicial do jogo e o main.c (DEFAULT_MODE). */
typedef enum { MODE_CRIATIVO = 0, MODE_SOBREVIVENCIA = 1 } GameMode;
extern int game_mode;

/* Custo de um objeto, ja com COST_PERCENT: unidades do item no campo "slot" (0 se o campo esta livre) */
int   cost_units(int type, int slot);
/* Segundos de obra, ja com TIME_PERCENT */
float build_seconds(int type);
/* Volume (mL) e massa (g) de TODO o material do custo: para o jogador saber o que vai carregar */
int   build_cost_vol_ml(int type);
int   build_cost_mass_g(int type);
/* A mochila tem agora TODO o material (em criativo, sempre 1)? So serve para avisos e
 * cores: construir e SEMPRE permitido; a obra fica a meio ate se depositar o material. */
int   can_afford(int type);

/* Direcoes: 0 = direita, 1 = baixo, 2 = esquerda, 3 = cima */
extern const int DIRX[4];
extern const int DIRY[4];

#define BELT_CAP 4          /* itens que cabem num tile de esteira */

/* Uma estrutura unica para todos os tipos (simples de ler); cada tipo usa
 * so a sua parte. */
typedef struct {
    uint8_t  type;          /* BuildType                               */
    uint8_t  dir;           /* direcao (esteira)                       */
    int16_t  x, y;          /* tile do canto superior esquerdo         */

    /* Esteira: itens por ordem; o 0 e o da frente (mais perto do fim).
     * pos vai de 0 (inicio do tile) a 1 (fim do tile).                 */
    uint8_t  n;
    uint8_t  item[BELT_CAP];    /* ItemType de cada item               */
    float    pos[BELT_CAP];

    /* Broca */
    float    timer;         /* tempo ate a proxima extracao            */
    uint8_t  cursor_out;    /* por onde comecar a tentar entregar      */
    uint8_t  cursor_type;   /* que tipo de item entregar a seguir      */
    uint16_t buf[ITEM_COUNT];   /* itens extraidos ainda por entregar  */

    /* Bau e contentor */
    Container store;

    /* Maquinas (carvoeira, forno): ingredientes que ja entraram, produtos
     * prontos por entregar, e a receita em curso (0 = nenhuma; senao
     * indice + 1 em RECIPES). Reutilizam cursor_out e cursor_type. */
    uint16_t in[ITEM_COUNT];
    uint16_t out[ITEM_COUNT];
    float    work;          /* tempo ja gasto na receita em curso      */
    uint8_t  recipe;
    uint16_t made;          /* lotes ja feitos (para a maquina que parte) */

    /* Todos */
    uint8_t  pile_item;     /* amontoado: o item de que e feito          */

    /* Obra (sobrevivencia): a estrutura comeca a 0 e a Luna vai-a construindo.
     * Enquanto build < 1 NAO funciona (nao aceita itens, nao anda, nao trabalha).
     * deposit = material DEPOSITADO na obra em cada campo do custo (o jogador mete-o
     * a mao pelo menu da obra, tecla E). A obra so avanca ate onde o material
     * depositado chega; demolir devolve-o todo. */
    float    build;         /* progresso da obra, 0 a 1 (1 = acabada)   */
    uint16_t deposit[COST_SLOTS];
    uint8_t  site_state;    /* SiteState (so conta enquanto build < 1)  */
    uint32_t site_order;    /* ordem de chegada: a Luna constroi a mais antiga primeiro */

    /* Demolicao (ver demolish_start): a estrutura desaparece aos poucos e o que
     * tinha vira amontoados a volta. Enquanto demo_on a estrutura NAO funciona.
     * demo_all = tudo o que vai sair (fixado no inicio); demo_left = o que ainda
     * nao saiu. demo_p = o que resta da estrutura (1 = inteira, 0 = desfeita). */
    uint8_t  demo_on;
    uint8_t  demo_state;    /* DemoState                                 */
    uint32_t demo_order;    /* ordem de marcacao: a Luna demole a mais antiga primeiro */
    float    demo_p;
    float    demo_secs;     /* duracao total desta demolicao, em segundos */
    uint16_t demo_all[ITEM_COUNT];
    uint16_t demo_left[ITEM_COUNT];

    /* Amontoado de terra: segundos desde que lhe chegou terra (desaparece aos TERRA_LIFE_S) */
    float    decay;
} Building;

/* Estado de uma obra */
typedef enum {
    SITE_WORKING = 0,       /* a Luna esta a construir                  */
    SITE_NO_MATERIAL,       /* parada: falta depositar material         */
    SITE_FAR,               /* parada: a Luna esta longe                */
    SITE_QUEUED             /* a espera: a Luna esta a construir outra  */
} SiteState;

/* Uma obra a meio (ainda nao funciona)? Uma estrutura que esta a ser demolida nao e obra. */
int site_active(const Building *b);

/* ---- Material de uma obra: o jogador DEPOSITA-o pelo menu (tecla E) ------
 * Cada campo do custo e um item; a obra so aceita esses itens, ate ao total que
 * o custo pede. Nada se perde: o que se tira volta para o contentor de origem. */
int site_slot(const Building *b, int item);     /* indice do campo do custo (0 a COST_SLOTS-1), ou -1 se a obra nao usa este item */
int site_need(const Building *b, int item);     /* quantas unidades ainda faltam depositar */
int site_deposit(Building *b, Container *from, int item, int n);   /* mochila -> obra; devolve quantas passou */
/* Material depositado que a obra AINDA NAO usou (o que ja entrou na construcao nao
 * volta a sair, a nao ser demolindo). So esse se pode tirar com site_take_back(). */
int site_free(const Building *b, int item);
int site_take_back(Building *b, Container *to, int item, int n);   /* obra -> mochila; devolve quantas passou */

/* A Luna constroi as obras que estao ao seu alcance (BUILD_REACH_PX), UMA de cada
 * vez (a mais antiga que tenha material), ao ritmo da obra e ate onde o material
 * DEPOSITADO nelas chegar. Nao mexe na mochila. Chamar uma vez por passo da
 * simulacao com a posicao da Luna. Em criativo acaba tudo logo, sem material. */
void construction_step(float px, float py, float dt);

/* ---- DEMOLICAO: o inverso da construcao ---------------------------------
 * Botao direito numa estrutura MARCA-a para demolir (demolish_start). A Luna, ao pe
 * (BUILD_REACH_PX), desfaz uma de cada vez, a mais antiga primeiro (demolition_step).
 * A estrutura desaparece aos poucos e o que tinha sai para AMONTOADOS no chao, a volta:
 *   - o que estava la dentro (esteira, baus, maquinas, broca) sai tal e qual;
 *   - o material que gastou a construir (o que se depositou) volta DEGRADADO:
 *     madeira e tijolo so em parte (ITEM[].salvage_pct), o ferro sai como sucata
 *     (ferro enferrujado, tem de se voltar a fundir) e o resto da MASSA vira terra;
 *   - uma obra a meio devolve o material ainda nao usado tal e qual;
 *   - o forno rupestre (demo_earth) vira so terra.
 * A MASSA conserva-se: massa da construcao = massa do que sai, a menos de 1 unidade de
 * terra (ver demolish_manifest). A terra desaparece do chao TERRA_LIFE_S depois de
 * a ultima chegar, e ainda nao serve para nada. Se nao houver chao livre a volta, a
 * demolicao pára (DEMO_NO_ROOM) sem perder nada. Em criativo e instantanea.
 * O amontoado nao se demole assim: apanha-se (remove_building_at). */
#define DEMOLISH_TIME_PERCENT 50    /* tempo de demolir = esta % do tempo de construir */
#define TERRA_LIFE_S          120.0f /* a terra no chao desaparece passado este tempo */
#define DEMO_SPILL_RINGS      6     /* ate quantos tiles a volta se procura chao livre */

typedef enum {
    DEMO_WORKING = 0,       /* a Luna esta a demolir                      */
    DEMO_NO_ROOM,           /* parada: nao ha chao livre para os amontoados */
    DEMO_FAR,               /* parada: a Luna esta longe                  */
    DEMO_QUEUED             /* a espera: a Luna esta a demolir outra      */
} DemoState;

/* Segundos que demora a demolir (ja com DEMOLISH_TIME_PERCENT e TIME_PERCENT) */
float demolish_seconds(int type);
/* Tudo o que sairia de demolir b agora, por item (give[ITEM_COUNT]). Nao muda nada. */
void  demolish_manifest(const Building *b, int give[ITEM_COUNT]);
/* Marca a estrutura que ocupa (x, y) para demolir. Devolve 1 se marcou, 0 se nao ha
 * estrutura, e um amontoado (esse apanha-se) ou ja esta marcada. */
int   demolish_start(int x, int y);
int   demo_active(const Building *b);
/* A Luna demole uma de cada vez (a mais antiga ao alcance), ao ritmo da tabela. Nao mexe
 * na mochila. Chamar uma vez por passo da simulacao. Em criativo acaba tudo logo. */
void  demolition_step(float px, float py, float dt);

#define MAX_BUILDINGS 2048

extern Building buildings[MAX_BUILDINGS];
extern uint16_t bgrid[MAP_H][MAP_W];

Building *building_at(int x, int y);
int       building_solid(int x, int y);     /* a Luna nao passa por brocas, baus e maquinas */
void      reset_buildings(void);
int       is_storage(int type);             /* bau ou contentor */
int       is_pile(int type);                /* amontoado */
int       has_store(int type);              /* guarda itens em b->store (bau, contentor, amontoado) */

/* ------------------------------------------------------------------ */
/* Jogador (Luna): so posicao e colisoes. O teclado esta em main.c     */
/* ------------------------------------------------------------------ */

typedef struct {
    float x, y;     /* posicao dos PES da Luna, em pixels do mundo */
} Player;

#define PLAYER_HALF   4.0f      /* metade da largura da caixa de colisao */
#define PLAYER_SPEED  80.0f     /* pixels por segundo (5 tiles/s)        */
#define PLAYER_RUN    1.8f      /* multiplicador ao correr               */

/* ---- Carga: o peso da mochila decide como a Luna anda -----------------
 *   ate 15 kg        : normal, pode correr
 *   de 15 a 30 kg    : anda devagar (e nao corre)
 *   de 30 a 50 kg    : consegue levar, mas NAO anda
 *   mais de 50 kg    : nao consegue apanhar (e o limite da mochila)       */
#define CARRY_FREE_G       15000
#define CARRY_SLOW_G       30000
#define CARRY_SLOW_FACTOR  0.6f     /* velocidade a andar devagar          */

/* 0 = leve (corre), 1 = devagar, 2 = sobrecarregada (nao anda) */
int   carry_state(int mass_g);
float carry_speed_factor(int mass_g);       /* 1.0, CARRY_SLOW_FACTOR ou 0.0 */
int   carry_can_run(int mass_g);

int  tile_blocked(int tx, int ty);
int  box_blocked(float cx, float cy);
void player_move(Player *p, float dx, float dy);
void player_spawn(Player *p);

/* ------------------------------------------------------------------ */
/* Extracao a mao                                                      */
/* ------------------------------------------------------------------ */

#define REACH_PX        40.0f   /* alcance da Luna (2.5 tiles)           */
#define MINE_TICK       0.20f   /* segundos entre cada extracao          */
#define MINE_PER_TICK   1       /* unidades extraidas em cada extracao   */

extern Container backpack;              /* a mochila (20 L, 50 kg)         */

int tile_in_reach(const Player *p, int tx, int ty);
int tile_within(const Player *p, int tx, int ty, float reach);
int extract_from_tile(int tx, int ty, int amount, int *type, int *got);
int mine_tile(int tx, int ty, int amount);
int mine_blocked(int tx, int ty);       /* ha minerio mas a mochila nao tem espaco */

/* ------------------------------------------------------------------ */
/* Construcao e logistica                                              */
/* ------------------------------------------------------------------ */

#define BELT_SPEED      3.0f    /* tiles por segundo                         */
#define ITEM_GAP        0.25f   /* distancia minima entre itens (fracao tile) */
#define DRILL_PERIOD    0.25f   /* segundos entre extracoes da broca          */
#define DRILL_BUF       16      /* itens que a broca guarda se a saida estiver cheia */
#define SIM_DT          (1.0f / 60.0f)  /* passo fixo da simulacao           */
#define BUILD_REACH_PX  160.0f  /* alcance para construir (10 tiles)         */

/* ---- Receitas ------------------------------------------------------
 * Cada receita pertence a um tipo de maquina. A maquina escolhe sozinha a
 * primeira receita para a qual ja tem todos os ingredientes. Os numeros
 * (tabela RECIPES em logic.c) conservam a massa; ajustar depois de jogar. */
#define MACH_OUT_CAP  16        /* produtos por entregar; cheio = maquina para  */
#define RUPESTRE_LIFE 20        /* tijolos que o forno rupestre faz antes de partir */

typedef struct {
    uint8_t     machine;        /* BuildType que faz esta receita           */
    const char *name;
    uint8_t     in_type[3];     /* ingredientes (ITEM_NONE = campo nao usado) */
    uint8_t     in_n[3];
    uint8_t     out_type;
    uint8_t     out_n;
    float       period;         /* segundos para fazer um lote              */
} Recipe;

extern const Recipe RECIPES[];
extern const int    RECIPE_COUNT;

int is_machine(int type);
int drill_total(const Building *b);
/* quanto de um ingrediente a maquina aceita guardar: 2 lotes da receita que
 * mais usa esse item (0 = a maquina nao usa este item) */
int machine_in_cap(int machine, int item);
int mach_out_total(const Building *b);

/* ---- Usadas pelos menus das estruturas (E com o rato em cima) ----------
 * Todas movem ate n unidades e devolvem quantas moveram; nada se perde. */
int machine_load(Building *b, Container *from, int item, int n);      /* mochila -> ingredientes da maquina */
int machine_take_in(Building *b, Container *to, int item, int n);     /* ingredientes -> mochila */
int machine_take_out(Building *b, Container *to, int item, int n);    /* produtos prontos -> mochila */
int drill_take(Building *b, Container *to, int item, int n);          /* o que a broca guarda -> mochila */

/* 0 = a trabalhar, 1 = sem minerio por baixo, 2 = saida cheia (bloqueada) */
int drill_status(const Building *b);
/* 0 = a trabalhar, 1 = parada (faltam ingredientes), 2 = saida cheia */
int machine_status(const Building *b);

int can_place(int type, int x, int y, float px, float py);
/* Coloca uma estrutura. Em CRIATIVO fica logo pronta e gratis. Em SOBREVIVENCIA
 * cria uma OBRA (nao precisa de material para a comecar): o jogador deposita o
 * material pelo menu da obra (site_deposit) e a Luna constroi-a aos poucos (ver
 * construction_step); se faltar material, fica a meio e nao funciona.
 * Devolve 1 se algo mudou. */
int place_building(int type, int x, int y, int dir, float px, float py);
/* Demole o objeto em (x, y) e mete tudo o que tinha dentro na mochila (e o material
 * ja gasto na sua construcao). Devolve 1 se demoliu, 0 se la nao
 * havia nada e -1 se a mochila nao tem espaco (volume ou massa) para tudo: nesse
 * caso nada muda. (Numa obra devolve-se o material depositado.) Excecao: num amontoado apanha-se o que couber e ele fica com
 * o resto (devolve -1 enquanto ainda sobrar alguma coisa). */
int remove_building_at(int x, int y);

/* ---- Itens no chao: o AMONTOADO ---------------------------------------
 * Largar itens num tile livre cria um amontoado desse item (200 L, massa
 * ilimitada). Cada amontoado e de UM so item. Nao bloqueia a Luna, mas nao se
 * constroi por cima. As esteiras e brocas nao largam itens nele. Fica vazio ->
 * desaparece. */
#define PILE_VOL_ML 200000

int ground_ok(int x, int y);                    /* tile livre (terreno onde se anda) ou ja com amontoado */
int ground_accepts(int x, int y, int item);     /* da para largar este item neste tile? */
/* Largar ate n unidades de "from" no tile (x, y): cria o amontoado se for
 * preciso. Devolve quantas largou (0 = nao deu). */
int drop_to_ground(Container *from, int x, int y, int item, int n);
/* Apanhar ate n unidades do amontoado b para "to" (apaga-o quando fica vazio) */
int pile_take(Building *b, Container *to, int item, int n);

/* Entrega 1 item ao objeto do tile (tx, ty); (dx, dy) = sentido do item.
 * Devolve 1 se foi aceite. */
int accept_item(int tx, int ty, int dx, int dy, int item);

/* Um passo da simulacao (sempre com o mesmo dt, ver SIM_DT) */
void sim_step(float dt);

#endif
