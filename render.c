/*
 * render.c  -  TUDO O QUE DESENHA (raylib): terreno, objetos, Luna, HUD.
 * Nao altera o estado do jogo; so o le (e a fila de tiles a redesenhar).
 */

#include "render.h"
#include "logic.h"
#include <math.h>
#include <stdio.h>

/* ------------------------------------------------------------------ */
/* Terreno                                                             */
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

    /* Jazida: 1 a 4 pedrinhas, quantas mais, mais minerio resta no tile */
    if (t->ore != ORE_NONE) {
        static const int ox[4] = { 2, 9, 2, 9 };
        static const int oy[4] = { 2, 3, 9, 10 };
        int n = ore_dots(t);
        for (int i = 0; i < n; i++) {
            int jx = (int)(hash2(x, y, 31u + (uint32_t)i * 2u) % 3u) - 1;
            int jy = (int)(hash2(x, y, 32u + (uint32_t)i * 2u) % 3u) - 1;
            DrawRectangle(px + ox[i] + jx, py + oy[i] + jy, 5, 4,
                          shaded(ITEM[ORE[t->ore].item].color, (x + y + i * 3) & 7));
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

void build_terrain_texture(void)
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
void flush_dirty(void)
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

void render_init(void)
{
    /* A textura do terreno so pode ser criada depois de abrir a janela */
    terrain_tex = LoadRenderTexture(MAP_W * TILE_SIZE, MAP_H * TILE_SIZE);
    SetTextureFilter(terrain_tex.texture, TEXTURE_FILTER_POINT);   /* pixel art nitida */
}

void render_shutdown(void)
{
    UnloadRenderTexture(terrain_tex);
}

void draw_world(Camera2D cam, int screen_w, int screen_h, int show_grid)
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

/* Luna placeholder: vestido azul-celeste, cabelo escuro, pingente topazio */
void draw_player(const Player *p)
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
/* Objetos construidos                                                 */
/* ------------------------------------------------------------------ */

void mouse_tile(Camera2D cam, int *tx, int *ty)
{
    Vector2 w = GetScreenToWorld2D(GetMousePosition(), cam);
    *tx = (int)floorf(w.x / TILE_SIZE);
    *ty = (int)floorf(w.y / TILE_SIZE);
}

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
        DrawRectangle((int)(ix - 2.0f), (int)(iy - 2.0f), 4, 4, ITEM[b->item[i]].color);
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
    /* barra de enchimento (por volume) */
    int w = (TILE_SIZE - 4) * container_vol_ml(&b->store) / b->store.cap_vol_ml;
    DrawRectangle(px + 2, py + TILE_SIZE - 4, w, 2, (Color){ 120, 220, 120, 255 });
}

/* Contentor: caixa de chapa ondulada, 2x2 tiles (1 m), 2000 L */
static void draw_container(const Building *b)
{
    int px = b->x * TILE_SIZE, py = b->y * TILE_SIZE;
    int w = TILE_SIZE * 2, h = TILE_SIZE * 2;
    DrawRectangle(px, py, w, h, (Color){ 36, 54, 72, 255 });
    DrawRectangle(px + 1, py + 1, w - 2, h - 2, (Color){ 62, 100, 134, 255 });
    for (int i = 0; i < 5; i++)                 /* chapa ondulada */
        DrawRectangle(px + 3 + i * 6, py + 3, 1, h - 8, (Color){ 46, 78, 106, 255 });
    DrawRectangle(px + w / 2 - 1, py + 3, 2, h - 8, (Color){ 30, 40, 52, 255 });       /* porta */
    DrawRectangle(px + w / 2 - 4, py + h / 2 - 1, 2, 3, (Color){ 235, 190, 60, 255 }); /* trinco */
    /* barra de enchimento (por volume) */
    int fw = (w - 6) * container_vol_ml(&b->store) / b->store.cap_vol_ml;
    DrawRectangle(px + 3, py + h - 4, fw, 2, (Color){ 120, 220, 120, 255 });
}

/* Carvoeira: monte de terra com fumo. Forno: cupula de tijolo com brasas. */
static void draw_machine(const Building *b)
{
    int px = b->x * TILE_SIZE, py = b->y * TILE_SIZE;
    int cx = px + TILE_SIZE, cy = py + TILE_SIZE;
    int st = machine_status(b);
    float prog = 0.0f;
    if (b->recipe != 0) prog = b->work / RECIPES[b->recipe - 1].period;

    if (b->type == B_CARVOEIRA) {
        DrawRectangle(px, py, TILE_SIZE * 2, TILE_SIZE * 2, (Color){ 52, 38, 28, 255 });
        DrawCircle(cx, cy + 1, 13.0f, (Color){ 92, 66, 44, 255 });
        DrawCircle(cx, cy - 1, 9.0f,  (Color){ 70, 50, 34, 255 });
        if (st == 0) {      /* fumo a subir */
            float t = (float)GetTime() * 1.5f;
            for (int i = 0; i < 3; i++) {
                float f = fmodf(t + (float)i * 0.33f, 1.0f);
                Color sm = (Color){ 200, 200, 205, (unsigned char)(150.0f * (1.0f - f)) };
                DrawCircle(cx + (i - 1) * 3, cy - 4 - (int)(f * 14.0f), 2.0f + f * 2.0f, sm);
            }
        }
    } else if (b->type == B_FORNO_RUPESTRE) {
        /* monte de argila com uma boca; racha-se a medida que se aproxima do fim */
        DrawRectangle(px, py, TILE_SIZE * 2, TILE_SIZE * 2, (Color){ 112, 70, 46, 255 });
        DrawCircle(cx, cy, 14.0f, (Color){ 196, 130, 88, 255 });
        DrawCircle(cx, cy - 2, 10.0f, (Color){ 172, 108, 70, 255 });
        Color fire = st == 0 ? (Color){ 255, 170, 50, 255 } : (Color){ 60, 30, 24, 255 };
        DrawRectangle(cx - 5, cy + 3, 10, 6, (Color){ 40, 24, 18, 255 });
        DrawRectangle(cx - 4, cy + 4, 8, 4, fire);
        if (BUILD[b->type].life > 0 && b->made * 4 >= BUILD[b->type].life * 3) {   /* rachas */
            Color crack = (Color){ 60, 36, 26, 255 };
            DrawLine(px + 9, py + 5, px + 13, py + 12, crack);
            DrawLine(px + 13, py + 12, px + 11, py + 17, crack);
            DrawLine(px + 22, py + 7, px + 19, py + 13, crack);
        }
    } else {
        DrawRectangle(px, py, TILE_SIZE * 2, TILE_SIZE * 2, (Color){ 74, 36, 28, 255 });
        DrawRectangle(px + 2, py + 2, TILE_SIZE * 2 - 4, TILE_SIZE * 2 - 4, (Color){ 168, 78, 56, 255 });
        DrawCircle(cx, cy, 10.0f, (Color){ 140, 62, 44, 255 });
        for (int i = 0; i < 4; i++)         /* linhas de tijolo */
            DrawRectangle(px + 3, py + 4 + i * 7, TILE_SIZE * 2 - 6, 1, (Color){ 112, 50, 36, 255 });
        Color fire = st == 0 ? (Color){ 255, 170, 50, 255 } : (Color){ 60, 30, 24, 255 };
        DrawRectangle(cx - 5, cy + 2, 10, 6, (Color){ 30, 18, 16, 255 });
        DrawRectangle(cx - 4, cy + 3, 8, 4, fire);
    }

    /* barra de progresso da receita */
    if (prog > 0.0f)
        DrawRectangle(px + 3, py + TILE_SIZE * 2 - 4, (int)((float)(TILE_SIZE * 2 - 6) * prog), 2,
                      (Color){ 120, 220, 120, 255 });

    /* luz de estado: verde = trabalha, vermelho = faltam ingredientes, amarelo = saida cheia */
    Color lamp = st == 0 ? (Color){ 90, 220, 110, 255 }
               : st == 1 ? (Color){ 220, 70, 70, 255 }
                         : (Color){ 235, 200, 60, 255 };
    DrawRectangle(px + TILE_SIZE * 2 - 7, py + 3, 4, 4, lamp);
}

/* Uma OBRA (estrutura ainda a ser construida): o icone da estrutura que vai
 * ficando a descoberto de baixo para cima, com contorno amarelo (a trabalhar),
 * vermelho (falta material) ou cinzento (a Luna esta longe / na fila) e uma
 * barra de progresso. */
static void draw_site(const Building *b)
{
    int w = BUILD[b->type].w * TILE_SIZE, h = BUILD[b->type].h * TILE_SIZE;
    int px = b->x * TILE_SIZE, py = b->y * TILE_SIZE;

    draw_build_icon(b->type, px, py, w);
    DrawRectangle(px, py, w, (int)((1.0f - b->build) * (float)h), (Color){ 16, 18, 22, 185 });

    Color edge = b->site_state == SITE_WORKING     ? (Color){ 255, 220, 60, 255 }
               : b->site_state == SITE_NO_MATERIAL ? (Color){ 230, 70, 70, 255 }
                                                   : (Color){ 150, 150, 160, 255 };
    DrawRectangleLinesEx((Rectangle){ (float)px, (float)py, (float)w, (float)h }, 1.0f, edge);
    DrawRectangle(px + 1, py + h - 3, (int)((float)(w - 2) * b->build), 2, (Color){ 120, 220, 120, 255 });
}

/* Uma DEMOLICAO: a estrutura vai desaparecendo de cima para baixo (mostra o terreno por
 * baixo), com contorno laranja (a Luna esta a demolir), vermelho (sem chao livre a volta
 * para o que sai) ou cinzento (longe / na fila) e uma barra de progresso. */
static void draw_demo(const Building *b)
{
    int tw = BUILD[b->type].w, th = BUILD[b->type].h;
    int w = tw * TILE_SIZE, h = th * TILE_SIZE;
    int px = b->x * TILE_SIZE, py = b->y * TILE_SIZE;
    int gone = (int)((1.0f - b->demo_p) * (float)h);        /* altura ja desfeita, em pixels */

    draw_build_icon(b->type, px, py, w);
    for (int j = 0; j < th; j++) {
        int rows = gone - j * TILE_SIZE;
        if (rows <= 0) continue;
        if (rows > TILE_SIZE) rows = TILE_SIZE;
        for (int i = 0; i < tw; i++) {
            const Tile *t = tile_at(b->x + i, b->y + j, 0);
            DrawRectangle(px + i * TILE_SIZE, py + j * TILE_SIZE, TILE_SIZE, rows, shaded(TERRAIN[t->terrain].color, t->shade));
        }
    }

    Color edge = b->demo_state == DEMO_WORKING ? (Color){ 255, 150, 50, 255 }
               : b->demo_state == DEMO_NO_ROOM ? (Color){ 230, 70, 70, 255 }
                                               : (Color){ 150, 150, 160, 255 };
    DrawRectangleLinesEx((Rectangle){ (float)px, (float)py, (float)w, (float)h }, 1.0f, edge);
    DrawRectangle(px + 1, py + h - 3, (int)((float)(w - 2) * (1.0f - b->demo_p)), 2, (Color){ 255, 150, 50, 255 });
}

/* Amontoado: uns montinhos da cor do item; mais montinhos quanto mais cheio */
static void draw_pile(const Building *b)
{
    static const int ox[8] = { 5, 10, 8, 3, 12, 6, 10, 8 };
    static const int oy[8] = { 11, 11, 7, 7, 7, 4, 4, 13 };
    int px = b->x * TILE_SIZE, py = b->y * TILE_SIZE;
    Color col = ITEM[b->pile_item].color;
    Color rim = (Color){ 20, 20, 26, 255 };
    if (b->pile_item == ITEM_TERRA && b->decay > TERRA_LIFE_S - 30.0f) {    /* a terra esvanece-se no fim */
        float f = (TERRA_LIFE_S - b->decay) / 30.0f;
        if (f < 0.1f) f = 0.1f;
        col.a = rim.a = (unsigned char)(255.0f * f);
    }
    float ratio = (float)container_vol_ml(&b->store) / (float)b->store.cap_vol_ml;
    int n = 3 + (int)(ratio * 40.0f);       /* 3 a 8 montinhos */
    if (n > 8) n = 8;

    DrawEllipse(px + 8, py + 12, 7, 3, (Color){ 0, 0, 0, 60 });
    for (int i = 0; i < n; i++) {
        DrawCircle(px + ox[i], py + oy[i], 3.0f, rim);
        DrawCircle(px + ox[i], py + oy[i], 2.0f, col);
    }
}

void draw_buildings(Camera2D cam, int screen_w, int screen_h)
{
    Vector2 tl = GetScreenToWorld2D((Vector2){ 0, 0 }, cam);
    Vector2 br = GetScreenToWorld2D((Vector2){ (float)screen_w, (float)screen_h }, cam);

    /* primeiro as esteiras e os amontoados (rasteiros), depois brocas, baus e maquinas por cima */
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < MAX_BUILDINGS; i++) {
            const Building *b = &buildings[i];
            if (b->type == B_NONE) continue;
            if ((b->type == B_ESTEIRA || b->type == B_AMONTOADO) != (pass == 0)) continue;
            if (demo_active(b)) { draw_demo(b); continue; }     /* a ser demolida */
            if (site_active(b)) { draw_site(b); continue; }     /* obra a meio */

            float x0 = (float)(b->x * TILE_SIZE), y0 = (float)(b->y * TILE_SIZE);
            if (x0 + BUILD[b->type].w * TILE_SIZE < tl.x || x0 > br.x ||
                y0 + BUILD[b->type].h * TILE_SIZE < tl.y || y0 > br.y) continue;

            if (b->type == B_ESTEIRA)    draw_belt(b);
            else if (b->type == B_AMONTOADO) draw_pile(b);
            else if (b->type == B_BROCA) draw_drill(b);
            else if (b->type == B_BAU)   draw_chest(b);
            else if (b->type == B_CONTENTOR) draw_container(b);
            else if (is_machine(b->type)) draw_machine(b);
        }
    }
}

