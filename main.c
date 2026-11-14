/*
 * A CLAREIRA DA LUNA  -  Marco 3a: Extracao automatica
 * ------------------------------------
 * Linguagem: C (C99)   Biblioteca: raylib
 *
 * Novidades do Marco 3a:
 *   - As ARVORES passam a ser mineraveis: dao madeira e, depois de cortadas,
 *     o tile fica "chao da floresta" (da para andar).
 *   - BROCA mecanica (2x2): colocada sobre uma jazida, extrai sozinha.
 *   - ESTEIRAS: levam itens de varios tipos misturados (como no Mindustry).
 *   - BAU: onde a esteira acaba. Guarda os itens (limite provisorio).
 *   - Simulacao em passo fixo (60 vezes por segundo), independente do FPS.
 *
 * O que faz o resto do jogo:
 *   - Gera o setor "A Clareira da Luna" (sempre igual, mesma SEED)
 *   - Desenha o mapa com quadrados coloridos (placeholder de pixel art)
 *   - A Luna (placeholder) anda pelo mapa e colide com arvores, agua e casa
 *   - Camara que segue a Luna, zoom, grelha opcional, info do tile sob o rato
 *   - O terreno e desenhado uma vez para uma textura (rapido mesmo com zoom out)
 *   - Trilhos e vaus (agua rasa) ligam a clareira a todos os recursos
 *   - Jazidas finitas (hematite, argila, enxofre, quartzo) que a Luna extrai
 *     com o rato e que se esgotam
 *
 * Controlos:
 *   W A S D ............. mover a Luna (SHIFT = correr)
 *   1 ................... ferramenta MAO: botao esquerdo (segurar) minera a
 *                         jazida ou a arvore sob o rato (ao alcance)
 *   2 / 3 / 4 ........... ferramenta BROCA / ESTEIRA / BAU: botao esquerdo
 *                         constroi (segurar e arrastar para fazer uma linha)
 *   Q / E ............... rodar a esteira (anti-horario / horario)
 *   Botao direito ....... demolir o que esta sob o rato (devolve os itens)
 *   F ................... pegar tudo do bau sob o rato (para a mochila)
 *   Roda do rato ........ zoom
 *   TAB ................. camara livre (WASD move a camara, botao
 *                         direito/meio arrasta o mapa) / voltar a seguir a Luna
 *   G ................... mostrar/esconder grelha
 *   R ................... gerar outra versao do mapa (muda a seed)
 *   F11 ................. ecra inteiro
 *
 * Nota para o futuro: o mundo ja e guardado como world[z][y][x].
 * Hoje so existe a camada z = 0 (superficie). As cavernas serao z = 1, 2, ...
 */

#include "raylib.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Configuracao                                                        */
/* ------------------------------------------------------------------ */

#define MAP_W      160          /* largura do setor em tiles  */
#define MAP_H      120          /* altura do setor em tiles   */
#define MAP_D      1            /* numero de camadas (z)      */
#define TILE_SIZE  16           /* tamanho de um tile em pixels do mundo */

#define DEFAULT_SEED 1234u

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

static const TerrainInfo TERRAIN[T_COUNT] = {
    [T_CLAREIRA]    = { "Clareira",          { 134, 190, 100, 255 }, 1 },
    [T_RELVA]       = { "Relva",             {  96, 156,  78, 255 }, 1 },
    [T_FLORESTA]    = { "Chao da floresta",  {  52,  92,  70, 255 }, 1 },
    [T_ARVORE]      = { "Arvore",            {  52,  92,  70, 255 }, 0 },
    [T_AGUA]        = { "Agua do ribeiro",   {  66, 135, 205, 255 }, 0 },
    [T_LAMA]        = { "Lama (argila)",     { 150, 112,  76, 255 }, 1 },
    [T_PANTANO]     = { "Pantano",           {  88, 108,  58, 255 }, 1 },
    [T_PEDRA]       = { "Encosta rochosa",   { 128, 124, 122, 255 }, 1 },
    [T_CASA_ARVORE] = { "Casa na arvore",    { 150,  96,  52, 255 }, 0 },
    [T_TRILHO]      = { "Trilho",            { 176, 150, 106, 255 }, 1 },
    [T_VAU]         = { "Vau (agua rasa)",   { 118, 174, 226, 255 }, 1 },
};

/* ------------------------------------------------------------------ */
/* Minerios / jazidas                                                  */
/* ------------------------------------------------------------------ */

typedef enum {
    ORE_NONE = 0,
    ORE_HEMATITE,       /* minerio de ferro (encosta sudeste)   */
    ORE_ARGILA,         /* argila (margens do ribeiro)          */
    ORE_ENXOFRE,        /* enxofre (pantano)                    */
    ORE_QUARTZO,        /* quartzo (afloramento noroeste)       */
    ORE_MADEIRA,        /* madeira (vem das arvores, T_ARVORE)  */
    ORE_COUNT
} OreType;

typedef struct {
    const char *name;
    Color       color;      /* cor das pedrinhas desenhadas no chao        */
    int         richest;    /* quantidade no centro de uma jazida (maximo) */
} OreInfo;

/* ATENCAO: valores baixos de proposito, para se ver uma jazida esgotar
 * com a extracao a mao. Quando houver maquinas (Marco 3), aumentar.      */
static const OreInfo ORE[ORE_COUNT] = {
    [ORE_NONE]     = { "-",        {   0,   0,   0, 255 },   0 },
    [ORE_HEMATITE] = { "Hematite", { 176,  70,  52, 255 }, 120 },
    [ORE_ARGILA]   = { "Argila",   { 222, 140, 100, 255 }, 150 },
    [ORE_ENXOFRE]  = { "Enxofre",  { 232, 208,  64, 255 },  80 },
    [ORE_QUARTZO]  = { "Quartzo",  { 232, 240, 247, 255 },  60 },
    [ORE_MADEIRA]  = { "Madeira",  { 138,  92,  48, 255 },  30 },   /* por arvore */
};

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

static Tile     world[MAP_D][MAP_H][MAP_W];
static uint32_t current_seed = DEFAULT_SEED;

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
 * aspeto; o ciclo principal redesenha-os na textura (flush_dirty). */

#define DIRTY_MAX 4096
static int16_t dirty_x[DIRTY_MAX], dirty_y[DIRTY_MAX];
static int     dirty_n;
static int     dirty_overflow;      /* 1 = demasiados: redesenhar o mapa todo */

static void mark_dirty(int x, int y)
{
    if (dirty_n < DIRTY_MAX) {
        dirty_x[dirty_n] = (int16_t)x;
        dirty_y[dirty_n] = (int16_t)y;
        dirty_n++;
    } else {
        dirty_overflow = 1;
    }
}

/* ---- Objetos construidos (Marco 3a) --------------------------------
 * Ficam SEPARADOS do Tile: uma lista de "Building" e uma grelha bgrid que
 * diz, para cada tile, qual e o objeto que o ocupa (indice + 1; 0 = livre).
 * A broca ocupa 2x2 tiles; esteira e bau ocupam 1x1. */

typedef enum {
    B_NONE = 0,         /* posicao livre na lista                  */
    B_BROCA,            /* broca mecanica 2x2                      */
    B_ESTEIRA,          /* esteira 1x1, com direcao                */
    B_BAU,              /* bau 1x1                                 */
    B_COUNT
} BuildType;

typedef struct {
    const char *name;
    int         w, h;       /* tamanho em tiles                    */
    int         solid;      /* 1 = a Luna nao passa por cima       */
} BuildInfo;

static const BuildInfo BUILD[B_COUNT] = {
    [B_NONE]    = { "-",              1, 1, 0 },
    [B_BROCA]   = { "Broca mecanica", 2, 2, 1 },
    [B_ESTEIRA] = { "Esteira",        1, 1, 0 },
    [B_BAU]     = { "Bau",            1, 1, 1 },
};

/* Direcoes: 0 = direita, 1 = baixo, 2 = esquerda, 3 = cima */
static const int DIRX[4] = { 1, 0, -1, 0 };
static const int DIRY[4] = { 0, 1, 0, -1 };

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
} Building;

#define MAX_BUILDINGS 2048

static Building buildings[MAX_BUILDINGS];
static uint16_t bgrid[MAP_H][MAP_W];

