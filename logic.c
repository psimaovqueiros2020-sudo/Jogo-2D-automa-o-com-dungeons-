/*
 * logic.c  -  A LOGICA DO JOGO: objetos construidos, receitas, maquinas,
 * esteiras, brocas, extracao, mochila, colisoes da Luna e simulacao.
 *
 * REGRA: aqui NAO se usa raylib (nem desenho, nem rato, nem teclado) para
 * isto se poder testar sem janela (tests/test_logica.c).
 */

#include "logic.h"
#include <string.h>
#include <math.h>

/* ------------------------------------------------------------------ */
/* Objetos construidos: tabelas e lista                                */
/* ------------------------------------------------------------------ */

/* ---- Custos e tempos de obra (modo SOBREVIVENCIA) ----------------------
 * Escala do mundo: 1 tile = 50 cm, logo uma estrutura 2x2 ocupa 1 m x 1 m.
 * 1 unidade de um item = 0,1 L (o tijolo e uma peca de 1 L). Os custos estao
 * pensados em volume de material de verdade; os numeros sao PROVISORIOS e
 * afinam-se depois de jogar (ou de uma vez com COST_PERCENT / TIME_PERCENT).
 *
 *   Estrutura        Material                              ~Volume  ~Massa   Obra
 *   Forno rupestre   800 argila                              80 L   136 kg    45 s
 *                    (cupula de 1 m com paredes de ~10 cm)
 *   Carvoeira        150 madeira + 400 argila                55 L    74 kg    30 s
 *                    (pilha de lenha e tronco coberta de terra)
 *   Forno            50 tijolos + 150 argila                 65 L   115 kg    90 s
 *                    (camara de tijolo de 1 m com argamassa)
 *   Broca mecanica   8 ferro + 10 tijolos + 60 madeira       17 L    27 kg    60 s
 *   Esteira (1 tile) 15 madeira                             1,5 L   0,6 kg  1,5 s
 *   Bau              100 madeira                             10 L     4 kg     6 s
 *   Contentor        40 ferro + 200 madeira                  24 L    39 kg   120 s
 *
 * A OBRA: pode-se sempre pousar a estrutura. O jogador DEPOSITA o material nela
 * pelo menu (tecla E, ver site_deposit) e a Luna constroi-a aos poucos (ver
 * construction_step), ate onde o material depositado chegar. Sem material a obra
 * fica a meio e nao funciona. O material nao tem de caber de uma vez na mochila
 * (20 L, 50 kg): vai-se trazendo e depositando. */
const BuildInfo BUILD[B_COUNT] = {
    [B_NONE] = { "-", "", 1, 1, 0, 0, 0, 0.0f, { { 0, 0 } } },

    [B_BROCA] = { "Broca mecanica", "Assenta numa jazida (ou arvores) e extrai sozinha.",
                  2, 2, 1, 0, 0, 60.0f,
                  { { ITEM_FERRO, 8 }, { ITEM_TIJOLO, 10 }, { ITEM_MADEIRA, 60 } } },

    [B_ESTEIRA] = { "Esteira", "Leva itens. R roda a direcao antes de construir.",
                    1, 1, 0, 0, 0, 1.5f, { { ITEM_MADEIRA, 15 } } },

    [B_BAU] = { "Bau", "Guarda 50 L de itens. Abre com E.",
                1, 1, 1, CHEST_VOL_ML, 0, 6.0f, { { ITEM_MADEIRA, 100 } } },

    [B_CARVOEIRA] = { "Carvoeira", "Madeira vira carvao vegetal (pirolise).",
                      2, 2, 1, 0, 0, 30.0f, { { ITEM_MADEIRA, 150 }, { ITEM_ARGILA, 400 } } },

    [B_FORNO] = { "Forno", "Tijolos e ferro. Nao parte.",
                  2, 2, 1, 0, 0, 90.0f, { { ITEM_TIJOLO, 50 }, { ITEM_ARGILA, 150 } } },

    [B_CONTENTOR] = { "Contentor", "Guarda 2000 L de itens. Abre com E.",
                      2, 2, 1, CONTAINER_VOL_ML, 0, 120.0f, { { ITEM_FERRO, 40 }, { ITEM_MADEIRA, 200 } } },

    [B_FORNO_RUPESTRE] = { "Forno rupestre", "So de argila: so cozinha tijolos e parte ao fim de 20.",
                           2, 2, 1, 0, RUPESTRE_LIFE, 45.0f, { { ITEM_ARGILA, 800 } },
                           1 },     /* demo_earth: ao demolir vira so terra */

    /* o amontoado nao tem custo nem se constroi: cria-se a largar itens */
    [B_AMONTOADO] = { "Amontoado", "Itens largados no chao.",
                      1, 1, 0, PILE_VOL_ML, 0, 0.0f, { { 0, 0 } } },
};

int cost_units(int type, int slot)
{
    const Cost *c = &BUILD[type].cost[slot];
    if (c->item == ITEM_NONE) return 0;
    int n = ((int)c->n * COST_PERCENT + 50) / 100;
    return n < 1 ? 1 : n;
}

float build_seconds(int type)
{
    float t = BUILD[type].build_s * (float)TIME_PERCENT / 100.0f;
    return t < 0.01f ? 0.01f : t;
}

int build_cost_vol_ml(int type)
{
    int v = 0;
    for (int k = 0; k < COST_SLOTS; k++)
        if (cost_units(type, k) > 0) v += cost_units(type, k) * ITEM[BUILD[type].cost[k].item].vol_ml;
    return v;
}

int build_cost_mass_g(int type)
{
    int m = 0;
    for (int k = 0; k < COST_SLOTS; k++)
        if (cost_units(type, k) > 0) m += cost_units(type, k) * item_mass_g(BUILD[type].cost[k].item);
    return m;
}

/* Ordem do menu de construcao (a da progressao) */
const int BUILDABLE[] = { B_FORNO_RUPESTRE, B_CARVOEIRA, B_FORNO, B_BROCA, B_ESTEIRA, B_BAU, B_CONTENTOR };
const int BUILDABLE_COUNT = (int)(sizeof BUILDABLE / sizeof BUILDABLE[0]);

/* Modo de jogo: ver logic.h */
int game_mode = MODE_CRIATIVO;

/* Direcoes: 0 = direita, 1 = baixo, 2 = esquerda, 3 = cima */
const int DIRX[4] = { 1, 0, -1, 0 };
const int DIRY[4] = { 0, 1, 0, -1 };