/* "Fantasma" do que vai ser construido: verde = pode, vermelho = nao pode */
void draw_build_preview(const Player *p, Camera2D cam, int tool, int build_dir)
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
void draw_hover(const Player *p, Camera2D cam, int is_mining)
{
    Vector2 w = GetScreenToWorld2D(GetMousePosition(), cam);
    int tx = (int)floorf(w.x / TILE_SIZE);
    int ty = (int)floorf(w.y / TILE_SIZE);
    if (!in_bounds(tx, ty) || tile_at(tx, ty, 0)->ore == ORE_NONE) return;
    if (building_at(tx, ty) != NULL) return;        /* nao se minera por baixo de estruturas */

    Color c = (Color){ 220, 70, 70, 255 };
    if (tile_in_reach(p, tx, ty)) c = is_mining ? (Color){ 255, 220, 60, 255 } : RAYWHITE;

    DrawRectangleLinesEx((Rectangle){ (float)(tx * TILE_SIZE), (float)(ty * TILE_SIZE),
                                      (float)TILE_SIZE, (float)TILE_SIZE },
                         1.5f / cam.zoom, c);
}

/* ------------------------------------------------------------------ */
/* Interface (HUD)                                                     */
/* ------------------------------------------------------------------ */

/* Texto do modo de jogo atual */
const char *game_mode_label(void)
{
    return game_mode == MODE_SOBREVIVENCIA ? "Sobrevivencia" : "Criativo";
}

