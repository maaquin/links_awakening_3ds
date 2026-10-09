#include "inventory_menu.h"
#include <citro3d.h>
#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include "ui_textures.h"

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
#define WRAM_SHIELD_LEVEL 0xDB44
#define WRAM_SWORD_LEVEL 0xDB4E
#define WRAM_SEASHELLS 0xDB5F
#define WRAM_INSTRUMENTS_START 0xDB65
#define WRAM_TUNIC_COLOR 0xDB6F
#define WRAM_HEART_PIECES 0xDB5C
#define WRAM_MAX_HEARTS 0xDB5B

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

#define WRAM_BOMBS_COUNT 0xDB4D
#define WRAM_ARROWS_COUNT 0xDB45
#define WRAM_POWDER_COUNT 0xDB4C

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

static const char *get_item_name(uint8_t id)
{
    switch (id)
    {
    case 0x00:
        return "VACIO";
    case 0x01:
        return "ESPADA";
    case 0x02:
        return "BOMBAS";
    case 0x03:
        return "ARCO";
    case 0x04:
        return "ESCUDO";
    case 0x05:
        return "GANCHO";
    case 0x06:
        return "VARITA";
    case 0x07:
        return "VIENTO";
    case 0x08:
        return "OCARINA";
    case 0x09:
        return "PLUMA";
    case 0x0A:
        return "PALA";
    case 0x0B:
        return "POLVOS";
    case 0x0C:
        return "BOOMERANG";
    default:
        return "ITEM";
    }
}

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
        count = gb_read8(ctx, WRAM_POWDER_COUNT);
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
    [0x00] = -1,         // Vacío
    [ITEM_SWORD] = 1,    // 0x01 -> Espada (índice 2)
    [ITEM_BOMBS] = 2,    // 0x02 -> Bomba (índice 0)
    [ITEM_BRACELET] = 3, // 0x03 -> Brazalete (índice 1)
    [ITEM_SHIELD] = 4,   // 0x04 -> Escudo (índice 3)
    [ITEM_BOW] = 5,      // 0x05 -> Arco (índice 4)
    [ITEM_HOOKSHOT] = 6, // 0x06 -> Gancho (índice 5)
    [ITEM_ROD] = 7,      // 0x07 -> Varita (índice 6)
    [ITEM_BOOTS] = 8,    // 0x08 -> Botas (índice 12)
    [ITEM_OCARINA] = 9,  // 0x09 -> Ocarina (índice 8)
    [ITEM_FEATHER] = 10, // 0x0A -> Pluma (índice 9)
    [ITEM_SHOVEL] = 11,  // 0x0B -> Pala (índice 11)
    [ITEM_POWDER] = 12,  // 0x0C -> Polvos mágicos (índice 7)
    [0x0D] = 18,         // Boomerang (índice 18)
    [0x0E] = 16,         // Poción (índice 16)
};

static void draw_item_icon(uint8_t item_id, float x, float y, float size)
{
    if (item_id == 0x00 || item_id >= 16)
        return;

    int atlas_idx = s_wram_id_to_atlas_index[item_id];
    if (atlas_idx < 0)
        return;

    // Coordenadas del cuadro en la textura atlas de 128x128 (8 columnas x 16px)
    int col = atlas_idx % 8;
    int row = atlas_idx / 8;

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
        gb_write8(ctx, WRAM_INV_ITEMS_START + 10, value);
    }
    else if (source_slot == -4)
    {
        // Origen fue el botón Y
        gb_write8(ctx, WRAM_INV_ITEMS_START + 11, value);
    }
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

    if (kDown & KEY_L)
        g_active_tab = (g_active_tab + TAB_COUNT - 1) % TAB_COUNT;
    if (kDown & KEY_R)
        g_active_tab = (g_active_tab + 1) % TAB_COUNT;

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
                uint8_t item = gb_read8(ctx, WRAM_INV_ITEMS_START + 10);
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
                uint8_t item = gb_read8(ctx, WRAM_INV_ITEMS_START + 11);
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
            uint8_t dragged_item = g_drag.item_id;
            float drop_x = g_drag.cur_x;
            float drop_y = g_drag.cur_y;

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
                uint8_t old_x = gb_read8(ctx, WRAM_INV_ITEMS_START + 10);
                gb_write8(ctx, WRAM_INV_ITEMS_START + 10, dragged_item);
                write_to_source_slot(ctx, g_drag.source_slot, old_x);
            }
            else if (is_point_inside(drop_x, drop_y, &g_slot_equip_y))
            {
                uint8_t old_y = gb_read8(ctx, WRAM_INV_ITEMS_START + 11);
                gb_write8(ctx, WRAM_INV_ITEMS_START + 11, dragged_item);
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
            g_drag.active = false;
        }
    }
}