static Building *building_at(int x, int y)
{
    if (!in_bounds(x, y) || bgrid[y][x] == 0) return NULL;
    return &buildings[bgrid[y][x] - 1];
}

/* A Luna nao passa por brocas e baus (esteiras sim). */
static int building_solid(int x, int y)
{
    const Building *b = building_at(x, y);
    return b != NULL && BUILD[b->type].solid;
}

static void reset_buildings(void)
{
    memset(buildings, 0, sizeof buildings);
    memset(bgrid, 0, sizeof bgrid);
}

/* ------------------------------------------------------------------ */
/* Ruido (value noise) - para o terreno parecer natural                */
/* ------------------------------------------------------------------ */

static uint32_t hash2(int x, int y, uint32_t seed)
{
    uint32_t h = (uint32_t)x * 374761393u
               + (uint32_t)y * 668265263u
               + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

static float rnd01(int x, int y, uint32_t seed)
{
    return (float)(hash2(x, y, seed) & 0xFFFFu) / 65535.0f;
}

static float smoothstep01(float t)
{
    return t * t * (3.0f - 2.0f * t);
}

static float lerpf(float a, float b, float t)
{
    return a + (b - a) * t;
}

static float value_noise(float x, float y, uint32_t seed)
{
    int   xi = (int)floorf(x);
    int   yi = (int)floorf(y);
    float fx = smoothstep01(x - (float)xi);
    float fy = smoothstep01(y - (float)yi);

    float a = rnd01(xi,     yi,     seed);
    float b = rnd01(xi + 1, yi,     seed);
    float c = rnd01(xi,     yi + 1, seed);
    float d = rnd01(xi + 1, yi + 1, seed);

    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}

/* Varias camadas de ruido somadas. Devolve valor entre 0 e 1. */
static float fbm(float x, float y, uint32_t seed)
{
    float sum = 0.0f, amp = 0.5f, norm = 0.0f;
    for (int i = 0; i < 4; i++) {
        sum  += amp * value_noise(x, y, seed + (uint32_t)i * 101u);
        norm += amp;
        x *= 2.0f;
        y *= 2.0f;
        amp *= 0.5f;
    }
    return sum / norm;
}

static float dist2d(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2, dy = y1 - y2;
    return sqrtf(dx * dx + dy * dy);
}

static float clampf(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

/* ------------------------------------------------------------------ */
/* Geracao do setor                                                    */
/* ------------------------------------------------------------------ */

/* Altura (y) do centro do ribeiro numa dada coluna x. */
static float river_center_y(int x, uint32_t seed)
{
    return 80.0f
         + 7.0f * sinf((float)x * 0.065f)
         + (fbm((float)x * 0.04f, 7.7f, seed + 5u) - 0.5f) * 14.0f;
}

static Terrain generate_tile(int x, int y, uint32_t seed)
{
    float fx = (float)x, fy = (float)y;

    /* 1. Bordo do mapa: parede de arvores (floresta que engole a luz) */
    int edge = x < 3 || y < 3 || x >= MAP_W - 3 || y >= MAP_H - 3;
    if (edge) return T_ARVORE;

    /* 2. Casa na arvore, no centro da clareira */
    if (x == CLEARING_X && y == CLEARING_Y) return T_CASA_ARVORE;

    /* 3. Ribeiro, a sul da clareira, a correr para o pantano */
    float river_dist = fabsf(fy - river_center_y(x, seed));
    if (x < SWAMP_X - 3) {
        if (river_dist < 1.7f) return T_AGUA;
        if (river_dist < 4.2f + 1.5f * fbm(fx * 0.3f, fy * 0.3f, seed + 9u))
            return T_LAMA;
    }

    /* 4. Pantano a leste, com poças de agua */
    float swamp_d = dist2d(fx, fy, SWAMP_X, SWAMP_Y)
                  + (fbm(fx * 0.12f, fy * 0.12f, seed + 21u) - 0.5f) * 14.0f;
    if (swamp_d < SWAMP_R) {
        if (fbm(fx * 0.25f, fy * 0.25f, seed + 33u) > 0.66f) return T_AGUA;
        return T_PANTANO;
    }

    /* 5. Encosta rochosa a sudeste e afloramento a noroeste */
    float hill_d = dist2d(fx, fy, HILL_X, HILL_Y)
                 + (fbm(fx * 0.15f, fy * 0.15f, seed + 41u) - 0.5f) * 12.0f;
    if (hill_d < HILL_R) return T_PEDRA;

    float quartz_d = dist2d(fx, fy, QUARTZ_X, QUARTZ_Y)
                   + (fbm(fx * 0.18f, fy * 0.18f, seed + 47u) - 0.5f) * 8.0f;
    if (quartz_d < QUARTZ_R) return T_PEDRA;

    /* 6. A clareira: relva clara, sem arvores */
    float clear_d = dist2d(fx, fy, CLEARING_X, CLEARING_Y)
                  + (fbm(fx * 0.2f, fy * 0.2f, seed + 55u) - 0.5f) * 8.0f;
    if (clear_d < CLEARING_R) return T_CLAREIRA;

    /* 7. Floresta: quanto mais longe da clareira, mais densa */
    float d_from_clearing = dist2d(fx, fy, CLEARING_X, CLEARING_Y);
    float density = clampf((d_from_clearing - CLEARING_R) / 40.0f, 0.0f, 1.0f);
    density = 0.10f + 0.55f * density;

    /* manchas de floresta mais cerrada, dadas pelo ruido */
    density += (fbm(fx * 0.08f, fy * 0.08f, seed + 63u) - 0.5f) * 0.5f;

    if (rnd01(x, y, seed + 71u) < density) return T_ARVORE;

    /* sem arvore: relva perto da clareira, chao escuro mais longe */
    if (d_from_clearing < CLEARING_R + 14.0f) return T_RELVA;
    return T_FLORESTA;
}

/* ---- Trilhos -------------------------------------------------------
 * A floresta e densa e o ribeiro corta o mapa, por isso abrimos trilhos
 * da clareira ate cada zona de recursos. Onde um trilho cruza agua,
 * a agua passa a "vau" (rasa), que se pode atravessar. */

static void carve_brush(int cx, int cy, float radius)
{
    int r = (int)ceilf(radius);
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            if (x < 3 || y < 3 || x >= MAP_W - 3 || y >= MAP_H - 3) continue;
            if (dist2d((float)x, (float)y, (float)cx, (float)cy) > radius) continue;

            Tile *t = tile_at(x, y, 0);
            switch (t->terrain) {
                case T_ARVORE:      /* a arvore abatida deixa de dar madeira */
                    t->ore = ORE_NONE;
                    t->ore_amount = 0;
                    t->terrain = T_TRILHO;
                    break;
                case T_FLORESTA:
                case T_RELVA:
                    t->terrain = T_TRILHO;
                    break;
                case T_AGUA:
                    t->terrain = T_VAU;
                    break;
                default:    /* clareira, lama, pantano, pedra, casa, etc.: ficam */
                    break;
            }
        }
    }
}

static void carve_trail(const int pts[][2], int n, uint32_t seed)
{
    for (int i = 0; i + 1 < n; i++) {
        float x0 = (float)pts[i][0],     y0 = (float)pts[i][1];
        float x1 = (float)pts[i + 1][0], y1 = (float)pts[i + 1][1];
        float dx = x1 - x0, dy = y1 - y0;
        float len = sqrtf(dx * dx + dy * dy);
        float nx = -dy / len, ny = dx / len;        /* perpendicular */
        int steps = (int)(len * 2.0f) + 1;

        for (int s = 0; s <= steps; s++) {
            float t  = (float)s / (float)steps;
            float px = x0 + dx * t;
            float py = y0 + dy * t;
            /* o trilho serpenteia um pouco, mas chega sempre ao destino */
            float wob = (fbm(px * 0.12f, py * 0.12f, seed + 77u) - 0.5f)
                      * 6.0f * sinf(3.14159265f * t);
            carve_brush((int)lroundf(px + nx * wob), (int)lroundf(py + ny * wob), 1.4f);
        }
    }
}