Building buildings[MAX_BUILDINGS];
uint16_t bgrid[MAP_H][MAP_W];

Building *building_at(int x, int y)
{
    if (!in_bounds(x, y) || bgrid[y][x] == 0) return NULL;
    return &buildings[bgrid[y][x] - 1];
}

/* A Luna nao passa por brocas e baus (esteiras sim). */
int building_solid(int x, int y)
{
    const Building *b = building_at(x, y);
    return b != NULL && BUILD[b->type].solid;
}

void reset_buildings(void)
{
    memset(buildings, 0, sizeof buildings);
    memset(bgrid, 0, sizeof bgrid);
}

int is_storage(int type)
{
    return type == B_BAU || type == B_CONTENTOR;
}

int is_pile(int type)
{
    return type == B_AMONTOADO;
}

int has_store(int type)
{
    return is_storage(type) || is_pile(type);
}

/* ------------------------------------------------------------------ */
/* Contentores (mochila, baus, contentor): volume e massa               */
/* ------------------------------------------------------------------ */

int container_vol_ml(const Container *c)
{
    int s = 0;
    for (int i = 1; i < ITEM_COUNT; i++) s += c->n[i] * ITEM[i].vol_ml;
    return s;
}

int container_mass_g(const Container *c)
{
    int s = 0;
    for (int i = 1; i < ITEM_COUNT; i++) s += c->n[i] * item_mass_g(i);
    return s;
}

int container_units(const Container *c)
{
    int s = 0;
    for (int i = 1; i < ITEM_COUNT; i++) s += c->n[i];
    return s;
}

/* Quantas unidades MAIS deste item cabem: basta um dos dois limites chegar
 * (volume ou massa) para nao caber mais. Nunca parte uma unidade. */
int container_room(const Container *c, int item)
{
    if (item <= ITEM_NONE || item >= ITEM_COUNT) return 0;

    int room = (c->cap_vol_ml - container_vol_ml(c)) / ITEM[item].vol_ml;
    if (c->cap_mass_g > 0) {
        int by_mass = (c->cap_mass_g - container_mass_g(c)) / item_mass_g(item);
        if (by_mass < room) room = by_mass;
    }
    if (65535 - (int)c->n[item] < room) room = 65535 - (int)c->n[item];
    return room > 0 ? room : 0;
}

int container_add(Container *c, int item, int n)
{
    int room = container_room(c, item);
    if (n > room) n = room;
    if (n <= 0) return 0;
    c->n[item] = (uint16_t)(c->n[item] + n);
    return n;
}

