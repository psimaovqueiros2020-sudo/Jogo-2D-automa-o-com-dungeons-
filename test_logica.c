/* Testes da LOGICA do jogo (sem janela, sem raylib verdadeira).
 * Correr, a partir da pasta do projeto:
 *   gcc -std=c99 -Wall -Wextra -Wno-unused-function -DCLAREIRA_TEST -Itests/fake tests/test_logica.c world.c logic.c items.c -o test_logica -lm && ./test_logica
 * (ou simplesmente: make test)
 * So se compilam world.c, logic.c e items.c (a logica); render.c, menu.c e main.c ficam de fora.
 * tests/fake/raylib.h e um cabecalho falso (funcoes que nao fazem nada) so para
 * compilar sem a raylib: world.h inclui raylib.h por causa do tipo Color. O
 * -DCLAREIRA_TEST e obrigatorio: sem ele o cabecalho falso da erro de proposito,
 * para nunca se compilar o jogo com ele. */
#include <stdio.h>
#include "../world.h"
#include "../logic.h"

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHOU: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

static void run(float seconds) { int n = (int)(seconds * 60); for (int i = 0; i < n; i++) sim_step(SIM_DT); }

/* limpa uma area aberta da clareira para montar circuitos de teste */
static void flat(int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++) for (int x = x0; x < x0 + w; x++) {
        Tile *t = tile_at(x, y, 0); t->terrain = T_CLAREIRA; t->ore = ORE_NONE; t->ore_amount = 0;
    }
}

/* ---- ajudas dos testes da demolicao ---- */
static int give_mass(const int give[ITEM_COUNT]) { int m = 0; for (int i = 1; i < ITEM_COUNT; i++) m += give[i] * item_mass_g(i); return m; }

/* soma o que ha em amontoados numa area (cada amontoado ocupa 1 tile) */
static void ground_totals(int x0, int y0, int x1, int y1, int sum[ITEM_COUNT])
{
    for (int i = 0; i < ITEM_COUNT; i++) sum[i] = 0;
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        const Building *pb = building_at(x, y);
        if (pb != NULL && is_pile(pb->type)) for (int i = 1; i < ITEM_COUNT; i++) sum[i] += pb->store.n[i];
    }
}

/* a Luna esta em (px, py) e demole durante "seconds" segundos */
static void demolish_for(float px, float py, float seconds)
{
    int n = (int)(seconds * 60.0f);
    for (int i = 0; i < n; i++) { demolition_step(px, py, SIM_DT); sim_step(SIM_DT); }
}

/* pousa, paga (deposita o custo todo) e constroi a estrutura em (x, y), em sobrevivencia */
static Building *build_paid(int type, int x, int y, float lx, float ly)
{
    flat(60, 50, 30, 20);
    if (type == B_BROCA) for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { Tile *q = tile_at(x + i, y + j, 0); q->ore = ORE_HEMATITE; q->ore_amount = 500; }
    if (!place_building(type, x, y, 0, lx, ly)) return NULL;
    Building *b = building_at(x, y);
    Container src = { 1000000000, 0, { 0 } };
    for (int c = 0; c < COST_SLOTS; c++) {
        if (cost_units(type, c) == 0) continue;
        int it = BUILD[type].cost[c].item;
        src.n[it] = (uint16_t)cost_units(type, c);
        site_deposit(b, &src, it, 65535);
    }
    for (int i = 0; i < (int)(build_seconds(type) * 60.0f) + 120; i++) construction_step(lx, ly, SIM_DT);
    return site_active(b) ? NULL : b;
}

