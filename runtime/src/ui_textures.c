#include "ui_textures.h"
#include "platform_sdl.h"

#include <citro3d.h>
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#include "stb_image.h"

#include "asset_bg_menu.h"
#include "asset_btn_equip.h"
#include "asset_grid_items.h"

C3D_Tex g_tex_bg;
C3D_Tex g_tex_btn_equip;
C3D_Tex g_tex_grid;
C3D_Tex g_tex_items;

void ui_draw_sub_sprite(C3D_Tex *tex, float x, float y, float w, float h, 
                        float src_x, float src_y, float src_w, float src_h)
{
    if (!tex || !tex->data) return;

    // En la 3DS con GX_TRANSFER_FLIP_VERT(0), el tope de la textura es 1.0f
    float u0 = src_x / (float)tex->width;
    float u1 = (src_x + src_w) / (float)tex->width;
    float v0 = 1.0f - (src_y / (float)tex->height);
    float v1 = 1.0f - ((src_y + src_h) / (float)tex->height);

    C3D_TexBind(0, tex);

    int start = debug_get_vbo_offset();
    debug_push_textured_rect(x, y, w, h, u0, v0, u1, v1);
    debug_flush_textured_rects(start);
}

static int next_pow2(int v) {
    int p = 64; // La 3DS necesita texturas de al menos 64x64
    while (p < v) p <<= 1;
    return p;
}

static bool load_embedded_png(const unsigned char *data, int len, C3D_Tex *tex)
{
    int w, h, channels;
    unsigned char *img = stbi_load_from_memory(data, len, &w, &h, &channels, 4);
    if (!img) return false;

    int tw = next_pow2(w);
    int th = next_pow2(h);

    C3D_TexInit(tex, tw, th, GPU_RGBA8);
    C3D_TexSetFilter(tex, GPU_NEAREST, GPU_NEAREST);

    tex->data = linearAlloc(tw * th * sizeof(uint32_t));
    if (!tex->data) {
        stbi_image_free(img);
        return false;
    }

    uint32_t *linear_buf = (uint32_t *)linearAlloc(tw * th * sizeof(uint32_t));
    memset(linear_buf, 0, tw * th * sizeof(uint32_t));

    // CRÍTICO 1: Empaquetar píxeles a formato 0xRRGGBBAA de hardware
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t *p = (uint8_t *)&img[(y * w + x) * 4];
            uint8_t r = p[0];
            uint8_t g = p[1];
            uint8_t b = p[2];
            uint8_t a = p[3];
            linear_buf[y * tw + x] = (r << 24) | (g << 16) | (b << 8) | a;
        }
    }
    stbi_image_free(img);

    GSPGPU_FlushDataCache(linear_buf, tw * th * sizeof(uint32_t));

    GX_DisplayTransfer((u32 *)linear_buf, GX_BUFFER_DIM(tw, th),
                       (u32 *)tex->data, GX_BUFFER_DIM(tw, th),
                       GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
                       GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
                       GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    gspWaitForPPF();

    linearFree(linear_buf);
    return true;
}

void ui_textures_init(void)
{
    load_embedded_png(bg_menu_png, bg_menu_png_len, &g_tex_bg);
    load_embedded_png(grid_items_png, grid_items_png_len, &g_tex_grid);
    load_embedded_png(btn_equip_png, btn_equip_png_len, &g_tex_btn_equip);
}

void ui_draw_sprite(C3D_Tex *tex, float x, float y, float w, float h, float orig_w, float orig_h)
{
    if (!tex || !tex->data) return;

    // CRÍTICO 2: Invertir el mapeo. En 3DS, V=1.0 es la parte de ARRIBA de la memoria.
    float u0 = 0.0f;
    float v0 = 1.0f;
    float u1 = orig_w / (float)tex->width;
    float v1 = 1.0f - (orig_h / (float)tex->height);

    C3D_TexBind(0, tex);

    int start = debug_get_vbo_offset();
    debug_push_textured_rect(x, y, w, h, u0, v0, u1, v1);
    debug_flush_textured_rects(start);
}