int container_take(Container *c, int item, int n)
{
    if (item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    if (n > (int)c->n[item]) n = (int)c->n[item];
    if (n <= 0) return 0;
    c->n[item] = (uint16_t)(c->n[item] - n);
    return n;
}

/* Passa ate n unidades de um contentor para outro. Nada se perde: so sai
 * de "from" o que cabe em "to". */
int container_move(Container *from, Container *to, int item, int n)
{
    if (n > (int)from->n[item]) n = (int)from->n[item];
    int moved = container_add(to, item, n);
    container_take(from, item, moved);
    return moved;
}

void container_clear(Container *c)
{
    memset(c->n, 0, sizeof c->n);
}

int counts_fit(const Container *c, const int add[ITEM_COUNT])
{
    int vol = container_vol_ml(c), mass = container_mass_g(c);
    for (int i = 1; i < ITEM_COUNT; i++) {
        vol  += add[i] * ITEM[i].vol_ml;
        mass += add[i] * item_mass_g(i);
        if ((int)c->n[i] + add[i] > 65535) return 0;
    }
    if (vol > c->cap_vol_ml) return 0;
    if (c->cap_mass_g > 0 && mass > c->cap_mass_g) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Jogador: colisoes e posicao (sem teclado)                           */
/* ------------------------------------------------------------------ */

int tile_blocked(int tx, int ty)
{
    if (!in_bounds(tx, ty)) return 1;
    if (building_solid(tx, ty)) return 1;
    return !TERRAIN[tile_at(tx, ty, 0)->terrain].walkable;
}

/* A caixa de colisao centrada em (cx, cy) bate em algum tile bloqueado? */
int box_blocked(float cx, float cy)
{
    int x0 = (int)floorf((cx - PLAYER_HALF) / TILE_SIZE);
    int x1 = (int)floorf((cx + PLAYER_HALF - 0.01f) / TILE_SIZE);
    int y0 = (int)floorf((cy - PLAYER_HALF) / TILE_SIZE);
    int y1 = (int)floorf((cy + PLAYER_HALF - 0.01f) / TILE_SIZE);

    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++)
            if (tile_blocked(x, y)) return 1;
    return 0;
}

/* Move em X e em Y separadamente, para a Luna "deslizar" ao longo das paredes */
void player_move(Player *p, float dx, float dy)
{
    if (!box_blocked(p->x + dx, p->y)) p->x += dx;
    if (!box_blocked(p->x, p->y + dy)) p->y += dy;
}

void player_spawn(Player *p)
{
    p->x = (CLEARING_X + 2) * TILE_SIZE + TILE_SIZE / 2.0f;
    p->y = (CLEARING_Y + 2) * TILE_SIZE + TILE_SIZE / 2.0f;
}

/* Carga: ver os limites em logic.h (CARRY_FREE_G, CARRY_SLOW_G) */
int carry_state(int mass_g)
{
    if (mass_g <= CARRY_FREE_G) return 0;
    if (mass_g <= CARRY_SLOW_G) return 1;
    return 2;
}

float carry_speed_factor(int mass_g)
{
    int s = carry_state(mass_g);
    if (s == 0) return 1.0f;
    if (s == 1) return CARRY_SLOW_FACTOR;
    return 0.0f;
}

int carry_can_run(int mass_g)
{
    return carry_state(mass_g) == 0;
}

/* ------------------------------------------------------------------ */
/* Extracao a mao                                                      */
/* ------------------------------------------------------------------ */

/* A mochila: 20 L e 50 kg. (Inventario "real", com construir a custo, e no
 * Marco 5; o sistema de volume e massa ja e este.) */
Container backpack = { BACKPACK_VOL_ML, BACKPACK_MASS_G, { 0 } };

int tile_in_reach(const Player *p, int tx, int ty)
{
    float cx = (float)tx * TILE_SIZE + TILE_SIZE / 2.0f;
    float cy = (float)ty * TILE_SIZE + TILE_SIZE / 2.0f;
    return dist2d(p->x, p->y - 4.0f, cx, cy) <= REACH_PX;
}

/* Tira ate "amount" unidades da jazida (ou da arvore) do tile. NAO mexe no
 * inventario: diz o que saiu em *type e *got, e quem chamou decide para onde
 * vai (a Luna mete na mochila, a broca mete no seu buffer).
 * Devolve 1 se o aspeto do tile mudou (e portanto precisa de ser redesenhado).
 * Sem raylib nem rato, para poder ser testada sem abrir uma janela. */
int extract_from_tile(int tx, int ty, int amount, int *type, int *got)
{
    Tile *t = tile_at(tx, ty, 0);
    *type = ITEM_NONE;
    *got  = 0;
    if (t->ore == ORE_NONE) return 0;

    int dots_before = ore_dots(t);
    int take = amount < (int)t->ore_amount ? amount : (int)t->ore_amount;
    int changed = 0;

    *type = ORE[t->ore].item;
    *got  = take;
    t->ore_amount = (uint16_t)(t->ore_amount - take);

    if (t->ore_amount == 0) {                       /* esgotado */
        t->ore = ORE_NONE;
        if (t->terrain == T_ARVORE) t->terrain = T_FLORESTA;   /* arvore cortada */
        changed = 1;
    }
    if (t->terrain == T_ARVORE) changed = 1;        /* a copa encolhe a cada corte */

    return changed || ore_dots(t) != dots_before;
}

/* Extracao a mao: o que sai vai direto para a mochila, mas so o que cabe
 * (volume e massa). Se nao couber nada, o tile fica como esta. */
int mine_tile(int tx, int ty, int amount)
{
    Tile *t = tile_at(tx, ty, 0);
    if (t->ore == ORE_NONE) return 0;

    int room = container_room(&backpack, ORE[t->ore].item);
    if (amount > room) amount = room;
    if (amount <= 0) return 0;

    int type, got;
    int changed = extract_from_tile(tx, ty, amount, &type, &got);
    if (got > 0) container_add(&backpack, type, got);
    return changed;
}

/* Ha minerio no tile mas a mochila ja nao tem espaco para 1 unidade? */
int mine_blocked(int tx, int ty)
{
    const Tile *t = tile_at(tx, ty, 0);
    return t->ore != ORE_NONE && container_room(&backpack, ORE[t->ore].item) < 1;
}

/* ------------------------------------------------------------------ */
/* Construcao e logistica                                              */
/* ------------------------------------------------------------------ */

int drill_total(const Building *b)
{
    int s = 0;
    for (int i = 0; i < ITEM_COUNT; i++) s += b->buf[i];
    return s;
}

/* ---- Receitas --------------------------------------------------------
 * Cada receita pertence a um tipo de maquina. A maquina escolhe sozinha a
 * primeira receita para a qual ja tem todos os ingredientes.
 *
 * REALISMO: os ingredientes gastam-se todos e a massa do produto NUNCA pode
 * ser maior do que a massa que entrou (ha um teste que verifica isto).
 * Massas por unidade (items.c): madeira 40 g, carvao 30 g, argila 170 g,
 * areia 160 g, hematite 250 g, tijolo 1800 g, ferro 780 g.
 * Os tempos e as quantidades sao PROVISORIOS: ajustar depois de jogar. */
const Recipe RECIPES[] = {
    /* Pirolise: 4 madeira (160 g) -> 1 carvao (30 g). Rendimento de ~19 %
     * em massa, o normal numa carvoeira de terra (15 a 25 %). */
    { B_CARVOEIRA, "Carvao vegetal", { ITEM_MADEIRA, ITEM_NONE, ITEM_NONE },
      { 4, 0, 0 }, ITEM_CARVAO, 1, 3.0f },

    /* Tijolo (1 L, 1,8 kg): 8 argila + 4 areia (2 : 1, a proporcao da
     * historia) = 2,0 kg de mistura, que perde ~10 % em agua ao cozer.
     * 12 madeira (480 g) de combustivel: queimar tijolo gasta muita lenha. */
    { B_FORNO, "Tijolo", { ITEM_ARGILA, ITEM_AREIA, ITEM_MADEIRA },
      { 8, 4, 12 }, ITEM_TIJOLO, 1, 8.0f },

    /* Ferro (barra de 780 g): 7 hematite (1,75 kg, ~60 % de ferro = 1,05 kg)
     * dao ~74 % de recuperacao num forno de reducao. A reducao gasta ~1,5 kg
     * de carvao por kg de ferro: 40 carvao (1,2 kg). */
    { B_FORNO, "Ferro", { ITEM_HEMATITE, ITEM_CARVAO, ITEM_NONE },
      { 7, 40, 0 }, ITEM_FERRO, 1, 15.0f },

    /* Sucata (ferro enferrujado de uma demolicao) volta a ser ferro: 1 barra de 780 g
     * + 5 carvao (150 g) para a refundir. Muito mais barato do que reduzir minerio. */
    { B_FORNO, "Ferro (sucata)", { ITEM_FERRO_ENF, ITEM_CARVAO, ITEM_NONE },
      { 1, 5, 0 }, ITEM_FERRO, 1, 6.0f },

    /* Forno rupestre: o mesmo tijolo do forno, mas mais devagar (12 s) e so
     * isto. Parte ao fim de RUPESTRE_LIFE tijolos (ver BUILD[].life). */
    { B_FORNO_RUPESTRE, "Tijolo", { ITEM_ARGILA, ITEM_AREIA, ITEM_MADEIRA },
      { 8, 4, 12 }, ITEM_TIJOLO, 1, 12.0f },
};

const int RECIPE_COUNT = (int)(sizeof RECIPES / sizeof RECIPES[0]);

int is_machine(int type)
{
    return type == B_CARVOEIRA || type == B_FORNO || type == B_FORNO_RUPESTRE;
}

/* Quanto desta maquina aceita guardar de um ingrediente: 2 lotes da receita
 * que mais usa esse item (assim ha sempre um lote pronto e outro a chegar).
 * Devolve 0 se nenhuma receita da maquina usa o item. (No Marco 4 isto passa
 * a ser um limite em litros.) */
int machine_in_cap(int machine, int item)
{
    int most = 0;
    if (item == ITEM_NONE) return 0;
    for (int r = 0; r < RECIPE_COUNT; r++) {
        if (RECIPES[r].machine != machine) continue;
        for (int k = 0; k < 3; k++)
            if (RECIPES[r].in_type[k] == item && RECIPES[r].in_n[k] > most)
                most = RECIPES[r].in_n[k];
    }
    return 2 * most;
}

int mach_out_total(const Building *b)
{
    int s = 0;
    for (int i = 0; i < ITEM_COUNT; i++) s += b->out[i];
    return s;
}

/* A maquina tem ingredientes para a receita r? */
static int recipe_ready(const Building *b, int r)
{
    for (int k = 0; k < 3; k++)
        if (RECIPES[r].in_type[k] != ITEM_NONE &&
            b->in[RECIPES[r].in_type[k]] < RECIPES[r].in_n[k]) return 0;
    return 1;
}

/* Algum dos 4 tiles debaixo da broca (origem x, y) ainda tem minerio? */
static int footprint_has_ore(int x, int y)
{
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 2; i++)
            if (in_bounds(x + i, y + j) && tile_at(x + i, y + j, 0)->ore != ORE_NONE)
                return 1;
    return 0;
}