int main(void)
{
    generate_world(DEFAULT_SEED);

    /* ---- 1. Areia: existe em varias seeds e e alcancavel ---- */
    for (uint32_t sd = 1234; sd < 1254; sd++) {
        generate_world(sd);
        int sand = 0, clay = 0;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            Tile *t = tile_at(x, y, 0);
            if (t->ore == ORE_AREIA) { sand++; CHECK(t->terrain == T_LAMA, "areia fora da lama (seed %u)", sd); }
            if (t->ore == ORE_ARGILA) clay++;
        }
        CHECK(sand >= 30, "seed %u: so %d tiles de areia", sd, sand);
        if (sd == 1234) printf("seed 1234: areia=%d tiles, argila=%d tiles\n", sand, clay);
    }
    generate_world(DEFAULT_SEED);

    /* ---- 2. Carvoeira alimentada por uma broca de madeira via esteira ---- */
    flat(60, 50, 30, 20);
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {      /* 4 tiles x 24 madeira = 96 */
        Tile *t = tile_at(62 + i, 52 + j, 0); t->ore = ORE_MADEIRA; t->ore_amount = 24;
    }
    CHECK(place_building(B_BROCA, 62, 52, 0, 0, 0), "broca nao colocou");
    for (int x = 64; x <= 66; x++) CHECK(place_building(B_ESTEIRA, x, 52, 0, 0, 0), "esteira %d", x);
    CHECK(place_building(B_CARVOEIRA, 67, 52, 0, 0, 0), "carvoeira nao colocou");
    for (int x = 69; x <= 71; x++) CHECK(place_building(B_ESTEIRA, x, 52, 0, 0, 0), "esteira saida %d", x);
    CHECK(place_building(B_BAU, 72, 52, 0, 0, 0), "bau");
    run(120.0f);
    Building *bau = building_at(72, 52), *car = building_at(67, 52);
    printf("carvoeira: carvao no bau=%d, dentro: madeira=%d, receita=%d\n", bau->store.n[ITEM_CARVAO], car->in[ITEM_MADEIRA], car->recipe);
    CHECK(bau->store.n[ITEM_CARVAO] == 24, "esperava 24 carvao, deu %d", bau->store.n[ITEM_CARVAO]);
    CHECK(car->in[ITEM_MADEIRA] == 0 && car->recipe == 0, "sobrou madeira/receita");
    CHECK(bau->store.n[ITEM_MADEIRA] == 0, "madeira nao devia chegar ao bau");

    /* ---- 3. Forno: ferro (7 hematite + 40 carvao -> 1 barra), por entrega direta ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    CHECK(place_building(B_FORNO, 70, 55, 0, 0, 0), "forno");
    CHECK(place_building(B_BAU, 72, 55, 0, 0, 0), "bau forno");
    int fed_h = 0, fed_c = 0;
    for (int step = 0; step < 60 * 120; step++) {
        if (fed_h < 28  && accept_item(70, 55, 1, 0, ITEM_HEMATITE)) fed_h++;
        if (fed_c < 160 && accept_item(70, 55, 1, 0, ITEM_CARVAO))   fed_c++;
        sim_step(SIM_DT);
    }
    Building *f = building_at(70, 55); bau = building_at(72, 55);
    printf("forno ferro: alimentou hem=%d carv=%d -> ferro no bau=%d (dentro hem=%d carv=%d)\n",
           fed_h, fed_c, bau->store.n[ITEM_FERRO], f->in[ITEM_HEMATITE], f->in[ITEM_CARVAO]);
    CHECK(fed_h == 28 && fed_c == 160, "alimentacao incompleta");
    CHECK(bau->store.n[ITEM_FERRO] == 4, "esperava 4 ferro, deu %d", bau->store.n[ITEM_FERRO]);
    CHECK(f->in[ITEM_HEMATITE] == 0 && f->in[ITEM_CARVAO] == 0, "sobraram ingredientes");

    /* ---- 4. Forno: tijolos + limites de entrada ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    place_building(B_FORNO, 70, 55, 0, 0, 0);
    place_building(B_BAU, 72, 55, 0, 0, 0);
    /* so hematite: tem de parar nos 12 e rejeitar */
    int acc = 0; for (int i = 0; i < 40; i++) acc += accept_item(70, 55, 1, 0, ITEM_HEMATITE);
    CHECK(acc == machine_in_cap(B_FORNO, ITEM_HEMATITE) && acc == 14, "aceitou %d hematite (cap %d)", acc, machine_in_cap(B_FORNO, ITEM_HEMATITE));
    CHECK(!accept_item(70, 55, 1, 0, ITEM_TIJOLO), "forno aceitou tijolo (produto)");
    CHECK(!accept_item(70, 55, 1, 0, ITEM_QUARTZO), "forno aceitou quartzo");
    CHECK(!accept_item(70, 55, 1, 0, ITEM_ENXOFRE), "forno aceitou enxofre");
    run(10.0f);
    f = building_at(70, 55);
    CHECK(f->recipe == 0 && machine_status(f) == 1, "forno devia estar parado sem carvao");
    int fa = 0, fs = 0, fm = 0;
    for (int step = 0; step < 60 * 60; step++) {
        if (fa < 16 && accept_item(70, 55, 1, 0, ITEM_ARGILA))  fa++;
        if (fs < 8  && accept_item(70, 55, 1, 0, ITEM_AREIA))   fs++;
        if (fm < 24 && accept_item(70, 55, 1, 0, ITEM_MADEIRA)) fm++;
        sim_step(SIM_DT);
    }
    bau = building_at(72, 55);
    printf("forno tijolos: argila=%d areia=%d madeira=%d -> tijolos=%d\n", fa, fs, fm, bau->store.n[ITEM_TIJOLO]);
    CHECK(bau->store.n[ITEM_TIJOLO] == 2, "esperava 2 tijolos, deu %d", bau->store.n[ITEM_TIJOLO]);

    /* ---- 5. Saida cheia: sem destino a maquina para (luz amarela) e nao perde itens ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    place_building(B_CARVOEIRA, 70, 55, 0, 0, 0);
    int fed = 0;
    for (int step = 0; step < 60 * 200; step++) { if (accept_item(70, 55, 1, 0, ITEM_MADEIRA)) fed++; sim_step(SIM_DT); }
    car = building_at(70, 55);
    CHECK(mach_out_total(car) == MACH_OUT_CAP, "saida devia ter %d, tem %d", MACH_OUT_CAP, mach_out_total(car));
    CHECK(machine_status(car) == 2, "estado devia ser 'saida cheia'");
    CHECK(fed == car->in[ITEM_MADEIRA] + 4 * (car->out[ITEM_CARVAO]), "conservacao falhou: fed=%d in=%d out=%d", fed, car->in[ITEM_MADEIRA], car->out[ITEM_CARVAO]);

    /* ---- 6. Demolir devolve tudo (incluindo a receita em curso) ---- */
    container_clear(&backpack);
    int before = fed;
    CHECK(remove_building_at(70, 55) == 1, "demolicao devia dar 1");
    CHECK(remove_building_at(70, 55) == 0, "demolir duas vezes devia dar 0");
    CHECK(backpack.n[ITEM_MADEIRA] + 4 * backpack.n[ITEM_CARVAO] == before, "demolicao perdeu itens: madeira=%d carvao=%d (fed=%d)", backpack.n[ITEM_MADEIRA], backpack.n[ITEM_CARVAO], before);

    /* ---- 7. Maquina ao lado de maquina: carvoeira -> forno directo ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    place_building(B_CARVOEIRA, 66, 55, 0, 0, 0);
    place_building(B_FORNO, 68, 55, 0, 0, 0);       /* encostado a direita da carvoeira */
    place_building(B_BAU, 70, 55, 0, 0, 0);
    int fw = 0, fh = 0;
    for (int step = 0; step < 60 * 400; step++) {
        if (fw < 600 && accept_item(66, 55, 1, 0, ITEM_MADEIRA))   fw++;
        if (fh < 28  && accept_item(68, 55, 1, 0, ITEM_HEMATITE))  fh++;
        sim_step(SIM_DT);
    }
    bau = building_at(70, 55);
    printf("cadeia direta: madeira=%d hematite=%d -> ferro no bau=%d\n", fw, fh, bau->store.n[ITEM_FERRO]);
    CHECK(bau->store.n[ITEM_FERRO] > 0, "cadeia direta nao produziu ferro");

    /* ---- 8. Broca sobre arvores: serra mecanica ---- */
    reset_buildings();
    generate_world(DEFAULT_SEED);
    int tx = -1, ty = -1;
    for (int y = 10; y < MAP_H - 10 && tx < 0; y++) for (int x = 10; x < MAP_W - 10; x++) {
        int ok = 1;
        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { Tile *q = tile_at(x+i,y+j,0); if (q->terrain != T_ARVORE) ok = 0; }
        if (ok) { tx = x; ty = y; break; }
    }
    CHECK(tx >= 0, "nao achei 2x2 de arvores");
    /* (o que se testa aqui e a arvore desaparecer ao esgotar: poe-se pouca madeira, senao
     * as arvores novas, com 40 a 60 L, nao acabavam em 30 s e o bau de 50 L enchia antes) */
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) tile_at(tx + i, ty + j, 0)->ore_amount = 25;
    CHECK(place_building(B_BROCA, tx, ty, 0, 0, 0), "broca nao assenta em arvores");
    /* a parede do bordo continua proibida */
    CHECK(!can_place(B_BROCA, 0, 0, 0, 0) && !can_place(B_BROCA, 1, 1, 0, 0), "broca assentou na parede do bordo");
    CHECK(!can_place(B_BAU, tx + 3, ty, 0, 0) || tile_at(tx+3,ty,0)->terrain != T_ARVORE, "bau assentou em arvore");
    tile_at(tx + 2, ty, 0)->terrain = T_FLORESTA; tile_at(tx + 2, ty, 0)->ore = ORE_NONE;
    CHECK(place_building(B_BAU, tx + 2, ty, 0, 0, 0), "bau ao lado da serra");
    container_clear(&backpack);
    run(30.0f);
    int still_trees = 0;
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) if (tile_at(tx+i,ty+j,0)->terrain == T_ARVORE) still_trees++;
    Building *dr = building_at(tx, ty);
    printf("serra: arvores restantes=%d, madeira no bau=%d\n", still_trees, container_units(&building_at(tx + 2, ty)->store));
    CHECK(still_trees == 0, "arvores nao foram cortadas");
    CHECK(drill_status(dr) == 1 || drill_total(dr) > 0, "estado da broca incoerente");
    CHECK(!is_machine(B_BROCA), "is_machine errado");
    /* ================= MARCO 3c: itens, volume e massa ================= */

    /* ---- 9. Tabela de itens: volume, densidade e massa ---- */
    CHECK(item_mass_g(ITEM_MADEIRA) == 40 && item_mass_g(ITEM_CARVAO) == 30, "massa de madeira/carvao");
    CHECK(item_mass_g(ITEM_HEMATITE) == 250 && item_mass_g(ITEM_FERRO) == 780, "massa de hematite/ferro");
    CHECK(ITEM[ITEM_TIJOLO].vol_ml == 1000 && item_mass_g(ITEM_TIJOLO) == 1800, "tijolo devia ser 1 L e 1800 g");
    CHECK(10 * item_mass_g(ITEM_CARVAO) == 300, "1 L de carvao devia pesar 300 g");
    for (int i = 1; i < ITEM_COUNT; i++) {
        CHECK(ITEM[i].vol_ml >= 100, "item %d com menos de 100 mL", i);
        CHECK(item_mass_g(i) > 0, "item %d sem massa", i);
    }
    CHECK(ORE[ORE_MADEIRA].item == ITEM_MADEIRA && ORE[ORE_HEMATITE].item == ITEM_HEMATITE, "jazida -> item errado");
    for (int o = 1; o < ORE_COUNT; o++) CHECK(ORE[o].richest > 0 && ORE[o].item != ITEM_NONE, "jazida %d mal definida", o);
    printf("massas por unidade (g): madeira=%d carvao=%d areia=%d argila=%d hematite=%d ferro=%d tijolo=%d\n",
           item_mass_g(ITEM_MADEIRA), item_mass_g(ITEM_CARVAO), item_mass_g(ITEM_AREIA),
           item_mass_g(ITEM_ARGILA), item_mass_g(ITEM_HEMATITE), item_mass_g(ITEM_FERRO), item_mass_g(ITEM_TIJOLO));

    /* ---- 10. Limites da mochila (20 L e 50 kg): basta um deles chegar ---- */
    Container bp = { BACKPACK_VOL_ML, BACKPACK_MASS_G, { 0 } };
    CHECK(container_add(&bp, ITEM_MADEIRA, 1000) == 200, "madeira: limite de volume (200 un = 20 L)");
    CHECK(container_vol_ml(&bp) == 20000 && container_mass_g(&bp) == 8000, "madeira: 20 L e 8 kg");
    CHECK(container_room(&bp, ITEM_AREIA) == 0, "mochila cheia de volume aceitou areia");
    container_clear(&bp);
    CHECK(container_add(&bp, ITEM_FERRO, 1000) == 64, "ferro: limite de MASSA (64 barras = 49,92 kg)");
    CHECK(container_mass_g(&bp) == 49920 && container_vol_ml(&bp) < 20000, "ferro: massa no limite, volume nao");
    CHECK(container_room(&bp, ITEM_CARVAO) == 0 || container_mass_g(&bp) + 30 <= 50000, "mochila passou os 50 kg");
    container_clear(&bp);
    CHECK(container_add(&bp, ITEM_HEMATITE, 1000) == 200 && container_mass_g(&bp) == 50000, "hematite: 20 L = 50 kg exatos");
    container_clear(&bp);
    CHECK(container_add(&bp, ITEM_MADEIRA, 150) == 150, "150 madeira (15 L)");
    CHECK(container_room(&bp, ITEM_FERRO) == 50, "ferro depois de 15 L de madeira: volume manda (50), nao massa (56)");
    container_clear(&bp);
    CHECK(container_add(&bp, ITEM_MADEIRA, 195) == 195, "195 madeira (19,5 L)");
    CHECK(container_room(&bp, ITEM_TIJOLO) == 0, "tijolo (1 L) nao cabe em 0,5 L: nao se parte a unidade");
    CHECK(container_room(&bp, ITEM_AREIA) == 5, "areia cabe nos 0,5 L que sobram (5 un)");
    container_clear(&bp);
    CHECK(container_add(&bp, ITEM_TIJOLO, 100) == 20 && container_mass_g(&bp) == 36000, "20 tijolos = 20 L = 36 kg");
    CHECK(container_room(&bp, ITEM_NONE) == 0 && container_add(&bp, ITEM_NONE, 5) == 0, "ITEM_NONE nao se guarda");
    CHECK(container_take(&bp, ITEM_TIJOLO, 7) == 7 && bp.n[ITEM_TIJOLO] == 13, "tirar 7 tijolos");
    CHECK(container_take(&bp, ITEM_TIJOLO, 100) == 13 && bp.n[ITEM_TIJOLO] == 0, "tirar mais do que ha");
    bp.n[ITEM_AREIA] = 50;
    container_clear(&bp);
    CHECK(container_units(&bp) == 0 && bp.cap_vol_ml == BACKPACK_VOL_ML && bp.cap_mass_g == BACKPACK_MASS_G, "clear mantem os limites");
    CHECK(backpack.cap_vol_ml == 20000 && backpack.cap_mass_g == 50000, "a mochila do jogo devia ter 20 L e 50 kg");

    /* ---- 11. Carga: como o peso afeta a Luna ---- */
    CHECK(carry_state(0) == 0 && carry_state(15000) == 0, "ate 15 kg: leve");
    CHECK(carry_state(15001) == 1 && carry_state(25000) == 1 && carry_state(30000) == 1, "15 a 30 kg: devagar");
    CHECK(carry_state(30001) == 2 && carry_state(50000) == 2, "30 a 50 kg: nao anda");
    CHECK(carry_speed_factor(10000) == 1.0f && carry_speed_factor(20000) == CARRY_SLOW_FACTOR && carry_speed_factor(40000) == 0.0f, "fator de velocidade");
    CHECK(carry_can_run(15000) && !carry_can_run(15001), "so corre ate 15 kg");
    CHECK(PLAYER_SPEED * CARRY_SLOW_FACTOR < PLAYER_SPEED, "devagar tem de ser mais lento");
    container_clear(&backpack);
    container_add(&backpack, ITEM_AREIA, 1000);          /* 20 L de areia = 32 kg */
    CHECK(container_mass_g(&backpack) == 32000 && carry_state(container_mass_g(&backpack)) == 2, "20 L de areia (32 kg) nao deixam andar");
    container_clear(&backpack);

    /* ---- 12. Bau (50 L) e contentor (2000 L): volume limita, massa nao ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    CHECK(BUILD[B_CONTENTOR].w == 2 && BUILD[B_CONTENTOR].h == 2 && BUILD[B_CONTENTOR].store_ml == 2000000, "contentor 2x2, 2000 L");
    CHECK(BUILD[B_BAU].store_ml == 50000, "bau 50 L");
    CHECK(place_building(B_BAU, 62, 52, 0, 0, 0) && place_building(B_CONTENTOR, 66, 52, 0, 0, 0), "bau e contentor");
    CHECK(!place_building(B_CONTENTOR, 67, 53, 0, 0, 0), "contentor por cima de contentor");
    int in_bau = 0, in_cont = 0;
    for (int i = 0; i < 1000; i++) in_bau += accept_item(62, 52, 1, 0, ITEM_HEMATITE);
    CHECK(in_bau == 500, "bau devia aceitar 500 un de hematite (50 L, 125 kg: massa ilimitada), aceitou %d", in_bau);
    CHECK(!accept_item(62, 52, 1, 0, ITEM_MADEIRA), "bau cheio aceitou madeira");
    for (int i = 0; i < 25000; i++) in_cont += accept_item(66, 52, 1, 0, ITEM_HEMATITE);
    CHECK(in_cont == 20000, "contentor devia aceitar 20000 un (2000 L), aceitou %d", in_cont);
    CHECK(container_mass_g(&building_at(66, 52)->store) == 5000000, "contentor com 5000 kg de hematite");
    CHECK(!accept_item(66, 52, 1, 0, ITEM_TIJOLO), "contentor cheio aceitou tijolo");
    reset_buildings();
    flat(60, 50, 30, 20);
    place_building(B_BAU, 62, 52, 0, 0, 0);
    Building *cx = building_at(62, 52);
    cx->store.n[ITEM_MADEIRA] = 400;                      /* 40 L */
    int bricks = 0;
    for (int i = 0; i < 20; i++) bricks += accept_item(62, 52, 1, 0, ITEM_TIJOLO);
    CHECK(bricks == 10, "restavam 10 L: 10 tijolos, entraram %d", bricks);
    CHECK(container_vol_ml(&cx->store) == 50000, "bau devia ficar com 50 L");

    /* ---- 13. Transferencias entre contentores (o menu): nada se perde ---- */
    container_clear(&backpack);
    Container ch = { CHEST_VOL_ML, 0, { 0 } };
    ch.n[ITEM_HEMATITE] = 300;                            /* 30 L, 75 kg */
    int moved = container_move(&ch, &backpack, ITEM_HEMATITE, 1000000);
    CHECK(moved == 200 && ch.n[ITEM_HEMATITE] == 100 && backpack.n[ITEM_HEMATITE] == 200, "passou %d (esperava 200, ficam 100)", moved);
    CHECK(container_move(&ch, &backpack, ITEM_HEMATITE, 10) == 0 && ch.n[ITEM_HEMATITE] == 100, "mochila cheia: nao passa nada");
    CHECK(container_move(&backpack, &ch, ITEM_HEMATITE, 50) == 50 && ch.n[ITEM_HEMATITE] == 150 && backpack.n[ITEM_HEMATITE] == 150, "devolver 50");
    CHECK(ch.n[ITEM_HEMATITE] + backpack.n[ITEM_HEMATITE] == 300, "conservacao nas transferencias");
    CHECK(container_move(&ch, &backpack, ITEM_FERRO, 10) == 0, "mover o que nao ha");
    container_clear(&backpack);

    /* ---- 14. Minerar a mao respeita a mochila ---- */
    flat(60, 50, 30, 20);
    Tile *mt = tile_at(64, 60, 0); mt->terrain = T_PEDRA; mt->ore = ORE_HEMATITE; mt->ore_amount = 400;
    int guard = 0;
    while (!mine_blocked(64, 60) && guard++ < 1000) mine_tile(64, 60, 1);
    CHECK(backpack.n[ITEM_HEMATITE] == 200, "mochila devia parar nas 200 un, tem %d", backpack.n[ITEM_HEMATITE]);
    CHECK(mt->ore_amount == 200, "a jazida devia ficar com 200 (conservacao), tem %d", mt->ore_amount);
    CHECK(mine_blocked(64, 60) && !mine_tile(64, 60, 5) && mt->ore_amount == 200, "mochila cheia: jazida nao pode perder minerio");
    container_take(&backpack, ITEM_HEMATITE, 150);
    mine_tile(64, 60, 1000);                              /* pede 1000, so cabem 150 */
    CHECK(backpack.n[ITEM_HEMATITE] == 200 && mt->ore_amount == 50, "pedir 1000 so tira o que cabe: mochila=%d jazida=%d (esperava 200 e 50)", backpack.n[ITEM_HEMATITE], mt->ore_amount);
    container_clear(&backpack);

    /* ---- 15. Demolir: tudo vai para a mochila, ou nao se demole nada ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    place_building(B_BAU, 62, 52, 0, 0, 0);
    building_at(62, 52)->store.n[ITEM_MADEIRA] = 120;
    container_clear(&backpack);
    CHECK(remove_building_at(62, 52) == 1 && building_at(62, 52) == NULL, "bau com 120 madeira devia demolir");
    CHECK(backpack.n[ITEM_MADEIRA] == 120, "mochila devia ter 120 madeira, tem %d", backpack.n[ITEM_MADEIRA]);
    container_clear(&backpack);

    place_building(B_BAU, 62, 52, 0, 0, 0);
    Building *big = building_at(62, 52);
    big->store.n[ITEM_HEMATITE] = 300;                    /* 30 L > 20 L da mochila */
    CHECK(remove_building_at(62, 52) == -1, "bau grande demais devia recusar (-1)");
    CHECK(building_at(62, 52) != NULL && big->store.n[ITEM_HEMATITE] == 300 && container_units(&backpack) == 0, "recusar nao pode mudar nada");
    big->store.n[ITEM_HEMATITE] = 0;
    big->store.n[ITEM_FERRO] = 70;                        /* 7 L mas 54,6 kg > 50 kg */
    CHECK(remove_building_at(62, 52) == -1, "70 barras de ferro (54,6 kg) devia recusar por MASSA");
    big->store.n[ITEM_FERRO] = 60;                        /* 46,8 kg: ja cabe */
    CHECK(remove_building_at(62, 52) == 1 && backpack.n[ITEM_FERRO] == 60, "60 barras cabem");
    container_clear(&backpack);

    /* demolir um bau cheio so e possivel depois de o esvaziar (como no menu) */
    place_building(B_BAU, 62, 52, 0, 0, 0);
    big = building_at(62, 52);
    big->store.n[ITEM_HEMATITE] = 300;
    container_move(&big->store, &backpack, ITEM_HEMATITE, 1000000);
    CHECK(remove_building_at(62, 52) == -1, "mochila cheia: continua a recusar");
    container_clear(&backpack);
    CHECK(remove_building_at(62, 52) == 1 && backpack.n[ITEM_HEMATITE] == 100, "esvaziada a mochila, os 100 que restavam cabem");
    container_clear(&backpack);

    /* uma esteira com itens: os itens vao para a mochila */
    place_building(B_ESTEIRA, 70, 52, 0, 0, 0);
    CHECK(accept_item(70, 52, 1, 0, ITEM_TIJOLO), "esteira devia aceitar o tijolo");
    CHECK(!accept_item(70, 52, 1, 0, ITEM_CARVAO), "sem espaco entre itens: a esteira recusa");
    CHECK(remove_building_at(70, 52) == 1 && backpack.n[ITEM_TIJOLO] == 1 && container_units(&backpack) == 1, "o tijolo da esteira vai para a mochila");
    container_clear(&backpack);

    /* ---- 16. A broca respeita o limite do bau: nao perde minerio ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
        Tile *q = tile_at(62 + i, 52 + j, 0); q->terrain = T_PEDRA; q->ore = ORE_HEMATITE; q->ore_amount = 600;
    }
    CHECK(place_building(B_BROCA, 62, 52, 0, 0, 0) && place_building(B_BAU, 64, 52, 0, 0, 0), "broca + bau");
    run(120.0f);
    Building *dbau = building_at(64, 52), *dbr = building_at(62, 52);
    int left_on_tiles = 0;
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) left_on_tiles += tile_at(62 + i, 52 + j, 0)->ore_amount;
    printf("broca + bau cheio: bau=%d un (%d L), broca guarda %d, jazida resta %d\n",
           dbau->store.n[ITEM_HEMATITE], container_vol_ml(&dbau->store) / 1000, drill_total(dbr), left_on_tiles);
    CHECK(dbau->store.n[ITEM_HEMATITE] == 500, "bau devia estar cheio (500 un), tem %d", dbau->store.n[ITEM_HEMATITE]);
    CHECK(drill_status(dbr) == 2, "broca devia estar parada com a saida cheia");
    CHECK(4 * 600 - left_on_tiles == dbau->store.n[ITEM_HEMATITE] + drill_total(dbr), "conservacao: extraido != bau + broca");

    /* ---- 17. Receitas realistas: a massa do produto nao passa a que entrou ---- */
    for (int r = 0; r < RECIPE_COUNT; r++) {
        int in_mass = 0;
        for (int k = 0; k < 3; k++)
            if (RECIPES[r].in_type[k] != ITEM_NONE) in_mass += RECIPES[r].in_n[k] * item_mass_g(RECIPES[r].in_type[k]);
        int out_mass = RECIPES[r].out_n * item_mass_g(RECIPES[r].out_type);
        printf("receita %-15s entra %5d g -> sai %5d g (rendimento %d%%)\n", RECIPES[r].name, in_mass, out_mass, 100 * out_mass / in_mass);
        CHECK(out_mass <= in_mass, "receita '%s' cria massa do nada (%d g -> %d g)", RECIPES[r].name, in_mass, out_mass);
        CHECK(out_mass * 10 >= in_mass, "receita '%s' desperdica quase tudo", RECIPES[r].name);
        for (int k = 0; k < 3; k++)
            if (RECIPES[r].in_type[k] != ITEM_NONE)
                CHECK(machine_in_cap(RECIPES[r].machine, RECIPES[r].in_type[k]) >= 2 * RECIPES[r].in_n[k], "cap pequeno para a receita '%s'", RECIPES[r].name);
    }


    /* ================= MARCO 3c: modos, custos, forno rupestre, amontoados ================= */
    {
    CHECK(game_mode == MODE_CRIATIVO, "a logica arranca em criativo (o main.c escolhe o modo do jogo)");

    /* ---- 18. Tabela de custos e tempos de obra ---- */
    CHECK(BUILDABLE_COUNT == 7, "o menu devia ter 7 estruturas");
    for (int k = 0; k < BUILDABLE_COUNT; k++) {
        int ty18 = BUILDABLE[k];
        CHECK(ty18 > B_NONE && ty18 < B_COUNT && ty18 != B_AMONTOADO, "BUILDABLE[%d] invalido", k);
        int slots = 0;
        for (int c = 0; c < COST_SLOTS; c++) {
            if (cost_units(ty18, c) == 0) continue;
            slots++;
            for (int d = c + 1; d < COST_SLOTS; d++)
                CHECK(BUILD[ty18].cost[c].item != BUILD[ty18].cost[d].item || cost_units(ty18, d) == 0, "%s: o mesmo item em dois campos do custo", BUILD[ty18].name);
        }
        CHECK(slots > 0, "%s nao tem custo", BUILD[ty18].name);
        CHECK(build_seconds(ty18) > 0.0f, "%s sem tempo de obra", BUILD[ty18].name);
        CHECK(BUILD[ty18].desc != NULL && BUILD[ty18].desc[0] != '\0', "%s sem descricao", BUILD[ty18].name);
        printf("custo %-15s %6.1f L %7.1f kg  obra %5.1f s\n", BUILD[ty18].name,
               (double)build_cost_vol_ml(ty18) / 1000.0, (double)build_cost_mass_g(ty18) / 1000.0, (double)build_seconds(ty18));
    }
    CHECK(BUILD[B_FORNO_RUPESTRE].cost[0].item == ITEM_ARGILA && BUILD[B_FORNO_RUPESTRE].cost[1].item == ITEM_NONE,
          "o forno rupestre e feito SO de argila");
#if COST_PERCENT >= 100
    CHECK(build_cost_mass_g(B_FORNO_RUPESTRE) >= 100000, "o forno rupestre custa %d kg: um forno de verdade leva centenas de kg de argila, nao uns 4", build_cost_mass_g(B_FORNO_RUPESTRE) / 1000);
    CHECK(build_cost_mass_g(B_FORNO) >= 100000, "o forno de tijolos custa tijolos de sobra");
#endif
    CHECK(build_seconds(B_ESTEIRA) < build_seconds(B_BAU) && build_seconds(B_BAU) < build_seconds(B_CARVOEIRA) &&
          build_seconds(B_CARVOEIRA) < build_seconds(B_FORNO_RUPESTRE) && build_seconds(B_FORNO_RUPESTRE) < build_seconds(B_BROCA) &&
          build_seconds(B_BROCA) < build_seconds(B_FORNO) && build_seconds(B_FORNO) < build_seconds(B_CONTENTOR),
          "o tempo de obra devia crescer com a complexidade");
    CHECK(BUILD[B_FORNO_RUPESTRE].life == RUPESTRE_LIFE && RUPESTRE_LIFE == 20, "o forno rupestre parte aos 20 tijolos");
    CHECK(is_machine(B_FORNO_RUPESTRE) && !is_machine(B_AMONTOADO) && is_pile(B_AMONTOADO) && has_store(B_AMONTOADO) && has_store(B_BAU) && !has_store(B_FORNO), "classificacao dos tipos");
    CHECK(!can_place(B_AMONTOADO, 62, 52, 0, 0), "o amontoado nao se constroi, so se cria a largar itens");

    /* ---- 19. Sobrevivencia: obras (o jogador DEPOSITA o material pelo menu) ---- */
    {
    const float lx = 64 * TILE_SIZE + 8.0f, ly = 54 * TILE_SIZE + 8.0f;     /* a Luna */
    reset_buildings();
    flat(60, 50, 30, 20);
    container_clear(&backpack);
    game_mode = MODE_SOBREVIVENCIA;

    /* pode-se sempre pousar a estrutura, mesmo sem material: fica uma obra a 0 */
    CHECK(!can_afford(B_FORNO_RUPESTRE), "mochila vazia: nao tem o material");
    CHECK(place_building(B_FORNO_RUPESTRE, 66, 55, 0, lx, ly) == 1, "devia deixar pousar a obra sem material");
    Building *ob = building_at(66, 55);
    CHECK(ob != NULL && site_active(ob) && ob->build == 0.0f && ob->deposit[0] == 0, "obra a 0");
    for (int step = 0; step < 600; step++) construction_step(lx, ly, SIM_DT);
    CHECK(ob->build == 0.0f && ob->site_state == SITE_NO_MATERIAL, "sem material a obra nao avanca (estado %d)", ob->site_state);
    /* a mochila NAO e usada sozinha: o material tem de ser depositado na obra */
    backpack.n[ITEM_ARGILA] = 200;
    for (int step = 0; step < 600; step++) construction_step(lx, ly, SIM_DT);
    CHECK(ob->build == 0.0f && backpack.n[ITEM_ARGILA] == 200 && ob->site_state == SITE_NO_MATERIAL,
          "a obra nao puxa material da mochila (build %.3f, mochila %d)", (double)ob->build, backpack.n[ITEM_ARGILA]);
    /* a meio ou a zero, nao funciona */
    CHECK(!accept_item(66, 55, 1, 0, ITEM_ARGILA) && machine_load(ob, &backpack, ITEM_ARGILA, 5) == 0, "uma obra nao aceita ingredientes");

    /* so se deposita o que a obra pede, e nunca mais do que o custo */
    backpack.n[ITEM_MADEIRA] = 10;
    CHECK(site_slot(ob, ITEM_MADEIRA) == -1 && site_deposit(ob, &backpack, ITEM_MADEIRA, 10) == 0 && backpack.n[ITEM_MADEIRA] == 10,
          "o forno rupestre e so de argila: nao aceita madeira");
    backpack.n[ITEM_MADEIRA] = 0;
    CHECK(site_slot(ob, ITEM_ARGILA) == 0 && site_need(ob, ITEM_ARGILA) == 800, "a obra pede 800 argila");
    CHECK(site_deposit(ob, &backpack, ITEM_ARGILA, 1000) == 200 && backpack.n[ITEM_ARGILA] == 0 && ob->deposit[0] == 200,
          "deposita o que ha na mochila (200): depositado %d", ob->deposit[0]);
    CHECK(site_need(ob, ITEM_ARGILA) == 600, "faltam 600");

    /* gasta ao ritmo da obra (10 s -> 10/45 do tempo), sem passar do que ha depositado */
    for (int step = 0; step < 600; step++) construction_step(lx, ly, SIM_DT);    /* 10 s */
    float expect = 10.0f / build_seconds(B_FORNO_RUPESTRE);
    CHECK(ob->build > expect - 0.01f && ob->build < expect + 0.01f, "10 s de obra: progresso %.3f (esperado %.3f)", (double)ob->build, (double)expect);
    CHECK(ob->site_state == SITE_WORKING, "a trabalhar (estado %d)", ob->site_state);

    /* tirar de volta: so o que a obra ainda nao usou */
    int fr = site_free(ob, ITEM_ARGILA);
    CHECK(fr >= 20 && fr <= 24, "depositou 200, a obra usou ~178: sobram ~22 (deu %d)", fr);
    CHECK(site_take_back(ob, &backpack, ITEM_ARGILA, 1000) == fr && backpack.n[ITEM_ARGILA] == fr && ob->deposit[0] == 200 - fr,
          "tira so o que nao foi usado (%d)", fr);
    CHECK(site_deposit(ob, &backpack, ITEM_ARGILA, 1000) == fr && ob->deposit[0] == 200 && backpack.n[ITEM_ARGILA] == 0, "volta a depositar");

    /* chega ao fim do material depositado e fica a meio (~25 % = 200/800) */
    for (int step = 0; step < 60 * 100; step++) construction_step(lx, ly, SIM_DT);
    CHECK(ob->deposit[0] == 200 && backpack.n[ITEM_ARGILA] == 0, "nada se gasta nem se perde: depositado %d", ob->deposit[0]);
    CHECK(site_active(ob) && ob->build > 0.24f && ob->build < 0.26f && ob->site_state == SITE_NO_MATERIAL, "fica a meio (~25%%): %.3f", (double)ob->build);
    CHECK(site_free(ob, ITEM_ARGILA) == 0 && site_take_back(ob, &backpack, ITEM_ARGILA, 5) == 0, "o que ja esta na obra nao se tira (so demolindo)");
    run(5.0f);
    CHECK(!accept_item(66, 55, 1, 0, ITEM_ARGILA), "a meio continua sem funcionar");

    /* depositar o resto: acaba e passa a funcionar (nao se deposita mais do que o custo) */
    backpack.n[ITEM_ARGILA] = 700;
    CHECK(site_deposit(ob, &backpack, ITEM_ARGILA, 1000) == 600 && backpack.n[ITEM_ARGILA] == 100 && ob->deposit[0] == 800, "so cabem os 600 que faltam");
    CHECK(site_deposit(ob, &backpack, ITEM_ARGILA, 1000) == 0 && backpack.n[ITEM_ARGILA] == 100, "ja tem tudo: nao aceita mais");
    for (int step = 0; step < 60 * 60; step++) construction_step(lx, ly, SIM_DT);
    CHECK(!site_active(ob) && ob->deposit[0] == 800 && backpack.n[ITEM_ARGILA] == 100, "obra acabada (depositado %d, mochila %d)", ob->deposit[0], backpack.n[ITEM_ARGILA]);
    CHECK(accept_item(66, 55, 1, 0, ITEM_ARGILA), "acabada, ja aceita ingredientes");
    CHECK(site_deposit(ob, &backpack, ITEM_ARGILA, 10) == 0 && site_take_back(ob, &backpack, ITEM_ARGILA, 10) == 0, "acabada: ja nao e obra, nao recebe nem devolve");

    /* o tempo da obra e o da tabela, nem mais cedo; o material fica todo na obra */
    for (int k = 0; k < BUILDABLE_COUNT; k++) {
        int ty19 = BUILDABLE[k];
        reset_buildings();
        flat(60, 50, 30, 20);
        container_clear(&backpack);
        if (ty19 == B_BROCA) for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { Tile *q = tile_at(66 + i, 55 + j, 0); q->ore = ORE_HEMATITE; q->ore_amount = 500; }
        CHECK(place_building(ty19, 66, 55, 0, lx, ly) == 1, "%s: pousar a obra", BUILD[ty19].name);
        Building *sb = building_at(66, 55);
        for (int c = 0; c < COST_SLOTS; c++) {
            if (cost_units(ty19, c) == 0) continue;
            int it = BUILD[ty19].cost[c].item;
            backpack.n[it] = (uint16_t)(2 * cost_units(ty19, c));           /* mais do que o preciso */
            CHECK(site_deposit(sb, &backpack, it, 100000) == cost_units(ty19, c), "%s: deposita exatamente o custo", BUILD[ty19].name);
        }
        int steps_early = (int)(build_seconds(ty19) * 0.9f * 60.0f);
        for (int step = 0; step < steps_early; step++) construction_step(lx, ly, SIM_DT);
        CHECK(site_active(sb), "%s: acabou antes do tempo", BUILD[ty19].name);
        for (int step = 0; step < (int)(build_seconds(ty19) * 0.2f * 60.0f) + 2; step++) construction_step(lx, ly, SIM_DT);
        CHECK(!site_active(sb), "%s: devia estar acabada (%.3f)", BUILD[ty19].name, (double)sb->build);
        for (int c = 0; c < COST_SLOTS; c++) {
            if (cost_units(ty19, c) == 0) continue;
            CHECK(sb->deposit[c] == cost_units(ty19, c) && backpack.n[BUILD[ty19].cost[c].item] == cost_units(ty19, c),
                  "%s: o custo ficou na obra (depositado %d, sobra %d, custo %d)", BUILD[ty19].name, sb->deposit[c], backpack.n[BUILD[ty19].cost[c].item], cost_units(ty19, c));
        }
    }

    /* varios materiais: o que menos chegou limita a obra (broca = 8 ferro + 10 tijolos + 60 madeira) */
    reset_buildings(); flat(60, 50, 30, 20); container_clear(&backpack);
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { Tile *q = tile_at(66 + i, 55 + j, 0); q->ore = ORE_HEMATITE; q->ore_amount = 500; }
    place_building(B_BROCA, 66, 55, 0, lx, ly);
    Building *dr19 = building_at(66, 55);
    backpack.n[ITEM_FERRO] = 8; backpack.n[ITEM_TIJOLO] = 10; backpack.n[ITEM_MADEIRA] = 30;
    site_deposit(dr19, &backpack, ITEM_FERRO, 100); site_deposit(dr19, &backpack, ITEM_TIJOLO, 100); site_deposit(dr19, &backpack, ITEM_MADEIRA, 100);
    for (int step = 0; step < 60 * 100; step++) construction_step(lx, ly, SIM_DT);
    CHECK(site_active(dr19) && dr19->build > 0.49f && dr19->build < 0.51f && dr19->site_state == SITE_NO_MATERIAL,
          "so ha metade da madeira: a obra pára a meio (%.3f)", (double)dr19->build);
    backpack.n[ITEM_MADEIRA] = 30;
    site_deposit(dr19, &backpack, ITEM_MADEIRA, 100);
    for (int step = 0; step < 60 * 100; step++) construction_step(lx, ly, SIM_DT);
    CHECK(!site_active(dr19), "com a madeira toda a obra acaba");

    /* longe: a Luna tem de estar ao pe (o material la fica, mas nao avanca) */
    reset_buildings(); flat(60, 50, 30, 20); container_clear(&backpack);
    backpack.n[ITEM_MADEIRA] = 100;
    place_building(B_BAU, 66, 55, 0, lx, ly);
    Building *far_site = building_at(66, 55);
    site_deposit(far_site, &backpack, ITEM_MADEIRA, 100);
    for (int step = 0; step < 600; step++) construction_step(lx + 2000.0f, ly, SIM_DT);
    CHECK(far_site->build == 0.0f && far_site->site_state == SITE_FAR && far_site->deposit[0] == 100, "longe: nao avanca");
    for (int step = 0; step < 60 * 10; step++) construction_step(lx, ly, SIM_DT);
    CHECK(!site_active(far_site) && far_site->deposit[0] == 100 && backpack.n[ITEM_MADEIRA] == 0, "perto: acaba");

    /* a Luna constroi uma de cada vez (a mais antiga); uma obra sem material nao trava as outras */
    reset_buildings(); flat(60, 50, 30, 20); container_clear(&backpack);
    place_building(B_FORNO_RUPESTRE, 66, 55, 0, lx, ly);          /* a mais antiga: precisa de argila */
    place_building(B_BAU, 70, 55, 0, lx, ly);                     /* precisa de madeira */
    Building *older = building_at(66, 55), *newer = building_at(70, 55);
    backpack.n[ITEM_MADEIRA] = 100;                               /* so ha material para a mais nova */
    site_deposit(newer, &backpack, ITEM_MADEIRA, 100);
    for (int step = 0; step < 60 * 10; step++) construction_step(lx, ly, SIM_DT);
    CHECK(older->build == 0.0f && older->site_state == SITE_NO_MATERIAL, "a mais antiga esta parada sem material");
    CHECK(!site_active(newer), "a mais nova avanca e acaba (nao fica presa atras da outra)");
    backpack.n[ITEM_ARGILA] = 100; backpack.n[ITEM_MADEIRA] = 100;
    place_building(B_BAU, 72, 57, 0, lx, ly);                     /* mais uma, a mais recente */
    Building *third = building_at(72, 57);
    site_deposit(older, &backpack, ITEM_ARGILA, 100);
    site_deposit(third, &backpack, ITEM_MADEIRA, 100);
    for (int step = 0; step < 60 * 5; step++) construction_step(lx, ly, SIM_DT);
    CHECK(older->build > 0.0f && third->build == 0.0f && third->site_state == SITE_QUEUED && older->site_state == SITE_WORKING,
          "so a mais antiga avanca; a outra espera na fila (estados %d e %d)", older->site_state, third->site_state);

    /* demolir uma obra devolve exatamente o material depositado (usado ou nao) */
    container_clear(&backpack);
    backpack.n[ITEM_ARGILA] = 50;
    int argila_total = backpack.n[ITEM_ARGILA] + older->deposit[0];
    CHECK(remove_building_at(66, 55) == 1 && building_at(66, 55) == NULL, "demolir a obra a meio");
    CHECK(backpack.n[ITEM_ARGILA] == argila_total && argila_total == 150, "recupera o depositado: %d (devia ser %d)", backpack.n[ITEM_ARGILA], argila_total);
    /* ... e se a mochila nao tiver espaco para o material, recusa e nao muda nada */
    container_clear(&backpack);
    backpack.n[ITEM_ARGILA] = 150;                                /* 15 L: cabe na mochila, mas nao com mais 20 L de madeira */
    place_building(B_FORNO_RUPESTRE, 66, 55, 0, lx, ly);
    Building *half = building_at(66, 55);
    site_deposit(half, &backpack, ITEM_ARGILA, 150);
    for (int step = 0; step < 60 * 20; step++) construction_step(lx, ly, SIM_DT);
    int gasto = half->deposit[0];
    CHECK(gasto == 150 && half->build > 0.1f, "tinha depositado 150 e a obra adiantou (%.3f)", (double)half->build);
    backpack.n[ITEM_MADEIRA] = 200;                               /* 20 L: cheia */
    CHECK(remove_building_at(66, 55) == -1 && building_at(66, 55) != NULL && half->deposit[0] == gasto, "o material nao cabe: recusa");
    container_clear(&backpack);
    CHECK(remove_building_at(66, 55) == 1 && backpack.n[ITEM_ARGILA] == gasto, "ja cabe: devolve %d", gasto);

    /* criativo: acaba logo, de graca, e nao devolve nada */
    container_clear(&backpack);
    game_mode = MODE_CRIATIVO;
    CHECK(place_building(B_FORNO, 66, 55, 0, lx, ly) == 1 && !site_active(building_at(66, 55)) && building_at(66, 55)->deposit[0] == 0, "criativo: logo pronta e gratis");
    game_mode = MODE_SOBREVIVENCIA;
    CHECK(remove_building_at(66, 55) == 1 && container_units(&backpack) == 0, "criativo -> sobrevivencia: nada de recursos de borla");
    /* passar a criativo com uma obra a meio: acaba logo sem material */
    place_building(B_BAU, 66, 55, 0, lx, ly);
    game_mode = MODE_CRIATIVO;
    construction_step(lx, ly, SIM_DT);
    CHECK(!site_active(building_at(66, 55)) && building_at(66, 55)->deposit[0] == 0, "obra a meio acaba logo em criativo, sem material");
    remove_building_at(66, 55);
    game_mode = MODE_CRIATIVO;
    container_clear(&backpack);
    }

    /* ---- 20. Forno rupestre: so tijolos, parte aos 20 e espalha o conteudo no chao ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    CHECK(place_building(B_FORNO_RUPESTRE, 70, 55, 0, 0, 0) && place_building(B_BAU, 72, 55, 0, 0, 0), "rupestre + bau");
    int n_rup = 0;
    for (int r = 0; r < RECIPE_COUNT; r++) if (RECIPES[r].machine == B_FORNO_RUPESTRE) { n_rup++; CHECK(RECIPES[r].out_type == ITEM_TIJOLO, "rupestre so faz tijolos"); }
    CHECK(n_rup == 1, "o forno rupestre devia ter 1 receita, tem %d", n_rup);
    CHECK(!accept_item(70, 55, 1, 0, ITEM_HEMATITE) && !accept_item(70, 55, 1, 0, ITEM_CARVAO), "rupestre nao aceita minerio nem carvao");
    int fa20 = 0, fs20 = 0, fm20 = 0;
    for (int step = 0; step < 60 * 600; step++) {
        if (accept_item(70, 55, 1, 0, ITEM_ARGILA))  fa20++;
        if (accept_item(70, 55, 1, 0, ITEM_AREIA))   fs20++;
        if (accept_item(70, 55, 1, 0, ITEM_MADEIRA)) fm20++;
        sim_step(SIM_DT);
    }
    CHECK(building_at(70, 55) == NULL || !is_machine(building_at(70, 55)->type), "o forno rupestre devia ter partido");
    int pile_sum[ITEM_COUNT] = { 0 }, piles = 0;
    for (int y = 53; y < 58; y++) for (int x = 68; x < 74; x++) {
        const Building *pb = building_at(x, y);
        if (pb == NULL || !is_pile(pb->type)) continue;
        piles++;
        int kinds = 0;
        for (int i = 1; i < ITEM_COUNT; i++) { pile_sum[i] += pb->store.n[i]; if (pb->store.n[i] > 0) { kinds++; CHECK(pb->pile_item == i, "amontoado com item errado"); } }
        CHECK(kinds == 1, "cada amontoado e de um so item (tem %d)", kinds);
    }
    const Building *cb = building_at(72, 55);
    int bricks_total = cb->store.n[ITEM_TIJOLO] + pile_sum[ITEM_TIJOLO];
    printf("rupestre: partiu; tijolos=%d (bau %d + chao %d), amontoados=%d, argila alimentada=%d (no chao %d)\n",
           bricks_total, cb->store.n[ITEM_TIJOLO], pile_sum[ITEM_TIJOLO], piles, fa20, pile_sum[ITEM_ARGILA]);
    CHECK(bricks_total == RUPESTRE_LIFE, "devia ter feito exatamente 20 tijolos, fez %d", bricks_total);
    CHECK(fa20 == 8 * bricks_total + pile_sum[ITEM_ARGILA], "conservacao da argila: %d != %d + %d", fa20, 8 * bricks_total, pile_sum[ITEM_ARGILA]);
    CHECK(fs20 == 4 * bricks_total + pile_sum[ITEM_AREIA], "conservacao da areia");
    CHECK(fm20 == 12 * bricks_total + pile_sum[ITEM_MADEIRA], "conservacao da madeira");
    CHECK(pile_sum[ITEM_ARGILA] > 0 && piles >= 3, "os ingredientes que sobraram ficam em amontoados no chao");

    /* ---- 21. Amontoados: largar, juntar, apanhar ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    container_clear(&backpack);
    backpack.n[ITEM_MADEIRA] = 100;
    backpack.n[ITEM_AREIA] = 30;
    CHECK(ground_ok(62, 52) && ground_accepts(62, 52, ITEM_MADEIRA), "tile livre aceita");
    CHECK(drop_to_ground(&backpack, 62, 52, ITEM_MADEIRA, 40) == 40, "largar 40 madeira");
    Building *pl = building_at(62, 52);
    CHECK(pl != NULL && pl->type == B_AMONTOADO && pl->pile_item == ITEM_MADEIRA && pl->store.n[ITEM_MADEIRA] == 40, "criou o amontoado de madeira");
    CHECK(backpack.n[ITEM_MADEIRA] == 60, "a mochila ficou com 60");
    CHECK(drop_to_ground(&backpack, 62, 52, ITEM_MADEIRA, 20) == 20 && pl->store.n[ITEM_MADEIRA] == 60, "junta ao amontoado que ja la esta");
    CHECK(!ground_accepts(62, 52, ITEM_AREIA) && drop_to_ground(&backpack, 62, 52, ITEM_AREIA, 5) == 0, "um amontoado e de um so item");
    CHECK(backpack.n[ITEM_AREIA] == 30, "recusar nao gasta");
    CHECK(!tile_blocked(62, 52), "o amontoado nao bloqueia a Luna");
    CHECK(!can_place(B_BAU, 62, 52, 0, 0) && !place_building(B_ESTEIRA, 62, 52, 0, 0, 0), "nao se constroi por cima de um amontoado");
    CHECK(!accept_item(62, 52, 1, 0, ITEM_MADEIRA), "esteiras e brocas nao largam itens num amontoado");
    tile_at(64, 52, 0)->terrain = T_AGUA;
    CHECK(!ground_ok(64, 52) && drop_to_ground(&backpack, 64, 52, ITEM_AREIA, 5) == 0, "nao se larga em agua");
    tile_at(65, 52, 0)->terrain = T_ARVORE;
    CHECK(!ground_ok(65, 52), "nao se larga numa arvore");
    place_building(B_BAU, 66, 52, 0, 0, 0);
    CHECK(!ground_ok(66, 52) && drop_to_ground(&backpack, 66, 52, ITEM_AREIA, 5) == 0, "nao se larga em cima de um bau");
    CHECK(!ground_ok(-1, 3) && !ground_ok(MAP_W, 3) && drop_to_ground(&backpack, -1, 3, ITEM_AREIA, 5) == 0, "fora do mapa");
    CHECK(drop_to_ground(&backpack, 62, 53, ITEM_AREIA, 1000) == 30 && backpack.n[ITEM_AREIA] == 0, "larga so o que tem");
    CHECK(drop_to_ground(&backpack, 63, 53, ITEM_AREIA, 5) == 0 && building_at(63, 53) == NULL, "sem itens nao cria amontoado vazio");
    /* apanhar */
    CHECK(pile_take(pl, &backpack, ITEM_MADEIRA, 25) == 25 && pl->store.n[ITEM_MADEIRA] == 35 && building_at(62, 52) != NULL, "apanhar 25");
    CHECK(pile_take(pl, &backpack, ITEM_MADEIRA, 1000) == 35 && building_at(62, 52) == NULL, "apanhar tudo apaga o amontoado");
    CHECK(backpack.n[ITEM_MADEIRA] == 100, "conservacao da madeira: largou 60 e apanhou 60, a mochila devia ter 100, tem %d", backpack.n[ITEM_MADEIRA]);
    container_clear(&backpack);
    /* capacidade: 200 L = 2000 unidades de 0,1 L */
    Container fonte = { 1000000000, 0, { 0 } };
    fonte.n[ITEM_AREIA] = 60000;
    CHECK(drop_to_ground(&fonte, 68, 52, ITEM_AREIA, 60000) == 2000 && fonte.n[ITEM_AREIA] == 58000, "um amontoado guarda 200 L (2000 un de areia)");
    CHECK(!ground_accepts(68, 52, ITEM_AREIA), "amontoado cheio nao aceita mais");
    /* demolir um amontoado muito cheio: apanha o que cabe e fica o resto */
    CHECK(remove_building_at(68, 52) == -1 && backpack.n[ITEM_AREIA] == 200 && building_at(68, 52)->store.n[ITEM_AREIA] == 1800, "mochila so leva 200 (20 L); ficam 1800");
    container_clear(&backpack);
    building_at(68, 52)->store.n[ITEM_AREIA] = 5;
    CHECK(remove_building_at(68, 52) == 1 && building_at(68, 52) == NULL && backpack.n[ITEM_AREIA] == 5, "amontoado pequeno: apanha tudo");
    container_clear(&backpack);
    /* um amontoado vazio desaparece sozinho */
    backpack.n[ITEM_CARVAO] = 10;
    drop_to_ground(&backpack, 70, 58, ITEM_CARVAO, 10);
    building_at(70, 58)->store.n[ITEM_CARVAO] = 0;
    run(0.1f);
    CHECK(building_at(70, 58) == NULL, "amontoado vazio devia desaparecer");

    /* ---- 22. Menus das estruturas: passar itens a mao ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    container_clear(&backpack);
    place_building(B_FORNO_RUPESTRE, 70, 55, 0, 0, 0);
    place_building(B_BAU, 72, 55, 0, 0, 0);
    Building *fr = building_at(70, 55);
    backpack.n[ITEM_ARGILA] = 60; backpack.n[ITEM_AREIA] = 30; backpack.n[ITEM_MADEIRA] = 90; backpack.n[ITEM_HEMATITE] = 5;
    CHECK(machine_load(fr, &backpack, ITEM_ARGILA, 1000) == 16 && fr->in[ITEM_ARGILA] == 16 && backpack.n[ITEM_ARGILA] == 44, "carregar argila: so 2 lotes (16)");
    CHECK(machine_load(fr, &backpack, ITEM_ARGILA, 5) == 0, "ja esta cheio de argila");
    CHECK(machine_load(fr, &backpack, ITEM_HEMATITE, 5) == 0 && backpack.n[ITEM_HEMATITE] == 5, "o rupestre nao usa hematite");
    CHECK(machine_load(fr, &backpack, ITEM_AREIA, 1000) == 8 && machine_load(fr, &backpack, ITEM_MADEIRA, 1000) == 24, "areia (8) e madeira (24)");
    run(30.0f);
    CHECK(fr->made == 2 && building_at(72, 55)->store.n[ITEM_TIJOLO] == 2, "2 tijolos para o bau em 30 s (12 s cada), made=%d", fr->made);
    /* segunda maquina, sem bau: tirar a mao */
    place_building(B_FORNO_RUPESTRE, 70, 60, 0, 0, 0);
    Building *f2 = building_at(70, 60);
    machine_load(f2, &backpack, ITEM_ARGILA, 8); machine_load(f2, &backpack, ITEM_AREIA, 4); machine_load(f2, &backpack, ITEM_MADEIRA, 12);
    run(13.0f);
    CHECK(f2->out[ITEM_TIJOLO] == 1 && f2->made == 1, "1 tijolo pronto");
    CHECK(machine_take_out(f2, &backpack, ITEM_TIJOLO, 5) == 1 && f2->out[ITEM_TIJOLO] == 0 && backpack.n[ITEM_TIJOLO] == 1, "tirar o tijolo pronto");
    machine_load(f2, &backpack, ITEM_ARGILA, 3);
    CHECK(machine_take_in(f2, &backpack, ITEM_ARGILA, 100) == 3 && f2->in[ITEM_ARGILA] == 0, "devolver ingredientes a mochila");
    /* a mochila cheia nao aceita: nada se perde */
    container_clear(&backpack);
    backpack.n[ITEM_MADEIRA] = 200;
    f2->in[ITEM_AREIA] = 5;
    CHECK(machine_take_in(f2, &backpack, ITEM_AREIA, 5) == 0 && f2->in[ITEM_AREIA] == 5, "mochila cheia: os ingredientes ficam na maquina");
    container_clear(&backpack);
    /* broca: tirar o que ela guarda */
    reset_buildings();
    flat(60, 50, 30, 20);
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { Tile *q = tile_at(62 + i, 52 + j, 0); q->terrain = T_PEDRA; q->ore = ORE_HEMATITE; q->ore_amount = 600; }
    place_building(B_BROCA, 62, 52, 0, 0, 0);
    Building *dr22 = building_at(62, 52);
    run(3.0f);
    int before_take = drill_total(dr22);
    CHECK(before_take == DRILL_BUF, "a broca (sem destino) enche o buffer (%d)", before_take);
    CHECK(drill_take(dr22, &backpack, ITEM_HEMATITE, 10) == 10 && drill_total(dr22) == before_take - 10 && backpack.n[ITEM_HEMATITE] == 10, "tirar 10 da broca");
    CHECK(drill_take(building_at(62, 52), &backpack, ITEM_FERRO, 5) == 0, "tirar o que nao ha");
    container_clear(&backpack);
    game_mode = MODE_CRIATIVO;
    }

    /* ---- 23. Quantidades de recursos: realistas (tile = 50 cm, 1 un = 0,1 L) ---- */
    {
    generate_world(DEFAULT_SEED);
    long total[ORE_COUNT] = { 0 };
    int trees = 0, bad_tree = 0, max_tile = 0;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        const Tile *t = tile_at(x, y, 0);
        if (t->ore == ORE_NONE) continue;
        total[t->ore] += t->ore_amount;
        if (t->ore == ORE_MADEIRA) {
            trees++;
            if (t->ore_amount < TREE_WOOD_MIN || t->ore_amount > TREE_WOOD_MAX) bad_tree++;
        } else if ((int)t->ore_amount > max_tile) max_tile = t->ore_amount;
    }
    CHECK(trees > 1000 && bad_tree == 0, "todas as arvores tem %d a %d un de madeira (%d fora, %d arvores)", TREE_WOOD_MIN, TREE_WOOD_MAX, bad_tree, trees);
    CHECK(TREE_WOOD_MIN >= 300, "uma arvore tem de dar pelo menos 30 L de lenha");
    for (int o = 1; o < ORE_COUNT; o++)
        CHECK((long)ORE[o].richest * 12 / 10 <= 65535, "jazida %d: o centro nao cabe em 16 bits", o);
    /* cada jazida do chao tem material para centenas de construcoes (o maior custo e 800 argila) */
    for (int o = 1; o < ORE_COUNT; o++) {
        if (o == ORE_MADEIRA) continue;
        CHECK(total[o] >= 20000, "jazida %d tem so %ld un (esperava >= 20000)", o, total[o]);
    }
    CHECK(total[ORE_ARGILA] >= 20 * 800, "argila para pelo menos 20 fornos rupestres (%ld un)", total[ORE_ARGILA]);
    printf("recursos (un): hematite=%ld argila=%ld areia=%ld enxofre=%ld quartzo=%ld | %d arvores, madeira=%ld un | maior tile=%d\n",
           total[ORE_HEMATITE], total[ORE_ARGILA], total[ORE_AREIA], total[ORE_ENXOFRE], total[ORE_QUARTZO], trees, total[ORE_MADEIRA], max_tile);
    }


    /* ================= DEMOLICAO: o inverso da construcao ================= */
    {
    const float lx = 64 * TILE_SIZE + 8.0f, ly = 54 * TILE_SIZE + 8.0f;     /* a Luna */
    const int TERRA_G = item_mass_g(ITEM_TERRA);
    CHECK(TERRA_G == 100 && item_mass_g(ITEM_FERRO_ENF) == item_mass_g(ITEM_FERRO), "terra 100 g/un; sucata pesa o mesmo que o ferro");

    /* ---- 24. A massa conserva-se, estrutura a estrutura ---- */
    for (int k = 0; k < BUILDABLE_COUNT; k++) {
        int ty = BUILDABLE[k];
        reset_buildings();
        container_clear(&backpack);
        game_mode = MODE_SOBREVIVENCIA;
        Building *b = build_paid(ty, 66, 55, lx, ly);
        CHECK(b != NULL, "%s: construir", BUILD[ty].name);
        if (b == NULL) continue;
        int give[ITEM_COUNT];
        demolish_manifest(b, give);
        int m0 = build_cost_mass_g(ty), m1 = give_mass(give);
        printf("demolir %-15s massa %7d g -> %7d g (terra %d un)\n", BUILD[ty].name, m0, m1, give[ITEM_TERRA]);
        CHECK(m1 <= m0 && m0 - m1 < TERRA_G, "%s: a massa nao se conserva (%d g -> %d g)", BUILD[ty].name, m0, m1);
        CHECK(demolish_start(66, 55) == 1 && demolish_start(66, 55) == 0 && demo_active(b) && !site_active(b), "%s: marcar uma vez", BUILD[ty].name);
        CHECK(remove_building_at(66, 55) == 0 && building_at(66, 55) == b, "%s: a demolicao em curso nao se demole de novo", BUILD[ty].name);
        CHECK(!accept_item(66, 55, 1, 0, ITEM_MADEIRA), "%s: a ser demolida nao aceita itens", BUILD[ty].name);
        /* demora metade da obra, nem menos */
        float secs = demolish_seconds(ty);
        CHECK(secs > 0.0f && secs * 100.0f / DEMOLISH_TIME_PERCENT > build_seconds(ty) - 0.01f, "%s: tempo de demolir", BUILD[ty].name);
        demolish_for(lx, ly, secs * 0.9f);
        CHECK(building_at(66, 55) == b && b->demo_p > 0.05f && b->demo_p < 0.15f, "%s: aos 90%% da demolicao ainda la esta (%.3f)", BUILD[ty].name, (double)b->demo_p);
        demolish_for(lx, ly, secs * 0.2f + 0.5f);
        CHECK(building_at(66, 55) == NULL && !is_pile(B_NONE), "%s: devia ter desaparecido", BUILD[ty].name);
        int sum[ITEM_COUNT];
        ground_totals(58, 48, 74, 64, sum);
        for (int i = 1; i < ITEM_COUNT; i++)
            CHECK(sum[i] == give[i], "%s: item %d no chao %d, esperava %d", BUILD[ty].name, i, sum[i], give[i]);
        CHECK(container_units(&backpack) == 0, "%s: a demolicao nao mexe na mochila", BUILD[ty].name);
    }

    /* ---- 25. O que sai de cada estrutura (contas fechadas) ---- */
    {
    reset_buildings(); container_clear(&backpack); game_mode = MODE_SOBREVIVENCIA;
    int give[ITEM_COUNT];
    /* esteira: 15 madeira (600 g) -> 7 madeira (280 g) + 3 terra (300 g) */
    Building *b = build_paid(B_ESTEIRA, 66, 55, lx, ly);
    demolish_manifest(b, give);
    CHECK(give[ITEM_MADEIRA] == 7 && give[ITEM_TERRA] == 3, "esteira: 7 madeira + 3 terra (deu %d + %d)", give[ITEM_MADEIRA], give[ITEM_TERRA]);
    /* broca: 8 ferro -> 8 SUCATA; 10 tijolos -> 6; 60 madeira -> 30; o resto (8400 g) vira 84 terra */
    reset_buildings();
    b = build_paid(B_BROCA, 66, 55, lx, ly);
    demolish_manifest(b, give);
    CHECK(give[ITEM_FERRO] == 0 && give[ITEM_FERRO_ENF] == 8, "broca: o ferro sai enferrujado (ferro %d, sucata %d)", give[ITEM_FERRO], give[ITEM_FERRO_ENF]);
    CHECK(give[ITEM_TIJOLO] == 6 && give[ITEM_MADEIRA] == 30 && give[ITEM_TERRA] == 84, "broca: 6 tijolos, 30 madeira, 84 terra (deu %d, %d, %d)", give[ITEM_TIJOLO], give[ITEM_MADEIRA], give[ITEM_TERRA]);
    /* forno rupestre: so terra (800 argila = 136 kg = 1360 un) */
    reset_buildings();
    b = build_paid(B_FORNO_RUPESTRE, 66, 55, lx, ly);
    demolish_manifest(b, give);
    CHECK(give[ITEM_TERRA] == 1360 && give[ITEM_ARGILA] == 0, "rupestre: so terra (terra %d, argila %d)", give[ITEM_TERRA], give[ITEM_ARGILA]);
    /* carvoeira: a argila volta toda, a madeira so em parte */
    reset_buildings();
    b = build_paid(B_CARVOEIRA, 66, 55, lx, ly);
    demolish_manifest(b, give);
    CHECK(give[ITEM_ARGILA] == 400 && give[ITEM_MADEIRA] == 75 && give[ITEM_TERRA] == 150 * 40 / 2 / 100 * 1, "carvoeira: 400 argila, 75 madeira, 30 terra (deu %d, %d, %d)", give[ITEM_ARGILA], give[ITEM_MADEIRA], give[ITEM_TERRA]);

    /* o que estava dentro sai tal e qual (sem degradar) e a estrutura esvazia-se */
    reset_buildings();
    b = build_paid(B_BAU, 66, 55, lx, ly);
    b->store.n[ITEM_MADEIRA] = 120; b->store.n[ITEM_HEMATITE] = 30;
    demolish_manifest(b, give);
    CHECK(give[ITEM_MADEIRA] == 120 + 50 && give[ITEM_HEMATITE] == 30, "bau: o conteudo sai intacto, a estrutura so metade (madeira %d)", give[ITEM_MADEIRA]);
    CHECK(demolish_start(66, 55) == 1 && container_units(&b->store) == 0, "marcar esvazia a estrutura (nao conta duas vezes)");
    int total_before = give[ITEM_MADEIRA];
    demolish_for(lx, ly, 10.0f);
    int sum[ITEM_COUNT]; ground_totals(58, 48, 74, 64, sum);
    CHECK(building_at(66, 55) == NULL && sum[ITEM_MADEIRA] == total_before && sum[ITEM_HEMATITE] == 30, "tudo no chao: madeira %d, hematite %d", sum[ITEM_MADEIRA], sum[ITEM_HEMATITE]);
    /* um amontoado e de um item so: varios itens, varios amontoados */
    int kinds = 0, piles_near = 0;
    for (int y = 48; y <= 62; y++) for (int x = 60; x <= 72; x++) {
        const Building *pb = building_at(x, y);
        if (pb == NULL || !is_pile(pb->type)) continue;
        piles_near++;
        int n_it = 0; for (int i = 1; i < ITEM_COUNT; i++) if (pb->store.n[i] > 0) n_it++;
        if (n_it == 1) kinds++;
    }
    CHECK(piles_near >= 3 && kinds == piles_near, "amontoados a volta, cada um de um item (%d, %d)", piles_near, kinds);

    /* uma esteira com itens: saem tal e qual */
    reset_buildings(); flat(60, 50, 30, 20);
    game_mode = MODE_CRIATIVO;
    place_building(B_ESTEIRA, 66, 55, 0, lx, ly);
    accept_item(66, 55, 1, 0, ITEM_TIJOLO);
    CHECK(demolish_start(66, 55) == 1, "marcar a esteira");
    demolition_step(lx, ly, SIM_DT);                  /* criativo: logo */
    ground_totals(58, 48, 74, 64, sum);
    CHECK(building_at(66, 55) == NULL && sum[ITEM_TIJOLO] == 1 && sum[ITEM_MADEIRA] == 0 && sum[ITEM_TERRA] == 0,
          "criativo: instantaneo, so o conteudo (tijolo %d, madeira %d, terra %d)", sum[ITEM_TIJOLO], sum[ITEM_MADEIRA], sum[ITEM_TERRA]);
    /* o que se construiu em criativo nao tem material para devolver */
    reset_buildings(); flat(60, 50, 30, 20);
    place_building(B_FORNO, 66, 55, 0, lx, ly);
    demolish_manifest(building_at(66, 55), give);
    CHECK(give_mass(give) == 0, "forno de criativo: nada de recursos de borla");
    game_mode = MODE_SOBREVIVENCIA;
    }

    /* ---- 26. Uma obra a meio: o nao usado sai tal e qual, o usado degrada-se ---- */
    {
    reset_buildings(); flat(60, 50, 30, 20); container_clear(&backpack);
    game_mode = MODE_SOBREVIVENCIA;
    place_building(B_FORNO_RUPESTRE, 66, 55, 0, lx, ly);
    Building *ob = building_at(66, 55);
    Container src = { 1000000000, 0, { 0 } };
    src.n[ITEM_ARGILA] = 300;
    site_deposit(ob, &src, ITEM_ARGILA, 300);
    ob->build = 0.25f;                                 /* 200 usadas, 100 ainda soltas */
    int give[ITEM_COUNT];
    demolish_manifest(ob, give);
    CHECK(give[ITEM_ARGILA] == 100 && give[ITEM_TERRA] == 340, "obra a meio: 100 argila soltas + 340 terra (200 usadas) (deu %d + %d)", give[ITEM_ARGILA], give[ITEM_TERRA]);
    CHECK(give_mass(give) == 300 * 170, "a massa depositada conserva-se");
    CHECK(demolish_start(66, 55) == 1, "marcar a obra");
    CHECK(ob->demo_secs > 0.24f * 0.5f * 45.0f && ob->demo_secs < 0.26f * 0.5f * 45.0f, "uma obra a 25%% demora so 25%% do tempo (%.2f s)", (double)ob->demo_secs);
    demolish_for(lx, ly, ob->demo_secs + 0.5f);
    int sum[ITEM_COUNT]; ground_totals(58, 48, 74, 64, sum);
    CHECK(building_at(66, 55) == NULL && sum[ITEM_ARGILA] == 100 && sum[ITEM_TERRA] == 340, "obra demolida: argila %d, terra %d", sum[ITEM_ARGILA], sum[ITEM_TERRA]);
    /* uma obra sem nada depositado desaparece sem deixar nada */
    reset_buildings(); flat(60, 50, 30, 20);
    place_building(B_CONTENTOR, 66, 55, 0, lx, ly);
    CHECK(demolish_start(66, 55) == 1, "marcar obra vazia");
    demolish_for(lx, ly, 5.0f);
    ground_totals(58, 48, 74, 64, sum);
    CHECK(building_at(66, 55) == NULL && give_mass(sum) == 0, "obra vazia: some sem deixar nada");
    }

    /* ---- 27. A terra desaparece passados 2 minutos ---- */
    {
    reset_buildings(); flat(60, 50, 30, 20); container_clear(&backpack);
    game_mode = MODE_SOBREVIVENCIA;
    build_paid(B_FORNO_RUPESTRE, 66, 55, lx, ly);
    demolish_start(66, 55);
    demolish_for(lx, ly, demolish_seconds(B_FORNO_RUPESTRE) + 1.0f);
    int sum[ITEM_COUNT]; ground_totals(58, 48, 74, 64, sum);
    CHECK(building_at(66, 55) == NULL && sum[ITEM_TERRA] == 1360, "rupestre demolido: 1360 terra no chao (%d)", sum[ITEM_TERRA]);
    CHECK(TERRA_LIFE_S == 120.0f, "a terra dura 2 minutos");
    run(TERRA_LIFE_S - 8.0f);
    ground_totals(58, 48, 74, 64, sum);
    CHECK(sum[ITEM_TERRA] == 1360, "ainda la esta aos ~112 s (%d)", sum[ITEM_TERRA]);
    run(10.0f);
    ground_totals(58, 48, 74, 64, sum);
    CHECK(sum[ITEM_TERRA] == 0, "passados 2 min a terra desapareceu (%d)", sum[ITEM_TERRA]);
    int still = 0; for (int y = 48; y <= 64; y++) for (int x = 58; x <= 74; x++) if (building_at(x, y) != NULL) still++;
    CHECK(still == 0, "nao ficam amontoados vazios (%d)", still);
    /* a terra pode ser apanhada ate la (nao tem uso, mas e um item como os outros) */
    container_clear(&backpack);
    Container tmp = { 1000000000, 0, { 0 } }; tmp.n[ITEM_TERRA] = 300;
    CHECK(drop_to_ground(&tmp, 70, 52, ITEM_TERRA, 300) == 300, "largar terra");
    Building *tp = building_at(70, 52);
    CHECK(pile_take(tp, &backpack, ITEM_TERRA, 100) == 100 && backpack.n[ITEM_TERRA] == 100, "apanhar terra");
    run(60.0f);
    CHECK(building_at(70, 52) != NULL, "aos 60 s ainda la esta");
    tmp.n[ITEM_TERRA] = 1;
    drop_to_ground(&tmp, 70, 52, ITEM_TERRA, 1);      /* chegou mais terra: o relogio recomeca */
    run(100.0f);
    CHECK(building_at(70, 52) != NULL, "o relogio recomeca quando chega mais terra");
    run(30.0f);
    CHECK(building_at(70, 52) == NULL, "e acaba por desaparecer");
    /* so a terra desaparece */
    tmp.n[ITEM_MADEIRA] = 50;
    drop_to_ground(&tmp, 70, 52, ITEM_MADEIRA, 50);
    run(300.0f);
    CHECK(building_at(70, 52) != NULL && building_at(70, 52)->store.n[ITEM_MADEIRA] == 50, "a madeira no chao nao desaparece");
    container_clear(&backpack);
    }

    /* ---- 28. A Luna tem de estar ao pe, demole uma de cada vez e sem chao livre pára ---- */
    {
    reset_buildings(); flat(60, 50, 30, 20); container_clear(&backpack);
    game_mode = MODE_SOBREVIVENCIA;
    Building *b1 = build_paid(B_BAU, 66, 55, lx, ly);
    Building *b2 = build_paid(B_BAU, 70, 55, lx, ly);
    CHECK(b1 != NULL && b2 != NULL, "dois baus");
    if (b1 != NULL && b2 != NULL) {
        demolish_start(70, 55); demolish_start(66, 55);          /* a mais antiga marcada e a de 70 */
        demolish_for(lx + 3000.0f, ly, 5.0f);
        CHECK(b1->demo_p == 1.0f && b1->demo_state == DEMO_FAR && b2->demo_state == DEMO_FAR, "longe: nao demole");
        demolish_for(lx, ly, 1.0f);
        CHECK(b2->demo_p < 1.0f && b1->demo_p == 1.0f && b2->demo_state == DEMO_WORKING && b1->demo_state == DEMO_QUEUED,
              "ao pe: so a mais antiga (a de 70) avanca; a outra espera (%.3f, %.3f)", (double)b2->demo_p, (double)b1->demo_p);
        demolish_for(lx, ly, 20.0f);
        CHECK(building_at(70, 55) == NULL && building_at(66, 55) == NULL, "acabam as duas");
    }

    /* sem chao livre a volta: pára, nao perde nada, e continua quando houver espaco */
    reset_buildings(); flat(60, 50, 30, 20); container_clear(&backpack);
    game_mode = MODE_SOBREVIVENCIA;
    const float px = (float)(79 * TILE_SIZE + 8), py = (float)(60 * TILE_SIZE + 8);     /* a Luna, ao lado do bau */
    b1 = build_paid(B_BAU, 76, 60, px, py);
    CHECK(b1 != NULL, "bau para o teste do chao");
    if (b1 != NULL) {
        b1->store.n[ITEM_AREIA] = 40;
        for (int y = 60 - DEMO_SPILL_RINGS; y <= 60 + DEMO_SPILL_RINGS; y++) for (int x = 76 - DEMO_SPILL_RINGS; x <= 76 + DEMO_SPILL_RINGS; x++)
            if (x != 76 || y != 60) tile_at(x, y, 0)->terrain = T_AGUA;
        int give[ITEM_COUNT]; demolish_manifest(b1, give);
        demolish_start(76, 60);
        demolish_for(px, py, 30.0f);
        CHECK(building_at(76, 60) == b1 && b1->demo_state == DEMO_NO_ROOM && b1->demo_p == 1.0f, "sem chao: pára (estado %d, p %.3f)", b1->demo_state, (double)b1->demo_p);
        int sum[ITEM_COUNT]; ground_totals(60, 50, 90, 70, sum);
        CHECK(give_mass(sum) == 0, "e nada se perdeu nem foi para um sitio estranho");
        for (int y = 60 - DEMO_SPILL_RINGS; y <= 60 + DEMO_SPILL_RINGS; y++) for (int x = 76 - DEMO_SPILL_RINGS; x <= 76 + DEMO_SPILL_RINGS; x++)
            if (x != 76 || y != 60) tile_at(x, y, 0)->terrain = T_CLAREIRA;
        demolish_for(px, py, 30.0f);
        ground_totals(60, 50, 90, 70, sum);
        CHECK(building_at(76, 60) == NULL, "com chao livre acaba");
        for (int i = 1; i < ITEM_COUNT; i++) CHECK(sum[i] == give[i], "depois de esperar: item %d no chao %d, esperava %d", i, sum[i], give[i]);
    }
    }

    /* ---- 29. Sucata volta a ser ferro no forno ---- */
    {
    reset_buildings(); flat(60, 50, 30, 20); container_clear(&backpack);
    game_mode = MODE_CRIATIVO;
    place_building(B_FORNO, 70, 55, 0, 0, 0);
    place_building(B_BAU, 72, 55, 0, 0, 0);
    int rs = -1;
    for (int r = 0; r < RECIPE_COUNT; r++) if (RECIPES[r].machine == B_FORNO && RECIPES[r].in_type[0] == ITEM_FERRO_ENF) rs = r;
    CHECK(rs >= 0 && RECIPES[rs].out_type == ITEM_FERRO, "ha uma receita sucata -> ferro no forno");
    CHECK(!accept_item(70, 55, 1, 0, ITEM_FERRO) && !accept_item(70, 55, 1, 0, ITEM_TERRA), "o forno nao aceita ferro nem terra");
    int fs2 = 0, fc2 = 0;
    for (int step = 0; step < 60 * 60; step++) {
        if (fs2 < 3  && accept_item(70, 55, 1, 0, ITEM_FERRO_ENF)) fs2++;
        if (fc2 < 15 && accept_item(70, 55, 1, 0, ITEM_CARVAO))    fc2++;
        sim_step(SIM_DT);
    }
    CHECK(fs2 == 3 && building_at(72, 55)->store.n[ITEM_FERRO] == 3, "3 sucatas + 15 carvao -> 3 barras de ferro (deu %d)", building_at(72, 55)->store.n[ITEM_FERRO]);
    CHECK(!is_machine(B_BAU), "ok");
    }
    game_mode = MODE_CRIATIVO;
    reset_buildings();
    container_clear(&backpack);
    }

    /* o carvao que nao chega ao forno nao pode sumir: a carvoeira so entrega ao forno (o bau e adjacente ao forno) */
    printf("%s\n", fails ? "TESTES COM FALHAS" : "TODOS OS TESTES PASSARAM");
    return fails != 0;
}