static void carve_trails(uint32_t seed)
{
    /* da clareira para a encosta de hematite (a sul, atravessa o ribeiro) */
    static const int to_hill[][2]   = { {80,55}, {86,68}, {94,84}, {106,96}, {118,102} };
    /* da clareira para o afloramento de quartzo (noroeste) */
    static const int to_quartz[][2] = { {80,55}, {62,42}, {44,32}, {30,25} };
    /* da clareira para o pantano (leste) */
    static const int to_swamp[][2]  = { {80,55}, {100,58}, {120,66}, {136,74} };
    /* da clareira para a margem norte do ribeiro (argila) */
    static const int to_river[][2]  = { {80,55}, {76,66}, {70,74} };

    carve_trail(to_hill,   5, seed);
    carve_trail(to_quartz, 4, seed);
    carve_trail(to_swamp,  4, seed);
    carve_trail(to_river,  3, seed);
}

/* ---- Jazidas -------------------------------------------------------
 * Uma jazida e uma mancha: quanto mais perto do centro, mais minerio.
 * So se coloca sobre o terreno certo (ex.: hematite so em pedra). */

static void place_patch(OreType ore, int cx, int cy, float radius,
                        Terrain on, uint32_t seed)
{
    int r = (int)ceilf(radius) + 3;
    for (int y = cy - r; y <= cy + r; y++) {
        for (int x = cx - r; x <= cx + r; x++) {
            if (!in_bounds(x, y)) continue;
            Tile *t = tile_at(x, y, 0);
            if (t->terrain != on || t->ore != ORE_NONE) continue;

            /* distancia com ruido, para a mancha ter forma irregular */
            float d = dist2d((float)x, (float)y, (float)cx, (float)cy)
                    + (fbm((float)x * 0.35f, (float)y * 0.35f, seed + (uint32_t)ore * 7u + 200u) - 0.5f) * 3.0f;
            if (d < 0.0f) d = 0.0f;
            if (d >= radius) continue;

            float richness = 0.25f + 0.75f * (1.0f - d / radius);
            float jitter   = 0.8f + 0.4f * rnd01(x, y, seed + 300u);
            int   amount   = (int)((float)ORE[ore].richest * richness * jitter);
            if (amount < 5) amount = 5;

            t->ore        = (uint8_t)ore;
            t->ore_amount = (uint16_t)amount;
        }
    }
}

static void generate_ores(uint32_t seed)
{
    /* Hematite: encosta rochosa a sudeste */
    place_patch(ORE_HEMATITE, 113, 100, 6.0f, T_PEDRA, seed);
    place_patch(ORE_HEMATITE, 124, 107, 5.0f, T_PEDRA, seed);

    /* Quartzo: afloramento a noroeste */
    place_patch(ORE_QUARTZO, 27, 23, 4.5f, T_PEDRA, seed);
    place_patch(ORE_QUARTZO, 33, 28, 3.0f, T_PEDRA, seed);

    /* Enxofre: pantano a leste */
    place_patch(ORE_ENXOFRE, 137, 76, 4.5f, T_PANTANO, seed);
    place_patch(ORE_ENXOFRE, 149, 68, 3.5f, T_PANTANO, seed);

    /* Argila: lama da margem norte do ribeiro (o ribeiro muda com a seed) */
    static const int clay_x[] = { 50, 70, 90, 118 };
    for (int i = 0; i < 4; i++) {
        int cy = (int)(river_center_y(clay_x[i], seed) - 3.0f);
        place_patch(ORE_ARGILA, clay_x[i], cy, 3.5f, T_LAMA, seed);
    }
}

/* Garante que nao existe minerio onde a Luna nao consegue chegar (por exemplo,
 * uma poca de agua no pantano a isolar alguns tiles). Faz um "flood fill" a
 * partir do ponto de partida e apaga as jazidas fora da zona alcancavel. */
static uint8_t reach_map[MAP_H][MAP_W];

static void remove_unreachable_ore(void)
{
    static int qx[MAP_W * MAP_H], qy[MAP_W * MAP_H];
    int head = 0, tail = 0;

    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            reach_map[y][x] = 0;

    int sx = CLEARING_X + 2, sy = CLEARING_Y + 2;      /* onde a Luna nasce */
    reach_map[sy][sx] = 1;
    qx[tail] = sx; qy[tail] = sy; tail++;

    while (head < tail) {
        int x = qx[head], y = qy[head]; head++;
        static const int d[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
        for (int i = 0; i < 4; i++) {
            int nx = x + d[i][0], ny = y + d[i][1];
            if (!in_bounds(nx, ny) || reach_map[ny][nx]) continue;
            if (!TERRAIN[tile_at(nx, ny, 0)->terrain].walkable) continue;
            reach_map[ny][nx] = 1;
            qx[tail] = nx; qy[tail] = ny; tail++;
        }
    }

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (!reach_map[y][x]) {
                Tile *t = tile_at(x, y, 0);
                /* Arvores nunca sao "alcancaveis a pe" (bloqueiam), mas
                 * cortam-se a partir do tile do lado: ficam como estao. */
                if (t->terrain == T_ARVORE) continue;
                t->ore = ORE_NONE;
                t->ore_amount = 0;
            }
        }
    }
}

static void generate_world(uint32_t seed)
{
    current_seed = seed;
    reset_buildings();
    dirty_n = 0;
    dirty_overflow = 0;
    for (int z = 0; z < MAP_D; z++) {
        for (int y = 0; y < MAP_H; y++) {
            for (int x = 0; x < MAP_W; x++) {
                /* Atribuir o Tile inteiro limpa tambem a jazida anterior */
                Tile t = (Tile){
                    .terrain    = (uint8_t)generate_tile(x, y, seed),
                    .shade      = (uint8_t)(hash2(x, y, seed + 99u) & 7u),
                    .ore        = ORE_NONE,
                    .ore_amount = 0
                };
                /* Cada arvore (menos a parede do bordo do mapa) tem madeira
                 * (20 a 30 unidades). Quando acaba, a arvore desaparece. */
                int interior = x >= 3 && y >= 3 && x < MAP_W - 3 && y < MAP_H - 3;
                if (t.terrain == T_ARVORE && interior) {
                    t.ore        = ORE_MADEIRA;
                    t.ore_amount = (uint16_t)(20u + hash2(x, y, seed + 123u) % 11u);
                }
                *tile_at(x, y, z) = t;
            }
        }
    }
    carve_trails(seed);
    generate_ores(seed);
    remove_unreachable_ore();
}

/* ------------------------------------------------------------------ */
/* Desenho                                                             */
/* ------------------------------------------------------------------ */

static Color shaded(Color c, int shade)
{
    int delta = (shade - 4) * 3;    /* -12 .. +9 */
    int r = c.r + delta, g = c.g + delta, b = c.b + delta;
    r = r < 0 ? 0 : (r > 255 ? 255 : r);
    g = g < 0 ? 0 : (g > 255 ? 255 : g);
    b = b < 0 ? 0 : (b > 255 ? 255 : b);
    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
}

/* Quantas pedrinhas desenhar (0 a 4) conforme o minerio que resta */
static int ore_dots(const Tile *t)
{
    if (t->ore == ORE_NONE || t->ore_amount == 0) return 0;
    if (t->terrain == T_ARVORE) return 0;       /* madeira nao se desenha como pedrinhas */
    float ratio = (float)t->ore_amount / (float)ORE[t->ore].richest;
    if (ratio > 1.0f) ratio = 1.0f;
    return 1 + (int)(ratio * 3.99f);
}

