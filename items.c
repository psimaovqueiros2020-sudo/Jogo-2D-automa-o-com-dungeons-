/*
 * items.c  -  Tabela dos itens: volume e massa.
 * Sem input e sem desenho (so o TIPO Color da raylib).
 */

#include "items.h"

/* ---- TABELA DOS ITENS ----------------------------------------------
 * Para ajustar um valor, muda so aqui.
 *
 *   vol_ml  : volume de 1 unidade. 100 mL (0,1 L) e o minimo para po,
 *             areia, rocha partida e lenha. O tijolo e uma peca inteira.
 *   density : g/L "a granel", ou seja, como se carrega (com ar entre as
 *             pedras ou os paus), e nao a densidade da rocha macica.
 *
 *   Item        g/L   massa/unidade   porque
 *   Madeira      400      40 g        lenha empilhada (ha ar entre os paus)
 *   Carvao       300      30 g        leve e poroso (o valor do autor)
 *   Enxofre     1000     100 g        po e pedacinhos
 *   Quartzo     1500     150 g        rocha partida
 *   Areia       1600     160 g        areia humida
 *   Argila      1700     170 g        argila humida
 *   Hematite    2500     250 g        minerio de ferro partido
 *   Tijolo      1800    1800 g        peca de 20x10x5 cm = 1 L
 *   Ferro       7800     780 g        barra pequena e macica
 *   Sucata      7800     780 g        ferro enferrujado (a mesma barra, so que ferrugenta)
 *   Terra       1000     100 g        terra solta (a massa perdida ao demolir)
 *
 * DEMOLIR: salvage_pct e salvage_item dizem o que acontece ao material que esteve
 * numa construcao quando ela e demolida:
 *   madeira  50 %  apodrece e racha: metade aproveita-se
 *   tijolo   60 %  parte-se ao desmontar a argamassa
 *   ferro   100 %  aproveita-se tudo, mas SAI ENFERRUJADO (sucata: volta a fundir-se)
 *   argila  100 %  desfaz-se e recolhe-se
 * O que NAO se aproveita (a massa que falta) transforma-se em TERRA: a massa de
 * uma construcao e a mesma antes e depois de demolir (a menos de 1 unidade de terra).
 */
const ItemInfo ITEM[ITEM_COUNT] = {
    /*                  nome        cor                         vol_ml dens  salv% salvou em */
    [ITEM_NONE]     = { "-",        {   0,   0,   0, 255 },  100,    0,  100, ITEM_NONE },
    [ITEM_HEMATITE] = { "Hematite", { 176,  70,  52, 255 },  100, 2500,  100, ITEM_NONE },
    [ITEM_ARGILA]   = { "Argila",   { 222, 140, 100, 255 },  100, 1700,  100, ITEM_NONE },
    [ITEM_ENXOFRE]  = { "Enxofre",  { 232, 208,  64, 255 },  100, 1000,  100, ITEM_NONE },
    [ITEM_QUARTZO]  = { "Quartzo",  { 232, 240, 247, 255 },  100, 1500,  100, ITEM_NONE },
    [ITEM_MADEIRA]  = { "Madeira",  { 138,  92,  48, 255 },  100,  400,   50, ITEM_NONE },
    [ITEM_AREIA]    = { "Areia",    { 232, 214, 150, 255 },  100, 1600,  100, ITEM_NONE },
    [ITEM_CARVAO]   = { "Carvao",   {  70,  72,  84, 255 },  100,  300,  100, ITEM_NONE },
    [ITEM_TIJOLO]   = { "Tijolo",   { 214, 100,  50, 255 }, 1000, 1800,   60, ITEM_NONE },
    [ITEM_FERRO]    = { "Ferro",    { 160, 172, 196, 255 },  100, 7800,  100, ITEM_FERRO_ENF },
    [ITEM_FERRO_ENF]= { "Sucata",   { 150,  86,  58, 255 },  100, 7800,  100, ITEM_NONE },
    [ITEM_TERRA]    = { "Terra",    { 112,  86,  62, 255 },  100, 1000,  100, ITEM_NONE },
};

int item_mass_g(int item)
{
    return ITEM[item].vol_ml * ITEM[item].density / 1000;
}
