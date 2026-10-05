#include "rom.h"
#include "gbrt.h"
#include "platform_sdl.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // 1. Inicializa audio ndsp, pantalla gfx y consola de texto
    if (!gb_platform_init(1)) {
        return 1;
    }

    // 2. Crea el contexto del recompilador
    GBContext* ctx = gb_context_create(NULL);
    if (!ctx) {
        gb_platform_shutdown();
        return 1;
    }

    // 3. Conecta callbacks de audio, sdmc y joypad
    gb_platform_register_context(ctx);

    // 4. Inicializa la ROM
    rom_init(ctx);

    // 5. Bucle de emulación/recompilación
    while (gb_platform_poll_events(ctx)) {
        gb_run_frame(ctx);

        if (ctx->frame_done) {
            const uint32_t* fb = gb_get_framebuffer(ctx);
            if (fb) {
                gb_platform_render_frame(fb);
            }
            gb_reset_frame(ctx);
            ctx->stopped = 0;
            gb_platform_vsync();
        }
    }

    // 6. Cierre ordenado
    gb_context_destroy(ctx);
    gb_platform_shutdown();
    return 0;
}