/* Icone de uma estrutura (para o menu de construcao e para a hotbar).
 * Desenhado numa grelha de 32 x 32 e escalado para "size" pixels. */
void draw_build_icon(int type, int x, int y, int size)
{
    float k = (float)size / 32.0f;
#define S(v) ((int)((float)(v) * k))
    float cx = (float)x + (float)size / 2.0f, cy = (float)y + (float)size / 2.0f;
    DrawRectangle(x, y, size, size, (Color){ 34, 38, 44, 255 });

    switch (type) {
    case B_ESTEIRA:
        DrawRectangle(x + S(3), y + S(3), S(26), S(26), (Color){ 64, 66, 74, 255 });
        DrawRectangle(x + S(4), y + S(4), S(24), S(24), (Color){ 96, 99, 108, 255 });
        draw_arrow(cx, cy, 0, 9.0f * k, (Color){ 60, 62, 72, 255 });
        break;
    case B_BROCA:
        DrawRectangle(x + S(2), y + S(2), S(28), S(28), (Color){ 46, 48, 56, 255 });
        DrawRectangle(x + S(4), y + S(4), S(24), S(24), (Color){ 112, 116, 128, 255 });
        DrawCircle((int)cx, (int)cy, 9.0f * k, (Color){ 60, 62, 72, 255 });
        DrawLineEx((Vector2){ cx - 7.0f * k, cy }, (Vector2){ cx + 7.0f * k, cy }, 2.0f * k, (Color){ 210, 214, 224, 255 });
        DrawLineEx((Vector2){ cx, cy - 7.0f * k }, (Vector2){ cx, cy + 7.0f * k }, 2.0f * k, (Color){ 210, 214, 224, 255 });
        break;
    case B_BAU:
        DrawRectangle(x + S(6), y + S(8), S(20), S(17), (Color){ 84, 52, 26, 255 });
        DrawRectangle(x + S(7), y + S(9), S(18), S(6), (Color){ 142, 94, 50, 255 });
        DrawRectangle(x + S(15), y + S(14), S(3), S(4), (Color){ 235, 190, 60, 255 });
        break;
    case B_CONTENTOR:
        DrawRectangle(x + S(2), y + S(2), S(28), S(28), (Color){ 36, 54, 72, 255 });
        DrawRectangle(x + S(3), y + S(3), S(26), S(26), (Color){ 62, 100, 134, 255 });
        for (int i = 0; i < 5; i++) DrawRectangle(x + S(6 + i * 5), y + S(5), S(1) > 0 ? S(1) : 1, S(20), (Color){ 46, 78, 106, 255 });
        DrawRectangle(x + S(15), y + S(5), S(2), S(20), (Color){ 30, 40, 52, 255 });
        break;
    case B_CARVOEIRA:
        DrawRectangle(x + S(2), y + S(2), S(28), S(28), (Color){ 52, 38, 28, 255 });
        DrawCircle((int)cx, (int)cy, 12.0f * k, (Color){ 92, 66, 44, 255 });
        DrawCircle((int)cx, (int)cy - S(1), 8.0f * k, (Color){ 70, 50, 34, 255 });
        break;
    case B_FORNO_RUPESTRE:
        DrawRectangle(x + S(2), y + S(2), S(28), S(28), (Color){ 112, 70, 46, 255 });
        DrawCircle((int)cx, (int)cy, 13.0f * k, (Color){ 196, 130, 88, 255 });
        DrawRectangle(x + S(11), y + S(18), S(10), S(7), (Color){ 40, 24, 18, 255 });
        DrawRectangle(x + S(12), y + S(20), S(8), S(4), (Color){ 255, 170, 50, 255 });
        break;
    case B_FORNO:
        DrawRectangle(x + S(2), y + S(2), S(28), S(28), (Color){ 74, 36, 28, 255 });
        DrawRectangle(x + S(4), y + S(4), S(24), S(24), (Color){ 168, 78, 56, 255 });
        DrawCircle((int)cx, (int)cy, 10.0f * k, (Color){ 140, 62, 44, 255 });
        DrawRectangle(x + S(11), y + S(18), S(10), S(7), (Color){ 30, 18, 16, 255 });
        DrawRectangle(x + S(12), y + S(20), S(8), S(4), (Color){ 255, 170, 50, 255 });
        break;
    default:
        DrawCircle((int)cx - S(4), (int)cy + S(4), 4.0f * k, (Color){ 200, 200, 200, 255 });
        DrawCircle((int)cx + S(4), (int)cy + S(4), 4.0f * k, (Color){ 170, 170, 170, 255 });
        break;
    }
#undef S
}

