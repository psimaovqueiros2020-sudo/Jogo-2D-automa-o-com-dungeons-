/*
 * A CLAREIRA DA LUNA  -  main.c: teclado, rato, camara e ciclo do jogo
 * ------------------------------------
 * Linguagem: C (C99)   Biblioteca: raylib
 *
 * O jogo esta dividido em modulos (ver README, secoes 0 e 4):
 *   items.c  - tabela dos itens (volume, massa)
 *   world.c  - terreno, minerios, ruido e geracao do setor
 *   logic.c  - objetos, custos, receitas, maquinas, simulacao, mochila (SEM raylib)
 *   render.c - tudo o que desenha (terreno, objetos, HUD, hotbar)
 *   menu.c   - os menus: construcao (Q) e mochila + estrutura (E)
 *   main.c   - (este) input, camara e o ciclo do jogo
 *
 * Marco 3c (jogabilidade):
 *   - Controlos novos (ver abaixo) e menu de CONSTRUCAO (Q) com hotbar.
 *   - MODOS: criativo (construir e gratis) e sobrevivencia (custa itens da
 *     mochila; demolir devolve-os). O jogo arranca em sobrevivencia (DEFAULT_MODE).
 *   - OBRAS: em sobrevivencia pousa-se sempre a estrutura; abre-se o menu da
 *     obra (E) e deposita-se la o material. A Luna constroi-a aos poucos (demora
 *     segundo a complexidade) com o que la esta e, se faltar, a obra fica a meio
 *     e nao funciona. Custos realistas.
 *   - INTERFACE AJUSTAVEL (F6 menor / F7 maior) e ajuda (H).
 *   - FORNO RUPESTRE: so de argila, so cozinha tijolos, parte aos 20.
 *   - Menus de todas as estruturas (E com o rato em cima): meter ingredientes
 *     a mao nas maquinas, tirar produtos, ver o estado.
 *   - Largar itens no chao (E sem estrutura): cria um AMONTOADO desse item.
 *
 * Controlos:
 *   W A S D ............. mover a Luna (SHIFT = correr)
 *   Botao esquerdo ...... sem estrutura selecionada: minera a jazida/arvore sob
 *                         o rato (ao alcance). Com uma estrutura: constroi
 *                         (segurar e arrastar faz uma linha de esteiras)
 *   Botao direito ....... demolir o que esta sob o rato: marca a demolicao e a Luna
 *                         desfaz-a ao pe; o material sai para amontoados no chao
 *                         (madeira e tijolo so em parte, ferro enferrujado, o resto
 *                         vira terra). Amontoados apanham-se na hora
 *   Q ................... menu de construcao: escolhe a estrutura
 *   R ................... roda a estrutura antes de a construir (Shift+R ao contrario)
 *   E ................... mochila; com o rato sobre uma estrutura abre o menu dela;
 *                         sem estrutura, abre a mochila com o chao ao lado (largar itens)
 *   ESC ................. fecha o menu; sem menu, larga a estrutura selecionada
 *                         (o ESC nao fecha o jogo: fecha-se na janela)
 *   M ................... alternar modo criativo / sobrevivencia
 *   F6 / F7 ............. interface MENOR / MAIOR (tambem Ctrl + roda do rato)
 *   H ................... ajuda com todos os controlos
 *   Roda do rato ........ zoom
 *   TAB ................. camara livre (WASD move a camara, botao
 *                         direito/meio arrasta o mapa) / voltar a seguir a Luna
 *   G ................... mostrar/esconder grelha
 *   F5 .................. gerar outra versao do mapa (muda a seed)
 *   F11 ................. ecra inteiro
 *
 * Nota para o futuro: o mundo ja e guardado como world[z][y][x].
 * Hoje so existe a camada z = 0 (superficie). As cavernas serao z = 1, 2, ...
 */

#include "raylib.h"
#include "world.h"
#include "logic.h"
#include "render.h"
#include "menu.h"
#include <math.h>
#include <stdlib.h>      /* abs() */
#include <stdio.h>       /* snprintf() */

/* Modo de jogo com que o jogo arranca (M alterna durante o jogo) */
#define DEFAULT_MODE MODE_SOBREVIVENCIA