/* So se constroi em terreno onde a Luna anda (e nao na agua rasa do vau) */
static int buildable_terrain(int terrain)
{
    return TERRAIN[terrain].walkable && terrain != T_VAU;
}

/* Da para colocar este tipo de objeto com o canto superior esquerdo em (x, y)?
 * (px, py) e a posicao da Luna em pixels: nao se constroi em cima dela. */
int can_place(int type, int x, int y, float px, float py)
{
    if (type <= B_NONE || type >= B_COUNT || type == B_AMONTOADO) return 0;
    int w = BUILD[type].w, h = BUILD[type].h;

    for (int j = 0; j < h; j++) {
        for (int i = 0; i < w; i++) {
            if (!in_bounds(x + i, y + j)) return 0;
            if (bgrid[y + j][x + i] != 0) return 0;
            const Tile *tt = tile_at(x + i, y + j, 0);
            /* A broca tambem assenta sobre arvores com madeira (serra
             * mecanica): ao esgotar a arvore o tile vira chao da floresta.
             * A parede de arvores do bordo nao tem madeira, por isso nunca. */
            int tree = type == B_BROCA && tt->terrain == T_ARVORE && tt->ore == ORE_MADEIRA;
            if (!tree && !buildable_terrain(tt->terrain)) return 0;
        }
    }
    if (type == B_BROCA && !footprint_has_ore(x, y)) return 0;

    if (BUILD[type].solid) {
        float x0 = (float)(x * TILE_SIZE),       y0 = (float)(y * TILE_SIZE);
        float x1 = (float)((x + w) * TILE_SIZE), y1 = (float)((y + h) * TILE_SIZE);
        if (px + PLAYER_HALF > x0 && px - PLAYER_HALF < x1 &&
            py + PLAYER_HALF > y0 && py - PLAYER_HALF < y1) return 0;
    }
    return 1;
}

/* Cria o objeto na primeira posicao livre da lista. NULL se a lista encheu. */
static Building *new_building(int type, int x, int y, int dir)
{
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        if (buildings[i].type != B_NONE) continue;
        Building *b = &buildings[i];
        memset(b, 0, sizeof *b);
        b->type = (uint8_t)type;
        b->build = 1.0f;                                /* acabado (as obras comecam a 0, ver place_building) */
        b->dir  = (uint8_t)(dir & 3);
        b->x    = (int16_t)x;
        b->y    = (int16_t)y;
        if (BUILD[type].store_ml > 0) {                 /* bau ou contentor */
            b->store.cap_vol_ml = BUILD[type].store_ml;
            b->store.cap_mass_g = 0;                    /* fica no chao: massa ilimitada */
        }
        for (int j = 0; j < BUILD[type].h; j++)
            for (int k = 0; k < BUILD[type].w; k++)
                bgrid[y + j][x + k] = (uint16_t)(i + 1);
        return b;
    }
    return NULL;
}

/* Coloca um objeto. Devolve 1 se algo mudou.
 * Esteira sobre esteira: so a roda (os itens que ela leva ficam). */
int place_building(int type, int x, int y, int dir, float px, float py)
{
    if (type == B_ESTEIRA) {
        Building *old = building_at(x, y);
        if (old != NULL && old->type == B_ESTEIRA) {
            if (old->dir == (dir & 3)) return 0;
            old->dir = (uint8_t)(dir & 3);
            return 1;
        }
    }
    if (!can_place(type, x, y, px, py)) return 0;
    Building *b = new_building(type, x, y, dir);
    if (b == NULL) return 0;
    if (game_mode == MODE_SOBREVIVENCIA) {      /* cria uma obra: a Luna constroi-a aos poucos */
        static uint32_t site_counter;
        b->build = 0.0f;
        b->site_order = ++site_counter;
        b->site_state = SITE_QUEUED;
    }
    return 1;
}

int site_active(const Building *b)
{
    return b->build < 1.0f && !b->demo_on;      /* (uma estrutura a ser demolida ja nao e obra) */
}

int can_afford(int type)
{
    if (game_mode != MODE_SOBREVIVENCIA) return 1;
    for (int k = 0; k < COST_SLOTS; k++)
        if (cost_units(type, k) > 0 && backpack.n[BUILD[type].cost[k].item] < cost_units(type, k)) return 0;
    return 1;
}

/* ---- A obra ------------------------------------------------------------- */

/* Indice do campo do custo que e este item (-1 se a obra nao o usa) */
int site_slot(const Building *b, int item)
{
    for (int k = 0; k < COST_SLOTS; k++)
        if (cost_units(b->type, k) > 0 && BUILD[b->type].cost[k].item == item) return k;
    return -1;
}

int site_need(const Building *b, int item)
{
    int k = site_slot(b, item);
    if (k < 0) return 0;
    int left = cost_units(b->type, k) - (int)b->deposit[k];
    return left > 0 ? left : 0;
}

