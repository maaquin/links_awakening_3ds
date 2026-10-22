#include "inventory_menu.h"
#include "ui_textures.h"
#include "gbrt.h"
#include "platform_sdl.h"

#include <citro3d.h>
#include <stdint.h>
#include <3ds.h>
#include <stdio.h>
#include <string.h>

uint8_t gb_read8(GBContext *ctx, uint16_t addr);
void gb_write8(GBContext *ctx, uint16_t addr, uint8_t value);
void debug_printf(float x, float y, const char *fmt, ...);
void debug_push_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void debug_printf_ex(float x, float y, float scale, float r, float g, float b, const char *fmt, ...);
void debug_flush_rects(int start_offset);
int debug_get_vbo_offset(void);

#define WRAM_EQUIP_SLOT_B 0xDB00
#define WRAM_EQUIP_SLOT_A 0xDB01
#define WRAM_INV_ITEMS_START 0xDB02
#define WRAM_EQUIP_SLOT_X 0xDC10
#define WRAM_EQUIP_SLOT_Y 0xDC11
#define WRAM_SHIELD_LEVEL 0xDB44
#define WRAM_SWORD_LEVEL 0xDB4E
#define WRAM_MAX_HEARTS 0xDB5B

// items del inventario
#define ITEM_SWORD 0x01
#define ITEM_BOMBS 0x02
#define ITEM_BRACELET 0x03
#define ITEM_SHIELD 0x04
#define ITEM_BOW 0x05
#define ITEM_HOOKSHOT 0x06
#define ITEM_ROD 0x07
#define ITEM_BOOTS 0x08
#define ITEM_OCARINA 0x09
#define ITEM_FEATHER 0x0A
#define ITEM_SHOVEL 0x0B
#define ITEM_POWDER 0x0C
#define ITEM_BUMERANG 0x0D

// contador de items
#define WRAM_BOMBS_COUNT 0xDB4D
#define WRAM_ARROWS_COUNT 0xDB45
#define WRAM_POWDER_COUNT 0x1B4C
#define WRAM_SEASHELLS_COUNT 0x1B0F

// información para equipo
#define WRAM_FLIPPERS 0xDB0C   // 0x01 = Aletas obtenidas
#define WRAM_TRADE_ITEM 0xDB0E // Objeto de intercambio activo (0x01 - 0x0E)
#define WRAM_RUPEES_H 0xDB5D   // Rupias (centenas)
#define WRAM_RUPEES_L 0xDB5E   // Rupias (decenas y unidades)
#define WRAM_PHOTOS_1 0xDC0C   // Fotos 1 a 8 (bits 0..7)
#define WRAM_PHOTOS_2 0xDC0D   // Fotos 9 a 12 (bits 0..3)
#define WRAM_TUNIC_COLOR 0xDB6F
#define WRAM_HEART_PIECES 0xDB5C
#define WRAM_MEDICINE 0xDB0D
#define WRAM_SEASHELLS 0x1B4F
#define WRAM_GOLDEN_LEAVES 0xDB15
#define WRAM_INSTRUMENTS_START 0xDB65

// llaves de mazmorra
#define WRAM_KEY_TAIL 0xDB11
#define WRAM_KEY_ANGLER 0xDB12
#define WRAM_KEY_FACE 0xDB13
#define WRAM_KEY_BIRD 0xDB14

// ocarina y canciones
#define WRAM_OCARINA_SONGS 0xDB49    // Máscara de bits: bit 0 = Marin, bit 1 = Manbo, bit 2 = Mamu
#define WRAM_OCARINA_SELECTED 0xDB4A // Índice activo: 0, 1 o 2

typedef struct
{
    bool open;
    float x, y, w, h;
} OcarinaPopup;

static OcarinaPopup g_ocarina_popup = {0};

// Colas reales de audio en HRAM (0xFFF2 - 0xFF80 = 0x72)
#define HRAM_SFX_JINGLE 0x72 // 0xFFF2: Cursores de menú y confirmaciones
#define HRAM_SFX_WAVE 0x73   // 0xFFF3: Efectos melódicos / salto
#define HRAM_SFX_NOISE 0x74  // 0xFFF4: Efectos físicos / golpes

bool hotswap_is_active(void);
uint8_t hotswap_get_backup_b(void);

typedef enum
{
    TAB_ITEMS = 0,
    TAB_QUEST,
    TAB_MAP,
    TAB_COUNT
} MenuTab;

typedef struct
{
    float x, y, w, h;
    int slot_idx;
} InventorySlotUI;

typedef struct
{
    bool active;
    uint8_t item_id;
    int source_slot;
    float cur_x, cur_y;
} DragState;

static MenuTab g_active_tab = TAB_ITEMS;
static MenuTab g_prev_tab = TAB_ITEMS;
static InventorySlotUI g_slots[10];
static InventorySlotUI g_slot_equip_b;
static InventorySlotUI g_slot_equip_a;
static InventorySlotUI g_slot_equip_x;
static InventorySlotUI g_slot_equip_y;
static DragState g_drag = {0};

static const char *s_instrument_names[8] = {
    "1.VIOLONCHELO", "2.CUERNO OLIFANTE", "3.CAMPANA", "4.ARPA",
    "5.MARIMBA", "6.TRIANGULO", "7.ORGANO", "8.TAMBOR"};

static bool is_point_inside(float px, float py, const InventorySlotUI *slot)
{
    return (px >= slot->x && px <= (slot->x + slot->w) &&
            py >= slot->y && py <= (slot->y + slot->h));
}

void inventory_menu_toggle_map(void)
{
    if (g_active_tab == TAB_MAP)
        g_active_tab = g_prev_tab;
    else
    {
        g_prev_tab = g_active_tab;
        g_active_tab = TAB_MAP;
    }
}

static void draw_item_ammo(GBContext *ctx, uint8_t item_id, float x, float y)
{
    if (!ctx)
        return;

    uint8_t count = 0;
    bool has_ammo = false;

    if (item_id == ITEM_BOMBS)
    {
        count = gb_read8(ctx, WRAM_BOMBS_COUNT);
        has_ammo = true;
    }
    else if (item_id == ITEM_BOW)
    {
        count = gb_read8(ctx, WRAM_ARROWS_COUNT);
        has_ammo = true;
    }
    else if (item_id == ITEM_POWDER)
    {
        count = ctx->wram[WRAM_POWDER_COUNT];
        has_ammo = true;
    }

    if (has_ammo)
    {
        // En Link's Awakening los contadores a veces usan codificación BCD (Binary Coded Decimal)
        // Si el valor viene en BCD desempacado, lo ajustamos; si es hex directo lo mostramos tal cual:
        int display_val = ((count >> 4) * 10) + (count & 0x0F);

        // Texto dorado pequeño en la esquina inferior derecha del recuadro
        debug_printf_ex(x + 18.0f, y + 20.0f, 0.8f, 0.85f, 0.80f, 0.53f, "%02d", display_val);
    }
}

static void draw_atlas_slot(int slot, float x, float y, float size)
{
    if (slot < 0 || slot >= 256)
        return;

    int col = slot % 16;
    int row = slot / 16;

    float src_x = col * 16.0f;
    float src_y = row * 16.0f;

    ui_draw_sub_sprite(&g_tex_items, x, y, size, size, src_x, src_y, 16.0f, 16.0f);
}