/* Estado da interface (so existe aqui; quem desenha recebe-o por parametro) */
static int   tool;              /* 0 = nada selecionado (a mao minera); senao um BuildType */
static int   build_dir;         /* direcao com que se coloca a esteira   */
static float mine_timer;
static int   is_mining;
static int   ignore_mouse;      /* 1 = depois de um menu, ignorar o rato ate largar os botoes */
static int   last_bx = -1, last_by = -1;     /* ultimo tile construido ao arrastar */

/* Aviso curto no ecra (mochila cheia, demolicao bloqueada...) */
static char  notice[96];
static float notice_timer;

static void set_notice(const char *text)
{
    snprintf(notice, sizeof notice, "%s", text);
    notice_timer = 1.5f;
}

/* ------------------------------------------------------------------ */
/* Jogador: teclado                                                    */
/* ------------------------------------------------------------------ */

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

    /* O peso da mochila decide como a Luna anda (ver logic.h) */
    int   mass   = container_mass_g(&backpack);
    float factor = carry_speed_factor(mass);
    if (factor <= 0.0f) {
        set_notice("Sobrecarregada: nao consegues andar. Esvazia a mochila (E)");
        return;
    }
    float speed = PLAYER_SPEED * factor;
    if ((IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) && carry_can_run(mass))
        speed *= PLAYER_RUN;

    player_move(p, dx * speed * dt, dy * speed * dt);
}

/* ------------------------------------------------------------------ */
/* Extracao a mao: rato                                                */
/* ------------------------------------------------------------------ */

