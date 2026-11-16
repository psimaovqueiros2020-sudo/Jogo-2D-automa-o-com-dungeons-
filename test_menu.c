/* Testes dos MENUS (menu.c) com entrada FALSA: simulam-se cliques do rato.
 * Verifica que os cliques passam mesmo os itens (mochila <-> chao, maquinas,
 * baus) e que o menu de construcao devolve a estrutura escolhida.
 * Nao verifica o aspeto (isso so se ve com a raylib verdadeira).
 *   make test   (compila com menu.c, render.c, world.c, logic.c, items.c) */
#ifndef CLAREIRA_TEST
#define CLAREIRA_TEST
#endif
#include <stdio.h>
#include "../menu.h"
#include "../render.h"      /* ui_scale */

Vector2 fake_mouse_pos;
int     fake_mouse_pressed[3];
int     fake_key_down[64];

static int fails = 0;
#define CHECK(c, ...) do { if (!(c)) { printf("FALHOU: "); printf(__VA_ARGS__); printf("\n"); fails++; } } while (0)

#define SW 1280
#define SH 720
/* Geometria dos paineis (igual a menu.c): mochila a esquerda, estrutura a direita */
#define LEFT_X   270
#define RIGHT_X  650
#define PANEL_Y  125
#define ROW0_Y   (PANEL_Y + 108)      /* primeira linha com barras de volume/peso */
#define ROW0_Y_NOBARS (PANEL_Y + 44)  /* primeira linha sem barras */
#define ROW_H    22

static void click(int button, float x, float y, int shift)
{
    fake_mouse_pos = (Vector2){ x, y };
    fake_mouse_pressed[button] = 1;
    fake_key_down[KEY_LEFT_SHIFT] = shift;
}
static void release(void) { fake_mouse_pressed[0] = fake_mouse_pressed[1] = fake_mouse_pressed[2] = 0; fake_key_down[KEY_LEFT_SHIFT] = 0; }

static void flat(int x0, int y0, int w, int h) {
    for (int y = y0; y < y0 + h; y++) for (int x = x0; x < x0 + w; x++) {
        Tile *t = tile_at(x, y, 0); t->terrain = T_CLAREIRA; t->ore = ORE_NONE; t->ore_amount = 0;
    }
}

