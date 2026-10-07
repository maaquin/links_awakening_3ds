#include "gbrt.h"
#include <stdio.h>

void debug_dump_game_state(GBContext* ctx) {
    if (!ctx || !ctx->wram) return;

    // En Link's Awakening DX, WRAM banco 0 está en ctx->wram
    // 0xDB5A = Cuartos de corazón
    // 0xDB5B = Corazones máximos
    // 0xDB5D / 0xDB5E = Rupias
    uint8_t health = ctx->wram[0x1B5A]; // Offset relativo a WRAM (0xDB5A - 0xC000)
    uint8_t max_health = ctx->wram[0x1B5B];
    uint8_t rupees_hi = ctx->wram[0x1B5D];
    uint8_t rupees_lo = ctx->wram[0x1B5E];
    uint8_t current_room = ctx->wram[0x1800]; // 0xD800 - 0xC000

    printf("\x1b[2;2H=== ESTADO DEL JUGADOR ===");
    printf("\x1b[4;2HSala Overworld: 0x%02X", current_room);
    printf("\x1b[5;2HVida: %d / %d cuartos", health, max_health * 4);
    printf("\x1b[6;2HRupias: %02X%02X", rupees_hi, rupees_lo);
}