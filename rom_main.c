#include "rom.h"
#include "rom_loader.h"
#include "gbrt.h"
#include "platform_sdl.h"

#include <stdio.h>
#include <3ds.h>

int main(int argc, char *argv[])
{
    (void)argc;
    (void)argv;

    // 1. Inicializa audio ndsp, pantalla gfx y consola de texto
    if (!gb_platform_init(1))
    {
        return 1;
    }

    // 2. Crea el contexto del recompilador
    GBContext *ctx = gb_context_create(NULL);
    if (!ctx)
    {
        gb_platform_shutdown();
        return 1;
    }

    gb_platform_set_context(ctx);

    // 3. Conecta callbacks de audio, sdmc y joypad
    gb_platform_register_context(ctx);

    // 4. Inicializa la ROM desde SD
    if (!rom_loader_init(ctx))
    {
        fprintf(stderr, "\nPresiona START para salir...\n");
        while (aptMainLoop())
        {
            hidScanInput();
            if (hidKeysDown() & KEY_START)
                break;
            gspWaitForVBlank();
        }
        gb_context_destroy(ctx);
        gb_platform_shutdown();
        return 1;
    }

    // 5. Bucle de emulación/recompilación
    while (gb_platform_poll_events(ctx))
    {

        if (!gb_platform_is_paused())
        {
            gb_run_frame(ctx);
        }

        // Renderizar si el emulador completó un frame O si estamos en pausa
        if (ctx->frame_done || gb_platform_is_paused())
        {
            const uint32_t *fb = gb_get_framebuffer(ctx);
            if (fb)
            {
                gb_platform_render_frame(fb);
            }

            // Solo resetear los flags internos del core si la CPU realmente corrió un frame
            if (ctx->frame_done)
            {
                gb_reset_frame(ctx);
                ctx->stopped = 0;
            }
        }
    }

    // 6. Cierre ordenado
    gb_context_destroy(ctx);
    gb_platform_shutdown();
    return 0;
}