void inventory_menu_init(void)
{
    float start_x = 40.5f;
    float start_y = 70.0f;
    float slot_w = 47.8f; // 239 / 5
    float slot_h = 39.5f; // 79 / 2

    for (int row = 0; row < 2; row++)
    {
        for (int col = 0; col < 5; col++)
        {
            int idx = row * 5 + col;
            g_slots[idx].x = start_x + col * slot_w;
            g_slots[idx].y = start_y + row * slot_h;
            g_slots[idx].w = slot_w;
            g_slots[idx].h = slot_h;
            g_slots[idx].slot_idx = idx;
        }
    }

    // Botones X, Y, B y A en la zona inferior
    g_slot_equip_b.x = 40.0f;
    g_slot_equip_b.y = 170.0f;
    g_slot_equip_b.w = 62.0f; // width
    g_slot_equip_b.h = 62.0f; // height
    g_slot_equip_b.slot_idx = -1;

    g_slot_equip_a.x = 112.0f;
    g_slot_equip_a.y = 170.0f;
    g_slot_equip_a.w = 62.0f;
    g_slot_equip_a.h = 62.0f;
    g_slot_equip_a.slot_idx = -2;

    g_slot_equip_x.x = 184.0f;
    g_slot_equip_x.y = 170.0f;
    g_slot_equip_x.w = 62.0f; // width
    g_slot_equip_x.h = 62.0f; // height
    g_slot_equip_x.slot_idx = -3;

    g_slot_equip_y.x = 256.0f;
    g_slot_equip_y.y = 170.0f;
    g_slot_equip_y.w = 62.0f;
    g_slot_equip_y.h = 62.0f;
    g_slot_equip_y.slot_idx = -4;
}

static const int s_wram_id_to_atlas_index[16] = {
    [0x00] = -1,          // Vacío
    [ITEM_BOMBS] = 1,     // 0x01 -> bomba (índice 2)
    [ITEM_BRACELET] = 2,  // 0x02 -> brazalete (índice 0)
    [ITEM_SWORD] = 3,     // 0x03 -> espada (índice 1)
    [ITEM_SHIELD] = 4,    // 0x04 -> escudo (índice 3)
    [ITEM_BOW] = 5,       // 0x05 -> arco (índice 4)
    [ITEM_HOOKSHOT] = 6,  // 0x06 -> gancho (índice 5)
    [ITEM_ROD] = 7,       // 0x07 -> varita (índice 6)
    [ITEM_POWDER] = 8,    // 0x08 -> polvos mágicos (índice 12)
    [ITEM_OCARINA] = 9,   // 0x09 -> ocarina (índice 8)
    [ITEM_FEATHER] = 10,  // 0x0A -> pluma (índice 9)
    [ITEM_SHOVEL] = 12,   // 0x0B -> pala (índice 11)
    [ITEM_BOOTS] = 13,    // 0x0C -> botas (índice 12)
    [ITEM_BUMERANG] = 17, // Boomerang (índice 17)
    [0x0E] = 16,          // Poción (índice 16)
};

static void draw_item_icon(uint8_t item_id, float x, float y, float size)
{
    if (item_id == 0x00 || item_id >= 16)
        return;

    int atlas_idx = s_wram_id_to_atlas_index[item_id];
    if (atlas_idx < 0)
        return;

    // Coordenadas en la textura atlas de 256x256 (16 columnas x 16px)
    int col = atlas_idx % 16;
    int row = atlas_idx / 16;

    float src_x = col * 16.0f;
    float src_y = row * 16.0f;

    ui_draw_sub_sprite(&g_tex_items, x, y, size, size, src_x, src_y, 16.0f, 16.0f);
}

static void write_to_source_slot(GBContext *ctx, int source_slot, uint8_t value)
{
    if (source_slot >= 0)
    {
        // Origen fue una de las 10 casillas de la grilla
        gb_write8(ctx, WRAM_INV_ITEMS_START + source_slot, value);
    }
    else if (source_slot == -1)
    {
        // Origen fue el botón B
        gb_write8(ctx, WRAM_EQUIP_SLOT_B, value);
    }
    else if (source_slot == -2)
    {
        // Origen fue el botón A
        gb_write8(ctx, WRAM_EQUIP_SLOT_A, value);
    }
    else if (source_slot == -3)
    {
        // Origen fue el botón X
        gb_write8(ctx, WRAM_EQUIP_SLOT_X, value);
    }
    else if (source_slot == -4)
    {
        // Origen fue el botón Y
        gb_write8(ctx, WRAM_EQUIP_SLOT_Y, value);
    }
}

static int s_inspect_x = 0; // Columna (0 a 15)
static int s_inspect_y = 0; // Fila (0 a 7)

// =============================================================================
// SISTEMA DE BANDERAS PARA HABITACIONES DE MAZMORRA (1 byte por celda)
// =============================================================================
enum DungeonCellFlags
{
    R_EMPTY = 0x00, // No hay habitación en esta celda (vacío)
    R_ROOM = 0x01,  // Existe habitación (incluso si no tiene puertas visibles)

    // Direcciones de puertas / pasillos abiertos
    R_N = 0x02, // Puerta al Norte (Arriba)
    R_S = 0x04, // Puerta al Sur   (Abajo)
    R_E = 0x08, // Puerta al Este  (Derecha)
    R_W = 0x10, // Puerta al Oeste (Izquierda)

    // Marcadores para cuando Link tiene la Brújula
    M_CHEST = 0x20, // Cofre en esta sala
    M_BOSS = 0x40,  // Jefe (Calavera)
    M_GOAL = 0x80,  // Cámara del Instrumento (Rombo blanco)

    // Diccionario de las 16 combinaciones posibles de puertas
    DRM_NONE = R_ROOM,
    DRM_N = R_ROOM | R_N,
    DRM_S = R_ROOM | R_S,
    DRM_E = R_ROOM | R_E,
    DRM_W = R_ROOM | R_W,
    DRM_NS = R_ROOM | R_N | R_S,
    DRM_NE = R_ROOM | R_N | R_E,
    DRM_NW = R_ROOM | R_N | R_W,
    DRM_SE = R_ROOM | R_S | R_E,
    DRM_SW = R_ROOM | R_S | R_W,
    DRM_EW = R_ROOM | R_E | R_W,
    DRM_NSE = R_ROOM | R_N | R_S | R_E,
    DRM_NSW = R_ROOM | R_N | R_S | R_W,
    DRM_NEW = R_ROOM | R_N | R_E | R_W,
    DRM_SEW = R_ROOM | R_S | R_E | R_W,
    DRM_NSEW = R_ROOM | R_N | R_S | R_E | R_W
};

