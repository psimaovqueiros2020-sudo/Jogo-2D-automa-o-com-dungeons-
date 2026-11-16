/*
 * menu.c  -  Os menus do jogo: construcao (Q) e mochila + estrutura (E).
 *
 * MENU DE CONSTRUCAO (Q): uma lista das estruturas com icone, descricao e custo.
 * Clicar numa escolhe-a: fecha o menu e fica selecionada (ver main.c).
 *
 * MENU DA MOCHILA (E): a mochila a esquerda e, a direita, a estrutura que estava
 * sob o rato (bau, contentor, amontoado, maquina, broca, esteira) ou o chao.
 *   Clique esquerdo numa linha: passa 10 unidades para o outro lado.
 *   Shift + clique esquerdo   : passa tudo o que couber.
 *   Clique direito            : passa 1 unidade.
 * So passa o que cabe: nada se perde (ver logic.c).
 *   - mochila -> bau/contentor/amontoado: guarda; -> chao: larga (cria amontoado);
 *     -> maquina: mete ingredientes (so os que a maquina usa, ate 2 lotes).
 *   - estrutura -> mochila: tira (conteudo, ingredientes ou produtos prontos).
 *   - mochila -> OBRA: deposita o material que a obra pede (o que falta); a Luna
 *     vai construindo com o que la esta. Da obra tira-se de volta o que ainda
 *     nao foi usado.
 */

#include "menu.h"
#include "render.h"         /* draw_fill_bar, carry_label, draw_build_icon, draw_cost */
#include <stdio.h>

#define PANEL_W     360
#define PANEL_H     470
#define PANEL_GAP    20
#define ROW_H        22
#define MAX_ROWS     32

#define BUILD_W     600     /* menu de construcao */
#define BUILD_ROW_H  50

enum { MENU_NONE, MENU_BUILD, MENU_INV };
enum { R_TEXT, R_HEAD, R_ITEM, R_SHOW };    /* linha: texto, titulo, item clicavel, item so para ver */

typedef struct {
    int  kind;
    int  item;          /* R_ITEM / R_SHOW: o ItemType             */
    int  count;         /* R_ITEM / R_SHOW: unidades               */
    int  total;         /* obra: unidades que o custo pede (0 = linha normal) */
    int  section;       /* estrutura: 0 = principal, 1 = produtos prontos */
    char text[100];     /* R_TEXT / R_HEAD                         */
} Row;

static int   mode = MENU_NONE;
static int   target_x = -1, target_y = -1;  /* tile da estrutura aberta ou do chao; x < 0 = nada */
static int   target_was_building;           /* na frame anterior havia uma estrutura aqui */
static int   target_was_pile;
static int   picked = B_NONE;
static char  message[80];
static float message_timer;

/* Tamanho da interface (ver render.h): os menus desenham-se num ecra "virtual"
 * (o real a dividir por mscale) e o rato divide-se pelo mesmo fator. */
static float mscale = 1.0f;

static void set_scale(int screen_w, int screen_h)
{
    mscale = ui_fit(screen_w, screen_h, 800, 500);
}

static Vector2 mouse(void)
{
    Vector2 m = GetMousePosition();
    m.x /= mscale;
    m.y /= mscale;
    return m;
}

int menu_is_open(void) { return mode != MENU_NONE; }

void menu_close(void)
{
    mode = MENU_NONE;
    target_x = target_y = -1;
    target_was_building = target_was_pile = 0;
}

static void set_target(int tx, int ty)
{
    const Building *b = (tx >= 0) ? building_at(tx, ty) : NULL;
    if (b != NULL) { tx = b->x; ty = b->y; }        /* o canto da estrutura */
    target_x = tx;
    target_y = ty;
    target_was_building = b != NULL;
    target_was_pile = b != NULL && is_pile(b->type);
}

void menu_toggle_build(void)
{
    if (mode == MENU_BUILD) { menu_close(); return; }
    menu_close();
    mode = MENU_BUILD;
}

