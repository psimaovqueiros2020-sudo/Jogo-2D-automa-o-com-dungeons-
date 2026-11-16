/*
 * menu.h  -  OS MENUS (implementado em menu.c). Usa a raylib.
 *
 *   Q ........ menu de CONSTRUCAO: escolhe-se uma estrutura (fica selecionada
 *              na hotbar; Esc deseleciona).
 *   E ........ com o rato em cima de uma estrutura (ou amontoado): abre o menu
 *              dessa estrutura ao lado da mochila. Sem nada em cima: abre a
 *              mochila com o chao ao lado (para largar itens: cria um amontoado).
 *
 * A logica das transferencias esta em logic.c (container_move, machine_load,
 * drop_to_ground...); aqui so ha o desenho e os cliques.
 */
#ifndef MENU_H
#define MENU_H

#include "logic.h"      /* Player */

int  menu_is_open(void);
void menu_close(void);

void menu_toggle_build(void);               /* tecla Q */
void menu_open_building(int tx, int ty);    /* tecla E sobre a estrutura que ocupa o tile (tx, ty) */
void menu_open_backpack(int gx, int gy);    /* tecla E sem estrutura: mochila + chao do tile (gx, gy); gx < 0 = nao da para largar */

/* A estrutura que se escolheu no menu de construcao (B_NONE se nao escolheu
 * nenhuma desde a ultima chamada: so devolve a escolha uma vez). */
int  menu_picked_build(void);

/* Trata dos cliques e fecha o menu sozinho se a estrutura desapareceu ou a Luna
 * se afastou. Chamar uma vez por frame. Devolve 1 se ha um menu aberto (nesse
 * caso o jogo ignora o rato: nao se minera nem constroi). */
int  menu_update(const Player *p, int screen_w, int screen_h);

void menu_draw(int screen_w, int screen_h);

#endif
