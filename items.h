/*
 * items.h  -  OS ITENS: tipos, volume e massa (implementado em items.c).
 *
 * Um ITEM e uma coisa que anda em esteiras e se guarda em baus e na mochila.
 * Nao confundir com OreType (world.h): esse e o que existe NO CHAO (jazidas).
 * Cada jazida da um item (ver ORE[].item em world.c), mas ha itens que nao
 * existem no chao (carvao, tijolo, ferro): so saem de maquinas.
 *
 * UNIDADE: a quantidade minima de um item e 1 unidade. Para po, areia, rocha
 * partida, lenha, etc. 1 unidade = 0,1 L (100 mL). Para coisas que nao se usam
 * "a metade" (tijolo) 1 unidade e a propria peca (1 L).
 * A MASSA de 1 unidade = volume x densidade (sempre em gramas inteiros).
 *
 * So se usa o TIPO Color da raylib (cores das esteiras e do HUD).
 */
#ifndef ITEMS_H
#define ITEMS_H

#include "raylib.h"

typedef enum {
    ITEM_NONE = 0,
    ITEM_HEMATITE,      /* minerio de ferro (rocha partida)       */
    ITEM_ARGILA,        /* argila                                 */
    ITEM_ENXOFRE,       /* enxofre                                */
    ITEM_QUARTZO,       /* quartzo (rocha partida)                */
    ITEM_MADEIRA,       /* lenha                                  */
    ITEM_AREIA,         /* areia                                  */
    /* Daqui para baixo: PRODUTOS (so saem de maquinas) */
    ITEM_CARVAO,        /* carvao vegetal                         */
    ITEM_TIJOLO,        /* tijolo (peca de 20 x 10 x 5 cm = 1 L)  */
    ITEM_FERRO,         /* barra de ferro                         */
    /* Daqui para baixo: o que sai de DEMOLIR (ver logic.c, demolicao) */
    ITEM_FERRO_ENF,     /* ferro enferrujado (sucata): funde-se no forno  */
    ITEM_TERRA,         /* terra: a massa que se perde ao demolir; desaparece do chao */
    ITEM_COUNT
} ItemType;

typedef struct {
    const char *name;
    Color       color;
    int         vol_ml;     /* volume de 1 unidade, em mL                */
    int         density;    /* densidade a granel, em g/L                */
    int         salvage_pct;    /* % das unidades que se aproveitam quando o item (usado numa
                                 * construcao) volta a sair ao demolir; o resto vira terra */
    int         salvage_item;   /* no que se transforma o que se aproveita (ITEM_NONE = o mesmo) */
} ItemInfo;

extern const ItemInfo ITEM[ITEM_COUNT];

/* Massa de 1 unidade do item, em gramas (volume x densidade) */
int item_mass_g(int item);

#endif
