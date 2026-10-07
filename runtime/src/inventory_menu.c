#include "inventory_menu.h"
#include <citro3d.h>
#include <3ds.h>
#include <stdio.h>
#include <string.h>

uint8_t gb_read8(GBContext *ctx, uint16_t addr);
void gb_write8(GBContext *ctx, uint16_t addr, uint8_t value);
void debug_printf(float x, float y, const char *fmt, ...);

#define WRAM_EQUIP_SLOT_B 0xDB00
#define WRAM_EQUIP_SLOT_A 0xDB01
#define WRAM_INV_ITEMS_START 0xDB02

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

typedef struct
{
    float pos[3];
    float color[4];
    float texcoord[2];
} BatchVertex;

static InventorySlotUI g_slots[12];
static InventorySlotUI g_slot_equip_b;
static InventorySlotUI g_slot_equip_a;
static DragState g_drag = {0};

void debug_push_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void debug_flush_rects(int start_offset);
int debug_get_vbo_offset(void);

static int s_batch_start_offset = 0;

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

static bool is_point_inside(float px, float py, const InventorySlotUI *slot)
{
    return (px >= slot->x && px <= (slot->x + slot->w) &&
            py >= slot->y && py <= (slot->y + slot->h));
}

// Agrega un rectángulo al buffer sin dibujar todavía
static void push_rect(float x, float y, float w, float h, float r, float g, float b, float a)
{
    debug_push_rect(x, y, w, h, r, g, b, a);
}

// Envía todas las cajas juntas a la GPU en un solo pase ultrarrápido
static void flush_rect_batch(int start_offset)
{
    debug_flush_rects(start_offset);
}

void inventory_menu_init(void)
{
    float start_x = 15.0f;
    float start_y = 35.0f;
    float slot_w = 68.0f;
    float slot_h = 38.0f;
    float gap_x = 7.0f;
    float gap_y = 8.0f;

    for (int row = 0; row < 3; row++)
    {
        for (int col = 0; col < 4; col++)
        {
            int idx = row * 4 + col;
            g_slots[idx].x = start_x + col * (slot_w + gap_x);
            g_slots[idx].y = start_y + row * (slot_h + gap_y);
            g_slots[idx].w = slot_w;
            g_slots[idx].h = slot_h;
            g_slots[idx].slot_idx = idx;
        }
    }

    g_slot_equip_b.x = 25.0f;
    g_slot_equip_b.y = 180.0f;
    g_slot_equip_b.w = 120.0f;
    g_slot_equip_b.h = 45.0f;
    g_slot_equip_b.slot_idx = -1;

    g_slot_equip_a.x = 175.0f;
    g_slot_equip_a.y = 180.0f;
    g_slot_equip_a.w = 120.0f;
    g_slot_equip_a.h = 45.0f;
    g_slot_equip_a.slot_idx = -2;
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

    // 1. Iniciar arrastre
    if (kDown & KEY_TOUCH)
    {
        for (int i = 0; i < 12; i++)
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
    }

    // 2. Moviendo el stylus: actualizar última posición válida
    if (g_drag.active && (kHeld & KEY_TOUCH))
    {
        if (touch.px > 0 || touch.py > 0)
        {
            g_drag.cur_x = touch.px;
            g_drag.cur_y = touch.py;
        }
    }

    // 3. Soltar el stylus: usar g_drag.cur_x / cur_y en lugar de touch.px / py
    if (g_drag.active && (kUp & KEY_TOUCH))
    {
        uint8_t dragged_item = g_drag.item_id;
        float drop_x = g_drag.cur_x;
        float drop_y = g_drag.cur_y;

        // ¿Se soltó en la ranura B?
        if (is_point_inside(drop_x, drop_y, &g_slot_equip_b))
        {
            uint8_t old_b = gb_read8(ctx, WRAM_EQUIP_SLOT_B);
            gb_write8(ctx, WRAM_EQUIP_SLOT_B, dragged_item);

            if (g_drag.source_slot >= 0)
            {
                gb_write8(ctx, WRAM_INV_ITEMS_START + g_drag.source_slot, old_b);
            }
            else if (g_drag.source_slot == -2)
            {
                gb_write8(ctx, WRAM_EQUIP_SLOT_A, old_b);
            }
        }
        // ¿Se soltó en la ranura A?
        else if (is_point_inside(drop_x, drop_y, &g_slot_equip_a))
        {
            uint8_t old_a = gb_read8(ctx, WRAM_EQUIP_SLOT_A);
            gb_write8(ctx, WRAM_EQUIP_SLOT_A, dragged_item);

            if (g_drag.source_slot >= 0)
            {
                gb_write8(ctx, WRAM_INV_ITEMS_START + g_drag.source_slot, old_a);
            }
            else if (g_drag.source_slot == -1)
            {
                gb_write8(ctx, WRAM_EQUIP_SLOT_B, old_a);
            }
        }
        // ¿Se soltó en una casilla de la cuadrícula?
        else
        {
            for (int i = 0; i < 12; i++)
            {
                if (is_point_inside(drop_x, drop_y, &g_slots[i]))
                {
                    if (g_drag.source_slot == -1)
                    {
                        uint8_t target_item = gb_read8(ctx, WRAM_INV_ITEMS_START + i);
                        gb_write8(ctx, WRAM_EQUIP_SLOT_B, target_item);
                        gb_write8(ctx, WRAM_INV_ITEMS_START + i, dragged_item);
                    }
                    else if (g_drag.source_slot == -2)
                    {
                        uint8_t target_item = gb_read8(ctx, WRAM_INV_ITEMS_START + i);
                        gb_write8(ctx, WRAM_EQUIP_SLOT_A, target_item);
                        gb_write8(ctx, WRAM_INV_ITEMS_START + i, dragged_item);
                    }
                    else if (g_drag.source_slot >= 0 && g_drag.source_slot != i)
                    {
                        uint8_t target_item = gb_read8(ctx, WRAM_INV_ITEMS_START + i);
                        gb_write8(ctx, WRAM_INV_ITEMS_START + g_drag.source_slot, target_item);
                        gb_write8(ctx, WRAM_INV_ITEMS_START + i, dragged_item);
                    }
                    break;
                }
            }
        }

        g_drag.active = false;
    }
}

