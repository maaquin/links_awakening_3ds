#ifndef UI_TEXTURES_H
#define UI_TEXTURES_H

#include <citro3d.h>
#include <stdbool.h>

extern C3D_Tex g_tex_bg;
extern C3D_Tex g_tex_grid;
extern C3D_Tex g_tex_btn_equip;
extern C3D_Tex g_tex_items;
extern C3D_Tex g_tex_quest_circle;
extern C3D_Tex g_tex_quest_slots;

void ui_textures_init(void);
bool rom_extract_item_textures(const char *rom_path);
void ui_draw_sprite(C3D_Tex *tex, float x, float y, float w, float h, float orig_w, float orig_h);
void ui_draw_sub_sprite(C3D_Tex *tex, float x, float y, float w, float h, 
                        float src_x, float src_y, float src_w, float src_h);

#endif