#ifndef INVENTORY_MENU_H
#define INVENTORY_MENU_H

#include <stdint.h>
#include <stdbool.h>

typedef struct GBContext GBContext;

void inventory_menu_init(void);
void inventory_menu_update(GBContext *ctx);
void inventory_menu_render(GBContext *ctx);
void inventory_menu_toggle_map(void);

#endif