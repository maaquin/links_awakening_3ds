#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ui_textures.h"

// Ubicación de los gráficos de ítems en la ROM (Banco 0x0C = 0x0C * 0x4000 = 0x30000)
#define ROM_ITEM_TILES_BASE 0x30000

// Cada tile 2bpp ocupa 16 bytes; 4 tiles por sprite = 64 bytes por ítem
#define ITEM_SPRITE_SIZE 64 

// Paleta auténtica de GBC para ítems (formato RGBA8 little-endian de PICA200)
// Color 0: Transparente (0x00000000)
// Color 1: Blanco/Crema  (#FFF8D8 -> 0xFFD8F8FF)
// Color 2: Naranja/Rojo  (#E05020 -> 0x2050E0FF)
// Color 3: Negro/Sombra  (#101020 -> 0x201010FF)
static const uint32_t s_gbc_palette[4] = {
    0x00000000, // 0: Transparente
    0xFFD8F8FF, // 1: Resalte
    0x2050E0FF, // 2: Primario
    0x201010FF  // 3: Delineado / Sombra
};

// Decodifica un tile de Game Boy (16 bytes, 2bpp planar) a un cuadro RGBA de 8x8 px
static void decode_tile(const uint8_t *tile_data, uint32_t *dst, int stride)
{
    for (int y = 0; y < 8; y++) {
        uint8_t b1 = tile_data[y * 2 + 0];
        uint8_t b2 = tile_data[y * 2 + 1];
        for (int x = 0; x < 8; x++) {
            int bit = 7 - x;
            int idx = ((b1 >> bit) & 1) | (((b2 >> bit) & 1) << 1);
            dst[y * stride + x] = s_gbc_palette[idx];
        }
    }
}


// Offset base donde empiezan la Espada, Bombas, Arco, Escudo, etc.
#define ROM_EQUIP_ITEMS_BASE 0x307E0 // Ítems de 8x16 (32 bytes)
#define ROM_TRADE_ITEMS_BASE 0x30400 // Ítems de 16x16 (64 bytes)

bool rom_extract_item_textures(const char *rom_path)
{
    FILE *f = fopen(rom_path, "rb");
    if (!f) return false;

    uint32_t *linear_atlas = (uint32_t *)linearAlloc(128 * 128 * sizeof(uint32_t));
    if (!linear_atlas) {
        fclose(f);
        return false;
    }
    memset(linear_atlas, 0, 128 * 128 * sizeof(uint32_t));

    uint8_t buffer[64];

    // 1. Extraer ítems equipables (8x16 px) -> Se guardan en las primeras 24 casillas (0 a 23)
    for (int i = 0; i < 24; i++) {
        fseek(f, ROM_EQUIP_ITEMS_BASE + (i * 32), SEEK_SET);
        if (fread(buffer, 1, 32, f) != 32) break;

        int col = i % 8;
        int row = i / 8;
        int px = col * 16 + 4; // +4 px para centrar los 8 px en la celda de 16 px
        int py = row * 16;

        uint32_t *base_ptr = &linear_atlas[py * 128 + px];
        decode_tile(&buffer[0],  base_ptr, 128);            // Arriba (8x8)
        decode_tile(&buffer[16], base_ptr + (8 * 128), 128); // Abajo (8x8)
    }

    // 2. Extraer ítems de misión (16x16 px) -> Se guardan a partir de la casilla 24 (fila 3 en adelante)
    for (int i = 0; i < 14; i++) {
        fseek(f, ROM_TRADE_ITEMS_BASE + (i * 64), SEEK_SET);
        if (fread(buffer, 1, 64, f) != 64) break;

        int slot = 24 + i;
        int col = slot % 8;
        int row = slot / 8;
        int px = col * 16;
        int py = row * 16;

        uint32_t *base_ptr = &linear_atlas[py * 128 + px];
        decode_tile(&buffer[0],  base_ptr, 128);                 // Arriba-Izq
        decode_tile(&buffer[16], base_ptr + (8 * 128), 128);     // Abajo-Izq
        decode_tile(&buffer[32], base_ptr + 8, 128);             // Arriba-Der
        decode_tile(&buffer[48], base_ptr + (8 * 128) + 8, 128); // Abajo-Der
    }

    fclose(f);
    GSPGPU_FlushDataCache(linear_atlas, 128 * 128 * sizeof(uint32_t));

    C3D_TexInit(&g_tex_items, 128, 128, GPU_RGBA8);
    C3D_TexSetFilter(&g_tex_items, GPU_NEAREST, GPU_NEAREST);
    g_tex_items.data = linearAlloc(128 * 128 * sizeof(uint32_t));

    GX_DisplayTransfer((u32 *)linear_atlas, GX_BUFFER_DIM(128, 128),
                       (u32 *)g_tex_items.data, GX_BUFFER_DIM(128, 128),
                       GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
                       GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                       GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    gspWaitForPPF();

    linearFree(linear_atlas);
    return true;
}