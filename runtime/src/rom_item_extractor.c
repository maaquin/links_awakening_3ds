#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ui_textures.h"

// =============================================================================
// UBICACIONES CONFIRMADAS EN LA ROM (LADX)
// =============================================================================
#define ROM_INSTRUMENTS_BASE 0x45000 // 8 Instrumentos (16x16 px = 64 bytes c/u)

#define ROM_KEY_TAIL_BASE 0x328E0   // Llave Tail (8x16 px = 32 bytes)
#define ROM_KEY_SLIME_BASE 0x30C00  // Llave Slime (8x16 px = 32 bytes)
#define ROM_KEY_ANGLER_BASE 0x30C20 // Llave Angler (8x16 px = 32 bytes)
#define ROM_KEY_FACE_BASE 0x30C40   // Llave Rostro (8x16 px = 32 bytes)
#define ROM_KEY_BIRD_BASE 0x30C60   // Llave Pájaro (8x16 px = 32 bytes)

#define ROM_TOADSTOOL_BASE 0x328C0  // Hongo (8x16 px = 32 bytes)
#define ROM_RUPEE_ICON_BASE 0x30D00 // Rupia del menú (8x16 px = 32 bytes)

#define ROM_HEART_PIECE_EMPTY_BASE 0x32900 // Mitad izquierda vacía (8x16 px = 32 bytes)
#define ROM_HEART_PIECE_Q1_BASE 0x32920    // Mitad izquierda 1/4 (8x16 px = 32 bytes)
#define ROM_HEART_PIECE_Q2_BASE 0x32940    // Mitad izquierda 2/4 (8x16 px = 32 bytes)

#define ROM_TRADE_ITEMS_BASE 0x30400 // Ítems de Intercambio (16x16 px = 64 bytes c/u)
#define ROM_EQUIP_ITEMS_BASE 0x307E0 // Ítems Equipables (8x16 px = 32 bytes c/u)
#define ROM_YOSHI_TRADE_ITEM 0x309A0 // Yoshi está separado de los trade

// =============================================================================
// PALETAS AUTÉNTICAS GBC PARA PICA200 (RGBA8 little-endian: 0xRRGGBBAA)
// =============================================================================
static const uint32_t s_pal_blue_steel[4] = {
    0x00000000, 0xFFFFFFFF, 0x2038F8FF, 0x200000FF // Azul / Plateado
};

static const uint32_t s_pal_red_fire[4] = {

    0x00000000, 0xFFFFFFFF, 0xF87048FF, 0x181011FF // Rojo / Carmesí

};

static const uint32_t s_pal_heart_pieces[4] = {
    0x00000000, 0xf7113fFF, 0xFFFFFFFF, 0x00000000 // Rojo corazón
};

static const uint32_t s_pal_gold_wood[4] = {
    0x00000000, 0xFFF8A8FF, 0xD89820FF, 0x401808FF // Dorado / Madera
};

static const uint32_t s_pal_magic_purple[4] = {
    0x00000000, 0xFFFFE0FF, 0xE830A0FF, 0x500030FF // Violeta / Mágico
};

static const uint32_t s_pal_green[4] = {
    0x00000000, 0xFFFFFFFF, 0x38C830FF, 0x104000FF // Verde
};

static const uint32_t s_pal_tunic_green[5] = {
    0x00000000, 0xFFFFFFFF, 0x22b14cFF, 0x101810FF, 0x91e3a8FF // Verde túnica y rupia
};

static const uint32_t s_pal_tunic_red[4] = {

    0x00000000, 0xFFFFFFFF, 0xDE1010FF, 0x181011FF // Rojo túnica

};

static const uint32_t s_pal_tunic_blue[5] = {
    0x00000000, 0xFFFFFFFF, 0x1A2DC4FF, 0x101318FF // Azul túnica
};

static const uint32_t s_pal_photo_frame[4] = {
    0x00000000, 0xFFFFFFFF, 0xC0B0A8FF, 0x101010FF // Blanco / Gris / Sombra
};