void menu_open_building(int tx, int ty)
{
    menu_close();
    mode = MENU_INV;
    set_target(tx, ty);
}

void menu_open_backpack(int gx, int gy)
{
    menu_close();
    mode = MENU_INV;
    set_target(gx, gy);
}

int menu_picked_build(void)
{
    int p = picked;
    picked = B_NONE;
    return p;
}

/* A estrutura (ou o tile do chao) esta ao alcance das maos? Numa estrutura
 * grande basta UM dos seus tiles estar ao alcance. */
static int target_in_reach(const Player *p, const Building *b)
{
    if (b == NULL) return tile_in_reach(p, target_x, target_y);
    for (int j = 0; j < BUILD[b->type].h; j++)
        for (int i = 0; i < BUILD[b->type].w; i++)
            if (tile_in_reach(p, b->x + i, b->y + j)) return 1;
    return 0;
}

static void say(const char *text)
{
    snprintf(message, sizeof message, "%s", text);
    message_timer = 2.0f;
}

/* ------------------------------------------------------------------ */
/* Linhas das listas                                                   */
/* ------------------------------------------------------------------ */

static void add_text(Row *rows, int *n, int kind, const char *text)
{
    if (*n >= MAX_ROWS) return;
    rows[*n].kind = kind;
    rows[*n].item = rows[*n].count = rows[*n].total = rows[*n].section = 0;
    snprintf(rows[*n].text, sizeof rows[*n].text, "%s", text);
    (*n)++;
}

/* Uma linha por item que la esta (ou "(vazio)") */
static void add_items(Row *rows, int *n, const uint16_t counts[ITEM_COUNT], int kind, int section)
{
    int any = 0;
    for (int i = 1; i < ITEM_COUNT && *n < MAX_ROWS; i++) {
        if (counts[i] == 0) continue;
        rows[*n].kind = kind;
        rows[*n].item = i;
        rows[*n].count = counts[i];
        rows[*n].total = 0;
        rows[*n].section = section;
        rows[*n].text[0] = '\0';
        (*n)++;
        any = 1;
    }
    if (!any) add_text(rows, n, R_TEXT, "(vazio)");
}

/* "8 Argila + 4 Areia -> 1 Tijolo" */
static void recipe_text(const Recipe *rc, char *out, int cap)
{
    int n = 0;
    out[0] = '\0';
    for (int k = 0; k < 3; k++) {
        if (rc->in_type[k] == ITEM_NONE || n >= cap - 1) continue;
        n += snprintf(out + n, (size_t)(cap - n), "%s%d %s", n > 0 ? " + " : "", rc->in_n[k], ITEM[rc->in_type[k]].name);
    }
    if (n < cap - 1) snprintf(out + n, (size_t)(cap - n), " -> %d %s", rc->out_n, ITEM[rc->out_type].name);
}

static int left_rows(Row rows[])
{
    int n = 0;
    add_items(rows, &n, backpack.n, R_ITEM, 0);
    return n;
}