static const uint8_t s_dungeon_maps[9][8][8] = {
    // -------------------------------------------------------------------------
    // [0] LEVEL 1 ~ TAIL CAVE
    // -------------------------------------------------------------------------
    {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, DRM_S | M_GOAL},
        {0, 0, DRM_E, DRM_SEW, DRM_W, 0, 0, DRM_NS | M_BOSS},
        {0, DRM_S | M_CHEST, 0, DRM_NSE, DRM_SEW | M_CHEST, DRM_SW, DRM_S, DRM_NS},
        {0, DRM_NS, DRM_E | M_CHEST, DRM_NSEW | M_CHEST, DRM_NEW | M_CHEST, DRM_NSEW, DRM_NEW, DRM_NW},
        {0, DRM_N, 0, DRM_NE, DRM_SEW | M_CHEST, DRM_NW | M_CHEST, 0, 0},
        {0, 0, DRM_E | M_CHEST, DRM_EW, DRM_NSW, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0}},

    // -------------------------------------------------------------------------
    // [1] LEVEL 2 ~ BOTTLE GROTTO
    // -------------------------------------------------------------------------
    {
        {0, DRM_E | M_CHEST, DRM_SEW | M_CHEST, DRM_EW | M_CHEST, DRM_EW, DRM_SEW, DRM_W, 0},
        {0, 0, DRM_NS | M_CHEST, 0, 0, DRM_N | M_CHEST, 0, 0},
        {0, DRM_SE, DRM_NW, 0, 0, DRM_E | M_GOAL, DRM_SW | M_BOSS, 0},
        {0, DRM_N, 0, 0, 0, 0, DRM_N, 0},
        {0, DRM_S | M_CHEST, 0, 0, 0, 0, DRM_S, 0},
        {0, DRM_NE, DRM_SEW, DRM_SEW, DRM_SEW, DRM_SEW, DRM_NW, 0},
        {0, 0, DRM_NS, DRM_N | M_CHEST, DRM_NE | M_CHEST, DRM_NW | M_CHEST, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0}},

    // -------------------------------------------------------------------------
    // [2] LEVEL 3 ~ KEY CAVERN
    // -------------------------------------------------------------------------
    {
        {DRM_SE, DRM_EW, DRM_EW | M_CHEST, DRM_SW, 0, 0, 0, 0},
        {DRM_NS, DRM_SE, DRM_SEW | M_CHEST, DRM_NW | M_CHEST, 0, 0, 0, 0},
        {DRM_NE, DRM_NEW, DRM_NSEW, DRM_W, 0, 0, DRM_S, 0},
        {0, DRM_S | M_CHEST, DRM_N, 0, 0, DRM_E, DRM_NSEW, DRM_W},
        {0, DRM_NS, 0, 0, 0, 0, DRM_N, 0},
        {0, DRM_NSE | M_CHEST, DRM_W | M_CHEST, 0, 0, 0, DRM_S | M_GOAL, 0},
        {0, DRM_NS | M_CHEST, 0, 0, 0, 0, DRM_NS | M_BOSS, 0},
        {0, DRM_NE, DRM_W | M_CHEST, 0, 0, 0, DRM_NE, DRM_W}},

    // -------------------------------------------------------------------------
    // [3] LEVEL 4 ~ ANGLER'S TUNNEL
    // -------------------------------------------------------------------------
    {
        {0, 0, 0, DRM_SE | M_CHEST, DRM_SW, 0, 0, 0},
        {0, DRM_S | M_GOAL, 0, DRM_NS, DRM_NS, 0, DRM_S | M_CHEST, 0},
        {0, DRM_NS | M_BOSS, DRM_SE, DRM_NEW | M_CHEST, DRM_NSEW, DRM_SW | M_CHEST, DRM_NS, 0},
        {0, DRM_NE, DRM_NSW | M_CHEST, DRM_SE | M_CHEST, DRM_NSEW, DRM_NSEW, DRM_NW | M_CHEST, 0},
        {0, 0, DRM_NSE, DRM_NSEW, DRM_NSW, DRM_NS | M_CHEST, 0, 0},
        {0, 0, DRM_NE | M_CHEST, DRM_NSEW, DRM_NSEW | M_CHEST, DRM_NW | M_CHEST, 0, 0},
        {0, 0, 0, DRM_NS, DRM_N | M_CHEST, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0}},

    // -------------------------------------------------------------------------
    // [4] LEVEL 5 ~ CATFISH'S MAW
    // -------------------------------------------------------------------------
    {
        {DRM_E, DRM_W, DRM_S | M_GOAL, DRM_E | M_CHEST, DRM_SW, 0, 0, 0},
        {0, 0, DRM_NS | M_BOSS, DRM_NONE | M_CHEST, DRM_NSE, DRM_SW | M_CHEST, 0, 0},
        {DRM_E, DRM_EW, DRM_NEW, DRM_W, DRM_NS, DRM_NSE | M_CHEST, DRM_W | M_CHEST, 0},
        {0, 0, 0, 0, DRM_NE, DRM_NSEW, DRM_W, 0},
        {0, 0, DRM_E, DRM_SW, DRM_SE, DRM_NW, 0, 0},
        {0, DRM_E | M_CHEST, DRM_EW, DRM_NSW, DRM_NS, 0, 0, 0},
        {0, 0, DRM_E | M_CHEST, DRM_NEW, DRM_NSW, 0, 0, 0},
        {0, 0, 0, DRM_E | M_CHEST, DRM_NEW, DRM_EW | M_CHEST, DRM_SW, 0}},

    // -------------------------------------------------------------------------
    // [5] LEVEL 6 ~ FACE SHRINE
    // -------------------------------------------------------------------------
    {
        {DRM_S | M_CHEST, 0, 0, 0, 0, 0, 0, DRM_S | M_CHEST},
        {DRM_NSE, DRM_SW | M_CHEST, 0, DRM_S, DRM_S | M_GOAL, 0, DRM_S | M_CHEST, DRM_NS},
        {DRM_NSE, DRM_NSEW | M_CHEST, DRM_EW, DRM_NSW, DRM_NS | M_BOSS, DRM_NS, DRM_NS | M_CHEST, DRM_NS},
        {DRM_NE | M_CHEST, DRM_NSEW, 0, DRM_NS, DRM_NS, 0, DRM_NSE, DRM_NW},
        {0, DRM_NE, DRM_SW | M_CHEST, DRM_NE, DRM_NW, DRM_NS, DRM_NS, 0},
        {0, DRM_S | M_CHEST, DRM_NS | M_CHEST, 0, 0, DRM_NSE, DRM_NSW | M_CHEST, 0},
        {0, DRM_N, DRM_NE, DRM_SEW, DRM_EW, DRM_NW, DRM_N, 0},
        {0, 0, 0, 0, 0, 0, 0, 0}},

    // -------------------------------------------------------------------------
    // [6] LEVEL 7 ~ EAGLE'S TOWER
    // -------------------------------------------------------------------------
    {
        {0, DRM_S | M_CHEST, DRM_S | M_CHEST, 0, 0, 0, 0, 0},
        {DRM_SE, DRM_NSEW, DRM_NSEW, DRM_SW, 0, 0, 0, 0},
        {DRM_NSE, DRM_NSW, DRM_NSE, DRM_NSW | M_CHEST, 0, 0, 0, 0},
        {DRM_NE, DRM_NEW | M_CHEST, DRM_NEW, DRM_NW, 0, 0, 0, 0},
        {DRM_SE | M_CHEST, DRM_SEW, DRM_EW, DRM_SW | M_CHEST, 0, DRM_SE, DRM_W | M_CHEST, 0},
        {DRM_NSE, DRM_NSEW, DRM_SEW, DRM_NSW, DRM_SE, DRM_NSW, DRM_S | M_GOAL, DRM_S | M_CHEST},
        {DRM_NSE | M_CHEST, DRM_NEW, DRM_NSEW, DRM_NSW, DRM_NE, DRM_NSEW, DRM_NEW | M_BOSS, DRM_NW},
        {DRM_NE, DRM_SEW, DRM_NEW, DRM_NW, 0, DRM_NE, DRM_W, 0}},

    // -------------------------------------------------------------------------
    // [7] LEVEL 8 ~ TURTLE ROCK
    // -------------------------------------------------------------------------
    {
        {0, 0, 0, DRM_S | M_GOAL, DRM_S, 0, 0, 0},
        {DRM_S | M_CHEST, 0, 0, DRM_NS | M_BOSS, DRM_NS | M_CHEST, 0, 0, DRM_S | M_CHEST},
        {DRM_NE, DRM_EW, DRM_SEW | M_CHEST, DRM_NEW, DRM_NEW, DRM_EW | M_CHEST, DRM_SW, DRM_N},
        {0, DRM_SE | M_CHEST, DRM_NSEW, DRM_EW, DRM_EW, DRM_EW, DRM_NSW, 0},
        {0, DRM_NS | M_CHEST, DRM_NE, DRM_EW, DRM_SEW, DRM_SEW, DRM_NW, 0},
        {DRM_S, DRM_NSE | M_CHEST, DRM_SW, DRM_SE | M_CHEST, DRM_NSEW, DRM_NSEW, DRM_SW, DRM_S},
        {DRM_NSE, DRM_NW | M_CHEST, DRM_NE, DRM_NSEW, DRM_NSEW, DRM_NEW | M_CHEST, DRM_NEW, DRM_NSW},
        {DRM_N | M_CHEST, 0, 0, DRM_NS, DRM_N, 0, 0, DRM_N | M_CHEST}},

    // -------------------------------------------------------------------------
    // [8] LEVEL 0 ~ COLOR DUNGEON
    // -------------------------------------------------------------------------
    {
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, DRM_SE | M_BOSS, DRM_W, 0, 0, DRM_E | M_CHEST, DRM_SW, 0},
        {0, DRM_NE, DRM_SW, DRM_SE | M_CHEST, DRM_W, DRM_SE, DRM_NW, 0},
        {0, 0, DRM_NE, DRM_NEW, DRM_SW, DRM_NS, 0, 0},
        {0, 0, DRM_SE, DRM_SEW | M_CHEST, DRM_NEW, DRM_NSW | M_CHEST, 0, 0},
        {0, 0, DRM_N, DRM_N, DRM_E | M_CHEST, DRM_NW, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0},
        {0, 0, 0, 0, 0, 0, 0, 0}}};