// Paletas originales GBC corregidas (Índice 1 = Delineado oscuro/negro | Índice 3 = Brillo/blanco)
static const uint32_t s_instrument_palettes[8][4] = {
    // 1. Violonchelo (Madera / Ocre)
    {0x00000000, 0x181008FF, 0xC89040FF, 0xFFFFFFFF},

    // 2. Cuerno Olifante (Naranja / Cobre)
    {0x00000000, 0x180800FF, 0xC04818FF, 0xF8D0A8FF},

    // 3. Campana Marina (Dorado brillante)
    {0x00000000, 0x181000FF, 0xC89820FF, 0xFFFFFFFF},

    // 4. Arpa de las Olas (Dorado / Bronce)
    {0x00000000, 0x181000FF, 0xB07820FF, 0xF8E8A8FF},

    // 5. Marimba del Viento (Azul marino)
    {0x00000000, 0x080820FF, 0x3038D8FF, 0xC8D8FFFF},

    // 6. Triángulo de Coral (Rojo coral)
    {0x00000000, 0x180008FF, 0xB01828FF, 0xFFC0C0FF},

    // 7. Órgano de la Calma (Púrpura / Violeta)
    {0x00000000, 0x100018FF, 0x6820A0FF, 0xF0D8FFFF},

    // 8. Tambor del Trueno (Rosa / Carmesí)
    {0x00000000, 0x200810FF, 0x981850FF, 0xFFFFFFFF}};