/* Custo de uma estrutura numa linha ("40 Argila"), a verde se a mochila o tem e
 * a vermelho se nao. Em criativo diz so "gratis". Devolve a largura gasta. */
int draw_cost(int x, int y, int type, int font)
{
    if (game_mode != MODE_SOBREVIVENCIA) {
        DrawText("gratis (criativo)", x, y, font, LIGHTGRAY);
        return MeasureText("gratis (criativo)", font);
    }
    int cx = x;
    for (int k = 0; k < COST_SLOTS; k++) {
        const Cost *c = &BUILD[type].cost[k];
        int need = cost_units(type, k);
        if (need == 0) continue;
        int have = backpack.n[c->item];
        Color col = have >= need ? (Color){ 140, 230, 150, 255 }          /* tem tudo */
                  : have > 0     ? (Color){ 235, 200, 80, 255 }           /* tem parte */
                                 : (Color){ 235, 100, 100, 255 };         /* nao tem */
        char buf[48];
        snprintf(buf, sizeof buf, "%d %s", need, ITEM[c->item].name);
        DrawRectangle(cx, y + 1, font - 3, font - 3, ITEM[c->item].color);
        DrawText(buf, cx + font, y, font, col);
        cx += font + MeasureText(buf, font) + 12;
    }
    return cx - x;
}

