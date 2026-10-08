#ifndef ROM_LOADER_H
#define ROM_LOADER_H

#include <stdbool.h>
#include "gbrt.h"

#define ROM_SD_PATH "sdmc:/3ds/links_awakening/zelda.gbc"

/**
 * Carga la ROM de Game Boy Color desde la tarjeta SD hacia la RAM
 * e inicializa el contexto de ejecución.
 * 
 * @param ctx Puntero al GBContext creado previamente.
 * @return true si la ROM se cargó y validó con éxito; false en caso de error.
 */
bool rom_loader_init(GBContext* ctx);

#endif // ROM_LOADER_H