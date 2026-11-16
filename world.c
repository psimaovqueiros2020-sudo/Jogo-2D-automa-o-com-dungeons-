/*
 * world.c  -  O MUNDO: terreno, minerios (tabelas), ruido e geracao do setor.
 * Sem input e sem desenho (so usa o TIPO Color da raylib nas tabelas).
 */

#include "world.h"
#include "logic.h"      /* generate_world() limpa os objetos (reset_buildings) */
#include <math.h>

/* ------------------------------------------------------------------ */
/* Tabelas                                                             */
/* ------------------------------------------------------------------ */

const TerrainInfo TERRAIN[T_COUNT] = {
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

/* QUANTO HA EM CADA TILE (o valor e o do CENTRO da mancha; nas bordas e ~25 %).
 * Escala: 1 tile = 50 cm = 0,25 m2; 1 unidade = 0,1 L. Logo 1000 un = 100 L = uma
 * coluna de 40 cm de material em cima de um tile. Os valores sao PROVISORIOS
 * (afinar depois de jogar), mas pensados assim:
 *
 *   Argila    2000 un  (200 L: ~80 cm de lama argilosa; ha 83 tiles)
 *   Areia     1500 un  (150 L: ~60 cm, banco de areia do ribeiro)
 *   Hematite  1500 un  (150 L de rocha partida; ha 198 tiles)
 *   Enxofre    800 un  (80 L: o pantano tem pouco e e dos mais raros)
 *   Quartzo    600 un  (60 L: um afloramento pequeno, o mais raro)
 *   Madeira    por ARVORE, ver TREE_WOOD_MIN/MAX em world.h (400 a 600 un)
 *
 * ("richest" tambem decide quantas pedrinhas se desenham em cada tile.) */
const OreInfo ORE[ORE_COUNT] = {
    [ORE_NONE]     = { ITEM_NONE,        0 },
    [ORE_HEMATITE] = { ITEM_HEMATITE, 1500 },
    [ORE_ARGILA]   = { ITEM_ARGILA,   2000 },
    [ORE_ENXOFRE]  = { ITEM_ENXOFRE,   800 },
    [ORE_QUARTZO]  = { ITEM_QUARTZO,   600 },
    [ORE_MADEIRA]  = { ITEM_MADEIRA,   TREE_WOOD_MAX },    /* por arvore */
    [ORE_AREIA]    = { ITEM_AREIA,    1500 },
};

/* ------------------------------------------------------------------ */
/* Mundo                                                               */
/* ------------------------------------------------------------------ */

Tile     world[MAP_D][MAP_H][MAP_W];
uint32_t current_seed = DEFAULT_SEED;

/* ---- Fila de tiles a redesenhar (ver world.h) ---------------------- */

int16_t dirty_x[DIRTY_MAX], dirty_y[DIRTY_MAX];
int     dirty_n;
int     dirty_overflow;      /* 1 = demasiados: redesenhar o mapa todo */

void mark_dirty(int x, int y)
{
    if (dirty_n < DIRTY_MAX) {
        dirty_x[dirty_n] = (int16_t)x;
        dirty_y[dirty_n] = (int16_t)y;
        dirty_n++;
    } else {
        dirty_overflow = 1;
    }
}

/* Quantas pedrinhas desenhar (0 a 4) conforme o minerio que resta */
int ore_dots(const Tile *t)
{
    if (t->ore == ORE_NONE || t->ore_amount == 0) return 0;
    if (t->terrain == T_ARVORE) return 0;       /* madeira nao se desenha como pedrinhas */
    float ratio = (float)t->ore_amount / (float)ORE[t->ore].richest;
    if (ratio > 1.0f) ratio = 1.0f;
    return 1 + (int)(ratio * 3.99f);
}


/* ------------------------------------------------------------------ */
/* Ruido (value noise) - para o terreno parecer natural                */
/* ------------------------------------------------------------------ */

uint32_t hash2(int x, int y, uint32_t seed)
{
    uint32_t h = (uint32_t)x * 374761393u
               + (uint32_t)y * 668265263u
               + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    return h ^ (h >> 16);
}

float rnd01(int x, int y, uint32_t seed)
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

float dist2d(float x1, float y1, float x2, float y2)
{
    float dx = x1 - x2, dy = y1 - y2;
    return sqrtf(dx * dx + dy * dy);
}

float clampf(float v, float lo, float hi)
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

    /* 4. Pantano a leste, com pocas de agua */
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

    /* Areia: tambem na margem norte do ribeiro, entre as manchas de argila
     * (assim um unico trilho de esteiras apanha as duas). */
    static const int sand_x[] = { 60, 80, 104, 128 };
    for (int i = 0; i < 4; i++) {
        int cy = (int)(river_center_y(sand_x[i], seed) - 3.0f);
        place_patch(ORE_AREIA, sand_x[i], cy, 3.0f, T_LAMA, seed);
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

void generate_world(uint32_t seed)
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
                 * (TREE_WOOD_MIN a TREE_WOOD_MAX unidades). Quando acaba, a arvore desaparece. */
                int interior = x >= 3 && y >= 3 && x < MAP_W - 3 && y < MAP_H - 3;
                if (t.terrain == T_ARVORE && interior) {
                    t.ore        = ORE_MADEIRA;
                    t.ore_amount = (uint16_t)(TREE_WOOD_MIN + hash2(x, y, seed + 123u) % (uint32_t)(TREE_WOOD_MAX - TREE_WOOD_MIN + 1));
                }
                *tile_at(x, y, z) = t;
            }
        }
    }
    carve_trails(seed);
    generate_ores(seed);
    remove_unreachable_ore();
}