/* A hotbar: a estrutura selecionada (ou a mao), a meio em baixo. Recebe o tamanho
 * "virtual" do ecra (ja dividido pelo fator da interface). */
static void draw_hotbar(int tool, int build_dir, int vw, int vh)
{
    static const char *dir_name[4] = { "direita", "baixo", "esquerda", "cima" };
    const int sz = 64;
    int x = vw / 2 - sz / 2, y = vh - 150;
    char line[96];

    DrawRectangle(x - 6, y - 6, sz + 12, sz + 12, (Color){ 0, 0, 0, 170 });
    DrawRectangleLines(x - 6, y - 6, sz + 12, sz + 12, tool != 0 ? (Color){ 255, 220, 60, 255 } : (Color){ 110, 114, 126, 255 });

    if (tool != 0) {
        draw_build_icon(tool, x, y, sz);
        if (tool == B_ESTEIRA) snprintf(line, sizeof line, "%s  (R roda: %s)", BUILD[tool].name, dir_name[build_dir & 3]);
        else                   snprintf(line, sizeof line, "%s", BUILD[tool].name);
        DrawText(line, vw / 2 - MeasureText(line, 16) / 2, y - 26, 16, (Color){ 255, 220, 60, 255 });

        int w = draw_cost(0, -100, tool, 14);       /* mede (fora do ecra) */
        draw_cost(vw / 2 - w / 2, y + sz + 10, tool, 14);

        if (game_mode == MODE_SOBREVIVENCIA) {
            float secs = build_seconds(tool);
            snprintf(line, sizeof line, secs < 10.0f ? "Obra %.1f s | material %.1f L, %.1f kg" : "Obra %.0f s | material %.0f L, %.0f kg",
                     (double)secs, (double)build_cost_vol_ml(tool) / 1000.0, (double)build_cost_mass_g(tool) / 1000.0);
            DrawText(line, vw / 2 - MeasureText(line, 12) / 2, y + sz + 28, 12, LIGHTGRAY);
        }
        DrawText("Esc: largar", vw / 2 - MeasureText("Esc: largar", 12) / 2, y + sz + 44, 12, LIGHTGRAY);
    } else {
        DrawText("Mao", vw / 2 - MeasureText("Mao", 18) / 2, y + sz / 2 - 9, 18, LIGHTGRAY);
        DrawText("Mao (minerar)", vw / 2 - MeasureText("Mao (minerar)", 16) / 2, y - 26, 16, LIGHTGRAY);
        DrawText("Q: escolher o que construir", vw / 2 - MeasureText("Q: escolher o que construir", 12) / 2, y + sz + 10, 12, LIGHTGRAY);
    }
}

/* Barra de enchimento: verde, amarela quando passa dos 75 %, vermelha quando enche */
void draw_fill_bar(int x, int y, int w, int h, float ratio)
{
    if (ratio < 0.0f) ratio = 0.0f;
    if (ratio > 1.0f) ratio = 1.0f;
    Color c = ratio >= 1.0f ? (Color){ 220, 70, 70, 255 }
            : ratio >= 0.75f ? (Color){ 235, 200, 60, 255 }
                             : (Color){ 110, 210, 120, 255 };
    DrawRectangle(x, y, w, h, (Color){ 40, 40, 48, 255 });
    DrawRectangle(x, y, (int)((float)w * ratio), h, c);
}

