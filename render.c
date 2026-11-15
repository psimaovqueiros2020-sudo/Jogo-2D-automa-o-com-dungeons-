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

void draw_buildings(Camera2D cam, int screen_w, int screen_h)
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

    Color c = (Color){ 220, 70, 70, 255 };
    if (tile_in_reach(p, tx, ty)) c = is_mining ? (Color){ 255, 220, 60, 255 } : RAYWHITE;

    DrawRectangleLinesEx((Rectangle){ (float)(tx * TILE_SIZE), (float)(ty * TILE_SIZE),
                                      (float)TILE_SIZE, (float)TILE_SIZE },
                         1.5f / cam.zoom, c);
}

/* ------------------------------------------------------------------ */
/* Interface (HUD)                                                     */
/* ------------------------------------------------------------------ */

void draw_hud(Camera2D cam, int screen_w, int screen_h,
              const Player *p, int free_cam, int tool, int build_dir)
{
    Vector2 world_pos = GetScreenToWorld2D(GetMousePosition(), cam);
    int tx = (int)floorf(world_pos.x / TILE_SIZE);
    int ty = (int)floorf(world_pos.y / TILE_SIZE);

    DrawRectangle(0, 0, 780, 148, (Color){ 0, 0, 0, 150 });
    DrawText("A CLAREIRA DA LUNA  -  Marco 3b", 10, 8, 16, RAYWHITE);

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
        } else if (is_machine(b->type)) {
            static const char *st_name[3] = { "a trabalhar", "parado (faltam ingredientes)", "saida cheia" };
            int n = snprintf(line, sizeof line, "%s: %s", BUILD[b->type].name, st_name[machine_status(b)]);
            if (b->recipe != 0)
                n = snprintf(line, sizeof line, "%s: %s %d%%", BUILD[b->type].name,
                             RECIPES[b->recipe - 1].name,
                             (int)(100.0f * b->work / RECIPES[b->recipe - 1].period));
            n += snprintf(line + n, sizeof line - (size_t)n, "  | dentro:");
            for (int i = 1; i < ORE_COUNT && n < (int)sizeof line - 24; i++)
                if (b->in[i] > 0) n += snprintf(line + n, sizeof line - (size_t)n, " %s %d", ORE[i].name, b->in[i]);
            n += snprintf(line + n, sizeof line - (size_t)n, "  | pronto:");
            for (int i = 1; i < ORE_COUNT && n < (int)sizeof line - 24; i++)
                if (b->out[i] > 0) n += snprintf(line + n, sizeof line - (size_t)n, " %s %d", ORE[i].name, b->out[i]);
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

    DrawText("WASD mover | shift correr | 1 mao 2 broca 3 esteira 4 bau 5 carvoeira 6 forno | Q/E rodar | esq. usar | dir. demolir | F pegar do bau | roda zoom | TAB camara | G grelha | R novo mapa",
             10, screen_h - 22, 14, RAYWHITE);
}