static void draw_tile(int x, int y, const Tile *t)
{
    const TerrainInfo *info = &TERRAIN[t->terrain];
    int px = x * TILE_SIZE;
    int py = y * TILE_SIZE;

    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, shaded(info->color, t->shade));

    /* Jazida: 1 a 4 pedrinhas, quantas mais, mais minerio resta no tile */
    if (t->ore != ORE_NONE) {
        static const int ox[4] = { 2, 9, 2, 9 };
        static const int oy[4] = { 2, 3, 9, 10 };
        int n = ore_dots(t);
        for (int i = 0; i < n; i++) {
            int jx = (int)(hash2(x, y, 31u + (uint32_t)i * 2u) % 3u) - 1;
            int jy = (int)(hash2(x, y, 32u + (uint32_t)i * 2u) % 3u) - 1;
            DrawRectangle(px + ox[i] + jx, py + oy[i] + jy, 5, 4,
                          shaded(ORE[t->ore].color, (x + y + i * 3) & 7));
        }
    }

    /* Pequenos detalhes de placeholder, so para ler o mapa mais facilmente */
    if (t->terrain == T_ARVORE) {
        /* a copa encolhe a medida que se corta madeira */
        float size = 0.45f;
        if (t->ore == ORE_MADEIRA) {
            float ratio = (float)t->ore_amount / (float)ORE[ORE_MADEIRA].richest;
            if (ratio > 1.0f) ratio = 1.0f;
            size = 0.25f + 0.20f * ratio;
        }
        DrawCircle(px + TILE_SIZE / 2, py + TILE_SIZE / 2, TILE_SIZE * size,
                   shaded((Color){ 30, 110, 60, 255 }, t->shade));
    } else if (t->terrain == T_CASA_ARVORE) {
        DrawRectangle(px + 3, py + 3, TILE_SIZE - 6, TILE_SIZE - 6,
                      (Color){ 110, 66, 34, 255 });
        DrawRectangle(px + 6, py + 7, 4, 6, (Color){ 240, 220, 120, 255 });
    }
}

/* O terreno e desenhado UMA vez para esta textura. Depois, em cada frame, so
 * a mostramos (1 chamada de desenho em vez de milhares). */
static RenderTexture2D terrain_tex;

static void build_terrain_texture(void)
{
    const int z = 0;    /* camada visivel (superficie) */

    BeginTextureMode(terrain_tex);
    ClearBackground(BLACK);
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            draw_tile(x, y, tile_at(x, y, z));
    EndTextureMode();
}

/* Redesenha todos os tiles marcados com mark_dirty() (de uma so vez) */
static void flush_dirty(void)
{
    if (dirty_overflow) {
        build_terrain_texture();
    } else if (dirty_n > 0) {
        BeginTextureMode(terrain_tex);
        for (int i = 0; i < dirty_n; i++)
            draw_tile(dirty_x[i], dirty_y[i], tile_at(dirty_x[i], dirty_y[i], 0));
        EndTextureMode();
    }
    dirty_n = 0;
    dirty_overflow = 0;
}

static void draw_world(Camera2D cam, int screen_w, int screen_h, int show_grid)
{
    /* As texturas de render ficam invertidas na vertical, dai o "-" na altura */
    Rectangle src = { 0, 0,
                      (float)terrain_tex.texture.width,
                      -(float)terrain_tex.texture.height };
    DrawTextureRec(terrain_tex.texture, src, (Vector2){ 0, 0 }, WHITE);

    if (show_grid && cam.zoom >= 2.0f) {
        Vector2 tl = GetScreenToWorld2D((Vector2){ 0, 0 }, cam);
        Vector2 br = GetScreenToWorld2D((Vector2){ (float)screen_w, (float)screen_h }, cam);

        int x0 = (int)floorf(tl.x / TILE_SIZE);
        int y0 = (int)floorf(tl.y / TILE_SIZE);
        int x1 = (int)floorf(br.x / TILE_SIZE);
        int y1 = (int)floorf(br.y / TILE_SIZE);
        if (x0 < 0) x0 = 0;
        if (y0 < 0) y0 = 0;
        if (x1 >= MAP_W) x1 = MAP_W - 1;
        if (y1 >= MAP_H) y1 = MAP_H - 1;

        Color g = { 0, 0, 0, 50 };
        for (int y = y0; y <= y1 + 1; y++)
            DrawLine(x0 * TILE_SIZE, y * TILE_SIZE, (x1 + 1) * TILE_SIZE, y * TILE_SIZE, g);
        for (int x = x0; x <= x1 + 1; x++)
            DrawLine(x * TILE_SIZE, y0 * TILE_SIZE, x * TILE_SIZE, (y1 + 1) * TILE_SIZE, g);
    }
}

/* ------------------------------------------------------------------ */
/* Jogador (Luna)                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    float x, y;     /* posicao dos PES da Luna, em pixels do mundo */
} Player;

#define PLAYER_HALF   4.0f      /* metade da largura da caixa de colisao */
#define PLAYER_SPEED  80.0f     /* pixels por segundo (5 tiles/s)        */
#define PLAYER_RUN    1.8f      /* multiplicador ao correr               */

static int tile_blocked(int tx, int ty)
{
    if (!in_bounds(tx, ty)) return 1;
    if (building_solid(tx, ty)) return 1;
    return !TERRAIN[tile_at(tx, ty, 0)->terrain].walkable;
}

/* A caixa de colisao centrada em (cx, cy) bate em algum tile bloqueado? */
static int box_blocked(float cx, float cy)
{
    int x0 = (int)floorf((cx - PLAYER_HALF) / TILE_SIZE);
    int x1 = (int)floorf((cx + PLAYER_HALF - 0.01f) / TILE_SIZE);
    int y0 = (int)floorf((cy - PLAYER_HALF) / TILE_SIZE);
    int y1 = (int)floorf((cy + PLAYER_HALF - 0.01f) / TILE_SIZE);

    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (tile_blocked(x, y)) return 1;
    return 0;
}

/* Move em X e em Y separadamente, para a Luna "deslizar" ao longo das paredes */
static void player_move(Player *p, float dx, float dy)
{
    if (!box_blocked(p->x + dx, p->y)) p->x += dx;
    if (!box_blocked(p->x, p->y + dy)) p->y += dy;
}

static void player_spawn(Player *p)
{
    p->x = (CLEARING_X + 2) * TILE_SIZE + TILE_SIZE / 2.0f;
    p->y = (CLEARING_Y + 2) * TILE_SIZE + TILE_SIZE / 2.0f;
}

static void update_player(Player *p, float dt)
{
    float dx = 0.0f, dy = 0.0f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dy -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dy += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dx -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dx += 1.0f;

    if (dx == 0.0f && dy == 0.0f) return;

    /* Na diagonal nao andar mais depressa */
    float len = sqrtf(dx * dx + dy * dy);
    dx /= len;
    dy /= len;

    float speed = PLAYER_SPEED;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) speed *= PLAYER_RUN;

    player_move(p, dx * speed * dt, dy * speed * dt);
}

/* Luna placeholder: vestido azul-celeste, cabelo escuro, pingente topazio */
static void draw_player(const Player *p)
{
    int px = (int)p->x;
    int py = (int)p->y;
    Color hair = { 40, 28, 40, 255 };

    DrawEllipse(px, py + 1, 5, 2, (Color){ 0, 0, 0, 70 });              /* sombra   */
    DrawRectangle(px - 4, py - 8, 8, 9, (Color){ 120, 185, 235, 255 }); /* vestido  */
    DrawRectangle(px - 1, py - 4, 2, 2, (Color){ 235, 190, 60, 255 });  /* topazio  */
    DrawRectangle(px - 3, py - 14, 6, 6, (Color){ 235, 200, 170, 255 });/* cara     */
    DrawRectangle(px - 4, py - 15, 8, 3, hair);                         /* cabelo   */
    DrawRectangle(px - 4, py - 12, 1, 4, hair);
    DrawRectangle(px + 3, py - 12, 1, 4, hair);
}

/* ------------------------------------------------------------------ */
/* Extracao a mao                                                      */
/* ------------------------------------------------------------------ */

#define REACH_PX        40.0f   /* alcance da Luna (2.5 tiles)           */
#define MINE_TICK       0.20f   /* segundos entre cada extracao          */
#define MINE_PER_TICK   1       /* unidades extraidas em cada extracao   */

/* Inventario PROVISORIO: so contagens. Em breve passa a ter massa e
 * volume (Marco 4). */
static int   inventory[ORE_COUNT];
static float mine_timer;
static int   is_mining;

static int tile_in_reach(const Player *p, int tx, int ty)
{
    float cx = (float)tx * TILE_SIZE + TILE_SIZE / 2.0f;
    float cy = (float)ty * TILE_SIZE + TILE_SIZE / 2.0f;
    return dist2d(p->x, p->y - 4.0f, cx, cy) <= REACH_PX;
}