/* O que a estrutura b (NULL = o chao) mostra no painel da direita */
static int right_rows(Row rows[], const Building *b)
{
    int n = 0;
    char buf[100];

    if (b == NULL) {
        if (target_x < 0 || !ground_ok(target_x, target_y)) {
            add_text(rows, &n, R_TEXT, "Aqui nao da para largar nada");
            add_text(rows, &n, R_TEXT, "(agua, arvores e estruturas nao deixam).");
        } else {
            add_text(rows, &n, R_TEXT, "Clica num item da mochila para o");
            add_text(rows, &n, R_TEXT, "largar aqui (cria um amontoado).");
        }
        return n;
    }

    if (demo_active(b)) {                       /* a ser demolida: so informacao (o conteudo ja esta a sair) */
        static const char *dst_name[4] = { "a demolir", "PARADA: sem chao livre a volta", "parada: a Luna esta longe", "na fila" };
        snprintf(buf, sizeof buf, "Estado: %s  (%d%%)", dst_name[b->demo_state & 3], (int)((1.0f - b->demo_p) * 100.0f));
        add_text(rows, &n, R_TEXT, buf);
        snprintf(buf, sizeof buf, "Demolicao total %.0f s", (double)b->demo_secs);
        add_text(rows, &n, R_TEXT, buf);
        add_text(rows, &n, R_HEAD, "Ainda por sair para o chao");
        add_items(rows, &n, b->demo_left, R_SHOW, 0);
        add_text(rows, &n, R_TEXT, "Sai para amontoados a volta. A terra");
        add_text(rows, &n, R_TEXT, "desaparece passado 2 min; a sucata funde-se.");
        return n;
    }

    if (site_active(b)) {
        static const char *st_name[4] = { "a construir", "PARADA: falta material", "parada: a Luna esta longe", "na fila" };
        snprintf(buf, sizeof buf, "Estado: %s  (%d%%)", st_name[b->site_state & 3], (int)(b->build * 100.0f));
        add_text(rows, &n, R_TEXT, buf);
        snprintf(buf, sizeof buf, "Obra total %.0f s (faltam ~%.0f s)", (double)build_seconds(b->type),
                 (double)((1.0f - b->build) * build_seconds(b->type)));
        add_text(rows, &n, R_TEXT, buf);
        add_text(rows, &n, R_HEAD, "Material depositado (clica p/ tirar)");
        for (int k = 0; k < COST_SLOTS && n < MAX_ROWS; k++) {
            if (cost_units(b->type, k) == 0) continue;
            rows[n].kind    = R_ITEM;                       /* clicavel: tira o que ainda nao foi usado */
            rows[n].item    = BUILD[b->type].cost[k].item;
            rows[n].count   = b->deposit[k];
            rows[n].total   = cost_units(b->type, k);
            rows[n].section = 0;
            rows[n].text[0] = '\0';
            n++;
        }
        add_text(rows, &n, R_TEXT, "Clica num item da mochila para o depositar.");
        add_text(rows, &n, R_TEXT, "A Luna so constroi ao pe da obra.");
        return n;
    }

    if (is_pile(b->type)) {
        add_text(rows, &n, R_HEAD, "Clica para apanhar");
        add_items(rows, &n, b->store.n, R_ITEM, 0);
    } else if (is_storage(b->type)) {
        add_text(rows, &n, R_HEAD, "Conteudo (clica para tirar)");
        add_items(rows, &n, b->store.n, R_ITEM, 0);
    } else if (is_machine(b->type)) {
        static const char *st_name[3] = { "a trabalhar", "parada (faltam ingredientes)", "saida cheia" };
        if (b->recipe != 0)
            snprintf(buf, sizeof buf, "Estado: %s %d%%", RECIPES[b->recipe - 1].name,
                     (int)(100.0f * b->work / RECIPES[b->recipe - 1].period));
        else
            snprintf(buf, sizeof buf, "Estado: %s", st_name[machine_status(b)]);
        add_text(rows, &n, R_TEXT, buf);
        if (BUILD[b->type].life > 0) {
            snprintf(buf, sizeof buf, "Lotes feitos: %d/%d (depois parte)", b->made, BUILD[b->type].life);
            add_text(rows, &n, R_TEXT, buf);
        }
        add_text(rows, &n, R_HEAD, "Receitas");
        for (int r = 0; r < RECIPE_COUNT; r++) {
            if (RECIPES[r].machine != b->type) continue;
            snprintf(buf, sizeof buf, "%s (%.0f s):", RECIPES[r].name, (double)RECIPES[r].period);
            add_text(rows, &n, R_TEXT, buf);
            recipe_text(&RECIPES[r], buf, (int)sizeof buf);
            add_text(rows, &n, R_TEXT, buf);
        }
        add_text(rows, &n, R_HEAD, "Dentro (clica para tirar)");
        add_items(rows, &n, b->in, R_ITEM, 0);
        add_text(rows, &n, R_HEAD, "Pronto (clica para tirar)");
        add_items(rows, &n, b->out, R_ITEM, 1);
    } else if (b->type == B_BROCA) {
        static const char *st_name[3] = { "a trabalhar", "sem minerio por baixo", "saida cheia" };
        snprintf(buf, sizeof buf, "Estado: %s", st_name[drill_status(b)]);
        add_text(rows, &n, R_TEXT, buf);
        add_text(rows, &n, R_HEAD, "Guardado (clica para tirar)");
        add_items(rows, &n, b->buf, R_ITEM, 0);
    } else if (b->type == B_ESTEIRA) {
        static const char *dir_name[4] = { "direita", "baixo", "esquerda", "cima" };
        uint16_t on_belt[ITEM_COUNT] = { 0 };
        for (int i = 0; i < b->n; i++) on_belt[b->item[i]]++;
        snprintf(buf, sizeof buf, "Direcao: %s", dir_name[b->dir & 3]);
        add_text(rows, &n, R_TEXT, buf);
        add_text(rows, &n, R_HEAD, "Itens na esteira");
        add_items(rows, &n, on_belt, R_SHOW, 0);
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* Geometria                                                           */
/* ------------------------------------------------------------------ */

static void inv_origin(int which, int screen_w, int screen_h, int *x, int *y)
{
    int total = 2 * PANEL_W + PANEL_GAP;
    *x = (screen_w - total) / 2 + which * (PANEL_W + PANEL_GAP);
    *y = (screen_h - PANEL_H) / 2;
}

/* Os paineis de armazenamento (baus, contentores, amontoados) mostram barras de volume e peso */
static int shows_bars(const Building *b)
{
    return b != NULL && has_store(b->type) && !site_active(b) && !demo_active(b);
}

/* Onde comecam as linhas dentro do painel: mais abaixo se ha barras de volume/peso */
static int rows_top(int has_bars) { return has_bars ? 108 : 44; }

static Rectangle row_rect(int x, int y, int top, int k)
{
    return (Rectangle){ (float)(x + 8), (float)(y + top + k * ROW_H), (float)(PANEL_W - 16), (float)(ROW_H - 2) };
}

/* Quantas linhas cabem. O painel da mochila (esquerda) deixa espaco ao fundo para a
 * carga, a ajuda e os avisos; o da estrutura (direita) so deixa uma folga pequena. */
static int rows_fit(int top, int left_panel) { return (PANEL_H - top - (left_panel ? 66 : 10)) / ROW_H; }

static Rectangle inv_close_rect(int screen_w, int screen_h)
{
    int x, y;
    inv_origin(1, screen_w, screen_h, &x, &y);
    return (Rectangle){ (float)(x + PANEL_W - 92), (float)(y + 6), 84.0f, 20.0f };
}

static Rectangle build_panel(int screen_w, int screen_h)
{
    float w = (float)BUILD_W, h = (float)(70 + BUILDABLE_COUNT * BUILD_ROW_H + 44);
    return (Rectangle){ ((float)screen_w - w) / 2.0f, ((float)screen_h - h) / 2.0f - 20.0f, w, h };
}

static Rectangle build_row_rect(Rectangle pnl, int i)
{
    return (Rectangle){ pnl.x + 10.0f, pnl.y + 56.0f + (float)(i * BUILD_ROW_H), pnl.width - 20.0f, (float)(BUILD_ROW_H - 4) };
}

static Rectangle build_close_rect(Rectangle pnl)
{
    return (Rectangle){ pnl.x + pnl.width - 92.0f, pnl.y + 8.0f, 84.0f, 20.0f };
}

/* ------------------------------------------------------------------ */
/* Cliques                                                             */
/* ------------------------------------------------------------------ */

/* Porque e que nao deu para largar no chao? */
static void say_ground_problem(int item)
{
    if (target_x < 0 || !in_bounds(target_x, target_y)) { say("Aqui nao da para largar nada"); return; }
    const Building *b = building_at(target_x, target_y);
    if (b == NULL)            say("Aqui nao se pode largar (terreno)");
    else if (!is_pile(b->type)) say("Aqui ha uma estrutura");
    else if (b->pile_item != item) {
        char t[80];
        snprintf(t, sizeof t, "Aqui ja ha um amontoado de %s", ITEM[b->pile_item].name);
        say(t);
    } else say("O amontoado esta cheio");
}

/* Passa itens da mochila (esquerda) para o que esta a direita */
static void put_item(Building *b, int item, int amount)
{
    int moved = 0;

    if (b == NULL) {
        moved = drop_to_ground(&backpack, target_x, target_y, item, amount);
        if (moved == 0) say_ground_problem(item);
        else            set_target(target_x, target_y);     /* agora ha ali um amontoado */
        return;
    }
    if (demo_active(b)) { say("Esta a ser demolida"); return; }
    if (site_active(b)) {                       /* obra: deposita o material que ela pede */
        moved = site_deposit(b, &backpack, item, amount);
        if (moved == 0) say(site_slot(b, item) < 0 ? "Esta obra nao usa isto" : "Ja tem deste material que chegue");
        return;
    }
    if (is_pile(b->type)) {
        if (b->pile_item != item) { say_ground_problem(item); return; }
        moved = drop_to_ground(&backpack, b->x, b->y, item, amount);
        if (moved == 0) say("O amontoado esta cheio");
    } else if (is_storage(b->type)) {
        moved = container_move(&backpack, &b->store, item, amount);
        if (moved == 0) say("Nao cabe: volume cheio");
    } else if (is_machine(b->type)) {
        moved = machine_load(b, &backpack, item, amount);
        if (moved == 0) say(machine_in_cap(b->type, item) == 0 ? "Esta maquina nao usa isto" : "Ja tem destes ingredientes que chegue");
    } else {
        say("Esta estrutura nao recebe itens da mao");
    }
}

/* Passa itens da estrutura (direita) para a mochila */
static void take_item(Building *b, const Row *row, int amount)
{
    int moved = 0;
    if (site_active(b)) {                       /* obra: so se tira o que ainda nao foi usado */
        moved = site_take_back(b, &backpack, row->item, amount);
        if (moved == 0) say(site_free(b, row->item) == 0 ? "Esse material ja esta a ser usado na obra" : "Nao cabe: mochila cheia (volume ou peso)");
        return;
    }
    if (is_pile(b->type))         moved = pile_take(b, &backpack, row->item, amount);    /* pode apagar o amontoado */
    else if (is_storage(b->type)) moved = container_move(&b->store, &backpack, row->item, amount);
    else if (is_machine(b->type)) moved = row->section == 0 ? machine_take_in(b, &backpack, row->item, amount)
                                                              : machine_take_out(b, &backpack, row->item, amount);
    else if (b->type == B_BROCA)  moved = drill_take(b, &backpack, row->item, amount);
    if (moved == 0) say("Nao cabe: mochila cheia (volume ou peso)");
}

/* O clique cai numa linha clicavel? Devolve o indice (ou -1) */
static int row_under_mouse(const Row rows[], int n, int x, int y, int top, int left_panel)
{
    Vector2 m = mouse();
    int fit = rows_fit(top, left_panel);
    for (int k = 0; k < n && k < fit; k++)
        if (rows[k].kind == R_ITEM && CheckCollisionPointRec(m, row_rect(x, y, top, k))) return k;
    return -1;
}

static int update_build_menu(int screen_w, int screen_h)
{
    int left = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    if (!left) return 1;

    Vector2 m = mouse();
    Rectangle pnl = build_panel(screen_w, screen_h);
    if (CheckCollisionPointRec(m, build_close_rect(pnl))) { menu_close(); return 0; }
    for (int i = 0; i < BUILDABLE_COUNT; i++) {
        if (CheckCollisionPointRec(m, build_row_rect(pnl, i))) {
            picked = BUILDABLE[i];
            menu_close();
            return 0;
        }
    }
    return 1;
}

int menu_update(const Player *p, int screen_w, int screen_h)
{
    if (mode == MENU_NONE) return 0;
    set_scale(screen_w, screen_h);
    screen_w = (int)((float)screen_w / mscale);         /* tamanho "virtual" do ecra */
    screen_h = (int)((float)screen_h / mscale);
    if (message_timer > 0.0f) message_timer -= GetFrameTime();
    if (mode == MENU_BUILD) return update_build_menu(screen_w, screen_h);

    /* ---- menu da mochila ---- */
    Building *b = (target_x >= 0) ? building_at(target_x, target_y) : NULL;
    if (target_was_building && b == NULL && !target_was_pile) { menu_close(); return 0; }  /* demolida */
    target_was_building = b != NULL;
    target_was_pile = b != NULL && is_pile(b->type);
    if (target_x >= 0 && !target_in_reach(p, b)) { menu_close(); return 0; }   /* Luna afastou-se */

    int left  = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    int right = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);
    if (!left && !right) return 1;

    if (left && CheckCollisionPointRec(mouse(), inv_close_rect(screen_w, screen_h))) {
        menu_close();
        return 0;
    }

    int amount = 1;
    if (left) amount = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 1000000 : 10;

    int x0, y0, x1, y1;
    inv_origin(0, screen_w, screen_h, &x0, &y0);
    inv_origin(1, screen_w, screen_h, &x1, &y1);

    Row lrows[MAX_ROWS], rrows[MAX_ROWS];
    int nl = left_rows(lrows), nr = right_rows(rrows, b);
    int right_bars = shows_bars(b);

    int k = row_under_mouse(lrows, nl, x0, y0, rows_top(1), 1);
    if (k >= 0) { put_item(b, lrows[k].item, amount); return 1; }
    if (b != NULL) {
        k = row_under_mouse(rrows, nr, x1, y1, rows_top(right_bars), 0);
        if (k >= 0) take_item(b, &rrows[k], amount);
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* Desenho                                                             */
/* ------------------------------------------------------------------ */

static void draw_bars(int x, int y, const Container *c)
{
    char line[96];
    int vol = container_vol_ml(c), mass = container_mass_g(c);

    snprintf(line, sizeof line, "Volume  %.1f / %.0f L", (double)vol / 1000.0, (double)c->cap_vol_ml / 1000.0);
    DrawText(line, x + 12, y + 36, 14, RAYWHITE);
    draw_fill_bar(x + 12, y + 54, PANEL_W - 24, 8, (float)vol / (float)c->cap_vol_ml);

    if (c->cap_mass_g > 0) {
        snprintf(line, sizeof line, "Peso  %.1f / %.0f kg", (double)mass / 1000.0, (double)c->cap_mass_g / 1000.0);
        DrawText(line, x + 12, y + 68, 14, RAYWHITE);
        draw_fill_bar(x + 12, y + 86, PANEL_W - 24, 8, (float)mass / (float)c->cap_mass_g);
    } else {
        snprintf(line, sizeof line, "Peso  %.1f kg  (sem limite)", (double)mass / 1000.0);
        DrawText(line, x + 12, y + 68, 14, LIGHTGRAY);
    }
}

static void draw_rows(int x, int y, int top, const Row rows[], int n, int left_panel)
{
    Vector2 m = mouse();
    char line[96];
    int fit = rows_fit(top, left_panel);

    for (int k = 0; k < n && k < fit; k++) {
        Rectangle r = row_rect(x, y, top, k);
        int ry = (int)r.y + 4;
        if (rows[k].kind == R_HEAD) {
            DrawText(rows[k].text, x + 12, ry - 1, 14, (Color){ 235, 200, 90, 255 });
        } else if (rows[k].kind == R_TEXT) {
            DrawText(rows[k].text, x + 14, ry - 1, 14, LIGHTGRAY);
        } else {
            int item = rows[k].item;
            if (rows[k].kind == R_ITEM && CheckCollisionPointRec(m, r))
                DrawRectangleRec(r, (Color){ 60, 70, 90, 255 });
            DrawRectangle(x + 14, ry, 12, 12, ITEM[item].color);
            DrawText(ITEM[item].name, x + 32, ry - 1, 14, RAYWHITE);
            if (rows[k].total > 0) {            /* linha de uma obra: depositado / total, e o que ha na mochila */
                int done = rows[k].count >= rows[k].total;
                snprintf(line, sizeof line, "%d / %d un", rows[k].count, rows[k].total);
                DrawText(line, x + 100, ry - 1, 14, done ? (Color){ 120, 220, 120, 255 } : RAYWHITE);
                snprintf(line, sizeof line, "mochila %d", backpack.n[item]);
                DrawText(line, x + 250, ry - 1, 14, LIGHTGRAY);
                continue;
            }
            snprintf(line, sizeof line, "%d un", rows[k].count);
            DrawText(line, x + 108, ry - 1, 14, RAYWHITE);
            snprintf(line, sizeof line, "%.1f L", (double)(rows[k].count * ITEM[item].vol_ml) / 1000.0);
            DrawText(line, x + 176, ry - 1, 14, LIGHTGRAY);
            snprintf(line, sizeof line, "%.1f kg", (double)(rows[k].count * item_mass_g(item)) / 1000.0);
            DrawText(line, x + 248, ry - 1, 14, LIGHTGRAY);
        }
    }
    if (n > fit) DrawText("...", x + 14, y + PANEL_H - (left_panel ? 62 : 20), 14, LIGHTGRAY);
}

static void draw_panel_frame(int x, int y, const char *title)
{
    DrawRectangle(x, y, PANEL_W, PANEL_H, (Color){ 24, 28, 34, 245 });
    DrawRectangleLines(x, y, PANEL_W, PANEL_H, (Color){ 120, 126, 140, 255 });
    DrawText(title, x + 12, y + 10, 18, RAYWHITE);
}

static void draw_build_menu(int screen_w, int screen_h)
{
    Rectangle pnl = build_panel(screen_w, screen_h);
    Vector2 m = mouse();
    char line[96];

    DrawRectangleRec(pnl, (Color){ 24, 28, 34, 245 });
    DrawRectangleLines((int)pnl.x, (int)pnl.y, (int)pnl.width, (int)pnl.height, (Color){ 120, 126, 140, 255 });
    DrawText("Construir", (int)pnl.x + 14, (int)pnl.y + 10, 20, RAYWHITE);
    snprintf(line, sizeof line, "Modo: %s", game_mode_label());
    DrawText(line, (int)pnl.x + 14, (int)pnl.y + 34, 14, game_mode == MODE_SOBREVIVENCIA ? (Color){ 235, 160, 90, 255 } : (Color){ 120, 200, 255, 255 });

    for (int i = 0; i < BUILDABLE_COUNT; i++) {
        int type = BUILDABLE[i];
        Rectangle r = build_row_rect(pnl, i);
        DrawRectangleRec(r, CheckCollisionPointRec(m, r) ? (Color){ 60, 70, 90, 255 } : (Color){ 36, 40, 48, 255 });
        draw_build_icon(type, (int)r.x + 4, (int)r.y + 3, 40);
        DrawText(BUILD[type].name, (int)r.x + 54, (int)r.y + 4, 16, RAYWHITE);
        DrawText(BUILD[type].desc, (int)r.x + 54, (int)r.y + 26, 12, LIGHTGRAY);
        int w = draw_cost(0, -100, type, 14);       /* mede (fora do ecra) para alinhar a direita */
        draw_cost((int)(r.x + r.width) - w - 10, (int)r.y + 5, type, 14);
        if (game_mode == MODE_SOBREVIVENCIA) {      /* quanto se vai carregar e quanto demora */
            snprintf(line, sizeof line, "%.0f L, %.0f kg, obra %.0f s", (double)build_cost_vol_ml(type) / 1000.0,
                     (double)build_cost_mass_g(type) / 1000.0, (double)build_seconds(type));
            DrawText(line, (int)(r.x + r.width) - MeasureText(line, 12) - 10, (int)r.y + 26, 12, LIGHTGRAY);
        }
    }

    Rectangle cr = build_close_rect(pnl);
    DrawRectangleRec(cr, CheckCollisionPointRec(m, cr) ? (Color){ 150, 60, 60, 255 } : (Color){ 100, 44, 44, 255 });
    DrawText("Fechar [Q]", (int)cr.x + 6, (int)cr.y + 3, 14, RAYWHITE);

    DrawText(game_mode == MODE_SOBREVIVENCIA ? "Pousa-se sempre: a obra gasta o material da mochila e fica a meio se faltar."
                                             : "Clica numa estrutura. Modo criativo: construir nao custa nada.",
             (int)pnl.x + 14, (int)(pnl.y + pnl.height) - 34, 12, LIGHTGRAY);
    DrawText("Depois: botao esquerdo constroi, R roda, Esc larga a estrutura.",
             (int)pnl.x + 14, (int)(pnl.y + pnl.height) - 18, 12, LIGHTGRAY);
}

static void draw_menus(int screen_w, int screen_h)
{
    if (mode == MENU_BUILD) { draw_build_menu(screen_w, screen_h); return; }

    int x0, y0, x1, y1;
    inv_origin(0, screen_w, screen_h, &x0, &y0);
    inv_origin(1, screen_w, screen_h, &x1, &y1);
    const Building *b = (target_x >= 0) ? building_at(target_x, target_y) : NULL;
    Row rows[MAX_ROWS];
    int n;
    char title[64];

    /* ---- mochila (esquerda) ---- */
    draw_panel_frame(x0, y0, "Mochila");
    draw_bars(x0, y0, &backpack);
    n = left_rows(rows);
    draw_rows(x0, y0, rows_top(1), rows, n, 1);
    int state = carry_state(container_mass_g(&backpack));
    DrawText(carry_label(state), x0 + 12, y0 + PANEL_H - 56, 14,
             state == 0 ? LIGHTGRAY : state == 1 ? (Color){ 235, 200, 60, 255 } : (Color){ 220, 70, 70, 255 });

    /* ---- estrutura ou chao (direita) ---- */
    if (b == NULL)                snprintf(title, sizeof title, "Chao");
    else if (demo_active(b))      snprintf(title, sizeof title, "%s (a demolir)", BUILD[b->type].name);
    else if (site_active(b))      snprintf(title, sizeof title, "%s (em obra)", BUILD[b->type].name);
    else if (is_pile(b->type))    snprintf(title, sizeof title, "Amontoado de %s", ITEM[b->pile_item].name);
    else                          snprintf(title, sizeof title, "%s", BUILD[b->type].name);
    draw_panel_frame(x1, y1, title);
    if (shows_bars(b)) draw_bars(x1, y1, &b->store);
    n = right_rows(rows, b);
    draw_rows(x1, y1, rows_top(shows_bars(b)), rows, n, 0);

    /* ---- botao fechar, ajuda e avisos ---- */
    Rectangle cr = inv_close_rect(screen_w, screen_h);
    DrawRectangleRec(cr, CheckCollisionPointRec(mouse(), cr) ? (Color){ 150, 60, 60, 255 } : (Color){ 100, 44, 44, 255 });
    DrawText("Fechar [E]", (int)cr.x + 6, (int)cr.y + 3, 14, RAYWHITE);

    DrawText("Clique: 10 un   Shift+clique: tudo   Botao dir.: 1 un", x0 + 12, y0 + PANEL_H - 36, 12, LIGHTGRAY);
    if (message_timer > 0.0f)
        DrawText(message, x0 + 12, y0 + PANEL_H - 20, 14, (Color){ 255, 220, 60, 255 });
}

void menu_draw(int screen_w, int screen_h)
{
    if (mode == MENU_NONE) return;

    DrawRectangle(0, 0, screen_w, screen_h, (Color){ 0, 0, 0, 120 });      /* escurece o jogo */
    set_scale(screen_w, screen_h);
    Camera2D c = { 0 };
    c.zoom = mscale;
    BeginMode2D(c);
    draw_menus((int)((float)screen_w / mscale), (int)((float)screen_h / mscale));
    EndMode2D();
}
