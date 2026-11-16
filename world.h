/*
 * world.h  -  O MUNDO: configuracao, terreno, minerios, tiles e geracao.
 * Implementado em world.c.
 *
 * Nota: a tabela TERRAIN[] guarda uma Color (so o TIPO da raylib, nenhuma
 * funcao dela). Por isso este cabecalho inclui raylib.h.
 */
#ifndef WORLD_H
#define WORLD_H

#include "raylib.h"
#include "items.h"
#include <stdint.h>

/* ------------------------------------------------------------------ */
/* Configuracao                                                        */
/* ------------------------------------------------------------------ */

#define MAP_W      160          /* largura do setor em tiles  */
#define MAP_H      120          /* altura do setor em tiles   */
#define MAP_D      1            /* numero de camadas (z)      */
#define TILE_SIZE  16           /* tamanho de um tile em pixels do mundo */

#define DEFAULT_SEED 1234u

/* Madeira de cada arvore do interior do mapa, em unidades de 0,1 L de lenha
 * empilhada (400 a 600 un = 40 a 60 L = ~16 a 24 kg de lenha: uma arvore pequena
 * ou media). Antes eram 20 a 30 un (2 a 3 L), que nem dava para uma esteira. */
#define TREE_WOOD_MIN 400
#define TREE_WOOD_MAX 600

/* Posicoes importantes (em tiles) */
#define CLEARING_X   80
#define CLEARING_Y   55
#define CLEARING_R   14.0f

#define SWAMP_X      143
#define SWAMP_Y      74
#define SWAMP_R      17.0f

#define HILL_X       118        /* encosta rochosa a sudeste (hematite) */
#define HILL_Y       102
#define HILL_R       15.0f

#define QUARTZ_X     28         /* afloramento a noroeste (quartzo/granito) */
#define QUARTZ_Y     24
#define QUARTZ_R     10.0f

/* ------------------------------------------------------------------ */
/* Tipos de terreno                                                    */
/* ------------------------------------------------------------------ */

typedef enum {
    T_CLAREIRA = 0,     /* relva clara, a clareira da Luna        */
    T_RELVA,            /* relva normal                           */
    T_FLORESTA,         /* chao da floresta densa (escuro)        */
    T_ARVORE,           /* arvore (obstaculo)                     */
    T_AGUA,             /* agua do ribeiro                        */
    T_LAMA,             /* margem do ribeiro: argila + areia      */
    T_PANTANO,          /* pantano (enxofre, resina)              */
    T_PEDRA,            /* encosta rochosa                        */
    T_CASA_ARVORE,      /* casa na arvore da Luna e do Gabriel    */
    T_TRILHO,           /* trilho de terra batida (sem arvores)   */
    T_VAU,              /* agua rasa: da para atravessar          */
    T_COUNT
} Terrain;

typedef struct {
    const char *name;
    Color       color;
    int         walkable;   /* 1 = da para andar, 0 = bloqueado */
} TerrainInfo;

extern const TerrainInfo TERRAIN[T_COUNT];

/* ------------------------------------------------------------------ */
/* Jazidas (o que existe no chao)                                      */
/* ------------------------------------------------------------------ */

/* O que existe NO CHAO. O que a jazida da quando se extrai (o ITEM) esta em
 * ORE[].item; nome e cor vem da tabela de itens (items.h). */
typedef enum {
    ORE_NONE = 0,
    ORE_HEMATITE,       /* minerio de ferro (encosta sudeste)   */
    ORE_ARGILA,         /* argila (margens do ribeiro)          */
    ORE_ENXOFRE,        /* enxofre (pantano)                    */
    ORE_QUARTZO,        /* quartzo (afloramento noroeste)       */
    ORE_MADEIRA,        /* madeira (vem das arvores, T_ARVORE)  */
    ORE_AREIA,          /* areia (margens do ribeiro, em lama)  */
    ORE_COUNT
} OreType;

typedef struct {
    int item;           /* ItemType que sai desta jazida               */
    int richest;        /* unidades no centro de uma jazida (maximo)   */
} OreInfo;

extern const OreInfo ORE[ORE_COUNT];

/* ------------------------------------------------------------------ */
/* Mundo                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t terrain;    /* valor de Terrain                              */
    uint8_t shade;      /* variacao de cor (0..7) para o mapa nao ficar  */
                        /* uma mancha chapada                            */
    uint8_t  ore;       /* valor de OreType (ORE_NONE = sem jazida)      */
    uint16_t ore_amount;/* quantidade que ainda resta neste tile         */
} Tile;

extern Tile     world[MAP_D][MAP_H][MAP_W];
extern uint32_t current_seed;

static inline int in_bounds(int x, int y)
{
    return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H;
}

static inline Tile *tile_at(int x, int y, int z)
{
    return &world[z][y][x];
}

/* ---- Fila de tiles a redesenhar ------------------------------------
 * A logica do jogo (que nao usa raylib) so "marca" tiles que mudaram de
 * aspeto; o ciclo principal redesenha-os na textura (flush_dirty, em render.c). */

#define DIRTY_MAX 4096
extern int16_t dirty_x[DIRTY_MAX], dirty_y[DIRTY_MAX];
extern int     dirty_n;
extern int     dirty_overflow;      /* 1 = demasiados: redesenhar o mapa todo */

void mark_dirty(int x, int y);

/* Quantas pedrinhas desenhar (0 a 4) conforme o minerio que resta */
int ore_dots(const Tile *t);

/* ---- Ruido e utilitarios (geracao determinista: nada de rand()) ----- */

uint32_t hash2(int x, int y, uint32_t seed);
float    rnd01(int x, int y, uint32_t seed);
float    dist2d(float x1, float y1, float x2, float y2);
float    clampf(float v, float lo, float hi);

/* ---- Geracao ------------------------------------------------------- */

/* Gera o setor todo (e limpa os objetos construidos e a fila de redesenho) */
void generate_world(uint32_t seed);

#endif