/* Tira ate "amount" unidades da jazida (ou da arvore) do tile. NAO mexe no
 * inventario: diz o que saiu em *type e *got, e quem chamou decide para onde
 * vai (a Luna mete na mochila, a broca mete no seu buffer).
 * Devolve 1 se o aspeto do tile mudou (e portanto precisa de ser redesenhado).
 * Sem raylib nem rato, para poder ser testada sem abrir uma janela. */
static int extract_from_tile(int tx, int ty, int amount, int *type, int *got)
{
    Tile *t = tile_at(tx, ty, 0);
    *type = ORE_NONE;
    *got  = 0;
    if (t->ore == ORE_NONE) return 0;

    int dots_before = ore_dots(t);
    int take = amount < (int)t->ore_amount ? amount : (int)t->ore_amount;
    int changed = 0;

    *type = t->ore;
    *got  = take;
    t->ore_amount = (uint16_t)(t->ore_amount - take);

    if (t->ore_amount == 0) {                       /* esgotado */
        t->ore = ORE_NONE;
        if (t->terrain == T_ARVORE) t->terrain = T_FLORESTA;   /* arvore cortada */
        changed = 1;
    }
    if (t->terrain == T_ARVORE) changed = 1;        /* a copa encolhe a cada corte */

    return changed || ore_dots(t) != dots_before;
}

/* Extracao a mao: o que sai vai direto para a mochila */
static int mine_tile(int tx, int ty, int amount)
{
    int type, got;
    int changed = extract_from_tile(tx, ty, amount, &type, &got);
    if (got > 0) inventory[type] += got;
    return changed;
}

static void update_mining(const Player *p, Camera2D cam, float dt)
{
    is_mining = 0;

    Vector2 w = GetScreenToWorld2D(GetMousePosition(), cam);
    int tx = (int)floorf(w.x / TILE_SIZE);
    int ty = (int)floorf(w.y / TILE_SIZE);

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) || !in_bounds(tx, ty) ||
        tile_at(tx, ty, 0)->ore == ORE_NONE || !tile_in_reach(p, tx, ty)) {
        mine_timer = 0.0f;
        return;
    }

    is_mining = 1;
    mine_timer += dt;
    while (mine_timer >= MINE_TICK) {
        mine_timer -= MINE_TICK;
        if (mine_tile(tx, ty, MINE_PER_TICK)) mark_dirty(tx, ty);
        if (tile_at(tx, ty, 0)->ore == ORE_NONE) { mine_timer = 0.0f; break; }
    }
}

/* ------------------------------------------------------------------ */
/* Construcao e logistica (Marco 3a)                                   */
/* ------------------------------------------------------------------ */

#define BELT_SPEED      3.0f    /* tiles por segundo                         */
#define ITEM_GAP        0.25f   /* distancia minima entre itens (fracao tile) */
#define DRILL_PERIOD    0.25f   /* segundos entre extracoes da broca          */
#define DRILL_BUF       16      /* itens que a broca guarda se a saida estiver cheia */
#define CHEST_CAP       500     /* PROVISORIO: unidades por bau (Marco 4: massa/volume) */
#define SIM_DT          (1.0f / 60.0f)  /* passo fixo da simulacao           */
#define BUILD_REACH_PX  160.0f  /* alcance para construir (10 tiles)         */

/* Ferramenta atual: 0 = mao (minerar); senao um BuildType (broca, esteira, bau) */
static int tool;
static int build_dir;           /* direcao com que se coloca a esteira */

static int chest_total(const Building *b)
{
    int s = 0;
    for (int i = 0; i < ORE_COUNT; i++) s += b->store[i];
    return s;
}

static int drill_total(const Building *b)
{
    int s = 0;
    for (int i = 0; i < ORE_COUNT; i++) s += b->buf[i];
    return s;
}

/* Algum dos 4 tiles debaixo da broca (origem x, y) ainda tem minerio? */
static int footprint_has_ore(int x, int y)
{
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++)
            if (in_bounds(x + i, y + j) && tile_at(x + i, y + j, 0)->ore != ORE_NONE)
                return 1;
    return 0;
}

/* So se constroi em terreno onde a Luna anda (e nao na agua rasa do vau) */
static int buildable_terrain(int terrain)
{
    return TERRAIN[terrain].walkable && terrain != T_VAU;
}

/* Da para colocar este tipo de objeto com o canto superior esquerdo em (x, y)?
 * (px, py) e a posicao da Luna em pixels: nao se constroi em cima dela. */
static int can_place(int type, int x, int y, float px, float py)
{
    int w = BUILD[type].w, h = BUILD[type].h;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (!in_bounds(x + i, y + j)) return 0;
            if (bgrid[y + j][x + i] != 0) return 0;
            if (!buildable_terrain(tile_at(x + i, y + j, 0)->terrain)) return 0;
        }
    }
    if (type == B_BROCA && !footprint_has_ore(x, y)) return 0;

    if (BUILD[type].solid) {
        float x0 = (float)(x * TILE_SIZE),       y0 = (float)(y * TILE_SIZE);
        float x1 = (float)((x + w) * TILE_SIZE), y1 = (float)((y + h) * TILE_SIZE);
        if (px + PLAYER_HALF > x0 && px - PLAYER_HALF < x1 &&
            py + PLAYER_HALF > y0 && py - PLAYER_HALF < y1) return 0;
    }
    return 1;
}

/* Cria o objeto na primeira posicao livre da lista. NULL se a lista encheu. */
static Building *new_building(int type, int x, int y, int dir)
{
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (buildings[i].type != B_NONE) continue;
        Building *b = &buildings[i];
        memset(b, 0, sizeof *b);
        b->type = (uint8_t)type;
        b->dir  = (uint8_t)(dir & 3);
        b->x    = (int16_t)x;
        b->y    = (int16_t)y;
        for (int j = 0; j < BUILD[type].h; j++)
            for (int k = 0; k < BUILD[type].w; k++)
                bgrid[y + j][x + k] = (uint16_t)(i + 1);
        return b;
    }
    return NULL;
}

/* Coloca um objeto. Devolve 1 se algo mudou.
 * Esteira sobre esteira: so a roda (os itens que ela leva ficam). */
static int place_building(int type, int x, int y, int dir, float px, float py)
{
    if (type == B_ESTEIRA) {
        Building *old = building_at(x, y);
        if (old != NULL && old->type == B_ESTEIRA) {
            if (old->dir == (dir & 3)) return 0;
            old->dir = (uint8_t)(dir & 3);
            return 1;
        }
    }
    if (!can_place(type, x, y, px, py)) return 0;
    return new_building(type, x, y, dir) != NULL;
}

/* Demole o objeto que ocupa (x, y). Os itens que ele tinha vao para a mochila. */
static int remove_building_at(int x, int y)
{
    Building *b = building_at(x, y);
    if (b == NULL) return 0;

    if (b->type == B_ESTEIRA) {
        for (int i = 0; i < b->n; i++) inventory[b->item[i]]++;
    } else if (b->type == B_BAU) {
        for (int i = 0; i < ORE_COUNT; i++) inventory[i] += b->store[i];
    } else if (b->type == B_BROCA) {
        for (int i = 0; i < ORE_COUNT; i++) inventory[i] += b->buf[i];
    }

    for (int j = 0; j < BUILD[b->type].h; j++)
        for (int i = 0; i < BUILD[b->type].w; i++)
            bgrid[b->y + j][b->x + i] = 0;
    b->type = B_NONE;
    return 1;
}

/* ---- Entregar itens: "esteira entra na maquina, sem inserters" -----
 * Tenta meter 1 item no objeto que esta no tile (tx, ty). (dx, dy) e o
 * sentido em que o item vai a andar (de quem entrega para este tile).
 * Devolve 1 se foi aceite. */
static int accept_item(int tx, int ty, int dx, int dy, int item)
{
    Building *b = building_at(tx, ty);
    if (b == NULL) return 0;

    if (b->type == B_ESTEIRA) {
        /* uma esteira virada para quem entrega nao aceita (esteiras de frente) */
        if (DIRX[b->dir] == -dx && DIRY[b->dir] == -dy) return 0;
        if (b->n >= BELT_CAP) return 0;
        if (b->n > 0 && b->pos[b->n - 1] < ITEM_GAP) return 0;   /* sem espaco */
        b->item[b->n] = (uint8_t)item;
        b->pos[b->n]  = 0.0f;
        b->n++;
        return 1;
    }
    if (b->type == B_BAU) {
        if (chest_total(b) >= CHEST_CAP) return 0;
        b->store[item]++;
        return 1;
    }
    return 0;       /* brocas nao aceitam itens */
}

