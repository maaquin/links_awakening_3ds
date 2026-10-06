#include "rom_loader.h"
#include <stdio.h>
#include <stdlib.h>

bool rom_loader_init(GBContext* ctx) {
    if (!ctx) return false;

    FILE* f = fopen(ROM_SD_PATH, "rb");
    if (!f) {
        fprintf(stderr, "[ROMLOADER] Error: no se encontro la ROM en:\n %s\n", ROM_SD_PATH);
        return false;
    }

    // Obtener tamaño del archivo
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0) {
        fprintf(stderr, "[ROMLOADER] Error: la ROM tiene un tamano invalido (%ld bytes)\n", size);
        fclose(f);
        return false;
    }

    // Buffer temporal para volcar la lectura directa de la SD
    uint8_t* temp_buffer = (uint8_t*)malloc((size_t)size);
    if (!temp_buffer) {
        fprintf(stderr, "[ROMLOADER] Error: memoria insuficiente para leer la ROM\n");
        fclose(f);
        return false;
    }

    size_t read_bytes = fread(temp_buffer, 1, (size_t)size, f);
    fclose(f);

    if (read_bytes != (size_t)size) {
        fprintf(stderr, "[ROMLOADER] Error al leer los datos de la ROM desde la SD\n");
        free(temp_buffer);
        return false;
    }

    // Inyectar el buffer en el contexto (gb_context_load_rom se encarga de malloc,
    // memcpy a ctx->rom, configuracion de bancos, MBC y ERAM)
    if (!gb_context_load_rom(ctx, temp_buffer, (size_t)size)) {
        fprintf(stderr, "[ROMLOADER] Error: gb_context_load_rom fallo al procesar los datos\n");
        free(temp_buffer);
        return false;
    }

    // Liberamos el buffer temporal; los datos ya residen en ctx->rom
    free(temp_buffer);

    fprintf(stderr, "[ROMLOADER] ROM cargada con exito desde SD (%ld bytes)\n", size);
    return true;
}