/* Mochila -> obra. So entra o que a obra ainda pede (nunca mais que o custo). */
int site_deposit(Building *b, Container *from, int item, int n)
{
    if (!site_active(b) || item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    int k = site_slot(b, item);
    if (k < 0) return 0;
    int need = site_need(b, item);
    if (n > need) n = need;
    if (n > (int)from->n[item]) n = (int)from->n[item];
    if (n <= 0) return 0;
    container_take(from, item, n);
    b->deposit[k] = (uint16_t)(b->deposit[k] + n);
    return n;
}

/* Unidades do campo k que o progresso atual ja usou (arredonda para cima) */
static int site_used(const Building *b, int k)
{
    int used = (int)ceilf((float)cost_units(b->type, k) * b->build - 0.001f);
    return used > 0 ? used : 0;
}

/* O que esta depositado e a obra ainda nao usou: so isto se pode tirar (senao
 * dava para depositar, construir e tirar o material de volta) */
int site_free(const Building *b, int item)
{
    int k = site_slot(b, item);
    if (k < 0 || !site_active(b)) return 0;
    int f = (int)b->deposit[k] - site_used(b, k);
    return f > 0 ? f : 0;
}

int site_take_back(Building *b, Container *to, int item, int n)
{
    int k = site_slot(b, item);
    if (k < 0) return 0;
    int f = site_free(b, item);
    if (n > f) n = f;
    if (n <= 0) return 0;
    int moved = container_add(to, item, n);         /* so sai o que cabe no destino */
    b->deposit[k] = (uint16_t)(b->deposit[k] - moved);
    return moved;
}

/* Ate que ponto (0 a 1) o material depositado na obra b deixa adiantar a obra?
 * Cada campo do custo limita: depositado / total. */
static float site_material_limit(const Building *b)
{
    float limit = 1.0f;
    for (int k = 0; k < COST_SLOTS; k++) {
        int n = cost_units(b->type, k);
        if (n == 0 || (int)b->deposit[k] >= n) continue;
        float p = (float)b->deposit[k] / (float)n;
        if (p < limit) limit = p;
    }
    return limit;
}

void construction_step(float px, float py, float dt)
{
    Building *best = NULL;
    float best_p = 0.0f;

    for (int i = 0; i < MAX_BUILDINGS; i++) {
        Building *b = &buildings[i];
        if (b->type == B_NONE || b->build >= 1.0f || b->demo_on) continue;

        if (game_mode == MODE_CRIATIVO) {       /* criativo: acaba logo e sem material */
            b->build = 1.0f;
            continue;
        }

        float cx = ((float)b->x + (float)BUILD[b->type].w / 2.0f) * TILE_SIZE;
        float cy = ((float)b->y + (float)BUILD[b->type].h / 2.0f) * TILE_SIZE;
        float dx = cx - px, dy = cy - (py - 4.0f);
        if (dx * dx + dy * dy > BUILD_REACH_PX * BUILD_REACH_PX) {
            b->site_state = SITE_FAR;
            continue;
        }

        float want  = b->build + dt / build_seconds(b->type);
        float limit = site_material_limit(b);
        float p = want < limit ? want : limit;
        if (p <= b->build + 1e-6f) {            /* nao da para adiantar: falta material depositado */
            b->site_state = SITE_NO_MATERIAL;
            continue;
        }
        b->site_state = SITE_QUEUED;            /* a Luna so constroi uma de cada vez: a mais antiga */
        if (best == NULL || b->site_order < best->site_order) { best = b; best_p = p; }
    }
    if (best == NULL) return;

    best->site_state = SITE_WORKING;
    best->build = best_p >= 1.0f ? 1.0f : best_p;
}

/* Soma, em give[], tudo o que um objeto tem dentro (para a demolicao) */
static void building_contents(const Building *b, int give[ITEM_COUNT])
{
    for (int i = 0; i < ITEM_COUNT; i++) give[i] = 0;

    if (b->type == B_ESTEIRA) {
        for (int i = 0; i < b->n; i++) give[b->item[i]]++;
    } else if (has_store(b->type)) {
        for (int i = 0; i < ITEM_COUNT; i++) give[i] += b->store.n[i];
    } else if (b->type == B_BROCA) {
        for (int i = 0; i < ITEM_COUNT; i++) give[i] += b->buf[i];
    } else if (is_machine(b->type)) {
        for (int i = 0; i < ITEM_COUNT; i++) give[i] += b->in[i] + b->out[i];
        /* a receita em curso ja gastou os ingredientes: devolve-os */
        if (b->recipe != 0) {
            const Recipe *rc = &RECIPES[b->recipe - 1];
            for (int k = 0; k < 3; k++)
                if (rc->in_type[k] != ITEM_NONE) give[rc->in_type[k]] += rc->in_n[k];
        }
    }
}

/* Liberta o espaco de um objeto (sem mexer nos itens: quem chama trata deles) */
static void free_building(Building *b)
{
    for (int j = 0; j < BUILD[b->type].h; j++)
        for (int i = 0; i < BUILD[b->type].w; i++)
            bgrid[b->y + j][b->x + i] = 0;
    b->type = B_NONE;
}

/* Demole o objeto que ocupa (x, y). Os itens que ele tinha (e o custo, se foi
 * construido a pagar) vao para a mochila; se a mochila nao tiver espaco para
 * TUDO (volume ou massa), nao se demole nada e devolve -1 (nenhum item se perde).
 * No amontoado apanha-se o que couber. */
int remove_building_at(int x, int y)
{
    Building *b = building_at(x, y);
    if (b == NULL) return 0;
    if (b->demo_on) return 0;       /* ja esta a ser demolida (o que tinha ja esta no chao ou a sair) */

    if (is_pile(b->type)) {
        for (int i = 1; i < ITEM_COUNT; i++) container_move(&b->store, &backpack, i, 65535);
        if (container_units(&b->store) > 0) return -1;      /* mochila cheia: fica o resto */
        free_building(b);
        return 1;
    }

    int give[ITEM_COUNT];
    building_contents(b, give);
    for (int k = 0; k < COST_SLOTS; k++)                /* o material depositado na obra volta */
        if (b->deposit[k] > 0) give[BUILD[b->type].cost[k].item] += b->deposit[k];
    if (!counts_fit(&backpack, give)) return -1;
    for (int i = 1; i < ITEM_COUNT; i++) container_add(&backpack, i, give[i]);

    free_building(b);
    return 1;
}

/* ---- Itens no chao: o amontoado ----------------------------------------- */

int ground_ok(int x, int y)
{
    if (!in_bounds(x, y)) return 0;
    const Building *b = building_at(x, y);
    if (b != NULL) return is_pile(b->type);
    return buildable_terrain(tile_at(x, y, 0)->terrain);
}

int ground_accepts(int x, int y, int item)
{
    if (item <= ITEM_NONE || item >= ITEM_COUNT || !ground_ok(x, y)) return 0;
    const Building *b = building_at(x, y);
    if (b == NULL) return 1;
    return b->pile_item == item && container_room(&b->store, item) > 0;
}

int drop_to_ground(Container *from, int x, int y, int item, int n)
{
    if (!ground_accepts(x, y, item)) return 0;
    if (n > (int)from->n[item]) n = (int)from->n[item];
    if (n <= 0) return 0;

    Building *b = building_at(x, y);
    int created = 0;
    if (b == NULL) {
        b = new_building(B_AMONTOADO, x, y, 0);
        if (b == NULL) return 0;
        b->pile_item = (uint8_t)item;
        created = 1;
    }
    int moved = container_move(from, &b->store, item, n);
    if (created && moved == 0) free_building(b);        /* nao deixa amontoados vazios */
    if (moved > 0 && item == ITEM_TERRA) b->decay = 0.0f;   /* a terra desaparece TERRA_LIFE_S depois da ultima */
    return moved;
}

int pile_take(Building *b, Container *to, int item, int n)
{
    if (!is_pile(b->type)) return 0;
    int moved = container_move(&b->store, to, item, n);
    if (container_units(&b->store) == 0) free_building(b);
    return moved;
}

/* Quando uma maquina parte, o que tinha dentro espalha-se pelo chao: um
 * amontoado por item, primeiro nos tiles onde ela estava e depois a volta. */
static void spill_and_remove(Building *b)
{
    int give[ITEM_COUNT];
    building_contents(b, give);
    int x = b->x, y = b->y, w = BUILD[b->type].w, h = BUILD[b->type].h;
    free_building(b);

    Container tmp = { 1000000000, 0, { 0 } };       /* so serve para passar os itens */
    for (int i = 1; i < ITEM_COUNT; i++) container_add(&tmp, i, give[i]);

    for (int ring = 0; ring < 2; ring++) {          /* 0 = onde estava, 1 = a volta */
        for (int ty = y - ring; ty < y + h + ring; ty++) {
            for (int tx = x - ring; tx < x + w + ring; tx++) {
                int inside = tx >= x && tx < x + w && ty >= y && ty < y + h;
                if ((ring == 0) != inside) continue;
                for (int i = 1; i < ITEM_COUNT; i++)
                    if (tmp.n[i] > 0) drop_to_ground(&tmp, tx, ty, i, tmp.n[i]);
            }
        }
    }
}

/* ---- Demolicao: o inverso da construcao (ver logic.h) -------------------- */

int demo_active(const Building *b)
{
    return b->demo_on != 0;
}

float demolish_seconds(int type)
{
    float t = build_seconds(type) * (float)DEMOLISH_TIME_PERCENT / 100.0f;
    return t < 0.01f ? 0.01f : t;
}

/* O que sai de demolir b. Tres coisas:
 *  1. o que la esta dentro (esteira, baus, maquinas, broca): sai tal e qual;
 *  2. o material do custo que ESTA NA ESTRUTURA (o que se depositou e a obra ja usou):
 *     sai degradado, ver ITEM[].salvage_pct / salvage_item. Em obra a meio, o material
 *     depositado e ainda nao usado sai tal e qual;
 *  3. a massa que se perdeu em 2. transforma-se em TERRA, por isso a massa total conserva-se
 *     (a menos de 1 unidade de terra, que nao se parte).
 * Usa-se deposit[] como registo do que se pagou: o que se construiu em criativo nao
 * tem deposit e portanto nao da material. */
void demolish_manifest(const Building *b, int give[ITEM_COUNT])
{
    building_contents(b, give);

    int lost_g = 0;
    for (int k = 0; k < COST_SLOTS; k++) {
        if (cost_units(b->type, k) == 0) continue;
        int item  = BUILD[b->type].cost[k].item;
        int paid  = (int)b->deposit[k];
        if (paid <= 0) continue;

        int built = paid;                               /* parte que ja e estrutura */
        if (site_active(b)) {
            built = site_used(b, k);
            if (built > paid) built = paid;
            give[item] += paid - built;                 /* o que a obra ainda nao usou: tal e qual */
        }

        int keep = 0, into = item;
        if (!BUILD[b->type].demo_earth) {
            keep = built * ITEM[item].salvage_pct / 100;
            if (ITEM[item].salvage_item != ITEM_NONE) into = ITEM[item].salvage_item;
        }
        give[into] += keep;
        lost_g += built * item_mass_g(item) - keep * item_mass_g(into);
    }
    if (lost_g > 0) give[ITEM_TERRA] += lost_g / item_mass_g(ITEM_TERRA);
}

int demolish_start(int x, int y)
{
    static uint32_t demo_counter;
    Building *b = building_at(x, y);
    if (b == NULL || is_pile(b->type) || b->demo_on) return 0;

    int give[ITEM_COUNT];
    demolish_manifest(b, give);

    /* uma obra a meio demora so o que ja se construiu */
    float scale = 1.0f;
    if (site_active(b)) scale = b->build < 0.05f ? 0.05f : b->build;
    b->demo_secs = demolish_seconds(b->type) * scale;

    for (int i = 0; i < ITEM_COUNT; i++) {
        b->demo_all[i] = b->demo_left[i] = (uint16_t)give[i];
    }
    /* tudo o que tinha ja esta no manifesto: esvazia-se a estrutura para nao contar duas vezes */
    memset(b->deposit, 0, sizeof b->deposit);
    memset(b->in,  0, sizeof b->in);
    memset(b->out, 0, sizeof b->out);
    memset(b->buf, 0, sizeof b->buf);
    memset(b->store.n, 0, sizeof b->store.n);
    b->n = 0;
    b->recipe = 0;
    b->work = 0.0f;

    b->demo_on    = 1;
    b->demo_p     = 1.0f;
    b->demo_state = DEMO_QUEUED;
    b->demo_order = ++demo_counter;
    return 1;
}

/* Larga ate n unidades de "from" em amontoados nos tiles a volta da estrutura (aneis a
 * distancia 1, 2, ...). A estrutura ainda la esta, por isso nunca se larga por baixo dela.
 * Devolve quantas largou. */
static int spill_item_around(const Building *b, Container *from, int item, int n)
{
    int x = b->x, y = b->y, w = BUILD[b->type].w, h = BUILD[b->type].h;
    int left = n;
    for (int ring = 1; ring <= DEMO_SPILL_RINGS && left > 0; ring++) {
        for (int ty = y - ring; ty < y + h + ring && left > 0; ty++) {
            for (int tx = x - ring; tx < x + w + ring && left > 0; tx++) {
                int border = tx == x - ring || tx == x + w + ring - 1 || ty == y - ring || ty == y + h + ring - 1;
                if (!border) continue;
                left -= drop_to_ground(from, tx, ty, item, left);
            }
        }
    }
    return n - left;
}

/* Deixa sair o que ja devia ter saido quando resta a fraccao p da estrutura: de cada
 * item sai (tudo * (1 - p)), arredondado para baixo. Devolve 0 se faltou chao livre
 * (o que nao coube fica em demo_left; nada se perde). */
static int demo_release(Building *b, float p)
{
    Container tmp = { 1000000000, 0, { 0 } };       /* so serve para passar os itens */
    int ok = 1;
    for (int i = 1; i < ITEM_COUNT; i++) {
        int want = (int)floorf((float)b->demo_all[i] * p);      /* o que ainda pode ficar dentro */
        int out  = (int)b->demo_left[i] - want;
        if (out <= 0) continue;
        tmp.n[i] = (uint16_t)out;
        int moved = spill_item_around(b, &tmp, i, out);
        tmp.n[i] = 0;
        b->demo_left[i] = (uint16_t)(b->demo_left[i] - moved);
        if (moved < out) ok = 0;
    }
    return ok;
}

static void demo_advance(Building *b, float dt, int instant)
{
    float p = instant ? 0.0f : b->demo_p - dt / b->demo_secs;
    if (p < 0.0f) p = 0.0f;

    if (demo_release(b, p)) {
        b->demo_p = p;
        b->demo_state = DEMO_WORKING;
        if (p <= 0.0f) free_building(b);            /* tudo saiu: a estrutura desaparece */
    } else {
        b->demo_state = DEMO_NO_ROOM;               /* nao avanca: espera por chao livre */
    }
}

void demolition_step(float px, float py, float dt)
{
    Building *best = NULL;

    for (int i = 0; i < MAX_BUILDINGS; i++) {
        Building *b = &buildings[i];
        if (b->type == B_NONE || !b->demo_on) continue;

        if (game_mode == MODE_CRIATIVO) {           /* criativo: desfaz-se logo */
            demo_advance(b, dt, 1);
            continue;
        }

        float cx = ((float)b->x + (float)BUILD[b->type].w / 2.0f) * TILE_SIZE;
        float cy = ((float)b->y + (float)BUILD[b->type].h / 2.0f) * TILE_SIZE;
        float dx = cx - px, dy = cy - (py - 4.0f);
        if (dx * dx + dy * dy > BUILD_REACH_PX * BUILD_REACH_PX) {
            b->demo_state = DEMO_FAR;
            continue;
        }
        if (best == NULL) best = b;
        else if (b->demo_order < best->demo_order) { best->demo_state = DEMO_QUEUED; best = b; }
        else b->demo_state = DEMO_QUEUED;           /* a Luna so demole uma de cada vez: a mais antiga */
    }
    if (best != NULL) demo_advance(best, dt, 0);
}

/* ---- Menus das estruturas: mover itens sem perder nenhum ---------------- */

/* Passa ate n unidades de um contador de uma maquina/broca para um contentor */
static int take_slot(uint16_t *slot, Container *to, int item, int n)
{
    if (item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    if (n > (int)*slot) n = (int)*slot;
    int moved = container_add(to, item, n);
    *slot = (uint16_t)(*slot - moved);
    return moved;
}

int machine_take_in(Building *b, Container *to, int item, int n)
{
    if (!is_machine(b->type) || item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    return take_slot(&b->in[item], to, item, n);
}

int machine_take_out(Building *b, Container *to, int item, int n)
{
    if (!is_machine(b->type) || item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    return take_slot(&b->out[item], to, item, n);
}

int drill_take(Building *b, Container *to, int item, int n)
{
    if (b->type != B_BROCA || item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    return take_slot(&b->buf[item], to, item, n);
}

int machine_load(Building *b, Container *from, int item, int n)
{
    if (!is_machine(b->type) || b->build < 1.0f || b->demo_on || item <= ITEM_NONE || item >= ITEM_COUNT) return 0;
    int room = machine_in_cap(b->type, item) - (int)b->in[item];   /* 0 se a maquina nao usa o item */
    if (n > room) n = room;
    if (n > (int)from->n[item]) n = (int)from->n[item];
    if (n <= 0) return 0;
    container_take(from, item, n);
    b->in[item] = (uint16_t)(b->in[item] + n);
    return n;
}

/* ---- Entregar itens: "esteira entra na maquina, sem inserters" -----
 * Tenta meter 1 item no objeto que esta no tile (tx, ty). (dx, dy) e o
 * sentido em que o item vai a andar (de quem entrega para este tile).
 * Devolve 1 se foi aceite. */
int accept_item(int tx, int ty, int dx, int dy, int item)
{
    Building *b = building_at(tx, ty);
    if (b == NULL || b->build < 1.0f || b->demo_on) return 0;   /* sem estrutura, em obra ou a ser demolida */

    if (b->type == B_ESTEIRA) {
        /* uma esteira virada para quem entrega nao aceita (esteiras de frente) */
        if (DIRX[b->dir] == -dx && DIRY[b->dir] == -dy) return 0;
        if (b->n >= BELT_CAP) return 0;
        if (b->n > 0 && b->pos[b->n - 1] < ITEM_GAP) return 0;   /* sem espaco */
        b->item[b->n] = (uint8_t)item;
        b->pos[b->n]  = 0.0f;
        b->n++;
        return 1;
    }
    if (is_storage(b->type)) {
        /* bau e contentor: aceitam enquanto couber (volume; a massa e ilimitada) */
        return container_add(&b->store, item, 1);
    }
    if (is_machine(b->type)) {
        /* a maquina so aceita ingredientes das suas receitas, ate ao limite */
        if (b->in[item] >= machine_in_cap(b->type, item)) return 0;
        b->in[item]++;
        return 1;
    }
    return 0;       /* brocas nao aceitam itens */
}

/* ---- Esteira ------------------------------------------------------- */

static void belt_step(Building *b, float dt)
{
    float adv = BELT_SPEED * dt;

    /* cada item anda, mas nunca ultrapassa o da frente nem o fim do tile */
    for (int i = 0; i < b->n; i++) {
        float limit = (i == 0) ? 1.0f : b->pos[i - 1] - ITEM_GAP;
        float np    = b->pos[i] + adv;
        if (np > limit) np = limit;
        if (np > b->pos[i]) b->pos[i] = np;
    }

    /* o item da frente chegou ao fim: passa para o objeto seguinte, se aceitar */
    if (b->n > 0 && b->pos[0] >= 1.0f) {
        int dx = DIRX[b->dir], dy = DIRY[b->dir];
        if (accept_item(b->x + dx, b->y + dy, dx, dy, b->item[0])) {
            for (int i = 1; i < b->n; i++) {
                b->item[i - 1] = b->item[i];
                b->pos[i - 1]  = b->pos[i];
            }
            b->n--;
        }
    }
}

/* ---- Broca --------------------------------------------------------- */

/* 0 = a trabalhar, 1 = sem minerio por baixo, 2 = saida cheia (bloqueada) */
int drill_status(const Building *b)
{
    if (!footprint_has_ore(b->x, b->y)) return 1;
    if (drill_total(b) >= DRILL_BUF)    return 2;
    return 0;
}

/* Entrega 1 item a um dos tiles em volta de um objeto de w x h tiles (esteira,
 * bau ou maquina). Comeca em cursor_out e roda, por isso reparte por todas as
 * saidas. Os tiles em volta contam-se: esquerda, direita, cima, baixo.
 * Devolve 1 se alguem aceitou o item. */
static int dump_item(Building *b, int w, int h, int item)
{
    int total = 2 * (w + h);
    for (int j = 0; j < total; j++) {
        int o = ((int)b->cursor_out + j) % total;
        int tx, ty, dx, dy;
        if (o < h)                  { tx = b->x - 1; ty = b->y + o;                dx = -1; dy = 0;  }
        else if (o < 2 * h)         { tx = b->x + w; ty = b->y + (o - h);          dx = 1;  dy = 0;  }
        else if (o < 2 * h + w)     { tx = b->x + (o - 2 * h); ty = b->y - 1;      dx = 0;  dy = -1; }
        else                        { tx = b->x + (o - 2 * h - w); ty = b->y + h;  dx = 0;  dy = 1;  }
        if (accept_item(tx, ty, dx, dy, item)) {
            b->cursor_out = (uint8_t)((o + 1) % total);
            return 1;
        }
    }
    return 0;
}

/* Alterna entre os tipos de item que tem, por isso a esteira sai misturada. */
static void drill_dump(Building *b)
{
    for (int k = 0; k < ITEM_COUNT; k++) {
        int type = (b->cursor_type + k) % ITEM_COUNT;
        if (type == ITEM_NONE || b->buf[type] == 0) continue;
        if (dump_item(b, 2, 2, type)) {
            b->buf[type]--;
            b->cursor_type = (uint8_t)(type + 1);
            return;
        }
    }
}

static void drill_step(Building *b, float dt)
{
    if (drill_total(b) < DRILL_BUF) {
        b->timer += dt;
        if (b->timer >= DRILL_PERIOD) {
            /* em cada ciclo tira 1 unidade de CADA tile com minerio por baixo */
            int any = 0;
            for (int i = 0; i < 4; i++) {
                if (drill_total(b) >= DRILL_BUF) break;
                int tx = b->x + (i & 1), ty = b->y + (i >> 1);
                int type, got;
                if (extract_from_tile(tx, ty, 1, &type, &got)) mark_dirty(tx, ty);
                if (got > 0) { b->buf[type] = (uint16_t)(b->buf[type] + got); any = 1; }
            }
            if (any) b->timer -= DRILL_PERIOD;
            else     b->timer = 0.0f;       /* nada para extrair */
        }
    }
    drill_dump(b);
}

/* ---- Maquinas (carvoeira e forno) ---------------------------------- */

/* 0 = a trabalhar, 1 = parada (faltam ingredientes), 2 = saida cheia */
int machine_status(const Building *b)
{
    if (b->recipe != 0) return 0;
    if (mach_out_total(b) >= MACH_OUT_CAP) return 2;
    return 1;
}

static void machine_step(Building *b, float dt)
{
    if (b->recipe == 0) {
        /* parada: procura uma receita para a qual ja tem tudo */
        if (mach_out_total(b) < MACH_OUT_CAP) {
            for (int r = 0; r < RECIPE_COUNT; r++) {
                if (RECIPES[r].machine != b->type || !recipe_ready(b, r)) continue;
                for (int k = 0; k < 3; k++)
                    if (RECIPES[r].in_type[k] != ITEM_NONE)
                        b->in[RECIPES[r].in_type[k]] -= RECIPES[r].in_n[k];
                b->recipe = (uint8_t)(r + 1);
                b->work   = 0.0f;
                break;
            }
        }
    } else {
        const Recipe *rc = &RECIPES[b->recipe - 1];
        b->work += dt;
        if (b->work >= rc->period) {
            b->out[rc->out_type] = (uint16_t)(b->out[rc->out_type] + rc->out_n);
            b->recipe = 0;
            b->work   = 0.0f;
            b->made++;
            /* algumas maquinas partem ao fim de N lotes (forno rupestre) */
            if (BUILD[b->type].life > 0 && b->made >= BUILD[b->type].life) {
                spill_and_remove(b);        /* o conteudo fica no chao */
                return;                     /* b ja nao existe */
            }
        }
    }

    /* despeja 1 produto por passo (esteira, bau ou outra maquina ao lado) */
    for (int k = 0; k < ITEM_COUNT; k++) {
        int type = (b->cursor_type + k) % ITEM_COUNT;
        if (type == ITEM_NONE || b->out[type] == 0) continue;
        if (dump_item(b, 2, 2, type)) {
            b->out[type]--;
            b->cursor_type = (uint8_t)(type + 1);
            break;
        }
    }
}

/* Um passo da simulacao (sempre com o mesmo dt, ver SIM_DT) */
void sim_step(float dt)
{
    for (int i = 0; i < MAX_BUILDINGS; i++) {
        Building *b = &buildings[i];
        if (b->type == B_NONE || b->build < 1.0f || b->demo_on) continue;   /* vazio, obra a meio ou a ser demolida: nao funciona */
        if (b->type == B_BROCA)        drill_step(b, dt);
        else if (b->type == B_ESTEIRA) belt_step(b, dt);
        else if (is_machine(b->type))  machine_step(b, dt);
        else if (is_pile(b->type)) {
            if (container_units(&b->store) == 0) free_building(b);
            else if (b->pile_item == ITEM_TERRA) {          /* a terra solta-se e some do chao */
                b->decay += dt;
                if (b->decay >= TERRA_LIFE_S) free_building(b);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Alcance                                                             */
/* ------------------------------------------------------------------ */

int tile_within(const Player *p, int tx, int ty, float reach)
{
    float cx = (float)tx * TILE_SIZE + TILE_SIZE / 2.0f;
    float cy = (float)ty * TILE_SIZE + TILE_SIZE / 2.0f;
    return dist2d(p->x, p->y - 4.0f, cx, cy) <= reach;
}