// Textura completa precompuesta en RAM
extern C3D_Tex g_tex_overworld_map;

bool is_in_dungeon(GBContext *ctx, uint8_t *out_dungeon_id)
{
    if (!ctx || !ctx->wram)
        return false;

    // 1. Debe estar en interiores (0x1BA5 != 0)
    uint8_t is_indoor = ctx->wram[0x1BA5];
    if (is_indoor == 0x00)
        return false;

    // 2. ID real de la mazmorra activa (0x00 a 0x07 = Niveles 1 a 8, 0xFF = Color)
    uint8_t dungeon_id = ctx->wram[0x1B60];
    if (dungeon_id > 0x07 && dungeon_id != 0xFF)
        return false; // No es una mazmorra válida

    // 3. Comprobar si es sala lateral 2D (wIsSideScrolling en 0x1402 o hIsSideScrolling)
    uint8_t is_side_scroll = ctx->wram[0x1402];
    uint8_t room = ctx->hram[0x76];

    // Si room >= 0x80 pero NO es una sección lateral 2D, es una casa/tienda/cueva exterior
    if (room >= 0x80 && !is_side_scroll)
        return false;

    if (out_dungeon_id)
        *out_dungeon_id = (dungeon_id == 0xFF) ? 8 : dungeon_id;

    return true;
}

static const char *s_dungeon_names[] = {
    "NIVEL 1: CUEVA TAIL",
    "NIVEL 2: GRUTA BOTELLA",
    "NIVEL 3: CAVERNA LLAVE",
    "NIVEL 4: TUNEL ANGLER",
    "NIVEL 5: FAUCES SILURO",
    "NIVEL 6: TEMPLO ROSTRO",
    "NIVEL 7: TORRE AGUILA",
    "NIVEL 8: ROCA TORTUGA",
    "MAZMORRA COLOR"};

void render_dungeon_map(GBContext *ctx, uint8_t dungeon_id)
{
    if (!ctx || !ctx->wram || dungeon_id > 8)
        return;

    const float map_size = 160.0f;
    const float start_x = 80.0f;
    const float start_y = 51.0f;
    const float cell_size = 20.0f; // 160 / 8 celdas

    // fondo azul oscuro
    int bg_offset = debug_get_vbo_offset();
    debug_push_rect(start_x, start_y, map_size, map_size, 0.09f, 0.12f, 0.28f, 1.0f);
    debug_flush_rects(bg_offset);

    // mapa y brújula
    uint16_t item_base = (dungeon_id < 8) ? (0x1B16 + (dungeon_id * 5)) : 0x1DDA;
    bool has_map = (ctx->wram[item_base + 0] != 0);
    bool has_compass = (ctx->wram[item_base + 1] != 0);

    if (!has_map)
    {
        // no hay mapa
        ui_draw_sprite(&g_tex_map_frame, start_x - 5.0f, start_y - 5.0f,
                       map_size + 11.0f, map_size + 11.0f, 188.0f, 188.0f);
        debug_printf(start_x + 36.0f, start_y + 76.0f, "NO HAY MAPA");
    }
    else
    {

        // mapa

        // 1. Dibujar habitaciones y pasillos sólidos
        int rooms_offset = debug_get_vbo_offset();
        const float cr = 0.95f, cg = 0.92f, cb = 0.65f;

        for (int row = 0; row < 8; row++)
        {
            for (int col = 0; col < 8; col++)
            {
                uint8_t cell = s_dungeon_maps[dungeon_id][row][col];
                if (!(cell & R_ROOM))
                    continue;

                float rx = start_x + (col * cell_size);
                float ry = start_y + (row * cell_size);

                debug_push_rect(rx + 3.0f, ry + 3.0f, 14.0f, 14.0f, cr, cg, cb, 1.0f);

                if (cell & R_N)
                    debug_push_rect(rx + 8.0f, ry + 0.0f, 4.0f, 3.0f, cr, cg, cb, 1.0f);
                if (cell & R_S)
                    debug_push_rect(rx + 8.0f, ry + 17.0f, 4.0f, 3.0f, cr, cg, cb, 1.0f);
                if (cell & R_W)
                    debug_push_rect(rx + 0.0f, ry + 8.0f, 3.0f, 4.0f, cr, cg, cb, 1.0f);
                if (cell & R_E)
                    debug_push_rect(rx + 17.0f, ry + 8.0f, 3.0f, 4.0f, cr, cg, cb, 1.0f);
            }
        }
        debug_flush_rects(rooms_offset);

        // 2. Dibujar íconos de la brújula con sprites del atlas (si se posee)
        if (has_compass)
        {
            for (int row = 0; row < 8; row++)
            {
                for (int col = 0; col < 8; col++)
                {
                    uint8_t cell = s_dungeon_maps[dungeon_id][row][col];
                    if (!(cell & R_ROOM))
                        continue;

                    float rx = start_x + (col * cell_size);
                    float ry = start_y + (row * cell_size);

                    // Cada ícono mide 8x8 px centrado dentro de su casilla de 16x16
                    if (cell & M_BOSS)
                        draw_atlas_slot(68, rx + 2.0f, ry + 2.0f, 16.0f);
                    else if (cell & M_GOAL)
                        draw_atlas_slot(69, rx + 2.0f, ry + 2.0f, 16.0f);
                    else if (cell & M_CHEST)
                        draw_atlas_slot(67, rx + 2.0f, ry + 2.0f, 16.0f);
                }
            }
        }

        // marco
        ui_draw_sprite(&g_tex_map_frame, start_x - 5.0f, start_y - 5.0f,
                       map_size + 11.0f, map_size + 11.0f, 188.0f, 188.0f);
    }

    // -------------------------------------------------------------------------
    // ítems de mazmorra
    // -------------------------------------------------------------------------
    const float right_x = 260.0f;
    const float left_x = 37.0;
    const float icon_sz = 30.0f;

    bool has_beak = (ctx->wram[item_base + 2] != 0);
    bool has_boss_key = (ctx->wram[item_base + 3] != 0);
    uint8_t small_keys = ctx->wram[item_base + 4];

    // pico de piedra
    if (has_beak)
    {
        draw_atlas_slot(63, left_x, 76.0f, icon_sz);
    }

    // brújula
    if (has_compass)
    {
        draw_atlas_slot(62, left_x, 113.0f, icon_sz); // 116 mitad exacta
    }

    // mapa
    if (has_map)
    {
        draw_atlas_slot(66, left_x, 156.0f, icon_sz);
    }

    // llave de jefe
    if (has_boss_key)
    {
        draw_atlas_slot(65, right_x, 141.0f, icon_sz);
    }

    // llaves pequeñas y su contador
    if (small_keys > 0)
    {
        draw_atlas_slot(64, right_x, 91.0f, icon_sz);
        debug_printf(right_x + 25.0f, 105.0f, "x%d", small_keys);
    }
}