int main(void)
{
    generate_world(DEFAULT_SEED);
    flat(60, 50, 30, 20);
    Player luna = { 64 * TILE_SIZE + 8.0f, 54 * TILE_SIZE + 8.0f };
    container_clear(&backpack);
    game_mode = MODE_CRIATIVO;
    ui_scale = 1.0f;            /* as coordenadas abaixo sao as do ecra a 100% */

    /* ---- Largar itens no chao: E sem estrutura ---- */
    backpack.n[ITEM_MADEIRA] = 50; backpack.n[ITEM_AREIA] = 30;
    menu_open_backpack(65, 54);
    CHECK(menu_is_open(), "menu devia abrir");
    CHECK(menu_update(&luna, SW, SH) == 1, "menu aberto: update devolve 1");
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + ROW_H + 5, 0);  /* 2a linha da mochila: areia (as linhas seguem a ordem dos itens: madeira, areia) */
    menu_update(&luna, SW, SH); release();
    Building *pile = building_at(65, 54);
    CHECK(pile != NULL && is_pile(pile->type), "clicar num item da mochila devia criar um amontoado no chao");
    CHECK(pile != NULL && pile->pile_item == ITEM_AREIA && pile->store.n[ITEM_AREIA] == 10 && backpack.n[ITEM_AREIA] == 20, "10 areia largadas");
    /* Shift+clique: tudo o que resta */
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + ROW_H + 5, 1);
    menu_update(&luna, SW, SH); release();
    CHECK(pile->store.n[ITEM_AREIA] == 30 && backpack.n[ITEM_AREIA] == 0, "Shift+clique larga tudo");
    /* a madeira (a unica linha que resta) nao cabe no amontoado de areia */
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + 5, 0);
    menu_update(&luna, SW, SH); release();
    CHECK(backpack.n[ITEM_MADEIRA] == 50 && pile->store.n[ITEM_MADEIRA] == 0, "um amontoado e de um so item");
    /* apanhar de volta: botao direito na 1a linha do painel da direita (1 un) e clique normal */
    click(MOUSE_BUTTON_RIGHT, RIGHT_X + 30, PANEL_Y + 108 + ROW_H + 5, 0);   /* 1a linha: depois do titulo "Clica para apanhar" */
    menu_update(&luna, SW, SH); release();
    CHECK(backpack.n[ITEM_AREIA] == 1 && pile->store.n[ITEM_AREIA] == 29, "botao direito apanha 1 (areia mochila=%d, chao=%d)", backpack.n[ITEM_AREIA], pile->store.n[ITEM_AREIA]);
    click(MOUSE_BUTTON_LEFT, RIGHT_X + 30, PANEL_Y + 108 + ROW_H + 5, 1);
    menu_update(&luna, SW, SH); release();
    CHECK(backpack.n[ITEM_AREIA] == 30 && building_at(65, 54) == NULL, "Shift+clique apanha tudo e o amontoado desaparece");
    CHECK(menu_update(&luna, SW, SH) == 1, "o menu continua aberto (agora so com o chao)");
    /* continua a poder largar no mesmo sitio (o chao ficou livre); a mochila tem areia (1a linha) e madeira */
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + ROW_H + 5, 1);
    menu_update(&luna, SW, SH); release();
    CHECK(building_at(65, 54) != NULL && backpack.n[ITEM_AREIA] == 0, "voltou a largar depois de o amontoado desaparecer");
    menu_close();
    CHECK(!menu_is_open(), "menu fechado");
    remove_building_at(65, 54); container_clear(&backpack);

    /* ---- Chao invalido (agua): nao larga e diz porque ---- */
    tile_at(65, 54, 0)->terrain = T_AGUA;
    backpack.n[ITEM_MADEIRA] = 20;
    menu_open_backpack(-1, -1);
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + 5, 0);
    menu_update(&luna, SW, SH); release();
    CHECK(backpack.n[ITEM_MADEIRA] == 20 && building_at(65, 54) == NULL, "sem chao valido nao larga nada");
    menu_close();
    tile_at(65, 54, 0)->terrain = T_CLAREIRA;

    /* ---- Menu de uma maquina: meter ingredientes e tirar produtos ---- */
    container_clear(&backpack);
    backpack.n[ITEM_ARGILA] = 60; backpack.n[ITEM_AREIA] = 30;
    place_building(B_FORNO_RUPESTRE, 66, 55, 0, 0, 0);
    menu_open_building(67, 56);                       /* qualquer tile da estrutura serve */
    CHECK(menu_is_open() && menu_update(&luna, SW, SH) == 1, "menu da maquina abre (o tile 67,56 e da estrutura)");
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + ROW_H + 5, 1);    /* mochila: argila, areia -> 2a linha = areia (shift: tudo o que couber) */
    menu_update(&luna, SW, SH); release();
    Building *fr = building_at(66, 55);
    CHECK(fr->in[ITEM_AREIA] == 8 && backpack.n[ITEM_AREIA] == 22, "carregar areia: so 8 (2 lotes)");
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + 5, 1);             /* 1a linha = argila */
    menu_update(&luna, SW, SH); release();
    CHECK(fr->in[ITEM_ARGILA] == 16 && backpack.n[ITEM_ARGILA] == 44, "carregar argila: 16");
    /* o rupestre nao usa hematite: o clique nao passa nada */
    backpack.n[ITEM_HEMATITE] = 5;
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + 5, 0);             /* 1a linha agora: hematite */
    menu_update(&luna, SW, SH); release();
    CHECK(backpack.n[ITEM_HEMATITE] == 5 && fr->in[ITEM_HEMATITE] == 0, "o rupestre nao aceita hematite");
    menu_close();
    /* ingredientes e produto: tirar pelo painel da direita (sem barras: linhas a partir de PANEL_Y + 44) */
    fr->out[ITEM_TIJOLO] = 3;
    menu_open_building(66, 55);
    menu_update(&luna, SW, SH);
    /* linhas: 0 Estado, 1 Lotes feitos, 2 Receitas, 3 titulo da receita, 4 receita, 5 Dentro, 6 argila, 7 areia, 8 Pronto, 9 tijolo */
    int row_tijolo = 9;
    click(MOUSE_BUTTON_LEFT, RIGHT_X + 30, ROW0_Y_NOBARS + row_tijolo * ROW_H + 5, 1);
    menu_update(&luna, SW, SH); release();
    CHECK(fr->out[ITEM_TIJOLO] == 0 && backpack.n[ITEM_TIJOLO] == 3, "tirar os 3 tijolos prontos (tem %d)", backpack.n[ITEM_TIJOLO]);
    click(MOUSE_BUTTON_LEFT, RIGHT_X + 30, ROW0_Y_NOBARS + 7 * ROW_H + 5, 1);    /* areia (ingrediente; argila e a linha 6) */
    menu_update(&luna, SW, SH); release();
    CHECK(fr->in[ITEM_AREIA] == 0 && backpack.n[ITEM_AREIA] == 30, "devolver a areia a mochila");
    menu_close();

    /* ---- O menu fecha sozinho se a Luna se afasta ou se a estrutura desaparece ---- */
    menu_open_building(66, 55);
    Player longe = { 80 * TILE_SIZE, 60 * TILE_SIZE };
    menu_update(&longe, SW, SH);
    CHECK(!menu_is_open(), "afastar-se fecha o menu");
    menu_open_building(66, 55);
    container_clear(&backpack);
    remove_building_at(66, 55);
    menu_update(&luna, SW, SH);
    CHECK(!menu_is_open(), "demolir a estrutura fecha o menu");

    /* ---- Menu de construcao (Q) ---- */
    menu_toggle_build();
    CHECK(menu_is_open() && menu_update(&luna, SW, SH) == 1, "Q abre o menu de construcao");
    CHECK(menu_picked_build() == B_NONE, "ainda nada escolhido");
    /* painel: BUILD_W 600, altura 70 + 7*50 + 44 = 464; x = 340, y = (720-464)/2 - 20 = 108; linhas em y + 56 + i*50 */
    click(MOUSE_BUTTON_LEFT, 340 + 100, 108 + 56 + 2 * 50 + 10, 0);   /* 3a linha: Forno (BUILDABLE[2]) */
    menu_update(&luna, SW, SH); release();
    CHECK(!menu_is_open(), "escolher fecha o menu");
    CHECK(menu_picked_build() == BUILDABLE[2] && BUILDABLE[2] == B_FORNO, "devia ter escolhido o Forno, escolheu %d", menu_picked_build());
    CHECK(menu_picked_build() == B_NONE, "a escolha so e entregue uma vez");
    menu_toggle_build(); menu_toggle_build();
    CHECK(!menu_is_open(), "Q duas vezes: abre e fecha");


    /* ---- Uma obra: o menu mostra o material e deposita-se la o que ela pede ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    container_clear(&backpack);
    game_mode = MODE_SOBREVIVENCIA;
    place_building(B_FORNO_RUPESTRE, 66, 55, 0, 0, 0);               /* sem material: obra a 0 (800 argila) */
    Building *obra = building_at(66, 55);
    CHECK(obra != NULL && site_active(obra), "obra pousada");
    backpack.n[ITEM_ARGILA] = 5; backpack.n[ITEM_MADEIRA] = 10;
    menu_open_building(66, 55);
    CHECK(menu_update(&luna, SW, SH) == 1, "o menu da obra abre");
    /* painel da obra (sem barras): 0 Estado, 1 Obra total, 2 titulo, 3 material 1, ... */
    const int OBRA_ROW0 = 3;
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + 5, 1);             /* mochila, 1a linha = argila (shift: tudo) */
    menu_update(&luna, SW, SH); release();
    CHECK(obra->deposit[0] == 5 && backpack.n[ITEM_ARGILA] == 0 && obra->in[ITEM_ARGILA] == 0, "a obra recebe o material (depositado %d)", obra->deposit[0]);
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + 5, 1);             /* agora a 1a linha da mochila e a madeira */
    menu_update(&luna, SW, SH); release();
    CHECK(backpack.n[ITEM_MADEIRA] == 10 && obra->deposit[0] == 5, "o forno rupestre e so de argila: a madeira fica na mochila");
    /* tirar de volta pelo painel da direita: botao direito = 1, Shift+clique = tudo */
    click(MOUSE_BUTTON_RIGHT, RIGHT_X + 30, ROW0_Y_NOBARS + OBRA_ROW0 * ROW_H + 5, 0);
    menu_update(&luna, SW, SH); release();
    CHECK(obra->deposit[0] == 4 && backpack.n[ITEM_ARGILA] == 1, "botao direito tira 1 (depositado %d, mochila %d)", obra->deposit[0], backpack.n[ITEM_ARGILA]);
    click(MOUSE_BUTTON_LEFT, RIGHT_X + 30, ROW0_Y_NOBARS + OBRA_ROW0 * ROW_H + 5, 1);
    menu_update(&luna, SW, SH); release();
    CHECK(obra->deposit[0] == 0 && backpack.n[ITEM_ARGILA] == 5, "Shift+clique tira tudo o que nao foi usado");
    /* nunca se deposita mais do que o custo (800) */
    backpack.n[ITEM_ARGILA] = 1000;
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + 5, 1);
    menu_update(&luna, SW, SH); release();
    CHECK(obra->deposit[0] == 800 && backpack.n[ITEM_ARGILA] == 200, "so entram os 800 que a obra pede (depositado %d, mochila %d)", obra->deposit[0], backpack.n[ITEM_ARGILA]);
    /* a obra avanca com o material depositado (a Luna esta ao pe) e depois ja nao e obra: o menu passa a ser o da maquina */
    for (int step = 0; step < 60 * 60; step++) construction_step(luna.x, luna.y, SIM_DT);
    CHECK(!site_active(obra), "obra acabada com o material depositado");
    CHECK(menu_update(&luna, SW, SH) == 1, "o menu continua aberto, agora da maquina");
    menu_close();
    reset_buildings();      /* (demolir devolveria 80 L de argila, que nao cabem na mochila de 20 L) */

    /* uma obra com dois materiais (carvoeira: 150 madeira + 400 argila): cada linha e o seu item */
    container_clear(&backpack);
    backpack.n[ITEM_ARGILA] = 30; backpack.n[ITEM_MADEIRA] = 20;
    place_building(B_CARVOEIRA, 66, 55, 0, 0, 0);
    obra = building_at(66, 55);
    menu_open_building(66, 55);
    menu_update(&luna, SW, SH);
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + 5, 0);             /* mochila: 1a linha = argila (10 un) */
    menu_update(&luna, SW, SH); release();
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + ROW_H + 5, 0);     /* 2a linha = madeira (10 un) */
    menu_update(&luna, SW, SH); release();
    CHECK(obra->deposit[0] == 10 && obra->deposit[1] == 10 && backpack.n[ITEM_MADEIRA] == 10 && backpack.n[ITEM_ARGILA] == 20,
          "cada linha deposita no seu campo (madeira %d, argila %d)", obra->deposit[0], obra->deposit[1]);
    /* no painel da obra a ordem e a do custo: 3 = madeira, 4 = argila */
    click(MOUSE_BUTTON_LEFT, RIGHT_X + 30, ROW0_Y_NOBARS + (OBRA_ROW0 + 1) * ROW_H + 5, 1);
    menu_update(&luna, SW, SH); release();
    CHECK(obra->deposit[1] == 0 && backpack.n[ITEM_ARGILA] == 30 && obra->deposit[0] == 10, "tirar a argila (2a linha da obra) nao mexe na madeira");
    menu_close();
    remove_building_at(66, 55);
    game_mode = MODE_CRIATIVO;
    container_clear(&backpack);

    /* ---- Uma estrutura a ser demolida: o menu so mostra, nao recebe itens, e fecha quando ela some ---- */
    reset_buildings();
    flat(60, 50, 30, 20);
    container_clear(&backpack);
    game_mode = MODE_SOBREVIVENCIA;
    place_building(B_BAU, 66, 55, 0, 0, 0);
    Building *dm = building_at(66, 55);
    backpack.n[ITEM_MADEIRA] = 50;
    CHECK(demolish_start(66, 55) == 1 && demo_active(dm), "marcar o bau para demolir");
    menu_open_building(66, 55);
    CHECK(menu_update(&luna, SW, SH) == 1, "o menu de uma demolicao abre");
    click(MOUSE_BUTTON_LEFT, LEFT_X + 30, ROW0_Y + 5, 1);             /* mochila: 1a linha = madeira (shift: tudo) */
    menu_update(&luna, SW, SH); release();
    CHECK(backpack.n[ITEM_MADEIRA] == 50 && container_units(&dm->store) == 0, "a estrutura a ser demolida nao recebe itens (mochila %d)", backpack.n[ITEM_MADEIRA]);
    for (int step = 0; step < 60 * 20; step++) demolition_step(luna.x, luna.y, SIM_DT);
    menu_update(&luna, SW, SH);
    CHECK(building_at(66, 55) == NULL && !menu_is_open(), "demolida: a estrutura some e o menu fecha");
    reset_buildings();
    game_mode = MODE_CRIATIVO;
    container_clear(&backpack);

    /* ---- Interface ajustavel: tamanhos, degraus e cliques com a interface ampliada ---- */
    ui_scale = 1.0f;
    CHECK(ui_fit(1280, 720, 800, 500) == 1.0f, "a 100%% cabe");
    ui_scale = 3.0f;
    float fit720 = ui_fit(1280, 720, 800, 500), fit1080 = ui_fit(1920, 1080, 800, 500);
    CHECK(fit720 > 1.4f && fit720 < 1.5f, "1280x720 so deixa ~144%% (deu %.2f)", (double)fit720);
    CHECK(fit1080 > 2.1f && fit1080 < 2.2f, "1920x1080 deixa ~216%% (deu %.2f)", (double)fit1080);
    CHECK(ui_fit(300, 200, 800, 500) >= 0.5f, "nunca desce de 50%%");
    ui_scale = 1.5f;
    ui_scale_step(1);  CHECK(ui_scale == 1.75f, "1.5 -> 1.75 (%.2f)", (double)ui_scale);
    for (int i = 0; i < 20; i++) ui_scale_step(1);
    CHECK(ui_scale == 3.0f, "o maximo e 300%% (%.2f)", (double)ui_scale);
    for (int i = 0; i < 20; i++) ui_scale_step(-1);
    CHECK(ui_scale == 1.0f, "o minimo e 100%% (%.2f)", (double)ui_scale);
    ui_scale = 1.4f;  ui_scale_step(1);
    CHECK(ui_scale == 1.75f, "de um valor fora dos degraus vai ao degrau mais perto e sobe (%.2f)", (double)ui_scale);

    /* os cliques acertam com a interface a 150% num ecra 1920x1080 (ecra virtual 1280x720) */
    ui_scale = 1.5f;
    container_clear(&backpack);
    backpack.n[ITEM_AREIA] = 30;
    menu_open_backpack(65, 54);
    click(MOUSE_BUTTON_LEFT, (LEFT_X + 30) * 1.5f, (ROW0_Y + 5) * 1.5f, 0);
    menu_update(&luna, 1920, 1080); release();
    CHECK(building_at(65, 54) != NULL && building_at(65, 54)->store.n[ITEM_AREIA] == 10 && backpack.n[ITEM_AREIA] == 20,
          "clique a 150%%: larga 10 areia");
    menu_close();
    remove_building_at(65, 54);
    /* e a 300% num ecra 1280x720, onde so cabe ~144%: o clique tem de usar o fator real (1.44), nao os 3.0 pedidos */
    ui_scale = 3.0f;
    container_clear(&backpack);
    backpack.n[ITEM_AREIA] = 30;
    menu_open_backpack(65, 54);
    float f = ui_fit(1280, 720, 800, 500);
    float vw = 1280.0f / f;
    float lx0 = (vw - 740.0f) / 2.0f;                                    /* origem do painel esquerdo, virtual */
    float ly0 = (720.0f / f - 470.0f) / 2.0f;
    click(MOUSE_BUTTON_LEFT, (lx0 + 30.0f) * f, (ly0 + 108.0f + 5.0f) * f, 0);
    menu_update(&luna, 1280, 720); release();
    CHECK(building_at(65, 54) != NULL && building_at(65, 54)->store.n[ITEM_AREIA] == 10, "clique com a interface limitada pela janela");
    menu_close();
    remove_building_at(65, 54);
    container_clear(&backpack);
    ui_scale = 1.0f;

    printf("%s\n", fails ? "TESTES DOS MENUS COM FALHAS" : "TESTES DOS MENUS PASSARAM");
    return fails != 0;
}
