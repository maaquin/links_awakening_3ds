#ifndef INVENTORY_MENU_H
#define INVENTORY_MENU_H

#include "gbrt.h"

// Inicializa coordenadas de casillas y estados
void inventory_menu_init(void);

// Procesa el toque táctil en la pantalla táctil y escribe en WRAM
void inventory_menu_update(GBContext *ctx);

// Dibuja el marco y las casillas del inventario en la pantalla inferior
void inventory_menu_render(GBContext *ctx);

#endif // INVENTORY_MENU_H