/* ---- Esteira ------------------------------------------------------- */

static void belt_step(Building *b, float dt)
{
    float adv = BELT_SPEED * dt;

    /* cada item anda, mas nunca ultrapassa o da frente nem o fim do tile */
    for (int i = 0; i < b->n; i++) {
        float limit = (i == 0) ? 1.0f : b->pos[i - 1] - ITEM_GAP;
        float np    = b->pos[i] + adv;
        if (np > limit) np = limit;
        if (np > b->pos[i]) b->pos[i] = np;
    }

    /* o item da frente chegou ao fim: passa para o objeto seguinte, se aceitar */
    if (b->n > 0 && b->pos[0] >= 1.0f) {
        int dx = DIRX[b->dir], dy = DIRY[b->dir];
        if (accept_item(b->x + dx, b->y + dy, dx, dy, b->item[0])) {
            for (int i = 1; i < b->n; i++) {
                b->item[i - 1] = b->item[i];
                b->pos[i - 1]  = b->pos[i];
            }
            b->n--;
        }
    }
}

/* ---- Broca --------------------------------------------------------- */

/* 0 = a trabalhar, 1 = sem minerio por baixo, 2 = saida cheia (bloqueada) */
static int drill_status(const Building *b)
{
    if (!footprint_has_ore(b->x, b->y)) return 1;
    if (drill_total(b) >= DRILL_BUF)    return 2;
    return 0;
}

/* Entrega 1 item a um dos 8 tiles em volta da broca (esteira ou bau).
 * Alterna entre saidas e entre tipos, por isso a esteira sai misturada. */
static void drill_dump(Building *b)
{
    /* ox, oy: tile vizinho relativo a origem; dx, dy: sentido para fora */
    static const int out[8][4] = {
        { -1, 0, -1, 0 }, { -1, 1, -1, 0 },     /* esquerda */
        {  2, 0,  1, 0 }, {  2, 1,  1, 0 },     /* direita  */
        {  0, -1, 0, -1 }, { 1, -1, 0, -1 },    /* cima     */
        {  0, 2,  0, 1 }, {  1, 2,  0, 1 },     /* baixo    */
    };

    for (int k = 0; k < ORE_COUNT; k++) {
        int type = (b->cursor_type + k) % ORE_COUNT;
        if (type == ORE_NONE || b->buf[type] == 0) continue;

        for (int j = 0; j < 8; j++) {
            int o = (b->cursor_out + j) & 7;
            if (accept_item(b->x + out[o][0], b->y + out[o][1],
                            out[o][2], out[o][3], type)) {
                b->buf[type]--;
                b->cursor_out  = (uint8_t)((o + 1) & 7);
                b->cursor_type = (uint8_t)(type + 1);
                return;
            }
        }
    }
}

static void drill_step(Building *b, float dt)
{
    if (drill_total(b) < DRILL_BUF) {
        b->timer += dt;
        if (b->timer >= DRILL_PERIOD) {
            /* em cada ciclo tira 1 unidade de CADA tile com minerio por baixo */
            int any = 0;
            for (int i = 0; i < 4; i++) {
                if (drill_total(b) >= DRILL_BUF) break;
                int tx = b->x + (i & 1), ty = b->y + (i >> 1);
                int type, got;
                if (extract_from_tile(tx, ty, 1, &type, &got)) mark_dirty(tx, ty);
                if (got > 0) { b->buf[type] = (uint16_t)(b->buf[type] + got); any = 1; }
            }
            if (any) b->timer -= DRILL_PERIOD;
            else     b->timer = 0.0f;       /* nada para extrair */
        }
    }
    drill_dump(b);
}

/* Um passo da simulacao (sempre com o mesmo dt, ver SIM_DT) */
static void sim_step(float dt)
{
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        Building *b = &buildings[i];
        if (b->type == B_BROCA)        drill_step(b, dt);
        else if (b->type == B_ESTEIRA) belt_step(b, dt);
    }
}

/* ---- Ferramentas (rato) -------------------------------------------- */

static void mouse_tile(Camera2D cam, int *tx, int *ty)
{
    Vector2 w = GetScreenToWorld2D(GetMousePosition(), cam);
    *tx = (int)floorf(w.x / TILE_SIZE);
    *ty = (int)floorf(w.y / TILE_SIZE);
}

static int tile_within(const Player *p, int tx, int ty, float reach)
{
    float cx = (float)tx * TILE_SIZE + TILE_SIZE / 2.0f;
    float cy = (float)ty * TILE_SIZE + TILE_SIZE / 2.0f;
    return dist2d(p->x, p->y - 4.0f, cx, cy) <= reach;
}

static int last_bx = -1, last_by = -1;     /* ultimo tile construido ao arrastar */

static void update_building(const Player *p, Camera2D cam, int free_cam)
{
    if (IsKeyPressed(KEY_Q)) build_dir = (build_dir + 3) & 3;
    if (IsKeyPressed(KEY_E)) build_dir = (build_dir + 1) & 3;

    int tx, ty;
    mouse_tile(cam, &tx, &ty);
    int ok = in_bounds(tx, ty) && tile_within(p, tx, ty, BUILD_REACH_PX);

    /* F: pegar tudo do bau sob o rato (tem de estar ao alcance das maos) */
    if (IsKeyPressed(KEY_F) && in_bounds(tx, ty) && tile_in_reach(p, tx, ty)) {
        Building *b = building_at(tx, ty);
        if (b != NULL && b->type == B_BAU) {
            for (int i = 0; i < ORE_COUNT; i++) {
                inventory[i] += b->store[i];
                b->store[i] = 0;
            }
        }
    }

    /* Botao direito: demolir (em camara livre serve para arrastar o mapa) */
    if (!free_cam && ok && IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
        remove_building_at(tx, ty);

    if (tool == 0 || !IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
        last_bx = last_by = -1;
        return;
    }
    if (!ok || (tx == last_bx && ty == last_by)) return;

    /* Ao arrastar esteiras, a anterior passa a apontar para a nova */
    if (tool == B_ESTEIRA && last_bx >= 0 &&
        abs(tx - last_bx) + abs(ty - last_by) == 1) {
        Building *prev = building_at(last_bx, last_by);
        if (prev != NULL && prev->type == B_ESTEIRA) {
            for (int d = 0; d < 4; d++) {
                if (DIRX[d] == tx - last_bx && DIRY[d] == ty - last_by) {
                    prev->dir = (uint8_t)d;
                    build_dir = d;
                }
            }
        }
    }

    place_building(tool, tx, ty, build_dir, p->x, p->y);
    if (building_at(tx, ty) != NULL) { last_bx = tx; last_by = ty; }
}

/* ---- Desenho dos objetos ------------------------------------------- */

static void draw_tri(Vector2 a, Vector2 b, Vector2 c, Color col)
{
    /* desenha nas duas ordens: a que tiver a orientacao certa aparece */
    DrawTriangle(a, b, c, col);
    DrawTriangle(a, c, b, col);
}

static void draw_arrow(float cx, float cy, int dir, float len, Color col)
{
    float fx = (float)DIRX[dir], fy = (float)DIRY[dir];
    float qx = -fy, qy = fx;
    draw_tri((Vector2){ cx + fx * len, cy + fy * len },
             (Vector2){ cx - fx * len * 0.6f + qx * len * 0.8f, cy - fy * len * 0.6f + qy * len * 0.8f },
             (Vector2){ cx - fx * len * 0.6f - qx * len * 0.8f, cy - fy * len * 0.6f - qy * len * 0.8f },
             col);
}

static void draw_belt(const Building *b)
{
    int px = b->x * TILE_SIZE, py = b->y * TILE_SIZE;
    float cx = (float)px + TILE_SIZE / 2.0f, cy = (float)py + TILE_SIZE / 2.0f;

    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){ 64, 66, 74, 255 });
    DrawRectangle(px + 1, py + 1, TILE_SIZE - 2, TILE_SIZE - 2, (Color){ 96, 99, 108, 255 });
    draw_arrow(cx, cy, b->dir, 4.5f, (Color){ 66, 68, 78, 255 });

    for (int i = 0; i < b->n; i++) {
        float t  = b->pos[i] - 0.5f;
        float ix = cx + (float)DIRX[b->dir] * t * TILE_SIZE;
        float iy = cy + (float)DIRY[b->dir] * t * TILE_SIZE;
        DrawRectangle((int)(ix - 3.0f), (int)(iy - 3.0f), 6, 6, (Color){ 20, 20, 26, 255 });
        DrawRectangle((int)(ix - 2.0f), (int)(iy - 2.0f), 4, 4, ORE[b->item[i]].color);
    }
}

