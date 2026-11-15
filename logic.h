/*
 * logic.h  -  A LOGICA DO JOGO (implementada em logic.c).
 * Itens a andar, objetos construidos, receitas, maquinas, simulacao,
 * extracao, mochila e colisoes da Luna.
 *
 * REGRA: nada aqui usa funcoes da raylib nem input (para se testar sem janela).
 */
#ifndef LOGIC_H
#define LOGIC_H

#include "world.h"

/* ------------------------------------------------------------------ */
/* Objetos construidos                                                 */
/* ------------------------------------------------------------------ */
/* Ficam SEPARADOS do Tile: uma lista de "Building" e uma grelha bgrid que
 * diz, para cada tile, qual e o objeto que o ocupa (indice + 1; 0 = livre).
 * Broca, carvoeira e forno ocupam 2x2 tiles; esteira e bau ocupam 1x1. */

typedef enum {
    B_NONE = 0,         /* posicao livre na lista                  */
    B_BROCA,            /* broca mecanica 2x2                      */
    B_ESTEIRA,          /* esteira 1x1, com direcao                */
    B_BAU,              /* bau 1x1                                 */
    B_CARVOEIRA,        /* carvoeira 2x2: madeira -> carvao        */
    B_FORNO,            /* forno 2x2: tijolos e ferro              */
    B_COUNT
} BuildType;

typedef struct {
    const char *name;
    int         w, h;       /* tamanho em tiles                    */
    int         solid;      /* 1 = a Luna nao passa por cima       */
} BuildInfo;

extern const BuildInfo BUILD[B_COUNT];

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
    uint8_t  item[BELT_CAP];    /* OreType de cada item                */
    float    pos[BELT_CAP];

    /* Broca */
    float    timer;         /* tempo ate a proxima extracao            */
    uint8_t  cursor_out;    /* por onde comecar a tentar entregar      */
    uint8_t  cursor_type;   /* que tipo de item entregar a seguir      */
    uint16_t buf[ORE_COUNT];    /* itens extraidos ainda por entregar  */

    /* Bau */
    uint16_t store[ORE_COUNT];

    /* Maquinas (carvoeira, forno): ingredientes que ja entraram, produtos
     * prontos por entregar, e a receita em curso (0 = nenhuma; senao
     * indice + 1 em RECIPES). Reutilizam cursor_out e cursor_type. */
    uint16_t in[ORE_COUNT];
    uint16_t out[ORE_COUNT];
    float    work;          /* tempo ja gasto na receita em curso      */
    uint8_t  recipe;
} Building;

#define MAX_BUILDINGS 2048

extern Building buildings[MAX_BUILDINGS];
extern uint16_t bgrid[MAP_H][MAP_W];

Building *building_at(int x, int y);
int       building_solid(int x, int y);     /* a Luna nao passa por brocas, baus e maquinas */
void      reset_buildings(void);

/* ------------------------------------------------------------------ */
/* Jogador (Luna): so posicao e colisoes. O teclado esta em main.c     */
/* ------------------------------------------------------------------ */

typedef struct {
    float x, y;     /* posicao dos PES da Luna, em pixels do mundo */
} Player;

#define PLAYER_HALF   4.0f      /* metade da largura da caixa de colisao */
#define PLAYER_SPEED  80.0f     /* pixels por segundo (5 tiles/s)        */
#define PLAYER_RUN    1.8f      /* multiplicador ao correr               */

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

extern int inventory[ORE_COUNT];        /* mochila PROVISORIA: so contagens */

int tile_in_reach(const Player *p, int tx, int ty);
int tile_within(const Player *p, int tx, int ty, float reach);
int extract_from_tile(int tx, int ty, int amount, int *type, int *got);
int mine_tile(int tx, int ty, int amount);

/* ------------------------------------------------------------------ */
/* Construcao e logistica                                              */
/* ------------------------------------------------------------------ */

#define BELT_SPEED      3.0f    /* tiles por segundo                         */
#define ITEM_GAP        0.25f   /* distancia minima entre itens (fracao tile) */
#define DRILL_PERIOD    0.25f   /* segundos entre extracoes da broca          */
#define DRILL_BUF       16      /* itens que a broca guarda se a saida estiver cheia */
#define CHEST_CAP       500     /* PROVISORIO: unidades por bau (Marco 4: massa/volume) */
#define SIM_DT          (1.0f / 60.0f)  /* passo fixo da simulacao           */
#define BUILD_REACH_PX  160.0f  /* alcance para construir (10 tiles)         */

/* ---- Receitas ------------------------------------------------------
 * Cada receita pertence a um tipo de maquina. A maquina escolhe sozinha a
 * primeira receita para a qual ja tem todos os ingredientes. Os numeros
 * (tabela RECIPES em logic.c) sao PROVISORIOS: ajustar depois de jogar. */
#define MACH_IN_CAP   12        /* maximo de cada ingrediente dentro da maquina */
#define MACH_OUT_CAP  16        /* produtos por entregar; cheio = maquina para  */

typedef struct {
    uint8_t     machine;        /* BuildType que faz esta receita           */
    const char *name;
    uint8_t     in_type[3];     /* ingredientes (ORE_NONE = campo nao usado) */
    uint8_t     in_n[3];
    uint8_t     out_type;
    uint8_t     out_n;
    float       period;         /* segundos para fazer um lote              */
} Recipe;

extern const Recipe RECIPES[];
extern const int    RECIPE_COUNT;

int is_machine(int type);
int chest_total(const Building *b);
int drill_total(const Building *b);
int mach_out_total(const Building *b);

/* 0 = a trabalhar, 1 = sem minerio por baixo, 2 = saida cheia (bloqueada) */
int drill_status(const Building *b);
/* 0 = a trabalhar, 1 = parada (faltam ingredientes), 2 = saida cheia */
int machine_status(const Building *b);

int can_place(int type, int x, int y, float px, float py);
int place_building(int type, int x, int y, int dir, float px, float py);
int remove_building_at(int x, int y);

/* Entrega 1 item ao objeto do tile (tx, ty); (dx, dy) = sentido do item.
 * Devolve 1 se foi aceite. */
int accept_item(int tx, int ty, int dx, int dy, int item);

/* Um passo da simulacao (sempre com o mesmo dt, ver SIM_DT) */
void sim_step(float dt);

#endif
