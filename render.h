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

/* Interface */
void draw_hud(Camera2D cam, int screen_w, int screen_h,
              const Player *p, int free_cam, int tool, int build_dir);

/* Tile que esta sob o rato (precisa da camara) */
void mouse_tile(Camera2D cam, int *tx, int *ty);

#endif
