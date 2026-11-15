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

const BuildInfo BUILD[B_COUNT] = {
    [B_NONE]    = { "-",              1, 1, 0 },
    [B_BROCA]   = { "Broca mecanica", 2, 2, 1 },
    [B_ESTEIRA] = { "Esteira",        1, 1, 0 },
    [B_BAU]     = { "Bau",            1, 1, 1 },
    [B_CARVOEIRA] = { "Carvoeira",    2, 2, 1 },
    [B_FORNO]   = { "Forno",          2, 2, 1 },
};

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

/* ------------------------------------------------------------------ */
/* Extracao a mao                                                      */
/* ------------------------------------------------------------------ */

/* Inventario PROVISORIO: so contagens. Em breve passa a ter massa e
 * volume (Marco 4). */
int inventory[ORE_COUNT];

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
    *type = ORE_NONE;
    *got  = 0;
    if (t->ore == ORE_NONE) return 0;

    int dots_before = ore_dots(t);
    int take = amount < (int)t->ore_amount ? amount : (int)t->ore_amount;
    int changed = 0;

    *type = t->ore;
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

/* Extracao a mao: o que sai vai direto para a mochila */
int mine_tile(int tx, int ty, int amount)
{
    int type, got;
    int changed = extract_from_tile(tx, ty, amount, &type, &got);
    if (got > 0) inventory[type] += got;
    return changed;
}

/* ------------------------------------------------------------------ */
/* Construcao e logistica                                              */
/* ------------------------------------------------------------------ */

int chest_total(const Building *b)
{
    int s = 0;
    for (int i = 0; i < ORE_COUNT; i++) s += b->store[i];
    return s;
}

int drill_total(const Building *b)
{
    int s = 0;
    for (int i = 0; i < ORE_COUNT; i++) s += b->buf[i];
    return s;
}

/* ---- Receitas (Marco 3b) --------------------------------------------
 * Cada receita pertence a um tipo de maquina. A maquina escolhe sozinha a
 * primeira receita para a qual ja tem todos os ingredientes. Os numeros sao
 * PROVISORIOS: ajustar depois de jogar. */
const Recipe RECIPES[] = {
    /* pirolise: 4 madeira -> 1 carvao */
    { B_CARVOEIRA, "Carvao vegetal", { ORE_MADEIRA, ORE_NONE, ORE_NONE },
      { 4, 0, 0 }, ORE_CARVAO, 1, 3.0f },
    /* tijolos: 2 argila + 1 areia (a proporcao da historia) + 1 madeira de
     * combustivel -> 3 tijolos. Nao precisa de carvao (o 1o forno da historia
     * levou paus secos). */
    { B_FORNO, "Tijolos", { ORE_ARGILA, ORE_AREIA, ORE_MADEIRA },
      { 2, 1, 1 }, ORE_TIJOLO, 3, 2.0f },
    /* ferro: 3 hematite + 1 carvao -> 2 ferro */
    { B_FORNO, "Ferro", { ORE_HEMATITE, ORE_CARVAO, ORE_NONE },
      { 3, 1, 0 }, ORE_FERRO, 2, 4.0f },
};

const int RECIPE_COUNT = (int)(sizeof RECIPES / sizeof RECIPES[0]);

int is_machine(int type)
{
    return type == B_CARVOEIRA || type == B_FORNO;
}

/* Alguma receita desta maquina usa este item como ingrediente? */
static int machine_wants(int machine, int item)
{
    for (int r = 0; r < RECIPE_COUNT; r++) {
        if (RECIPES[r].machine != machine) continue;
        for (int k = 0; k < 3; k++)
            if (RECIPES[r].in_type[k] == item && item != ORE_NONE) return 1;
    }
    return 0;
}

int mach_out_total(const Building *b)
{
    int s = 0;
    for (int i = 0; i < ORE_COUNT; i++) s += b->out[i];
    return s;
}

/* A maquina tem ingredientes para a receita r? */
static int recipe_ready(const Building *b, int r)
{
    for (int k = 0; k < 3; k++)
        if (RECIPES[r].in_type[k] != ORE_NONE &&
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
        b->dir  = (uint8_t)(dir & 3);
        b->x    = (int16_t)x;
        b->y    = (int16_t)y;
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
    return new_building(type, x, y, dir) != NULL;
}

/* Demole o objeto que ocupa (x, y). Os itens que ele tinha vao para a mochila. */
int remove_building_at(int x, int y)
{
    Building *b = building_at(x, y);
    if (b == NULL) return 0;

    if (b->type == B_ESTEIRA) {
        for (int i = 0; i < b->n; i++) inventory[b->item[i]]++;
    } else if (b->type == B_BAU) {
        for (int i = 0; i < ORE_COUNT; i++) inventory[i] += b->store[i];
    } else if (b->type == B_BROCA) {
        for (int i = 0; i < ORE_COUNT; i++) inventory[i] += b->buf[i];
    } else if (is_machine(b->type)) {
        for (int i = 0; i < ORE_COUNT; i++) inventory[i] += b->in[i] + b->out[i];
        /* a receita em curso ja gastou os ingredientes: devolve-os */
        if (b->recipe != 0) {
            const Recipe *rc = &RECIPES[b->recipe - 1];
            for (int k = 0; k < 3; k++)
                if (rc->in_type[k] != ORE_NONE) inventory[rc->in_type[k]] += rc->in_n[k];
        }
    }

    for (int j = 0; j < BUILD[b->type].h; j++)
        for (int i = 0; i < BUILD[b->type].w; i++)
            bgrid[b->y + j][b->x + i] = 0;
    b->type = B_NONE;
    return 1;
}

/* ---- Entregar itens: "esteira entra na maquina, sem inserters" -----
 * Tenta meter 1 item no objeto que esta no tile (tx, ty). (dx, dy) e o
 * sentido em que o item vai a andar (de quem entrega para este tile).
 * Devolve 1 se foi aceite. */
int accept_item(int tx, int ty, int dx, int dy, int item)
{
    Building *b = building_at(tx, ty);
    if (b == NULL) return 0;

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
    if (b->type == B_BAU) {
        if (chest_total(b) >= CHEST_CAP) return 0;
        b->store[item]++;
        return 1;
    }
    if (is_machine(b->type)) {
        /* a maquina so aceita ingredientes das suas receitas, ate ao limite */
        if (!machine_wants(b->type, item)) return 0;
        if (b->in[item] >= MACH_IN_CAP) return 0;
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
    for (int k = 0; k < ORE_COUNT; k++) {
        int type = (b->cursor_type + k) % ORE_COUNT;
        if (type == ORE_NONE || b->buf[type] == 0) continue;
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
                    if (RECIPES[r].in_type[k] != ORE_NONE)
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
        }
    }

    /* despeja 1 produto por passo (esteira, bau ou outra maquina ao lado) */
    for (int k = 0; k < ORE_COUNT; k++) {
        int type = (b->cursor_type + k) % ORE_COUNT;
        if (type == ORE_NONE || b->out[type] == 0) continue;
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
        if (b->type == B_BROCA)        drill_step(b, dt);
        else if (b->type == B_ESTEIRA) belt_step(b, dt);
        else if (is_machine(b->type))  machine_step(b, dt);
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