static void draw_drill(const Building *b)
{
    int px = b->x * TILE_SIZE, py = b->y * TILE_SIZE;
    int cx = px + TILE_SIZE, cy = py + TILE_SIZE;
    int st = drill_status(b);

    DrawRectangle(px, py, TILE_SIZE * 2, TILE_SIZE * 2, (Color){ 46, 48, 56, 255 });
    DrawRectangle(px + 2, py + 2, TILE_SIZE * 2 - 4, TILE_SIZE * 2 - 4, (Color){ 112, 116, 128, 255 });
    DrawCircle(cx, cy, 9.0f, (Color){ 60, 62, 72, 255 });

    /* broca a rodar quando esta a trabalhar */
    float a = (st == 0) ? (float)GetTime() * 6.0f : 0.0f;
    for (int i = 0; i < 2; i++) {
        float ang = a + (float)i * 1.5708f;
        float ddx = cosf(ang) * 7.0f, ddy = sinf(ang) * 7.0f;
        DrawLineEx((Vector2){ (float)cx - ddx, (float)cy - ddy },
                   (Vector2){ (float)cx + ddx, (float)cy + ddy }, 2.0f, (Color){ 210, 214, 224, 255 });
    }

    /* luz de estado: verde = trabalha, vermelho = sem minerio, amarelo = cheia */
    Color lamp = st == 0 ? (Color){ 90, 220, 110, 255 }
               : st == 1 ? (Color){ 220, 70, 70, 255 }
                         : (Color){ 235, 200, 60, 255 };
    DrawRectangle(px + TILE_SIZE * 2 - 7, py + 3, 4, 4, lamp);
}

static void draw_chest(const Building *b)
{
    int px = b->x * TILE_SIZE, py = b->y * TILE_SIZE;
    DrawRectangle(px + 1, py + 2, TILE_SIZE - 2, TILE_SIZE - 3, (Color){ 84, 52, 26, 255 });
    DrawRectangle(px + 2, py + 3, TILE_SIZE - 4, 4, (Color){ 142, 94, 50, 255 });
    DrawRectangle(px + 7, py + 6, 2, 3, (Color){ 235, 190, 60, 255 });
    /* barra de enchimento */
    int w = (TILE_SIZE - 4) * chest_total(b) / CHEST_CAP;
    DrawRectangle(px + 2, py + TILE_SIZE - 4, w, 2, (Color){ 120, 220, 120, 255 });
}

static void draw_buildings(Camera2D cam, int screen_w, int screen_h)
{
    Vector2 tl = GetScreenToWorld2D((Vector2){ 0, 0 }, cam);
    Vector2 br = GetScreenToWorld2D((Vector2){ (float)screen_w, (float)screen_h }, cam);

    /* primeiro as esteiras, depois brocas e baus por cima */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < MAX_BUILDINGS; i++) {
            const Building *b = &buildings[i];
            if (b->type == B_NONE) continue;
            if ((b->type == B_ESTEIRA) != (pass == 0)) continue;

            float x0 = (float)(b->x * TILE_SIZE), y0 = (float)(b->y * TILE_SIZE);
            if (x0 + BUILD[b->type].w * TILE_SIZE < tl.x || x0 > br.x ||
                y0 + BUILD[b->type].h * TILE_SIZE < tl.y || y0 > br.y) continue;

            if (b->type == B_ESTEIRA)    draw_belt(b);
            else if (b->type == B_BROCA) draw_drill(b);
            else if (b->type == B_BAU)   draw_chest(b);
        }
    }
}

/* "Fantasma" do que vai ser construido: verde = pode, vermelho = nao pode */
static void draw_build_preview(const Player *p, Camera2D cam)
{
    int tx, ty;
    mouse_tile(cam, &tx, &ty);
    if (!in_bounds(tx, ty)) return;

    int w = BUILD[tool].w, h = BUILD[tool].h;
    int ok = tile_within(p, tx, ty, BUILD_REACH_PX);
    if (ok) {
        if (tool == B_ESTEIRA && building_at(tx, ty) != NULL &&
            building_at(tx, ty)->type == B_ESTEIRA) ok = 1;      /* roda a que existe */
        else ok = can_place(tool, tx, ty, p->x, p->y);
    }

    Color c = ok ? (Color){ 90, 220, 110, 255 } : (Color){ 220, 70, 70, 255 };
    Color fill = c;
    fill.a = 70;
    DrawRectangle(tx * TILE_SIZE, ty * TILE_SIZE, w * TILE_SIZE, h * TILE_SIZE, fill);
    DrawRectangleLinesEx((Rectangle){ (float)(tx * TILE_SIZE), (float)(ty * TILE_SIZE),
                                      (float)(w * TILE_SIZE), (float)(h * TILE_SIZE) },
                         1.5f / cam.zoom, c);
    if (tool == B_ESTEIRA)
        draw_arrow((float)(tx * TILE_SIZE) + TILE_SIZE / 2.0f,
                   (float)(ty * TILE_SIZE) + TILE_SIZE / 2.0f, build_dir, 4.5f, c);
}

/* Contorno no tile da jazida sob o rato: branco = ao alcance, vermelho = longe */
static void draw_hover(const Player *p, Camera2D cam)
{
    Vector2 w = GetScreenToWorld2D(GetMousePosition(), cam);
    int tx = (int)floorf(w.x / TILE_SIZE);
    int ty = (int)floorf(w.y / TILE_SIZE);
    if (!in_bounds(tx, ty) || tile_at(tx, ty, 0)->ore == ORE_NONE) return;

    Color c = (Color){ 220, 70, 70, 255 };
    if (tile_in_reach(p, tx, ty)) c = is_mining ? (Color){ 255, 220, 60, 255 } : RAYWHITE;

    DrawRectangleLinesEx((Rectangle){ (float)(tx * TILE_SIZE), (float)(ty * TILE_SIZE),
                                      (float)TILE_SIZE, (float)TILE_SIZE },
                         1.5f / cam.zoom, c);
}

/* ------------------------------------------------------------------ */
/* Camara                                                              */
/* ------------------------------------------------------------------ */

static void update_camera(Camera2D *cam, int screen_w, int screen_h,
                          const Player *follow, int free_cam)
{
    float dt = GetFrameTime();
    cam->offset = (Vector2){ screen_w / 2.0f, screen_h / 2.0f };

    if (free_cam) {
        /* Camara livre: WASD move a camara, botao direito/meio arrasta */
        float speed = 500.0f / cam->zoom;
        if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) speed *= 2.5f;
        if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    cam->target.y -= speed * dt;
        if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  cam->target.y += speed * dt;
        if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  cam->target.x -= speed * dt;
        if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) cam->target.x += speed * dt;

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
            Vector2 d = GetMouseDelta();
            cam->target.x -= d.x / cam->zoom;
            cam->target.y -= d.y / cam->zoom;
        }
    } else {
        /* Seguir a Luna de forma suave */
        float k = 8.0f * dt;
        if (k > 1.0f) k = 1.0f;
        cam->target.x += (follow->x - cam->target.x) * k;
        cam->target.y += (follow->y - 6.0f - cam->target.y) * k;
    }

    /* Zoom em direcao ao cursor */
    float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        Vector2 before = GetScreenToWorld2D(GetMousePosition(), *cam);
        cam->zoom *= powf(1.15f, wheel);
        cam->zoom = clampf(cam->zoom, 0.5f, 8.0f);
        Vector2 after = GetScreenToWorld2D(GetMousePosition(), *cam);
        cam->target.x += before.x - after.x;
        cam->target.y += before.y - after.y;
    }

    /* Nao deixar a camara sair do mapa */
    cam->target.x = clampf(cam->target.x, 0.0f, (float)(MAP_W * TILE_SIZE));
    cam->target.y = clampf(cam->target.y, 0.0f, (float)(MAP_H * TILE_SIZE));
}

