/*
 * A CLAREIRA DA LUNA  -  main.c: teclado, rato, camara e ciclo do jogo
 * ------------------------------------
 * Linguagem: C (C99)   Biblioteca: raylib
 *
 * O jogo esta dividido em modulos (ver README, secoes 0 e 4):
 *   world.c  - terreno, minerios, ruido e geracao do setor
 *   logic.c  - objetos, receitas, maquinas, simulacao, mochila (SEM raylib)
 *   render.c - tudo o que desenha
 *   main.c   - (este) input, camara e o ciclo do jogo
 *
 * Novidades do Marco 3b:
 *   - AREIA nas margens do ribeiro (jazida nova, em lama).
 *   - Produtos novos: CARVAO, TIJOLO, FERRO (itens que andam nas esteiras).
 *   - CARVOEIRA (2x2): pirolise. A esteira entra com madeira, sai carvao.
 *   - FORNO (2x2): tijolos (argila + areia + madeira) e ferro (hematite +
 *     carvao). Sem inserters: a esteira entra na maquina e ela despeja o
 *     produto numa esteira/bau/maquina encostada a ela.
 *   - As receitas estao numa tabela (RECIPES) facil de ajustar ou ampliar.
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
 *   2 / 3 / 4 / 5 / 6 ... ferramenta BROCA / ESTEIRA / BAU / CARVOEIRA /
 *                         FORNO: botao esquerdo constroi (segurar e arrastar
 *                         para fazer uma linha de esteiras)
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
#include "world.h"
#include "logic.h"
#include "render.h"
#include <math.h>
#include <stdlib.h>      /* abs() */

/* Estado da interface (so existe aqui; quem desenha recebe-o por parametro) */
static int   tool;              /* 0 = mao (minerar); senao um BuildType */
static int   build_dir;         /* direcao com que se coloca a esteira   */
static float mine_timer;
static int   is_mining;
static int   last_bx = -1, last_by = -1;     /* ultimo tile construido ao arrastar */

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

    float speed = PLAYER_SPEED;
    if (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) speed *= PLAYER_RUN;

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
/* Construcao: rato                                                    */
/* ------------------------------------------------------------------ */

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
/* Programa principal                                                  */
/* ------------------------------------------------------------------ */

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT);
    InitWindow(1280, 720, "A Clareira da Luna");
    SetTargetFPS(60);

    render_init();      /* a textura do terreno so pode ser criada depois de abrir a janela */

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
        if (IsKeyPressed(KEY_FIVE))  tool = B_CARVOEIRA;
        if (IsKeyPressed(KEY_SIX))   tool = B_FORNO;
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
        if (tool == 0) draw_hover(&luna, cam, is_mining);
        else           draw_build_preview(&luna, cam, tool, build_dir);
        EndMode2D();

        draw_hud(cam, sw, sh, &luna, free_cam, tool, build_dir);
        EndDrawing();
    }

    render_shutdown();
    CloseWindow();
    return 0;
}
