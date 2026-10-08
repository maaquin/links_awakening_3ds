/**
 * @file platform_sdl.h
 * @brief SDL2 platform layer for GameBoy runtime
 */

#ifndef GB_PLATFORM_SDL_H
#define GB_PLATFORM_SDL_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Joypad state variables
extern uint8_t g_joypad_buttons;
extern uint8_t g_joypad_dpad;

typedef struct GBContext GBContext;
bool gb_platform_is_paused(void);

/**
 * @brief Initialize SDL2 platform (window, renderer)
 * @param scale Window scale factor (1-4)
 * @return true on success
 */
bool gb_platform_init(int scale);

void gb_platform_set_context(GBContext *ctx);

/**
 * @brief Register context with platform (sets up callbacks)
 */
void gb_platform_register_context(GBContext* ctx);

/**
 * @brief Shutdown SDL2 platform
 */
void gb_platform_shutdown(void);

/**
 * @brief Process SDL events
 * @return false if quit requested
 */
bool gb_platform_poll_events(GBContext* ctx);

/**
 * @brief Render frame to screen
 */
void gb_platform_render_frame(const uint32_t* framebuffer);

/**
 * @brief Get joypad state
 * @return Joypad byte (active low)
 */
uint8_t gb_platform_get_joypad(void);

/**
 * @brief Wait for vsync / frame timing
 */
void gb_platform_vsync(void);

/**
 * @brief Set window title
 */
void gb_platform_set_title(const char* title);

/**
 * @brief Save full program state to disk
 */
void gb_platform_save_state(GBContext* ctx);

/**
 * @brief Load full program state from disk
 */
void gb_platform_load_state(GBContext* ctx);

// Funciones gráficas y de depuración para 3DS (Citro3D / VBO)
int  debug_get_vbo_offset(void);
void debug_push_rect(float x, float y, float w, float h, float r, float g, float b, float a);
void debug_push_textured_rect(float x, float y, float w, float h, float u0, float v0, float u1, float v1);
void debug_flush_rects(int start_offset);
void debug_flush_textured_rects(int start_offset);
void debug_printf(float x, float y, const char *fmt, ...);
void debug_printf_ex(float x, float y, float scale, float r, float g, float b, const char *fmt, ...);
void draw_rect(float x, float y, float w, float h, float r, float g, float b, float a);

#ifdef __cplusplus
}
#endif

#endif /* GB_PLATFORM_SDL_H */