void inventory_menu_render(GBContext *ctx)
{
    if (!ctx)
        return;

    uint8_t eq_b = gb_read8(ctx, WRAM_EQUIP_SLOT_B);
    uint8_t eq_a = gb_read8(ctx, WRAM_EQUIP_SLOT_A);

    // ==========================================
    // FASE 1: ACUMULAR TODAS LAS CAJAS EN EL VBO
    // ==========================================
    int start_offset = debug_get_vbo_offset();

    for (int i = 0; i < 12; i++)
    {
        uint8_t item = gb_read8(ctx, WRAM_INV_ITEMS_START + i);
        float x = g_slots[i].x;
        float y = g_slots[i].y;
        float w = g_slots[i].w;
        float h = g_slots[i].h;

        if (g_drag.active && g_drag.source_slot == i)
        {
            push_rect(x, y, w, h, 0.10f, 0.15f, 0.22f, 1.0f);
        }
        else if (item != 0x00)
        {
            push_rect(x, y, w, h, 0.22f, 0.32f, 0.50f, 1.0f);
        }
        else
        {
            push_rect(x, y, w, h, 0.12f, 0.18f, 0.28f, 1.0f);
        }
    }

    // Botón B
    bool highlight_b = g_drag.active && is_point_inside(g_drag.cur_x, g_drag.cur_y, &g_slot_equip_b);
    if (highlight_b)
    {
        push_rect(g_slot_equip_b.x, g_slot_equip_b.y, g_slot_equip_b.w, g_slot_equip_b.h, 0.25f, 0.60f, 0.30f, 1.0f);
    }
    else
    {
        push_rect(g_slot_equip_b.x, g_slot_equip_b.y, g_slot_equip_b.w, g_slot_equip_b.h, 0.18f, 0.25f, 0.38f, 1.0f);
    }
    push_rect(g_slot_equip_b.x, g_slot_equip_b.y, g_slot_equip_b.w, 2.0f, 1.0f, 0.6f, 0.0f, 1.0f);

    // Botón A
    bool highlight_a = g_drag.active && is_point_inside(g_drag.cur_x, g_drag.cur_y, &g_slot_equip_a);
    if (highlight_a)
    {
        push_rect(g_slot_equip_a.x, g_slot_equip_a.y, g_slot_equip_a.w, g_slot_equip_a.h, 0.25f, 0.60f, 0.30f, 1.0f);
    }
    else
    {
        push_rect(g_slot_equip_a.x, g_slot_equip_a.y, g_slot_equip_a.w, g_slot_equip_a.h, 0.18f, 0.25f, 0.38f, 1.0f);
    }
    push_rect(g_slot_equip_a.x, g_slot_equip_a.y, g_slot_equip_a.w, 2.0f, 0.2f, 0.8f, 1.0f, 1.0f);

    // Caja flotante bajo el stylus al arrastrar
    if (g_drag.active)
    {
        float fx = g_drag.cur_x - 30.0f;
        float fy = g_drag.cur_y - 20.0f;
        push_rect(fx, fy, 65.0f, 30.0f, 0.85f, 0.75f, 0.15f, 1.0f);
    }

    // DISPARAR TODO EL LOTE DE CAJAS A LA GPU DE GOLPE
    flush_rect_batch(start_offset);

    // ==========================================
    // FASE 2: TEXTO ENCIMA DE LAS CAJAS
    // ==========================================
    debug_printf(15.0f, 15.0f, "INVENTARIO (ARRASTRA A [B] O [A])");

    for (int i = 0; i < 12; i++)
    {
        uint8_t item = gb_read8(ctx, WRAM_INV_ITEMS_START + i);
        float x = g_slots[i].x;
        float y = g_slots[i].y;
        debug_printf(x + 4.0f, y + 6.0f, "[%02X]", item);
        debug_printf(x + 4.0f, y + 20.0f, "%s", get_item_name(item));
    }

    debug_printf(g_slot_equip_b.x + 8.0f, g_slot_equip_b.y + 8.0f, "BOTON [ B ]");
    debug_printf(g_slot_equip_b.x + 8.0f, g_slot_equip_b.y + 24.0f, "%s (%02X)", get_item_name(eq_b), eq_b);

    debug_printf(g_slot_equip_a.x + 8.0f, g_slot_equip_a.y + 8.0f, "BOTON [ A ]");
    debug_printf(g_slot_equip_a.x + 8.0f, g_slot_equip_a.y + 24.0f, "%s (%02X)", get_item_name(eq_a), eq_a);

    if (g_drag.active)
    {
        float fx = g_drag.cur_x - 30.0f;
        float fy = g_drag.cur_y - 20.0f;
        debug_printf(fx + 6.0f, fy + 10.0f, "%s", get_item_name(g_drag.item_id));
    }
}