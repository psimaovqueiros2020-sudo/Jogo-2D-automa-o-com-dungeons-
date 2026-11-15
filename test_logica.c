/* Testes da LOGICA do jogo (sem janela, sem raylib verdadeira).
 * Correr, a partir da pasta do projeto:
 *   gcc -std=c99 -Wall -Wextra -Wno-unused-function -Itests/fake tests/test_logica.c world.c logic.c -o test_logica -lm && ./test_logica
 * So se compilam world.c e logic.c (a logica); render.c e main.c ficam de fora.
 * tests/fake/raylib.h e um cabecalho falso (funcoes que nao fazem nada) so para
 * compilar sem a raylib: world.h inclui raylib.h por causa do tipo Color. */
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
    printf("carvoeira: carvao no bau=%d, dentro: madeira=%d, receita=%d\n", bau->store[ORE_CARVAO], car->in[ORE_MADEIRA], car->recipe);
    CHECK(bau->store[ORE_CARVAO] == 24, "esperava 24 carvao, deu %d", bau->store[ORE_CARVAO]);
    CHECK(car->in[ORE_MADEIRA] == 0 && car->recipe == 0, "sobrou madeira/receita");
    CHECK(bau->store[ORE_MADEIRA] == 0, "madeira nao devia chegar ao bau");

    /* ---- 3. Forno: ferro (hematite + carvao) por entrega direta, conservacao ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    CHECK(place_building(B_FORNO, 70, 55, 0, 0, 0), "forno");
    CHECK(place_building(B_BAU, 72, 55, 0, 0, 0), "bau forno");
    int fed_h = 0, fed_c = 0;
    for (int step = 0; step < 60 * 90; step++) {
        if (fed_h < 30 && accept_item(70, 55, 1, 0, ORE_HEMATITE)) fed_h++;
        if (fed_c < 10 && accept_item(70, 55, 1, 0, ORE_CARVAO))   fed_c++;
        sim_step(SIM_DT);
    }
    Building *f = building_at(70, 55); bau = building_at(72, 55);
    printf("forno ferro: alimentou hem=%d carv=%d -> ferro no bau=%d (dentro hem=%d carv=%d)\n",
           fed_h, fed_c, bau->store[ORE_FERRO], f->in[ORE_HEMATITE], f->in[ORE_CARVAO]);
    CHECK(fed_h == 30 && fed_c == 10, "alimentacao incompleta");
    CHECK(bau->store[ORE_FERRO] == 20, "esperava 20 ferro, deu %d", bau->store[ORE_FERRO]);

    /* ---- 4. Forno: tijolos + limites de entrada ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    place_building(B_FORNO, 70, 55, 0, 0, 0);
    place_building(B_BAU, 72, 55, 0, 0, 0);
    /* so hematite: tem de parar nos 12 e rejeitar */
    int acc = 0; for (int i = 0; i < 40; i++) acc += accept_item(70, 55, 1, 0, ORE_HEMATITE);
    CHECK(acc == MACH_IN_CAP, "aceitou %d hematite (cap %d)", acc, MACH_IN_CAP);
    CHECK(!accept_item(70, 55, 1, 0, ORE_TIJOLO), "forno aceitou tijolo (produto)");
    CHECK(!accept_item(70, 55, 1, 0, ORE_QUARTZO), "forno aceitou quartzo");
    CHECK(!accept_item(70, 55, 1, 0, ORE_ENXOFRE), "forno aceitou enxofre");
    run(10.0f);
    f = building_at(70, 55);
    CHECK(f->recipe == 0 && machine_status(f) == 1, "forno devia estar parado sem carvao");
    int fa = 0, fs = 0, fm = 0;
    for (int step = 0; step < 60 * 60; step++) {
        if (fa < 20 && accept_item(70, 55, 1, 0, ORE_ARGILA)) fa++;
        if (fs < 10 && accept_item(70, 55, 1, 0, ORE_AREIA))  fs++;
        if (fm < 10 && accept_item(70, 55, 1, 0, ORE_MADEIRA)) fm++;
        sim_step(SIM_DT);
    }
    bau = building_at(72, 55);
    printf("forno tijolos: argila=%d areia=%d madeira=%d -> tijolos=%d\n", fa, fs, fm, bau->store[ORE_TIJOLO]);
    CHECK(bau->store[ORE_TIJOLO] == 30, "esperava 30 tijolos, deu %d", bau->store[ORE_TIJOLO]);

    /* ---- 5. Saida cheia: sem destino a maquina para (luz amarela) e nao perde itens ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    place_building(B_CARVOEIRA, 70, 55, 0, 0, 0);
    int fed = 0;
    for (int step = 0; step < 60 * 200; step++) { if (accept_item(70, 55, 1, 0, ORE_MADEIRA)) fed++; sim_step(SIM_DT); }
    car = building_at(70, 55);
    CHECK(mach_out_total(car) == MACH_OUT_CAP, "saida devia ter %d, tem %d", MACH_OUT_CAP, mach_out_total(car));
    CHECK(machine_status(car) == 2, "estado devia ser 'saida cheia'");
    CHECK(fed == car->in[ORE_MADEIRA] + 4 * (car->out[ORE_CARVAO]), "conservacao falhou: fed=%d in=%d out=%d", fed, car->in[ORE_MADEIRA], car->out[ORE_CARVAO]);

    /* ---- 6. Demolir devolve tudo (incluindo a receita em curso) ---- */
    for (int i = 0; i < ORE_COUNT; i++) inventory[i] = 0;
    int before = fed;
    remove_building_at(70, 55);
    CHECK(inventory[ORE_MADEIRA] + 4 * inventory[ORE_CARVAO] == before, "demolicao perdeu itens: madeira=%d carvao=%d (fed=%d)", inventory[ORE_MADEIRA], inventory[ORE_CARVAO], before);

    /* ---- 7. Maquina ao lado de maquina: carvoeira -> forno directo ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    place_building(B_CARVOEIRA, 66, 55, 0, 0, 0);
    place_building(B_FORNO, 68, 55, 0, 0, 0);       /* encostado a direita da carvoeira */
    place_building(B_BAU, 70, 55, 0, 0, 0);
    int fw = 0, fh = 0;
    for (int step = 0; step < 60 * 120; step++) {
        if (fw < 40 && accept_item(66, 55, 1, 0, ORE_MADEIRA)) fw++;
        if (fh < 30 && accept_item(68, 55, 1, 0, ORE_HEMATITE)) fh++;
        sim_step(SIM_DT);
    }
    bau = building_at(70, 55);
    printf("cadeia direta: madeira=%d hematite=%d -> ferro no bau=%d\n", fw, fh, bau->store[ORE_FERRO]);
    CHECK(bau->store[ORE_FERRO] > 0, "cadeia direta nao produziu ferro");

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
    CHECK(place_building(B_BROCA, tx, ty, 0, 0, 0), "broca nao assenta em arvores");
    /* a parede do bordo continua proibida */
    CHECK(!can_place(B_BROCA, 0, 0, 0, 0) && !can_place(B_BROCA, 1, 1, 0, 0), "broca assentou na parede do bordo");
    CHECK(!can_place(B_BAU, tx + 3, ty, 0, 0) || tile_at(tx+3,ty,0)->terrain != T_ARVORE, "bau assentou em arvore");
    tile_at(tx + 2, ty, 0)->terrain = T_FLORESTA; tile_at(tx + 2, ty, 0)->ore = ORE_NONE;
    CHECK(place_building(B_BAU, tx + 2, ty, 0, 0, 0), "bau ao lado da serra");
    for (int i = 0; i < ORE_COUNT; i++) inventory[i] = 0;
    run(30.0f);
    int still_trees = 0;
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) if (tile_at(tx+i,ty+j,0)->terrain == T_ARVORE) still_trees++;
    Building *dr = building_at(tx, ty);
    printf("serra: arvores restantes=%d, madeira no bau=%d\n", still_trees, chest_total(building_at(tx + 2, ty)));
    CHECK(still_trees == 0, "arvores nao foram cortadas");
    CHECK(drill_status(dr) == 1 || drill_total(dr) > 0, "estado da broca incoerente");
    CHECK(!is_machine(B_BROCA), "is_machine errado");
    /* o carvao que nao chega ao forno nao pode sumir: a carvoeira so entrega ao forno (o bau e adjacente ao forno) */
    printf("%s\n", fails ? "TESTES COM FALHAS" : "TODOS OS TESTES PASSARAM");
    return fails != 0;
}
