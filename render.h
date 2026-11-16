/*
 * render.h  -  Desenho (implementado em render.c). Usa a raylib.
 */
#ifndef RENDER_H
#define RENDER_H

#include "raylib.h"
#include "logic.h"      /* Player */

/* Cria / liberta a textura do terreno (chamar depois de abrir a janela) */
void render_init(void);
void render_shutdown(void);

/* Terreno */
void build_terrain_texture(void);   /* redesenha o mapa todo na textura */
void flush_dirty(void);             /* redesenha so os tiles marcados com mark_dirty() */
void draw_world(Camera2D cam, int screen_w, int screen_h, int show_grid);

/* Coisas do mundo */
void draw_player(const Player *p);
void draw_buildings(Camera2D cam, int screen_w, int screen_h);
void draw_build_preview(const Player *p, Camera2D cam, int tool, int build_dir);
void draw_hover(const Player *p, Camera2D cam, int is_mining);

/* Interface. screen_w/h sao o tamanho REAL da janela: a escala da interface
 * (ui_scale) e aplicada la dentro. */
void draw_hud(Camera2D cam, int screen_w, int screen_h,
              const Player *p, int free_cam, int tool, int build_dir,
              const char *notice);
void draw_help(int screen_w, int screen_h);     /* a ajuda (tecla H) */

/* ---- Tamanho da interface ------------------------------------------------
 * ui_scale = o tamanho pedido (1.0 = 100%). F6/F7 mudam-no por degraus. Cada
 * ecra (HUD, menus, ajuda) usa ui_fit(): o pedido, ou o maior que cabe na janela. */
#define UI_SCALE_DEFAULT 1.5f
extern float ui_scale;
void  ui_scale_step(int dir);       /* +1 = maior, -1 = menor (degraus: 100% a 300%) */
float ui_fit(int screen_w, int screen_h, int need_w, int need_h);

/* Pecas partilhadas com os menus */
void        draw_fill_bar(int x, int y, int w, int h, float ratio);
const char *carry_label(int state);     /* texto do estado de carga (0, 1 ou 2) */
const char *game_mode_label(void);      /* "Criativo" ou "Sobrevivencia" */
void        draw_build_icon(int type, int x, int y, int size);   /* icone de uma estrutura */
/* Custo de uma estrutura numa linha (verde = a mochila tem, vermelho = falta).
 * Em criativo escreve "gratis". Devolve a largura em pixels. */
int         draw_cost(int x, int y, int type, int font);

/* Tile que esta sob o rato (precisa da camara) */
void mouse_tile(Camera2D cam, int *tx, int *ty);

#endif