/* ------------------------------------------------------------------ */
/* Interface (HUD)                                                     */
/* ------------------------------------------------------------------ */

static void draw_hud(Camera2D cam, int screen_w, int screen_h,
                     const Player *p, int free_cam)
{
    Vector2 world_pos = GetScreenToWorld2D(GetMousePosition(), cam);
    int tx = (int)floorf(world_pos.x / TILE_SIZE);
    int ty = (int)floorf(world_pos.y / TILE_SIZE);

    DrawRectangle(0, 0, 560, 148, (Color){ 0, 0, 0, 150 });
    DrawText("A CLAREIRA DA LUNA  -  Marco 3a", 10, 8, 16, RAYWHITE);

    char line[200];
    if (in_bounds(tx, ty)) {
        const Tile *t = tile_at(tx, ty, 0);
        if (t->ore != ORE_NONE) {
            snprintf(line, sizeof line, "Rato: (%d, %d)  %s | %s: %d/%d", tx, ty,
                     TERRAIN[t->terrain].name, ORE[t->ore].name,
                     (int)t->ore_amount, ORE[t->ore].richest);
        } else {
            snprintf(line, sizeof line, "Rato: (%d, %d)  %s", tx, ty, TERRAIN[t->terrain].name);
        }
    } else {
        snprintf(line, sizeof line, "Rato: fora do mapa");
    }
    DrawText(line, 10, 30, 16, RAYWHITE);

    snprintf(line, sizeof line, "Luna: (%d, %d)   %s",
             (int)floorf(p->x / TILE_SIZE), (int)floorf(p->y / TILE_SIZE),
             free_cam ? "[camara livre]" : "[a seguir]");
    DrawText(line, 10, 52, 16, RAYWHITE);

    snprintf(line, sizeof line, "Zoom %.1fx   FPS %d   seed %u",
             cam.zoom, GetFPS(), (unsigned)current_seed);
    DrawText(line, 10, 76, 14, LIGHTGRAY);

    /* Ferramenta atual */
    static const char *dir_name[4] = { "direita", "baixo", "esquerda", "cima" };
    if (tool == 0) snprintf(line, sizeof line, "Ferramenta: [1] Mao (minerar)");
    else if (tool == B_ESTEIRA)
        snprintf(line, sizeof line, "Ferramenta: %s  (Q/E roda: %s)", BUILD[tool].name, dir_name[build_dir]);
    else snprintf(line, sizeof line, "Ferramenta: %s", BUILD[tool].name);
    DrawText(line, 10, 98, 16, (Color){ 255, 220, 60, 255 });

    /* Info do objeto sob o rato */
    line[0] = '\0';
    if (in_bounds(tx, ty) && building_at(tx, ty) != NULL) {
        const Building *b = building_at(tx, ty);
        if (b->type == B_BAU) {
            int n = snprintf(line, sizeof line, "Bau %d/%d:", chest_total(b), CHEST_CAP);
            for (int i = 1; i < ORE_COUNT && n < (int)sizeof line - 24; i++)
                if (b->store[i] > 0)
                    n += snprintf(line + n, sizeof line - (size_t)n, " %s %d", ORE[i].name, b->store[i]);
            if (chest_total(b) > 0) snprintf(line + n, sizeof line - (size_t)n, "  (F pegar)");
        } else if (b->type == B_BROCA) {
            static const char *st_name[3] = { "a trabalhar", "sem minerio por baixo", "saida cheia" };
            snprintf(line, sizeof line, "Broca: %s", st_name[drill_status(b)]);
        } else if (b->type == B_ESTEIRA) {
            snprintf(line, sizeof line, "Esteira: %d item(ns)", b->n);
        }
    }
    if (line[0] != '\0') DrawText(line, 10, 122, 16, RAYWHITE);

    /* Mochila (canto superior direito) */
    int bx = screen_w - 200;
    DrawRectangle(bx, 0, 200, 22 + (ORE_COUNT - 1) * 22 + 8, (Color){ 0, 0, 0, 150 });
    DrawText("Mochila (provisoria)", bx + 10, 6, 14, RAYWHITE);
    for (int i = 1; i < ORE_COUNT; i++) {
        int y = 26 + (i - 1) * 22;
        DrawRectangle(bx + 10, y + 2, 12, 12, ORE[i].color);
        snprintf(line, sizeof line, "%s: %d", ORE[i].name, inventory[i]);
        DrawText(line, bx + 30, y, 16, RAYWHITE);
    }

    DrawText("WASD mover | shift correr | 1 mao 2 broca 3 esteira 4 bau | Q/E rodar | clique esq. usar | clique dir. demolir | F pegar do bau | roda zoom | TAB camara | G grelha | R novo mapa",
             10, screen_h - 22, 14, RAYWHITE);
}

/* ------------------------------------------------------------------ */
/* Programa principal                                                  */
/* ------------------------------------------------------------------ */

#ifndef CLAREIRA_TEST
int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "A Clareira da Luna");
    SetTargetFPS(60);

    /* A textura do terreno so pode ser criada depois de abrir a janela */
    terrain_tex = LoadRenderTexture(MAP_W * TILE_SIZE, MAP_H * TILE_SIZE);
    SetTextureFilter(terrain_tex.texture, TEXTURE_FILTER_POINT);   /* pixel art nitida */

    generate_world(DEFAULT_SEED);
    build_terrain_texture();

    float sim_acc = 0.0f;       /* tempo acumulado ainda por simular */

    Player luna;
    player_spawn(&luna);

    Camera2D cam = { 0 };
    cam.zoom   = 3.0f;
    cam.target = (Vector2){ luna.x, luna.y };

    int show_grid = 0;
    int free_cam  = 0;

    while (!WindowShouldClose()) {
        int   sw = GetScreenWidth();
        int   sh = GetScreenHeight();
        float dt = GetFrameTime();

        if (IsKeyPressed(KEY_G))   show_grid = !show_grid;
        if (IsKeyPressed(KEY_TAB)) free_cam  = !free_cam;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_ONE))   tool = 0;
        if (IsKeyPressed(KEY_TWO))   tool = B_BROCA;
        if (IsKeyPressed(KEY_THREE)) tool = B_ESTEIRA;
        if (IsKeyPressed(KEY_FOUR))  tool = B_BAU;
        if (IsKeyPressed(KEY_R)) {
            generate_world(current_seed + 1u);     /* tambem limpa os objetos */
            build_terrain_texture();
            player_spawn(&luna);
            for (int i = 0; i < ORE_COUNT; i++) inventory[i] = 0;
        }

        if (!free_cam) update_player(&luna, dt);
        if (tool == 0) {
            update_mining(&luna, cam, dt);
        } else {
            is_mining = 0;
            mine_timer = 0.0f;
        }
        update_building(&luna, cam, free_cam);

        /* Simulacao em passo fixo: igual em qualquer computador/FPS */
        sim_acc += dt;
        if (sim_acc > 0.25f) sim_acc = 0.25f;      /* evita "espiral" se o jogo engasgar */
        while (sim_acc >= SIM_DT) {
            sim_step(SIM_DT);
            sim_acc -= SIM_DT;
        }
        flush_dirty();

        update_camera(&cam, sw, sh, &luna, free_cam);

        BeginDrawing();
        ClearBackground((Color){ 20, 28, 24, 255 });

        BeginMode2D(cam);
        draw_world(cam, sw, sh, show_grid);
        draw_buildings(cam, sw, sh);
        draw_player(&luna);
        if (tool == 0) draw_hover(&luna, cam);
        else           draw_build_preview(&luna, cam);
        EndMode2D();

        draw_hud(cam, sw, sh, &luna, free_cam);
        EndDrawing();
    }

    UnloadRenderTexture(terrain_tex);
    CloseWindow();
    return 0;
}
#endif