void render_overworld_map(GBContext *ctx)
{
    if (!ctx || !ctx->wram)
        return;

    const float map_size = 160.0f;
    const float start_x = 80.0f;
    const float start_y = 51.0f;
    const float cell_size = 10.0f; // 160 / 16

    // koholint
    ui_draw_sub_sprite(&g_tex_overworld_map, start_x, start_y, map_size, map_size, 0.0f, 0.0f, 256.0f, 256.0f);

    // niebla
    C3D_TexBind(0, &g_tex_fog);
    int fog_start = debug_get_vbo_offset();

    const float png_w = 22.0f;
    const float png_h = 22.0f;
    float u0 = 0.0f;
    float v0 = 1.0f;
    float u1 = png_w / (float)g_tex_fog.width;
    float v1 = 1.0f - (png_h / (float)g_tex_fog.height);

    for (int r = 0; r < 256; r++)
    {
        uint8_t room_flags = ctx->wram[0x1800 + r];
        bool visited = (room_flags != 0);

        if (!visited)
        {
            int c_col = r % 16;
            int c_row = r / 16;
            float fx = start_x + (c_col * cell_size);
            float fy = start_y + (c_row * cell_size);

            debug_push_textured_rect(fx, fy, cell_size, cell_size, u0, v0, u1, v1);
        }
    }

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                   GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);

    debug_flush_textured_rects(fog_start);

    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD,
                   GPU_ONE, GPU_ZERO,
                   GPU_ONE, GPU_ZERO);

    // marco
    ui_draw_sprite(&g_tex_map_frame, start_x - 5.0f, start_y - 5.0f, map_size + 11.0f, map_size + 11.0f, 188.0f, 188.0f);

    // link
    uint8_t room = ctx->hram[0x76];
    uint8_t is_indoor = ctx->wram[0x1BA5];

    static int s_last_overworld_room = -1;
    if (is_indoor == 0x00)
    {
        s_last_overworld_room = room;
    }
    uint8_t display_room = (is_indoor == 0x00) ? room : s_last_overworld_room;

    if (s_last_overworld_room > 0)
    {

        int col = display_room % 16;
        int row = display_room / 16;

        float cell_x = start_x + (col * cell_size);
        float cell_y = start_y + (row * cell_size);

        static int frame_counter = 0;
        frame_counter++;

        if ((frame_counter / 30) % 2 == 0)
        {
            int marker_offset = debug_get_vbo_offset();
            float px = cell_x + 2.0f;
            float py = cell_y + 2.0f;

            debug_push_rect(px + 2.0f, py + 0.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            debug_push_rect(px + 1.0f, py + 1.0f, 5.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            debug_push_rect(px + 0.0f, py + 2.0f, 7.0f, 3.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            debug_push_rect(px + 1.0f, py + 5.0f, 5.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
            debug_push_rect(px + 2.0f, py + 6.0f, 3.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);

            const float yr = 1.0f, yg = 0.85f, yb = 0.10f;
            debug_push_rect(px + 2.0f, py + 1.0f, 3.0f, 1.0f, yr, yg, yb, 1.0f);
            debug_push_rect(px + 1.0f, py + 2.0f, 5.0f, 3.0f, yr, yg, yb, 1.0f);
            debug_push_rect(px + 2.0f, py + 5.0f, 3.0f, 1.0f, yr, yg, yb, 1.0f);

            debug_flush_rects(marker_offset);
        }
    }
}

static void play_menu_sfx(GBContext *ctx, uint8_t channel_offset, uint8_t sfx_id)
{
    if (!ctx || !ctx->hram)
        return;

    ctx->hram[channel_offset] = sfx_id;
}

void inventory_menu_update(GBContext *ctx)
{
    if (!ctx)
        return;

    u32 kDown = hidKeysDown();
    u32 kHeld = hidKeysHeld();
    u32 kUp = hidKeysUp();

    touchPosition touch;
    hidTouchRead(&touch);

    if (g_ocarina_popup.open)
    {
        if (kDown & KEY_TOUCH)
        {
            uint8_t songs = gb_read8(ctx, WRAM_OCARINA_SONGS);
            float pop_x = 75.0f;
            float pop_y = 90.0f;
            float btn_size = 36.0f;
            float spacing = 50.0f;

            // Comprobamos cada una de las 3 canciones
            static const uint8_t s_song_masks[3] = {0x04, 0x02, 0x01};

            bool touched_option = false;
            for (int s = 0; s < 3; s++)
            {
                if (songs & s_song_masks[s])
                {
                    float bx = pop_x + 15.0f + (s * spacing);
                    float by = pop_y + 12.0f;

                    if (touch.px >= bx && touch.px <= (bx + btn_size) &&
                        touch.py >= by && touch.py <= (by + btn_size))
                    {
                        gb_write8(ctx, WRAM_OCARINA_SELECTED, s);
                        touched_option = true;
                        play_menu_sfx(ctx, HRAM_SFX_WAVE, 0x1A); // Tono de confirmación
                        g_ocarina_popup.open = false;
                        break;
                    }
                }
            }

            // Si tocó en cualquier otra parte fuera o dentro de la ventana, cerrar el popup
            if (!touched_option)
            {
                play_menu_sfx(ctx, HRAM_SFX_JINGLE, 0x08);
                g_ocarina_popup.open = false;
            }
            return; // Consumimos el input para que no toque nada del fondo
        }
        return;
    }

    if (kDown & KEY_L)
    {
        g_active_tab = (g_active_tab + TAB_COUNT - 1) % TAB_COUNT;
        play_menu_sfx(ctx, HRAM_SFX_WAVE, 0x06);
    }
    if (kDown & KEY_R)
    {
        g_active_tab = (g_active_tab + 1) % TAB_COUNT;
        play_menu_sfx(ctx, HRAM_SFX_WAVE, 0x06);
    }

    if (kDown & KEY_TOUCH)
    {
        if (touch.py < 30.0f)
        {
            if (touch.px < 110.0f)
                g_active_tab = TAB_ITEMS;
            else if (touch.px < 210.0f)
                g_active_tab = TAB_QUEST;
            else
                g_active_tab = TAB_MAP;
            play_menu_sfx(ctx, HRAM_SFX_WAVE, 0x06);
            return;
        }
    }

    if (g_active_tab == TAB_ITEMS)
    {
        if (kDown & KEY_TOUCH)
        {
            for (int i = 0; i < 10; i++)
            {
                if (is_point_inside(touch.px, touch.py, &g_slots[i]))
                {
                    uint8_t item = gb_read8(ctx, WRAM_INV_ITEMS_START + i);
                    if (item != 0x00)
                    {
                        g_drag.active = true;
                        g_drag.item_id = item;
                        g_drag.source_slot = i;
                        g_drag.cur_x = touch.px;
                        g_drag.cur_y = touch.py;
                    }
                    return;
                }
            }

            if (is_point_inside(touch.px, touch.py, &g_slot_equip_b))
            {
                uint8_t item = gb_read8(ctx, WRAM_EQUIP_SLOT_B);
                if (item != 0x00)
                {
                    g_drag.active = true;
                    g_drag.item_id = item;
                    g_drag.source_slot = -1;
                    g_drag.cur_x = touch.px;
                    g_drag.cur_y = touch.py;
                }
                return;
            }

            if (is_point_inside(touch.px, touch.py, &g_slot_equip_a))
            {
                uint8_t item = gb_read8(ctx, WRAM_EQUIP_SLOT_A);
                if (item != 0x00)
                {
                    g_drag.active = true;
                    g_drag.item_id = item;
                    g_drag.source_slot = -2;
                    g_drag.cur_x = touch.px;
                    g_drag.cur_y = touch.py;
                }
                return;
            }

            if (is_point_inside(touch.px, touch.py, &g_slot_equip_x))
            {
                uint8_t item = gb_read8(ctx, WRAM_EQUIP_SLOT_X);
                if (item != 0x00)
                {
                    g_drag.active = true;
                    g_drag.item_id = item;
                    g_drag.source_slot = -3;
                    g_drag.cur_x = touch.px;
                    g_drag.cur_y = touch.py;
                }
                return;
            }

            if (is_point_inside(touch.px, touch.py, &g_slot_equip_y))
            {
                uint8_t item = gb_read8(ctx, WRAM_EQUIP_SLOT_Y);
                if (item != 0x00)
                {
                    g_drag.active = true;
                    g_drag.item_id = item;
                    g_drag.source_slot = -4;
                    g_drag.cur_x = touch.px;
                    g_drag.cur_y = touch.py;
                }
                return;
            }
        }

        if (g_drag.active && (kHeld & KEY_TOUCH))
        {
            if (touch.px > 0 || touch.py > 0)
            {
                g_drag.cur_x = touch.px;
                g_drag.cur_y = touch.py;
            }
        }

        if (g_drag.active && (kUp & KEY_TOUCH))
        {
            float drop_x = g_drag.cur_x;
            float drop_y = g_drag.cur_y;

            // Si es la ocarina y apenas se movió de la celda de origen, abrir popup
            if (g_drag.item_id == ITEM_OCARINA)
            {
                // Si la soltó dentro de la misma casilla de donde la tomó
                bool dropped_in_origin = false;
                if (g_drag.source_slot >= 0 && is_point_inside(drop_x, drop_y, &g_slots[g_drag.source_slot]))
                    dropped_in_origin = true;
                else if (g_drag.source_slot == -1 && is_point_inside(drop_x, drop_y, &g_slot_equip_b))
                    dropped_in_origin = true;
                else if (g_drag.source_slot == -2 && is_point_inside(drop_x, drop_y, &g_slot_equip_a))
                    dropped_in_origin = true;
                else if (g_drag.source_slot == -3 && is_point_inside(drop_x, drop_y, &g_slot_equip_x))
                    dropped_in_origin = true;
                else if (g_drag.source_slot == -4 && is_point_inside(drop_x, drop_y, &g_slot_equip_y))
                    dropped_in_origin = true;

                if (dropped_in_origin)
                {
                    g_drag.active = false;
                    g_ocarina_popup.open = true;
                    return;
                }
            }

            uint8_t dragged_item = g_drag.item_id;

            if (is_point_inside(drop_x, drop_y, &g_slot_equip_b))
            {
                uint8_t old_b = gb_read8(ctx, WRAM_EQUIP_SLOT_B);
                gb_write8(ctx, WRAM_EQUIP_SLOT_B, dragged_item);
                write_to_source_slot(ctx, g_drag.source_slot, old_b);
            }
            else if (is_point_inside(drop_x, drop_y, &g_slot_equip_a))
            {
                uint8_t old_a = gb_read8(ctx, WRAM_EQUIP_SLOT_A);
                gb_write8(ctx, WRAM_EQUIP_SLOT_A, dragged_item);
                write_to_source_slot(ctx, g_drag.source_slot, old_a);
            }
            else if (is_point_inside(drop_x, drop_y, &g_slot_equip_x))
            {
                uint8_t old_x = gb_read8(ctx, WRAM_EQUIP_SLOT_X);
                gb_write8(ctx, WRAM_EQUIP_SLOT_X, dragged_item);
                write_to_source_slot(ctx, g_drag.source_slot, old_x);
            }
            else if (is_point_inside(drop_x, drop_y, &g_slot_equip_y))
            {
                uint8_t old_y = gb_read8(ctx, WRAM_EQUIP_SLOT_Y);
                gb_write8(ctx, WRAM_EQUIP_SLOT_Y, dragged_item);
                write_to_source_slot(ctx, g_drag.source_slot, old_y);
            }
            else
            {
                for (int i = 0; i < 10; i++)
                {
                    if (is_point_inside(drop_x, drop_y, &g_slots[i]))
                    {
                        uint8_t old_slot = gb_read8(ctx, WRAM_INV_ITEMS_START + i);
                        gb_write8(ctx, WRAM_INV_ITEMS_START + i, dragged_item);
                        write_to_source_slot(ctx, g_drag.source_slot, old_slot);
                        break;
                    }
                }
            }
            play_menu_sfx(ctx, HRAM_SFX_WAVE, 0x1A); // Sonido corto al colocar/equipar
            g_drag.active = false;
        }
    }

    // Limpieza automática: si la ROM generó un duplicado de lo que tienes en X o Y
    if (!g_drag.active)
    {
        uint8_t item_x = gb_read8(ctx, WRAM_EQUIP_SLOT_X);
        uint8_t item_y = gb_read8(ctx, WRAM_EQUIP_SLOT_Y);

        for (int i = 0; i < 10; i++)
        {
            uint8_t grid_item = gb_read8(ctx, WRAM_INV_ITEMS_START + i);
            if (grid_item != 0x00 && (grid_item == item_x || grid_item == item_y))
            {
                gb_write8(ctx, WRAM_INV_ITEMS_START + i, 0x00);
            }
        }
    }
}

static void draw_slot_item(GBContext *ctx, uint8_t item, float x, float y, float size)
{
    if (item == 0x00)
        return;

    // Si es ITEM_POWDER pero no hay polvos fabricados, pintar el hongo (slot 51) sin munición
    if (item == ITEM_POWDER && ctx->wram[WRAM_POWDER_COUNT] == 0)
    {
        draw_atlas_slot(51, x, y, size);
        return;
    }

    draw_item_icon(item, x, y, size);
    draw_item_ammo(ctx, item, x + 7, y);
}

void inventory_menu_render(GBContext *ctx)
{
    if (!ctx)
        return;

    const float col_r = 0.85f, col_g = 0.80f, col_b = 0.53f; // #D9CC88

    uint8_t item_x = gb_read8(ctx, WRAM_EQUIP_SLOT_X);
    uint8_t item_y = gb_read8(ctx, WRAM_EQUIP_SLOT_Y);

    // 1. Fondo completo (320x240)
    ui_draw_sprite(&g_tex_bg, 0.0f, 0.0f, 320.0f, 240.0f, 256.0f, 256.0f);

    // 2. Cabecera centrada
    debug_printf_ex(45.0f, 15.0f, 1.3f, col_r, col_g, col_b, "Items");
    debug_printf_ex(130.0f, 15.0f, 1.3f, col_r, col_g, col_b, "Equipo");
    debug_printf_ex(222.0f, 15.0f, 1.3f, col_r, col_g, col_b, "Mapa");

    // Línea activa dorada
    int line_offset = debug_get_vbo_offset();
    if (g_active_tab == TAB_ITEMS)
        debug_push_rect(42.0f, 29.0f, 55.0f, 2.0f, col_r, col_g, col_b, 1.0f);
    if (g_active_tab == TAB_QUEST)
        debug_push_rect(126.0f, 29.0f, 66.0f, 2.0f, col_r, col_g, col_b, 1.0f);
    if (g_active_tab == TAB_MAP)
        debug_push_rect(218.0f, 29.0f, 47.0f, 2.0f, col_r, col_g, col_b, 1.0f);
    debug_flush_rects(line_offset);

    // ==========================================
    // PESTAÑA 1: ITEMS
    // ==========================================
    if (g_active_tab == TAB_ITEMS)
    {
        // Grid central centrado al píxel (239x79)
        ui_draw_sprite(&g_tex_grid, 40.5f, 70.0f, 239.0f, 79.0f, 239.0f, 79.0f);

        // Corchetes de botones
        ui_draw_sprite(&g_tex_btn_equip, g_slot_equip_b.x, g_slot_equip_b.y, g_slot_equip_b.w, g_slot_equip_b.h, (float)g_tex_btn_equip.width, (float)g_tex_btn_equip.height);
        ui_draw_sprite(&g_tex_btn_equip, g_slot_equip_a.x, g_slot_equip_a.y, g_slot_equip_a.w, g_slot_equip_a.h, (float)g_tex_btn_equip.width, (float)g_tex_btn_equip.height);
        ui_draw_sprite(&g_tex_btn_equip, g_slot_equip_x.x, g_slot_equip_x.y, g_slot_equip_x.w, g_slot_equip_x.h, (float)g_tex_btn_equip.width, (float)g_tex_btn_equip.height);
        ui_draw_sprite(&g_tex_btn_equip, g_slot_equip_y.x, g_slot_equip_y.y, g_slot_equip_y.w, g_slot_equip_y.h, (float)g_tex_btn_equip.width, (float)g_tex_btn_equip.height);

        // Letras X, Y, B y A más grandes (escala 2.0 = 16x16 px) al pie del corchete
        debug_printf_ex(g_slot_equip_b.x - 13.0f, g_slot_equip_b.y, 1.3f, col_r, col_g, col_b, "B");
        debug_printf_ex(g_slot_equip_a.x - 13.0f, g_slot_equip_a.y, 1.3f, col_r, col_g, col_b, "A");
        debug_printf_ex(g_slot_equip_x.x - 13.0f, g_slot_equip_x.y, 1.3f, col_r, col_g, col_b, "X");
        debug_printf_ex(g_slot_equip_y.x - 13.0f, g_slot_equip_y.y, 1.3f, col_r, col_g, col_b, "Y");

        // Ítems equipados (solo se dibujan si no se están arrastrando desde aquí)
        uint8_t eq_b = hotswap_is_active() ? hotswap_get_backup_b() : gb_read8(ctx, WRAM_EQUIP_SLOT_B);
        uint8_t eq_a = gb_read8(ctx, WRAM_EQUIP_SLOT_A);
        uint8_t eq_x = item_x;
        uint8_t eq_y = item_y;

        if (!(g_drag.active && g_drag.source_slot == -1))
            draw_slot_item(ctx, eq_b, g_slot_equip_b.x + 4.0f, g_slot_equip_b.y, 32.0f);

        if (!(g_drag.active && g_drag.source_slot == -2))
            draw_slot_item(ctx, eq_a, g_slot_equip_a.x + 4.0f, g_slot_equip_a.y, 32.0f);

        if (!(g_drag.active && g_drag.source_slot == -3))
            draw_slot_item(ctx, eq_x, g_slot_equip_x.x + 4.0f, g_slot_equip_x.y, 32.0f);

        if (!(g_drag.active && g_drag.source_slot == -4))
            draw_slot_item(ctx, eq_y, g_slot_equip_y.x + 4.0f, g_slot_equip_y.y, 32.0f);

        // Ítems de las 10 casillas (centrados dentro de cada celda de 48x39 px)
        for (int i = 0; i < 10; i++)
        {
            uint8_t item = gb_read8(ctx, WRAM_INV_ITEMS_START + i);

            // Omitir si está vacío, si está en X/Y, o si es la casilla que se está arrastrando
            if (item == 0x00 || item == item_x || item == item_y)
                continue;

            if (g_drag.active && g_drag.source_slot == i)
                continue;

            draw_slot_item(ctx, item, g_slots[i].x + 8.0f, g_slots[i].y + 4.0f, 32.0f);
        }

        // Ítem arrastrado con el Stylus (centrado bajo la punta del lápiz)
        if (g_drag.active)
        {
            draw_slot_item(ctx, g_drag.item_id, g_drag.cur_x - 16.0f, g_drag.cur_y - 16.0f, 32.0f);
        }

        // testeo supremo
    }

    // ==========================================
    // PESTAÑA 2: EQUIPO
    // ==========================================
    else if (g_active_tab == TAB_QUEST)
    {
        // Círculo centrado (128x128)
        ui_draw_sprite(&g_tex_quest_circle, 96.0f, 60.0f, 128.0f, 128.0f, 128.0f, 128.0f);

        // Grid lateral (47x159)
        ui_draw_sprite(&g_tex_quest_slots, 25.0f, 50.0f, 47.0f, 159.0f, 47.0f, 159.0f);

        // -------------------------------------------------------------
        // columna izquierda
        // -------------------------------------------------------------
        // aletas u hojas doradas
        uint8_t has_flippers = gb_read8(ctx, WRAM_FLIPPERS);
        uint8_t golden_leaves = gb_read8(ctx, WRAM_GOLDEN_LEAVES);

        if (has_flippers)
        {
            draw_atlas_slot(11, 37.0f, 57.0f, 26.0f);
        }
        else if (golden_leaves >= 1 && golden_leaves <= 5)
        {
            draw_atlas_slot(70, 26.0f, 61.0f, 20.0f);
            debug_printf_ex(46.0f, 65.0f, 1.0f, col_r, col_g, col_b, "%02d", golden_leaves);
        }

        uint8_t trade_item = gb_read8(ctx, WRAM_TRADE_ITEM);

        // trade item
        if (trade_item >= 2 && trade_item <= 14)
        {
            int slot = 24 + (trade_item - 2); // 0x02 -> Slot 24 (Moño), 0x0E -> Slot 36 (Lupa)
            draw_atlas_slot(slot, 37.0f, 97.0f, 26.0f);
        }
        else if (trade_item == 1)
        {
            draw_atlas_slot(61, 37.0f, 97.0f, 26.0f);
        }

        // poción
        uint8_t has_medicine = gb_read8(ctx, WRAM_MEDICINE);
        if (has_medicine != 0x00)
        {
            draw_atlas_slot(17, 37.0f, 137.0f, 24.0f);
        }

        // caracolas
        uint8_t seashells = ctx->wram[WRAM_SEASHELLS_COUNT];
        if (seashells > 0)
        {
            draw_atlas_slot(16, 26.0f, 177.0f, 20.0f);
            debug_printf_ex(46.0f, 181.0f, 1.0f, col_r, col_g, col_b, "%02d", seashells);
        }

        // -------------------------------------------------------------
        // parte central
        // -------------------------------------------------------------

        // instrumentos
        uint8_t instruments = gb_read8(ctx, WRAM_INSTRUMENTS_START);

        static const float s_inst_positions[8][2] = {
            {147.0f, 65.0f},  // 1. Violonchelo (N)
            {180.0f, 78.0f},  // 2. Cuerno (NE)
            {193.0f, 111.0f}, // 3. Campana (E)
            {180.0f, 144.0f}, // 4. Arpa (SE)
            {147.0f, 158.0f}, // 5. Marimba (S)
            {114.0f, 144.0f}, // 6. Triángulo (SO)
            {101.0f, 111.0f}, // 7. Órgano (O)
            {114.0f, 78.0f}   // 8. Tambor (NO)
        };

        const float inst_size = 26.0f; // o 24.0f según el ajuste dentro del círculo

        for (int i = 0; i < 8; i++)
        {
            // Leemos el byte de la mazmorra i (0xDB65 a 0xDB6C)
            uint8_t dungeon_flags = gb_read8(ctx, WRAM_INSTRUMENTS_START + i);

            // Bit 1 (0x02) = Instrumento de las Sirenas obtenido
            if (dungeon_flags & 0x02)
            {
                draw_atlas_slot(38 + i, s_inst_positions[i][0], s_inst_positions[i][1], inst_size);
            }
        }

        // llaves de mazmorra
        bool has_keys[5] = {
            gb_read8(ctx, WRAM_KEY_TAIL) != 0,      // Llave Cola
            gb_read8(ctx, WRAM_GOLDEN_LEAVES) == 6, // Llave Baba (Slime Key = 6)
            gb_read8(ctx, WRAM_KEY_ANGLER) != 0,    // Llave Pez
            gb_read8(ctx, WRAM_KEY_FACE) != 0,      // Llave Rostro
            gb_read8(ctx, WRAM_KEY_BIRD) != 0       // Llave Pájaro
        };

        float key_size = 22.0f;
        float key_step = 24.0f;    // Espaciado entre llaves
        float key_base_x = 101.0f; // Centrado exacto horizontal (320 px)
        float key_y = 192.0f;      // 4 px más arriba para separarlas del marco inferior

        for (int k = 0; k < 5; k++)
        {
            if (has_keys[k])
            {
                draw_atlas_slot(46 + k, key_base_x + (k * key_step), key_y, key_size);
            }
        }

        // -------------------------------------------------------------
        // columna derecha
        // -------------------------------------------------------------
        float right_x = 290.0f;

        // 1. Rupia (Slot 52)
        uint8_t rup_h = gb_read8(ctx, WRAM_RUPEES_H);
        uint8_t rup_l = gb_read8(ctx, WRAM_RUPEES_L);
        int rupees = ((rup_h & 0x0F) * 100) + (((rup_l >> 4) & 0x0F) * 10) + (rup_l & 0x0F);
        draw_atlas_slot(52, right_x - 40.0f, 58.0f, 20.0f);
        debug_printf_ex(right_x - 23.0f, 58.0f, 1.1f, col_r, col_g, col_b, "%03d", rupees);

        // 2. Túnica (Slot 57)
        uint8_t tunic_type = gb_read8(ctx, 0xDC0F); // 1 = Verde, 2 = Azul, 3 = Roja

        // Si aún no ha entrado al Color Dungeon el valor puede ser 0 o 1
        int tunic_slot = 57; // Slot 57: Túnica verde base
        if (tunic_type == 2)
        {
            tunic_slot = 58; // azul
        }
        else if (tunic_type == 3)
        {
            tunic_slot = 59; // rojo
        }

        draw_atlas_slot(tunic_slot, right_x - 36.0f, 80.0f, 28.0f);

        // 3. Cuartos de Corazón (Slots 53 a 56 según el valor actual)
        uint8_t hearts = gb_read8(ctx, WRAM_HEART_PIECES) & 0x03;
        draw_atlas_slot(53 + hearts, right_x - 34.0f, 120.0f, 24.0f);

        // 4. Álbum de Fotos (Slot 60)
        uint16_t photo_flags = gb_read8(ctx, WRAM_PHOTOS_1) | ((uint16_t)(gb_read8(ctx, WRAM_PHOTOS_2) & 0x0F) << 8);
        int photo_count = 0;

        for (int i = 0; i < 12; i++)
        {
            if (photo_flags & (1 << i))
                photo_count++;
        }
        draw_atlas_slot(60, right_x - 34.0f, 155.0f, 24.0f);
        debug_printf_ex(right_x - 41.0f, 183.0f, 1.0f, col_r, col_g, col_b, "%02d/12", photo_count);
    }

    // ==========================================
    // PESTAÑA 3: MAPA
    // ==========================================
    else if (g_active_tab == TAB_MAP)
    {
        uint8_t dungeon_id = 0;
        if (is_in_dungeon(ctx, &dungeon_id))
        {
            render_dungeon_map(ctx, dungeon_id);
        }
        else
        {
            render_overworld_map(ctx);
        }
    }

    // =============================================================================
    // MENÚ CONTEXTUAL FLOTANTE DE LA OCARINA
    // =============================================================================
    if (g_ocarina_popup.open)
    {
        uint8_t songs = gb_read8(ctx, WRAM_OCARINA_SONGS);
        uint8_t selected = gb_read8(ctx, WRAM_OCARINA_SELECTED);
        static const uint8_t s_song_masks[3] = {0x04, 0x02, 0x01};

        float pop_x = 75.0f;
        float pop_y = 90.0f;
        float pop_w = 170.0f;
        float pop_h = 60.0f;
        float icon_sz = 28.0f;
        float spacing = 50.0f;

        // 1. Oscurecer el fondo para dar efecto modal/contextual
        int dim_offset = debug_get_vbo_offset();
        debug_push_rect(0.0f, 0.0f, 320.0f, 240.0f, 0.0f, 0.0f, 0.0f, 0.55f);
        debug_flush_rects(dim_offset);

        // 2. Caja base del popup (borde dorado y fondo café oscuro)
        int box_offset = debug_get_vbo_offset();
        // Borde exterior
        debug_push_rect(pop_x - 2.0f, pop_y - 2.0f, pop_w + 4.0f, pop_h + 4.0f, 0.85f, 0.80f, 0.53f, 1.0f);
        // Fondo interior
        debug_push_rect(pop_x, pop_y, pop_w, pop_h, 0.0f, 0.0f, 0.0f, 1.0f);
        debug_flush_rects(box_offset);

        // 3. Dibujar las 3 canciones (Slots 71: Marin, 72: Manbo, 73: Mamu)
        for (int s = 0; s < 3; s++)
        {
            float ix = pop_x + 15.0f + (s * spacing);
            float iy = pop_y + 16.0f;

            if (songs & s_song_masks[s])
            {
                // Si es la canción activa, dibujar un marco selector dorado detrás
                if (selected == s)
                {
                    int sel_offset = debug_get_vbo_offset();
                    debug_push_rect(ix - 3.0f, iy - 3.0f, icon_sz + 6.0f, icon_sz + 6.0f, 0.98f, 0.90f, 0.40f, 1.0f);
                    debug_push_rect(ix - 1.0f, iy - 1.0f, icon_sz + 2.0f, icon_sz + 2.0f, 0.22f, 0.14f, 0.07f, 1.0f);
                    debug_flush_rects(sel_offset);
                }

                // Dibuja la carita del personaje correspondiente
                draw_atlas_slot(71 + s, ix, iy, icon_sz);
            }
            else
            {
                // Canción aún no aprendida: cuadro vacío tenue
                int locked_offset = debug_get_vbo_offset();
                debug_push_rect(ix + 6.0f, iy + 6.0f, 16.0f, 16.0f, 0.35f, 0.25f, 0.18f, 1.0f);
                debug_flush_rects(locked_offset);
            }
        }
    }
}