static void update_mining(const Player *p, Camera2D cam, float dt)
{
    is_mining = 0;

    Vector2 w = GetScreenToWorld2D(GetMousePosition(), cam);
    int tx = (int)floorf(w.x / TILE_SIZE);
    int ty = (int)floorf(w.y / TILE_SIZE);

    if (!IsMouseButtonDown(MOUSE_BUTTON_LEFT) || !in_bounds(tx, ty) ||
        tile_at(tx, ty, 0)->ore == ORE_NONE || building_at(tx, ty) != NULL ||
        !tile_in_reach(p, tx, ty)) {
        mine_timer = 0.0f;
        return;
    }

    if (mine_blocked(tx, ty)) {             /* mochila sem espaco (volume ou peso) */
        set_notice("Mochila cheia (volume ou peso): abre-a com E");
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
/* Abrir um bau ou contentor                                           */
/* ------------------------------------------------------------------ */

/* Tecla E. Com o rato em cima de uma estrutura (ao alcance das maos) abre o
 * menu dela. Senao abre a mochila com o chao ao lado: o tile sob o rato, se
 * estiver ao alcance, ou o tile onde a Luna esta. */
static void open_menu_at_mouse(const Player *p, Camera2D cam)
{
    int tx, ty;
    mouse_tile(cam, &tx, &ty);

    if (in_bounds(tx, ty) && building_at(tx, ty) != NULL) {
        if (!tile_in_reach(p, tx, ty)) {
            set_notice("Fica mais perto para abrir");
            return;
        }
        menu_open_building(tx, ty);
        return;
    }

    int gx = (int)floorf(p->x / TILE_SIZE), gy = (int)floorf(p->y / TILE_SIZE);
    if (in_bounds(tx, ty) && tile_in_reach(p, tx, ty) && ground_ok(tx, ty)) { gx = tx; gy = ty; }
    if (!ground_ok(gx, gy)) gx = gy = -1;       /* nao da para largar aqui */
    menu_open_backpack(gx, gy);
}

/* ------------------------------------------------------------------ */
/* Construcao: rato                                                    */
/* ------------------------------------------------------------------ */

static void update_building(const Player *p, Camera2D cam, int free_cam)
{
    int tx, ty;
    mouse_tile(cam, &tx, &ty);
    int ok = in_bounds(tx, ty) && tile_within(p, tx, ty, BUILD_REACH_PX);

    /* Botao direito: demolir (em camara livre serve para arrastar o mapa).
     * Um amontoado apanha-se na hora; o resto MARCA-se para demolir: a Luna desfaz-o
     * ao pe dela e o que tinha sai para amontoados no chao (ver logic.h, demolicao). */
    if (!free_cam && ok && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
        const Building *target = building_at(tx, ty);
        if (target != NULL && is_pile(target->type)) {
            if (remove_building_at(tx, ty) < 0) set_notice("Mochila cheia: o resto ficou no chao");
        } else if (demolish_start(tx, ty) && game_mode == MODE_SOBREVIVENCIA) {
            set_notice("Demolicao marcada: fica ao pe; o material sai para o chao a volta");
        }
    }

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

    int placed = place_building(tool, tx, ty, build_dir, p->x, p->y);

    /* Explica o que aconteceu (so no clique, para nao encher o ecra de avisos) */
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        const Building *there = building_at(tx, ty);
        if (placed) {
            if (game_mode == MODE_SOBREVIVENCIA && there != NULL && site_active(there))     /* (rodar uma esteira nao e uma obra) */
                set_notice("Obra pousada: chega-te e carrega E em cima dela para depositar o material");
        } else if (there != NULL && !(tool == B_ESTEIRA && there->type == B_ESTEIRA)) {
            set_notice("Aqui ja ha uma estrutura (botao direito demole)");
        } else if (there == NULL) {
            set_notice("Aqui nao da para construir");
        }
    }
    if (building_at(tx, ty) != NULL) { last_bx = tx; last_by = ty; }
}

/* ------------------------------------------------------------------ */
/* Camara                                                              */
/* ------------------------------------------------------------------ */

static void update_camera(Camera2D *cam, int screen_w, int screen_h,
                          const Player *follow, int free_cam, int allow_zoom)
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
    float wheel = allow_zoom ? GetMouseWheelMove() : 0.0f;
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
/* Programa principal                                                  */
/* ------------------------------------------------------------------ */

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "A Clareira da Luna");
    SetExitKey(KEY_NULL);       /* o ESC serve para fechar menus e largar a estrutura, nao fecha o jogo */
    SetTargetFPS(60);

    render_init();      /* a textura do terreno so pode ser criada depois de abrir a janela */

    generate_world(DEFAULT_SEED);
    build_terrain_texture();
    game_mode = DEFAULT_MODE;

    float sim_acc = 0.0f;       /* tempo acumulado ainda por simular */

    Player luna;
    player_spawn(&luna);

    Camera2D cam = { 0 };
    cam.zoom   = 3.0f;
    cam.target = (Vector2){ luna.x, luna.y };

    int show_grid = 0;
    int free_cam  = 0;
    int show_help = 0;

    while (!WindowShouldClose()) {
        int   sw = GetScreenWidth();
        int   sh = GetScreenHeight();
        float dt = GetFrameTime();
        int   shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
        int   ctrl  = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);

        if (IsKeyPressed(KEY_G))   show_grid = !show_grid;
        if (IsKeyPressed(KEY_TAB)) free_cam  = !free_cam;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();
        if (IsKeyPressed(KEY_H))   show_help = !show_help;

        /* Tamanho da interface: F6 menor, F7 maior (ou Ctrl + roda do rato) */
        int ui_dir = 0;
        if (IsKeyPressed(KEY_F7) || IsKeyPressed(KEY_KP_ADD))      ui_dir = 1;
        if (IsKeyPressed(KEY_F6) || IsKeyPressed(KEY_KP_SUBTRACT)) ui_dir = -1;
        if (ctrl && GetMouseWheelMove() > 0.0f) ui_dir = 1;
        if (ctrl && GetMouseWheelMove() < 0.0f) ui_dir = -1;
        if (ui_dir != 0) {
            ui_scale_step(ui_dir);
            char msg[64];
            float fit = ui_fit(sw, sh, 800, 500);
            if (fit + 0.01f < ui_scale) snprintf(msg, sizeof msg, "Interface: %d%% (a janela so deixa %d%%)", (int)(ui_scale * 100.0f + 0.5f), (int)(fit * 100.0f));
            else                        snprintf(msg, sizeof msg, "Interface: %d%%", (int)(ui_scale * 100.0f + 0.5f));
            set_notice(msg);
        }
        if (IsKeyPressed(KEY_M)) {
            game_mode = (game_mode == MODE_CRIATIVO) ? MODE_SOBREVIVENCIA : MODE_CRIATIVO;
            set_notice(game_mode == MODE_CRIATIVO ? "Modo criativo: construir nao custa nada"
                                                  : "Modo sobrevivencia: construir gasta itens da mochila");
        }
        if (IsKeyPressed(KEY_F5)) {
            generate_world(current_seed + 1u);     /* tambem limpa os objetos */
            build_terrain_texture();
            player_spawn(&luna);
            container_clear(&backpack);
            menu_close();
            tool = 0;
        }
        if (notice_timer > 0.0f) notice_timer -= dt;

        /* R: roda a estrutura antes de a construir (so as esteiras tem direcao) */
        if (IsKeyPressed(KEY_R) && !menu_is_open() && tool != 0) {
            if (tool == B_ESTEIRA) build_dir = (build_dir + (shift ? 3 : 1)) & 3;
            else                   set_notice("Esta estrutura nao precisa de rodar");
        }

        /* Menus: Q construcao, E mochila/estrutura, ESC fecha ou larga a selecao */
        if (IsKeyPressed(KEY_Q)) menu_toggle_build();
        if (IsKeyPressed(KEY_E)) {
            if (menu_is_open()) menu_close();
            else                open_menu_at_mouse(&luna, cam);
        }
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (show_help)       show_help = 0;
            else if (menu_is_open())  menu_close();
            else if (tool != 0)  { tool = 0; last_bx = last_by = -1; }
        }

        int menu_before = menu_is_open();
        int menu_open   = menu_update(&luna, sw, sh);
        int picked      = menu_picked_build();
        if (picked != B_NONE) { tool = picked; last_bx = last_by = -1; }

        /* Depois de um clique num menu, o rato fica "preso" ate se largarem os
         * botoes (senao o clique que escolheu a estrutura construia logo uma). */
        if (menu_open || menu_before) ignore_mouse = 1;
        else if (ignore_mouse && !IsMouseButtonDown(MOUSE_BUTTON_LEFT) && !IsMouseButtonDown(MOUSE_BUTTON_RIGHT))
            ignore_mouse = 0;

        if (!free_cam) update_player(&luna, dt);
        if (menu_open || ignore_mouse) {
            is_mining = 0;                  /* com o menu aberto o rato e do menu */
            mine_timer = 0.0f;
            last_bx = last_by = -1;
        } else {
            if (tool == 0) {
                update_mining(&luna, cam, dt);
            } else {
                is_mining = 0;
                mine_timer = 0.0f;
            }
            update_building(&luna, cam, free_cam);
        }

        /* Simulacao em passo fixo: igual em qualquer computador/FPS */
        sim_acc += dt;
        if (sim_acc > 0.25f) sim_acc = 0.25f;      /* evita "espiral" se o jogo engasgar */
        while (sim_acc >= SIM_DT) {
            demolition_step(luna.x, luna.y, SIM_DT);       /* ... e desfazendo as demolicoes marcadas */
            construction_step(luna.x, luna.y, SIM_DT);     /* a Luna vai construindo as obras ao pe dela */
            sim_step(SIM_DT);
            sim_acc -= SIM_DT;
        }
        flush_dirty();

        update_camera(&cam, sw, sh, &luna, free_cam, !ctrl);

        BeginDrawing();
        ClearBackground((Color){ 20, 28, 24, 255 });

        BeginMode2D(cam);
        draw_world(cam, sw, sh, show_grid);
        draw_buildings(cam, sw, sh);
        draw_player(&luna);
        if (!menu_open) {
            if (tool == 0) draw_hover(&luna, cam, is_mining);
            else           draw_build_preview(&luna, cam, tool, build_dir);
        }
        EndMode2D();

        draw_hud(cam, sw, sh, &luna, free_cam, tool, build_dir,
                 notice_timer > 0.0f ? notice : "");
        menu_draw(sw, sh);
        if (show_help) draw_help(sw, sh);
        EndDrawing();
    }

    render_shutdown();
    CloseWindow();
    return 0;
}