const char *carry_label(int state)
{
    static const char *label[3] = {
        "carga leve: podes correr",
        "carga pesada: andas devagar",
        "sobrecarregada: nao consegues andar"
    };
    return label[state];
}

/* ------------------------------------------------------------------ */
/* Tamanho da interface                                                */
/* ------------------------------------------------------------------ */
/* Todo o HUD e os menus desenham-se num ecra "virtual" mais pequeno e sao
 * ampliados pelo fator da interface (BeginMode2D com zoom = fator). O rato dos
 * menus divide-se pelo mesmo fator. Se a janela for pequena demais para o fator
 * pedido, usa-se o maior que cabe (ui_fit): nunca fica nada cortado. */

float ui_scale = UI_SCALE_DEFAULT;

static const float UI_STEPS[] = { 1.0f, 1.25f, 1.5f, 1.75f, 2.0f, 2.5f, 3.0f };
#define UI_STEP_COUNT ((int)(sizeof UI_STEPS / sizeof UI_STEPS[0]))

void ui_scale_step(int dir)
{
    int cur = 0;
    for (int i = 0; i < UI_STEP_COUNT; i++)                 /* o degrau mais perto do atual */
        if (fabsf(UI_STEPS[i] - ui_scale) < fabsf(UI_STEPS[cur] - ui_scale)) cur = i;
    cur += dir > 0 ? 1 : -1;
    if (cur < 0) cur = 0;
    if (cur >= UI_STEP_COUNT) cur = UI_STEP_COUNT - 1;
    ui_scale = UI_STEPS[cur];
}

float ui_fit(int screen_w, int screen_h, int need_w, int need_h)
{
    float s = ui_scale;
    float fw = (float)screen_w / (float)need_w;
    float fh = (float)screen_h / (float)need_h;
    if (fw < s) s = fw;
    if (fh < s) s = fh;
    return s < 0.5f ? 0.5f : s;
}

/* ------------------------------------------------------------------ */
/* Ajuda (tecla H)                                                     */
/* ------------------------------------------------------------------ */

void draw_help(int screen_w, int screen_h)
{
    static const char *lines[] = {
        "WASD ............ mover (Shift: correr)",
        "Botao esquerdo .. minerar (sem nada selecionado) / construir",
        "Botao direito ... demolir",
        "Q ............... menu de construcao",
        "R ............... roda a estrutura (Shift+R: ao contrario)",
        "E ............... mochila; com o rato numa estrutura: o menu dela",
        "Esc ............. fecha menus / larga a estrutura selecionada",
        "M ............... modo criativo / sobrevivencia",
        "F6 / F7 ......... interface MENOR / MAIOR (ou Ctrl + roda do rato)",
        "Roda do rato .... zoom do mapa",
        "TAB ............. camara livre",
        "G ............... grelha",
        "H ............... esta ajuda",
        "F5 .............. gerar outro mapa",
        "F11 ............. ecra inteiro",
    };
    const int n = (int)(sizeof lines / sizeof lines[0]);
    float s = ui_fit(screen_w, screen_h, 640, 520);
    int vw = (int)((float)screen_w / s), vh = (int)((float)screen_h / s);
    Camera2D c = { 0 };
    c.zoom = s;

    DrawRectangle(0, 0, screen_w, screen_h, (Color){ 0, 0, 0, 150 });
    BeginMode2D(c);
    int pw = 600, ph = 70 + n * 24 + 40;
    int x = (vw - pw) / 2, y = (vh - ph) / 2;
    DrawRectangle(x, y, pw, ph, (Color){ 24, 28, 34, 250 });
    DrawRectangleLines(x, y, pw, ph, (Color){ 120, 126, 140, 255 });
    DrawText("Ajuda: controlos", x + 16, y + 12, 22, RAYWHITE);
    for (int i = 0; i < n; i++)
        DrawText(lines[i], x + 16, y + 50 + i * 24, 16, RAYWHITE);
    char line[96];
    snprintf(line, sizeof line, "Tamanho da interface: %d%%   (H ou Esc para fechar)", (int)(ui_scale * 100.0f + 0.5f));
    DrawText(line, x + 16, y + ph - 28, 14, (Color){ 255, 220, 60, 255 });
    EndMode2D();
}

/* ------------------------------------------------------------------ */
/* HUD                                                                 */
/* ------------------------------------------------------------------ */

#define HUD_W      540          /* largura do painel de cima, no ecra virtual */
#define HUD_LINES   24

typedef struct { char text[200]; Color col; int font; } HudLine;
static HudLine hud_line[HUD_LINES];
static int     hud_n;

static void hud_push(const char *text, int font, Color col)
{
    if (hud_n >= HUD_LINES) return;
    snprintf(hud_line[hud_n].text, sizeof hud_line[hud_n].text, "%s", text);
    hud_line[hud_n].font = font;
    hud_line[hud_n].col = col;
    hud_n++;
}

