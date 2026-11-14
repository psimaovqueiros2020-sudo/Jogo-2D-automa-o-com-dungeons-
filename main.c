/*
 * A CLAREIRA DA LUNA  -  Marco 1: Mapa
 * ------------------------------------
 * Linguagem: C (C99)   Biblioteca: raylib
 *
 * O que faz esta versao:
 *   - Gera o setor "A Clareira da Luna" (sempre igual, mesma SEED)
 *   - Desenha o mapa com quadrados coloridos (placeholder de pixel art)
 *   - Camara com movimento e zoom, grelha opcional, info do tile sob o rato
 *
 * Controlos:
 *   W A S D / setas ..... mover camara (SHIFT = mais rapido)
 *   Roda do rato ........ zoom
 *   Botao direito/meio .. arrastar o mapa
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
};

/* ------------------------------------------------------------------ */
/* Mundo                                                               */
/* ------------------------------------------------------------------ */

typedef struct {
    uint8_t terrain;    /* valor de Terrain                              */
    uint8_t shade;      /* variacao de cor (0..7) para o mapa nao ficar  */
                        /* uma mancha chapada                            */
    /* Marco 2 vai acrescentar aqui: jazida (tipo + quantidade restante) */
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

static void generate_world(uint32_t seed)
{
    current_seed = seed;
    for (int z = 0; z < MAP_D; z++) {
        for (int y = 0; y < MAP_H; y++) {
            for (int x = 0; x < MAP_W; x++) {
                Tile *t = tile_at(x, y, z);
                t->terrain = (uint8_t)generate_tile(x, y, seed);
                t->shade   = (uint8_t)(hash2(x, y, seed + 99u) & 7u);
            }
        }
    }
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

static void draw_tile(int x, int y, const Tile *t)
{
    const TerrainInfo *info = &TERRAIN[t->terrain];
    int px = x * TILE_SIZE;
    int py = y * TILE_SIZE;

    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, shaded(info->color, t->shade));

    /* Pequenos detalhes de placeholder, so para ler o mapa mais facilmente */
    if (t->terrain == T_ARVORE) {
        DrawCircle(px + TILE_SIZE / 2, py + TILE_SIZE / 2, TILE_SIZE * 0.45f,
                   shaded((Color){ 30, 110, 60, 255 }, t->shade));
    } else if (t->terrain == T_CASA_ARVORE) {
        DrawRectangle(px + 3, py + 3, TILE_SIZE - 6, TILE_SIZE - 6,
                      (Color){ 110, 66, 34, 255 });
        DrawRectangle(px + 6, py + 7, 4, 6, (Color){ 240, 220, 120, 255 });
    }
}

static void draw_world(Camera2D cam, int screen_w, int screen_h, int show_grid)
{
    /* So desenhamos os tiles que aparecem no ecra */
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

    const int z = 0;    /* camada visivel (superficie) */

    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            draw_tile(x, y, tile_at(x, y, z));

    if (show_grid && cam.zoom >= 2.0f) {
        Color g = { 0, 0, 0, 50 };
        for (int y = y0; y <= y1 + 1; y++)
            DrawLine(x0 * TILE_SIZE, y * TILE_SIZE, (x1 + 1) * TILE_SIZE, y * TILE_SIZE, g);
        for (int x = x0; x <= x1 + 1; x++)
            DrawLine(x * TILE_SIZE, y0 * TILE_SIZE, x * TILE_SIZE, (y1 + 1) * TILE_SIZE, g);
    }
}

/* ------------------------------------------------------------------ */
/* Camara                                                              */
/* ------------------------------------------------------------------ */

static void update_camera(Camera2D *cam, int screen_w, int screen_h)
{
    float dt = GetFrameTime();
    cam->offset = (Vector2){ screen_w / 2.0f, screen_h / 2.0f };

    /* Movimento com teclado */
    float speed = 500.0f / cam->zoom;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) speed *= 2.5f;
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    cam->target.y -= speed * dt;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  cam->target.y += speed * dt;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  cam->target.x -= speed * dt;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) cam->target.x += speed * dt;

    /* Arrastar com botao direito ou do meio */
    if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
        Vector2 d = GetMouseDelta();
        cam->target.x -= d.x / cam->zoom;
        cam->target.y -= d.y / cam->zoom;
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

static void draw_hud(Camera2D cam, int screen_w, int screen_h)
{
    Vector2 world_pos = GetScreenToWorld2D(GetMousePosition(), cam);
    int tx = (int)floorf(world_pos.x / TILE_SIZE);
    int ty = (int)floorf(world_pos.y / TILE_SIZE);

    DrawRectangle(0, 0, 330, 78, (Color){ 0, 0, 0, 150 });
    DrawText("A CLAREIRA DA LUNA  -  Marco 1", 10, 8, 16, RAYWHITE);

    char line[160];
    if (in_bounds(tx, ty)) {
        const Tile *t = tile_at(tx, ty, 0);
        snprintf(line, sizeof line, "Tile (%d, %d)  %s", tx, ty, TERRAIN[t->terrain].name);
    } else {
        snprintf(line, sizeof line, "Fora do mapa");
    }
    DrawText(line, 10, 30, 16, RAYWHITE);

    snprintf(line, sizeof line, "Zoom %.1fx   FPS %d   seed %u",
             cam.zoom, GetFPS(), (unsigned)current_seed);
    DrawText(line, 10, 52, 14, LIGHTGRAY);

    DrawText("WASD mover | roda zoom | dir. arrastar | G grelha | R novo mapa",
             10, screen_h - 22, 14, RAYWHITE);
    (void)screen_w;
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

    generate_world(DEFAULT_SEED);

    Camera2D cam = { 0 };
    cam.zoom   = 3.0f;
    cam.target = (Vector2){ CLEARING_X * TILE_SIZE + TILE_SIZE / 2.0f,
                            CLEARING_Y * TILE_SIZE + TILE_SIZE / 2.0f };

    int show_grid = 0;

    while (!WindowShouldClose()) {
        int sw = GetScreenWidth();
        int sh = GetScreenHeight();

        if (IsKeyPressed(KEY_G))   show_grid = !show_grid;
        if (IsKeyPressed(KEY_R))   generate_world(current_seed + 1u);
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        update_camera(&cam, sw, sh);

        BeginDrawing();
        ClearBackground((Color){ 20, 28, 24, 255 });

        BeginMode2D(cam);
        draw_world(cam, sw, sh, show_grid);
        EndMode2D();

        draw_hud(cam, sw, sh);
        EndDrawing();
    }

    CloseWindow();
    return 0;
}
#endif