void inventory_menu_render(GBContext *ctx)
{
    if (!ctx)
        return;

    const float col_r = 0.85f, col_g = 0.80f, col_b = 0.53f; // #D9CC88

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
        // 3. Grid central centrado al píxel (239x79)
        ui_draw_sprite(&g_tex_grid, 40.5f, 70.0f, 239.0f, 79.0f, 239.0f, 79.0f);

        // 4. Corchetes de botones
        ui_draw_sprite(&g_tex_btn_equip, g_slot_equip_b.x, g_slot_equip_b.y, g_slot_equip_b.w, g_slot_equip_b.h, (float)g_tex_btn_equip.width, (float)g_tex_btn_equip.height);
        ui_draw_sprite(&g_tex_btn_equip, g_slot_equip_a.x, g_slot_equip_a.y, g_slot_equip_a.w, g_slot_equip_a.h, (float)g_tex_btn_equip.width, (float)g_tex_btn_equip.height);
        ui_draw_sprite(&g_tex_btn_equip, g_slot_equip_x.x, g_slot_equip_x.y, g_slot_equip_x.w, g_slot_equip_x.h, (float)g_tex_btn_equip.width, (float)g_tex_btn_equip.height);
        ui_draw_sprite(&g_tex_btn_equip, g_slot_equip_y.x, g_slot_equip_y.y, g_slot_equip_y.w, g_slot_equip_y.h, (float)g_tex_btn_equip.width, (float)g_tex_btn_equip.height);

        // 5. Letras X, Y, B y A más grandes (escala 2.0 = 16x16 px) al pie del corchete
        debug_printf_ex(g_slot_equip_b.x - 13.0f, g_slot_equip_b.y, 1.3f, col_r, col_g, col_b, "B");
        debug_printf_ex(g_slot_equip_a.x - 13.0f, g_slot_equip_a.y, 1.3f, col_r, col_g, col_b, "A");
        debug_printf_ex(g_slot_equip_x.x - 13.0f, g_slot_equip_x.y, 1.3f, col_r, col_g, col_b, "X");
        debug_printf_ex(g_slot_equip_y.x - 13.0f, g_slot_equip_y.y, 1.3f, col_r, col_g, col_b, "Y");

        // 1. Ítems equipados dentro de los corchetes (centrados dentro de los 62x62 px)
        // Centrado exacto en los botones X, Y, B y A
        uint8_t eq_b = hotswap_is_active() ? hotswap_get_backup_b() : gb_read8(ctx, WRAM_EQUIP_SLOT_B);
        uint8_t eq_a = gb_read8(ctx, WRAM_EQUIP_SLOT_A);
        uint8_t eq_x = gb_read8(ctx, WRAM_INV_ITEMS_START + 10);
        uint8_t eq_y = gb_read8(ctx, WRAM_INV_ITEMS_START + 11);
        if (eq_b != 0x00)
            draw_item_icon(eq_b, g_slot_equip_b.x + 4.0f, g_slot_equip_b.y, 32.0f);
        draw_item_ammo(ctx, eq_b, g_slot_equip_b.x + 4.0f, g_slot_equip_b.y);
        if (eq_a != 0x00)
            draw_item_icon(eq_a, g_slot_equip_a.x + 4.0f, g_slot_equip_a.y, 32.0f);
        draw_item_ammo(ctx, eq_a, g_slot_equip_a.x + 4.0f, g_slot_equip_a.y);
        if (eq_x != 0x00)
            draw_item_icon(eq_x, g_slot_equip_x.x + 4.0f, g_slot_equip_x.y, 32.0f);
        draw_item_ammo(ctx, eq_x, g_slot_equip_x.x + 4.0f, g_slot_equip_x.y);
        if (eq_y != 0x00)
            draw_item_icon(eq_y, g_slot_equip_y.x + 4.0f, g_slot_equip_y.y, 32.0f);
        draw_item_ammo(ctx, eq_y, g_slot_equip_y.x + 4.0f, g_slot_equip_y.y);

        // 2. Ítems de las 10 casillas (centrados dentro de cada celda de 48x39 px)

        for (int i = 0; i < 10; i++)
        {
            uint8_t item = gb_read8(ctx, WRAM_INV_ITEMS_START + i);
            if (item != 0x00)
            {
                draw_item_icon(item, g_slots[i].x + 8.0f, g_slots[i].y + 4.0f, 32.0f);
                draw_item_ammo(ctx, item, g_slots[i].x + 8.0f, g_slots[i].y + 4.0f);
            }
        }

        // 3. Ítem arrastrado con el Stylus (centrado bajo la punta del lápiz)
        if (g_drag.active)
        {
            draw_item_icon(g_drag.item_id, g_drag.cur_x - 16.0f, g_drag.cur_y - 16.0f, 32.0f);
        }
    }
}