/* Acrescenta um texto ao painel, partindo-o em varias linhas se for largo */
static void hud_add(const char *text, int font, Color col)
{
    char cur[200] = "";
    const char *p = text;
    while (*p != '\0') {
        char word[100];
        int n = 0;
        while (*p == ' ') p++;
        while (*p != '\0' && *p != ' ' && n < 99) word[n++] = *p++;
        word[n] = '\0';
        if (n == 0) break;

        char trial[200];
        if (cur[0] != '\0') snprintf(trial, sizeof trial, "%.99s %.99s", cur, word);
        else                snprintf(trial, sizeof trial, "%.99s", word);
        if (cur[0] != '\0' && MeasureText(trial, font) > HUD_W - 24) {
            hud_push(cur, font, col);
            snprintf(cur, sizeof cur, "%.99s", word);
        } else {
            snprintf(cur, sizeof cur, "%.199s", trial);
        }
    }
    if (cur[0] != '\0') hud_push(cur, font, col);
}

void draw_hud(Camera2D cam, int screen_w, int screen_h,
              const Player *p, int free_cam, int tool, int build_dir,
              const char *notice)
{
    /* o rato no MUNDO (a camara do mapa nao tem nada a ver com a escala da interface) */
    Vector2 world_pos = GetScreenToWorld2D(GetMousePosition(), cam);
    int tx = (int)floorf(world_pos.x / TILE_SIZE);
    int ty = (int)floorf(world_pos.y / TILE_SIZE);

    float s = ui_fit(screen_w, screen_h, HUD_W + 240, 460);
    int vw = (int)((float)screen_w / s), vh = (int)((float)screen_h / s);
    Camera2D ui = { 0 };
    ui.zoom = s;
    BeginMode2D(ui);

    char line[200];
    const Color white = RAYWHITE, yellow = (Color){ 255, 220, 60, 255 };
    hud_n = 0;

    hud_add("A CLAREIRA DA LUNA  -  Marco 3c", 16, white);
    snprintf(line, sizeof line, "Modo: %s  [M]", game_mode_label());
    hud_add(line, 16, game_mode == MODE_SOBREVIVENCIA ? (Color){ 235, 160, 90, 255 } : (Color){ 120, 200, 255, 255 });

    if (in_bounds(tx, ty)) {
        const Tile *t = tile_at(tx, ty, 0);
        if (t->ore != ORE_NONE) {
            snprintf(line, sizeof line, "Rato: (%d, %d)  %s | %s: %d/%d", tx, ty,
                     TERRAIN[t->terrain].name, ITEM[ORE[t->ore].item].name,
                     (int)t->ore_amount, ORE[t->ore].richest);
        } else {
            snprintf(line, sizeof line, "Rato: (%d, %d)  %s", tx, ty, TERRAIN[t->terrain].name);
        }
    } else {
        snprintf(line, sizeof line, "Rato: fora do mapa");
    }
    hud_add(line, 16, white);

    snprintf(line, sizeof line, "Luna: (%d, %d)   %s",
             (int)floorf(p->x / TILE_SIZE), (int)floorf(p->y / TILE_SIZE),
             free_cam ? "[camara livre]" : "[a seguir]");
    hud_add(line, 16, white);

    snprintf(line, sizeof line, "Zoom %.1fx   FPS %d   seed %u", cam.zoom, GetFPS(), (unsigned)current_seed);
    hud_add(line, 14, LIGHTGRAY);

    if (tool == 0) hud_add("Selecionado: nada (a mao minera)   [Q] escolher o que construir", 16, yellow);
    else {
        snprintf(line, sizeof line, "Selecionado: %s   [Esc] largar", BUILD[tool].name);
        hud_add(line, 16, yellow);
    }

    /* Info do objeto sob o rato */
    line[0] = '\0';
    if (in_bounds(tx, ty) && building_at(tx, ty) != NULL) {
        const Building *b = building_at(tx, ty);
        if (demo_active(b)) {
            static const char *dst_name[4] = { "a demolir", "PARADA: sem chao livre a volta", "parada: a Luna esta longe", "na fila" };
            snprintf(line, sizeof line, "%s em demolicao %d%%: %s", BUILD[b->type].name,
                     (int)((1.0f - b->demo_p) * 100.0f), dst_name[b->demo_state & 3]);
        } else if (site_active(b)) {
            static const char *st_name[4] = { "a construir", "PARADA: falta material", "parada: a Luna esta longe", "na fila" };
            snprintf(line, sizeof line, "%s em obra %d%%: %s  (E: depositar material)",
                     BUILD[b->type].name, (int)(b->build * 100.0f), st_name[b->site_state & 3]);
        } else if (is_pile(b->type)) {
            snprintf(line, sizeof line, "Amontoado de %s: %d un (%.1f L, %.1f kg)  (E: abrir)",
                     ITEM[b->pile_item].name, container_units(&b->store),
                     (double)container_vol_ml(&b->store) / 1000.0, (double)container_mass_g(&b->store) / 1000.0);
        } else if (is_storage(b->type)) {
            int n = snprintf(line, sizeof line, "%s %.1f/%.0f L, %.1f kg:", BUILD[b->type].name,
                             (double)container_vol_ml(&b->store) / 1000.0,
                             (double)b->store.cap_vol_ml / 1000.0,
                             (double)container_mass_g(&b->store) / 1000.0);
            for (int i = 1; i < ITEM_COUNT && n < (int)sizeof line - 24; i++)
                if (b->store.n[i] > 0)
                    n += snprintf(line + n, sizeof line - (size_t)n, " %s %d", ITEM[i].name, b->store.n[i]);
            snprintf(line + n, sizeof line - (size_t)n, "  (E: abrir)");
        } else if (b->type == B_BROCA) {
            static const char *st_name[3] = { "a trabalhar", "sem minerio por baixo", "saida cheia" };
            snprintf(line, sizeof line, "Broca: %s  (E: abrir)", st_name[drill_status(b)]);
        } else if (b->type == B_ESTEIRA) {
            snprintf(line, sizeof line, "Esteira: %d item(ns)", b->n);
        } else if (is_machine(b->type)) {
            static const char *st_name[3] = { "a trabalhar", "parado (faltam ingredientes)", "saida cheia" };
            int n = snprintf(line, sizeof line, "%s: %s", BUILD[b->type].name, st_name[machine_status(b)]);
            if (b->recipe != 0)
                n = snprintf(line, sizeof line, "%s: %s %d%%", BUILD[b->type].name,
                             RECIPES[b->recipe - 1].name,
                             (int)(100.0f * b->work / RECIPES[b->recipe - 1].period));
            n += snprintf(line + n, sizeof line - (size_t)n, "  | dentro:");
            for (int i = 1; i < ITEM_COUNT && n < (int)sizeof line - 24; i++)
                if (b->in[i] > 0) n += snprintf(line + n, sizeof line - (size_t)n, " %s %d", ITEM[i].name, b->in[i]);
            n += snprintf(line + n, sizeof line - (size_t)n, "  | pronto:");
            for (int i = 1; i < ITEM_COUNT && n < (int)sizeof line - 24; i++)
                if (b->out[i] > 0) n += snprintf(line + n, sizeof line - (size_t)n, " %s %d", ITEM[i].name, b->out[i]);
            if (BUILD[b->type].life > 0 && n < (int)sizeof line - 24)
                snprintf(line + n, sizeof line - (size_t)n, "  | feitos %d/%d", b->made, BUILD[b->type].life);
        }
    }
    if (line[0] != '\0') hud_add(line, 16, white);

    /* o painel de cima: tantas linhas quantas forem precisas */
    int ph = 10;
    for (int i = 0; i < hud_n; i++) ph += hud_line[i].font + 6;
    DrawRectangle(0, 0, HUD_W, ph, (Color){ 0, 0, 0, 150 });
    int y = 8;
    for (int i = 0; i < hud_n; i++) {
        DrawText(hud_line[i].text, 10, y, hud_line[i].font, hud_line[i].col);
        y += hud_line[i].font + 6;
    }

    /* Mochila (canto superior direito): so o resumo; a lista esta no menu (E) */
    int bx = vw - 230;
    int mass = container_mass_g(&backpack);
    int state = carry_state(mass);
    DrawRectangle(bx, 0, 230, 102, (Color){ 0, 0, 0, 150 });
    DrawText("Mochila  [E] abrir", bx + 10, 6, 14, RAYWHITE);
    snprintf(line, sizeof line, "Volume %.1f / %.0f L", (double)container_vol_ml(&backpack) / 1000.0,
             (double)backpack.cap_vol_ml / 1000.0);
    DrawText(line, bx + 10, 26, 14, RAYWHITE);
    draw_fill_bar(bx + 10, 43, 210, 6, (float)container_vol_ml(&backpack) / (float)backpack.cap_vol_ml);
    snprintf(line, sizeof line, "Peso %.1f / %.0f kg", (double)mass / 1000.0, (double)backpack.cap_mass_g / 1000.0);
    DrawText(line, bx + 10, 54, 14, RAYWHITE);
    draw_fill_bar(bx + 10, 71, 210, 6, (float)mass / (float)backpack.cap_mass_g);
    DrawText(carry_label(state), bx + 10, 82, 12,
             state == 0 ? LIGHTGRAY : state == 1 ? (Color){ 235, 200, 60, 255 } : (Color){ 220, 70, 70, 255 });

    /* Aviso curto (mochila cheia, demolicao bloqueada...), por cima da hotbar */
    if (notice != NULL && notice[0] != '\0')
        DrawText(notice, vw / 2 - MeasureText(notice, 18) / 2, vh - 196, 18, yellow);

    draw_hotbar(tool, build_dir, vw, vh);

    snprintf(line, sizeof line, "H: ajuda   |   F6/F7: tamanho da interface (%d%%)", (int)(ui_scale * 100.0f + 0.5f));
    DrawText(line, 10, vh - 22, 14, RAYWHITE);

    EndMode2D();
}