// =============================================================================
// SPRITES MATRICIALES (Túnica y Marco de Foto)
// =============================================================================
static const uint8_t s_sprite_tunic[16][16] = {
    {0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 3, 1, 1, 1, 1, 3, 0, 0, 0, 0, 0},
    {0, 0, 0, 3, 3, 1, 3, 3, 3, 3, 1, 3, 3, 0, 0, 0},
    {0, 0, 3, 2, 3, 1, 3, 3, 3, 3, 1, 3, 2, 3, 0, 0},
    {0, 3, 2, 2, 2, 3, 1, 3, 3, 1, 3, 2, 2, 2, 3, 0},
    {3, 2, 2, 2, 2, 2, 3, 1, 1, 3, 2, 2, 3, 2, 2, 3},
    {3, 2, 2, 2, 3, 2, 2, 3, 3, 2, 2, 3, 2, 2, 2, 3},
    {0, 3, 2, 3, 2, 2, 2, 2, 2, 2, 2, 2, 3, 2, 3, 0},
    {0, 0, 3, 1, 3, 3, 2, 2, 2, 2, 3, 3, 1, 3, 0, 0},
    {0, 0, 0, 3, 1, 1, 3, 3, 3, 3, 1, 1, 3, 0, 0, 0},
    {0, 0, 3, 2, 3, 3, 1, 1, 1, 1, 3, 3, 2, 3, 0, 0},
    {0, 0, 3, 2, 2, 2, 3, 3, 3, 3, 2, 2, 2, 3, 0, 0},
    {0, 0, 0, 3, 2, 2, 2, 2, 2, 2, 2, 2, 3, 0, 0, 0},
    {0, 0, 0, 0, 3, 3, 2, 2, 2, 2, 3, 3, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 3, 3, 3, 3, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

static const uint8_t s_sprite_album[16][16] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 0},
    {0, 1, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 0},
    {0, 1, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 0},
    {0, 1, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 0},
    {0, 1, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 0},
    {0, 1, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 0},
    {0, 1, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 0},
    {0, 1, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 0},
    {0, 1, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 0},
    {0, 1, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 0},
    {0, 1, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 1, 0},
    {0, 1, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 1, 0},
    {0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};

static const uint8_t s_sprite_rupee[8][8] = {
    {0, 0, 0, 3, 3, 3, 3, 0},
    {0, 0, 3, 1, 1, 1, 3, 0},
    {0, 3, 1, 1, 1, 1, 3, 0},
    {3, 1, 1, 2, 4, 4, 3, 0},
    {3, 2, 2, 4, 4, 3, 0, 0},
    {3, 2, 2, 4, 3, 0, 0, 0},
    {3, 3, 3, 3, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0},
};

static const uint32_t *get_equip_item_palette(int slot_idx)
{
    switch (slot_idx)
    {
    case 1:  // bombas -> rojo
    case 5:  // arco -> rojo
    case 10: // pluma -> rojo
    case 12: // pala -> rojo / naranja
    case 19: // bumerang -> rojo / naranja
    case 24: // moño -> rojo
    case 25: // comida de perro -> rojo
    case 30: // flor -> rojo
        return s_pal_red_fire;

    case 2:  // brazalete -> dorado
    case 9:  // ocarina -> dorado
    case 26: // banano -> amarillo
    case 27: // vara -> café
    case 28: // panal -> amarillo
    case 29: // piña -> amarillo
    case 32: // escoba -> café
        return s_pal_gold_wood;

    case 7:  // varita mágica -> violeta
    case 8:  // polvos mágicos -> violeta
    case 17: // medicina -> violeta
    case 34: // collar -> violeta
        return s_pal_magic_purple;

    case 3:  // espada L-1 -> azul / plateado
    case 4:  // escudo L-1 -> azul / plateado
    case 6:  // gancho -> plateado
    case 11: // aletas -> azul
    case 13: // botas -> plateado
    case 31: // darta -> celeste / blanca
    case 33: // anzuelo -> azul
    case 35: // escama -> azul
    case 36: // lupa -> plateado
    default:
        return s_pal_blue_steel;
    }
}

// Rellena los huecos interiores transparentes con blanco opaco (0xFFFFFFFF)
static void fill_internal_holes(uint32_t *base_ptr, int w, int h, int stride)
{
    // 0: no visitado, 1: conectado al exterior
    uint8_t visited[16][16] = {0};
    int queue_x[256], queue_y[256];
    int q_head = 0, q_tail = 0;

    // Encolar todos los píxeles transparentes que tocan los 4 bordes exteriores
    for (int x = 0; x < w; x++)
    {
        // Borde superior
        if (base_ptr[0 * stride + x] == 0)
        {
            visited[0][x] = 1;
            queue_x[q_tail] = x;
            queue_y[q_tail++] = 0;
        }
        // Borde inferior
        if (base_ptr[(h - 1) * stride + x] == 0 && !visited[h - 1][x])
        {
            visited[h - 1][x] = 1;
            queue_x[q_tail] = x;
            queue_y[q_tail++] = h - 1;
        }
    }
    for (int y = 0; y < h; y++)
    {
        // Borde izquierdo
        if (base_ptr[y * stride + 0] == 0 && !visited[y][0])
        {
            visited[y][0] = 1;
            queue_x[q_tail] = 0;
            queue_y[q_tail++] = y;
        }
        // Borde derecho
        if (base_ptr[y * stride + (w - 1)] == 0 && !visited[y][w - 1])
        {
            visited[y][w - 1] = 1;
            queue_x[q_tail] = w - 1;
            queue_y[q_tail++] = y;
        }
    }

    // BFS (Breadth-First Search) para marcar todo el exterior transparente
    static const int dx[4] = {1, -1, 0, 0};
    static const int dy[4] = {0, 0, 1, -1};

    while (q_head < q_tail)
    {
        int cx = queue_x[q_head];
        int cy = queue_y[q_head++];

        for (int i = 0; i < 4; i++)
        {
            int nx = cx + dx[i];
            int ny = cy + dy[i];

            if (nx >= 0 && nx < w && ny >= 0 && ny < h)
            {
                if (!visited[ny][nx] && base_ptr[ny * stride + nx] == 0)
                {
                    visited[ny][nx] = 1;
                    queue_x[q_tail] = nx;
                    queue_y[q_tail++] = ny;
                }
            }
        }
    }

    // Cualquier píxel transparente que NO se conectó al exterior es un hueco interior
    for (int y = 0; y < h; y++)
    {
        for (int x = 0; x < w; x++)
        {
            if (base_ptr[y * stride + x] == 0 && !visited[y][x])
            {
                base_ptr[y * stride + x] = 0xFFFFFFFF; // Relleno blanco
            }
        }
    }
}

// Decodifica un tile (8x8 px, 2bpp) normal o con volteo horizontal (flip_x)
static void decode_tile_pal_ex(const uint8_t *tile_data, uint32_t *dst, int stride, const uint32_t *palette, bool flip_x)
{
    for (int y = 0; y < 8; y++)
    {
        uint8_t b1 = tile_data[y * 2 + 0];
        uint8_t b2 = tile_data[y * 2 + 1];
        for (int x = 0; x < 8; x++)
        {
            int bit = flip_x ? x : (7 - x);
            int idx = ((b1 >> bit) & 1) | (((b2 >> bit) & 1) << 1);
            dst[y * stride + x] = palette[idx];
        }
    }
}

static inline void decode_tile_pal(const uint8_t *tile_data, uint32_t *dst, int stride, const uint32_t *palette)
{
    decode_tile_pal_ex(tile_data, dst, stride, palette, false);
}

// Ensambla una mitad izquierda y una mitad derecha volteada en un slot de 16x16
static void assemble_heart_slot(uint32_t *linear_atlas, int slot, const uint8_t *left_half, const uint8_t *right_half)
{
    int px = (slot % 8) * 16;
    int py = (slot / 8) * 16;
    uint32_t *base = &linear_atlas[py * 128 + px];

    // Mitad izquierda (8x16 px)
    decode_tile_pal(&left_half[0], base, 128, s_pal_heart_pieces);
    decode_tile_pal(&left_half[16], base + (8 * 128), 128, s_pal_heart_pieces);

    // Mitad derecha con flip X (8x16 px)
    decode_tile_pal_ex(&right_half[0], base + 8, 128, s_pal_heart_pieces, true);
    decode_tile_pal_ex(&right_half[16], base + (8 * 128) + 8, 128, s_pal_heart_pieces, true);
}

bool rom_extract_item_textures(const char *rom_path)
{
    FILE *f = fopen(rom_path, "rb");
    if (!f)
        return false;

    uint32_t *linear_atlas = (uint32_t *)linearAlloc(128 * 128 * sizeof(uint32_t));
    if (!linear_atlas)
    {
        fclose(f);
        return false;
    }
    memset(linear_atlas, 0, 128 * 128 * sizeof(uint32_t));

    uint8_t buffer[64];

    // =========================================================================
    // 1. Ítems Equipables de Inventario (8x16 px) -> Slots 0 a 23
    // =========================================================================
    for (int i = 0; i < 24; i++)
    {
        fseek(f, ROM_EQUIP_ITEMS_BASE + (i * 32), SEEK_SET);
        if (fread(buffer, 1, 32, f) != 32)
            break;

        const uint32_t *pal = get_equip_item_palette(i);
        int px = (i % 8) * 16 + 4;
        int py = (i / 8) * 16;

        uint32_t *base_ptr = &linear_atlas[py * 128 + px];
        decode_tile_pal(&buffer[0], base_ptr, 128, pal);
        decode_tile_pal(&buffer[16], base_ptr + (8 * 128), 128, pal);

        if (i != 5)
        {
            fill_internal_holes(base_ptr, 8, 16, 128);
        }
    }

    // =========================================================================
    // 2. Ítems de Intercambio (16x16 px) -> Slots 24 a 37
    // =========================================================================
    for (int i = 0; i < 14; i++)
    {
        fseek(f, ROM_TRADE_ITEMS_BASE + (i * 64), SEEK_SET);
        if (fread(buffer, 1, 64, f) != 64)
            break;

        int slot = 24 + i;
        const uint32_t *pal = get_equip_item_palette(slot);
        int px = (slot % 8) * 16;
        int py = (slot / 8) * 16;

        uint32_t *base_ptr = &linear_atlas[py * 128 + px];
        decode_tile_pal(&buffer[0], base_ptr, 128, pal);
        decode_tile_pal(&buffer[16], base_ptr + (8 * 128), 128, pal);
        decode_tile_pal(&buffer[32], base_ptr + 8, 128, pal);
        decode_tile_pal(&buffer[48], base_ptr + (8 * 128) + 8, 128, pal);

        if (slot != 34)
        {
            fill_internal_holes(base_ptr, 16, 16, 128);
        }
    }

    // =========================================================================
    // 3. Los 8 Instrumentos de las Sirenas (16x16 px) -> Slots 38 a 45
    // =========================================================================
    for (int i = 0; i < 8; i++)
    {
        fseek(f, ROM_INSTRUMENTS_BASE + (i * 64), SEEK_SET);
        if (fread(buffer, 1, 64, f) != 64)
            break;

        const uint32_t *pal = s_instrument_palettes[i];

        int slot = 38 + i;
        int px = (slot % 8) * 16;
        int py = (slot / 8) * 16;

        uint32_t *base_ptr = &linear_atlas[py * 128 + px];
        decode_tile_pal(&buffer[0], base_ptr, 128, pal);
        decode_tile_pal(&buffer[16], base_ptr + (8 * 128), 128, pal);
        decode_tile_pal(&buffer[32], base_ptr + 8, 128, pal);
        decode_tile_pal(&buffer[48], base_ptr + (8 * 128) + 8, 128, pal);
    }

    // =========================================================================
    // 4. Las 5 Llaves de Mazmorra (8x16 px) -> Slots 46 a 50
    // =========================================================================
    const uint32_t key_rom_offsets[5] = {
        ROM_KEY_TAIL_BASE, ROM_KEY_SLIME_BASE, ROM_KEY_ANGLER_BASE, ROM_KEY_FACE_BASE, ROM_KEY_BIRD_BASE};
    for (int i = 0; i < 5; i++)
    {
        fseek(f, key_rom_offsets[i], SEEK_SET);
        if (fread(buffer, 1, 32, f) != 32)
            break;

        int slot = 46 + i;
        int px = (slot % 8) * 16 + 4;
        int py = (slot / 8) * 16;

        uint32_t *base_ptr = &linear_atlas[py * 128 + px];
        decode_tile_pal(&buffer[0], base_ptr, 128, s_pal_gold_wood);
        decode_tile_pal(&buffer[16], base_ptr + (8 * 128), 128, s_pal_gold_wood);
    }

    // =========================================================================
    // 5. Hongo (Slot 51) y Rupia del Menú (Slot 52)
    // =========================================================================
    // Slot 51: Hongo (8x16 px) - Rojo
    fseek(f, ROM_TOADSTOOL_BASE, SEEK_SET);
    if (fread(buffer, 1, 32, f) == 32)
    {
        int px = (51 % 8) * 16 + 4;
        int py = (51 / 8) * 16;
        uint32_t *base_ptr = &linear_atlas[py * 128 + px];
        decode_tile_pal(&buffer[0], base_ptr, 128, s_pal_red_fire);
        decode_tile_pal(&buffer[16], base_ptr + (8 * 128), 128, s_pal_red_fire);
    }

    {
        // Slot 52: Rupia del Menú (8x8 px) - Verde
        int px = (52 % 8) * 16;
        int py = (52 / 8) * 16;
        for (int y = 0; y < 8; y++)
        {
            for (int x = 0; x < 8; x++)
            {
                uint8_t idx = s_sprite_rupee[y][x];
                linear_atlas[(py + y) * 128 + (px + x)] = s_pal_tunic_green[idx];
            }
        }
    }

    // =========================================================================
    // 6. Piezas de Corazón (16x16 px ensambladas) -> Slots 53 a 56
    // =========================================================================
    uint8_t raw_heart_empty[32], raw_heart_q1[32], raw_heart_q2[32];
    fseek(f, ROM_HEART_PIECE_EMPTY_BASE, SEEK_SET);
    fread(raw_heart_empty, 1, 32, f);
    fseek(f, ROM_HEART_PIECE_Q1_BASE, SEEK_SET);
    fread(raw_heart_q1, 1, 32, f);
    fseek(f, ROM_HEART_PIECE_Q2_BASE, SEEK_SET);
    fread(raw_heart_q2, 1, 32, f);

    assemble_heart_slot(linear_atlas, 53, raw_heart_empty, raw_heart_empty); // 0/4
    assemble_heart_slot(linear_atlas, 54, raw_heart_q1, raw_heart_empty);    // 1/4
    assemble_heart_slot(linear_atlas, 55, raw_heart_q2, raw_heart_empty);    // 2/4
    assemble_heart_slot(linear_atlas, 56, raw_heart_q2, raw_heart_q1);       // 3/4

    // =========================================================================
    // 7. Túnicas -> Slots 57 a 59
    // =========================================================================
    // Slot 57: Túnica Verde (16x16 px)
    {
        int px = (57 % 8) * 16;
        int py = (57 / 8) * 16;
        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 16; x++)
            {
                uint8_t idx = s_sprite_tunic[y][x];
                linear_atlas[(py + y) * 128 + (px + x)] = s_pal_tunic_green[idx];
            }
        }
    }

    // Slot 58: Túnica Azul (16x16 px)
    {
        int px = (58 % 8) * 16;
        int py = (58 / 8) * 16;
        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 16; x++)
            {
                uint8_t idx = s_sprite_tunic[y][x];
                linear_atlas[(py + y) * 128 + (px + x)] = s_pal_tunic_blue[idx];
            }
        }
    }

    // Slot 59: Túnica Roja (16x16 px)
    {
        int px = (59 % 8) * 16;
        int py = (59 / 8) * 16;
        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 16; x++)
            {
                uint8_t idx = s_sprite_tunic[y][x];
                linear_atlas[(py + y) * 128 + (px + x)] = s_pal_tunic_red[idx];
            }
        }
    }

    // =========================================================================
    // 8. Álbum (Slot 60) y Yoshi (Slot 61)
    // =========================================================================
    // Slot 60: Álbum de Fotos (16x16 px)
    {
        int px = (60 % 8) * 16;
        int py = (60 / 8) * 16;
        for (int y = 0; y < 16; y++)
        {
            for (int x = 0; x < 16; x++)
            {
                uint8_t idx = s_sprite_album[y][x];
                linear_atlas[(py + y) * 128 + (px + x)] = s_pal_photo_frame[idx];
            }
        }
    }

    // Slot 61: Yoshi (16x16 px = 64 bytes)
    {
        fseek(f, ROM_YOSHI_TRADE_ITEM, SEEK_SET);
        if (fread(buffer, 1, 64, f) == 64)
        {
            int slot = 61;
            int px = (slot % 8) * 16;
            int py = (slot / 8) * 16;
            uint32_t *base_ptr = &linear_atlas[py * 128 + px];
            decode_tile_pal(&buffer[0], base_ptr, 128, s_pal_green);
            decode_tile_pal(&buffer[16], base_ptr + (8 * 128), 128, s_pal_green);
            decode_tile_pal(&buffer[32], base_ptr + 8, 128, s_pal_green);
            decode_tile_pal(&buffer[48], base_ptr + (8 * 128) + 8, 128, s_pal_green);

            // Rellena la panza y mejillas de Yoshi (ancho 16, alto 16)
            fill_internal_holes(base_ptr, 16, 16, 128);
        }
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