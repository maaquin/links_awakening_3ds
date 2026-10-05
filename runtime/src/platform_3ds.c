/*
 * platform_3ds.c — Nintendo 3DS backend for the gb-recompiled runtime.
 * Renderizado acelerado por GPU (Citro3D + PICA200) y audio con ndsp.
 */

#include "platform_sdl.h"
#include "gbrt.h"
#include "ppu.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <3ds.h>
#include <citro3d.h>
#include "default_shbin_data.h"

/* --- GPU & Citro3D ------------------------------------------------------- */

typedef struct
{
    float position[3];
    float color[4];
    float texcoord[2];
} Vertex;

#define TEX_WIDTH 256
#define TEX_HEIGHT 256
#define GB_W 160
#define GB_H 144
#define TOP_W 400
#define TOP_H 240

static C3D_RenderTarget *g_target_top = NULL;
static DVLB_s *g_vshader_dvlb = NULL;
static shaderProgram_s g_program;
static s8 g_uLoc_mvp = -1;
static C3D_Mtx g_projection;

static C3D_Tex g_fb_texture;
static Vertex *g_vbo_data = NULL;
static uint32_t *g_linear_fb = NULL;

static void gpu_init(void)
{
    C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);

    // Target superior (240x400 por rotación física)
    g_target_top = C3D_RenderTargetCreate(240, 400, GPU_RB_RGBA8, GPU_RB_DEPTH24_STENCIL8);
    C3D_RenderTargetSetOutput(g_target_top, GFX_TOP, GFX_LEFT,
                              GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(0) | GX_TRANSFER_RAW_COPY(0) |
                                  GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGB8) |
                                  GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));

    // Shader PICA200
    g_vshader_dvlb = DVLB_ParseFile((u32 *)default_shbin, default_shbin_len);
    shaderProgramInit(&g_program);
    shaderProgramSetVsh(&g_program, &g_vshader_dvlb->DVLE[0]);
    C3D_BindProgram(&g_program);

    g_uLoc_mvp = shaderInstanceGetUniformLocation(g_program.vertexShader, "mvp_mat");

    // Proyección con tilt vertical nativo (0..400 horizontal, 0..240 vertical)
    Mtx_OrthoTilt(&g_projection, 0.0f, 400.0f, 240.0f, 0.0f, 0.0f, 1.0f, true);

    // Atributos de vértice (Pos, Color, UV)
    C3D_AttrInfo *attrInfo = C3D_GetAttrInfo();
    AttrInfo_Init(attrInfo);
    AttrInfo_AddLoader(attrInfo, 0, GPU_FLOAT, 3);
    AttrInfo_AddLoader(attrInfo, 1, GPU_FLOAT, 4);
    AttrInfo_AddLoader(attrInfo, 2, GPU_FLOAT, 2);

    // Textura GPU 256x256 RGBA8
    C3D_TexInit(&g_fb_texture, TEX_WIDTH, TEX_HEIGHT, GPU_RGBA8);
    C3D_TexSetFilter(&g_fb_texture, GPU_NEAREST, GPU_NEAREST);

    // Combinador simple para reemplazar color con textura
    C3D_TexEnv *env = C3D_GetTexEnv(0);
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);

    // Buffer lineal para que el DMA convierta a Tiled
    g_linear_fb = (uint32_t *)linearAlloc(TEX_WIDTH * TEX_HEIGHT * sizeof(uint32_t));
    memset(g_linear_fb, 0, TEX_WIDTH * TEX_HEIGHT * sizeof(uint32_t));

    // VBO centrado: 160x144 centrado en 400x240
    g_vbo_data = (Vertex *)linearAlloc(sizeof(Vertex) * 6);

    float u1 = 160.0f / (float)TEX_WIDTH;
    float v1 = 1.0f;

    // Ancho proporcional centrado en los 400 px
    float render_w = 240.0f * (160.0f / 144.0f); // ~266.67f
    float x0 = (400.0f - render_w) / 2.0f;       // ~66.66f
    float x1 = x0 + render_w;                    // ~333.33f

    // Alto: 435.0f compensa exactamente el margen faltante abajo
    float y0 = 0.0f;
    float y1 = 435.0f;

    Vertex quad[6] = {
        {{x0, y0, 0.5f}, {1, 1, 1, 1}, {0.0f, v1}},
        {{x1, y0, 0.5f}, {1, 1, 1, 1}, {u1, v1}},
        {{x0, y1, 0.5f}, {1, 1, 1, 1}, {0.0f, 0.0f}},

        {{x1, y0, 0.5f}, {1, 1, 1, 1}, {u1, v1}},
        {{x1, y1, 0.5f}, {1, 1, 1, 1}, {u1, 0.0f}},
        {{x0, y1, 0.5f}, {1, 1, 1, 1}, {0.0f, 0.0f}},
    };
    memcpy(g_vbo_data, quad, sizeof(quad));

    C3D_BufInfo *bufInfo = C3D_GetBufInfo();
    BufInfo_Init(bufInfo);
    BufInfo_Add(bufInfo, g_vbo_data, sizeof(Vertex), 3, 0x210);

    C3D_CullFace(GPU_CULL_NONE);
}

/* --- Joypad ------------------------------------------------------------- */

uint8_t g_joypad_buttons = 0xFF;
uint8_t g_joypad_dpad = 0xFF;
static GBContext *g_ctx = NULL;
static int g_quit = 0;

/* --- Audio (ndsp) ------------------------------------------------------- */

#define SR 44100
#define ARING 8192
#define AWBUF_FR 1024
#define AWBUF_N 2

static int g_audio_ok = 0;
static int16_t g_aring[ARING * 2];
static volatile int g_aw = 0, g_ar = 0;
static ndspWaveBuf g_wbuf[AWBUF_N];
static int16_t *g_wbuf_mem[AWBUF_N];

static void audio_init(void)
{
    Result res = ndspInit();
    if (R_FAILED(res))
    {
        g_audio_ok = 0;
        return;
    }
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
    ndspChnSetRate(0, SR);
    ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

    for (int i = 0; i < AWBUF_N; ++i)
    {
        g_wbuf_mem[i] = (int16_t *)linearAlloc(AWBUF_FR * 2 * sizeof(int16_t));
        memset(&g_wbuf[i], 0, sizeof(ndspWaveBuf));
        if (g_wbuf_mem[i])
        {
            memset(g_wbuf_mem[i], 0, AWBUF_FR * 2 * sizeof(int16_t));
            g_wbuf[i].data_vaddr = g_wbuf_mem[i];
            g_wbuf[i].nsamples = AWBUF_FR;
            g_wbuf[i].status = NDSP_WBUF_DONE;
        }
    }
    g_audio_ok = 1;
}

static void audio_update(void)
{
    if (!g_audio_ok)
        return;
    for (int i = 0; i < AWBUF_N; ++i)
    {
        if (g_wbuf[i].status != NDSP_WBUF_DONE && g_wbuf[i].status != NDSP_WBUF_FREE)
            continue;
        int avail = (g_aw - g_ar) & (ARING - 1);
        if (avail < AWBUF_FR)
            return;

        for (int f = 0; f < AWBUF_FR; ++f)
        {
            int rp = (g_ar + f) & (ARING - 1);
            g_wbuf_mem[i][f * 2 + 0] = g_aring[rp * 2 + 0];
            g_wbuf_mem[i][f * 2 + 1] = g_aring[rp * 2 + 1];
        }
        g_ar = (g_ar + AWBUF_FR) & (ARING - 1);

        DSP_FlushDataCache(g_wbuf_mem[i], AWBUF_FR * 2 * sizeof(int16_t));
        g_wbuf[i].nsamples = AWBUF_FR;
        ndspChnWaveBufAdd(0, &g_wbuf[i]);
    }
}

static void xb_on_audio_sample(GBContext *ctx, int16_t left, int16_t right)
{
    (void)ctx;
    int nw = (g_aw + 1) & (ARING - 1);
    if (nw == g_ar)
        return;
    g_aring[g_aw * 2 + 0] = left;
    g_aring[g_aw * 2 + 1] = right;
    g_aw = nw;
}

/* --- Save data ---------------------------------------------------------- */

#define SAVE_DIR "sdmc:/3ds/linksawakening"

static void save_path(char *out, size_t n, const char *rom, const char *suffix)
{
    const char *base = rom ? rom : "rom";
    for (const char *p = base; *p; ++p)
        if (*p == '/' || *p == '\\')
            base = p + 1;
    snprintf(out, n, "%s/%s%s", SAVE_DIR, base, suffix);
}

static bool xb_load_battery_ram(GBContext *ctx, const char *rom, void *data, size_t size)
{
    (void)ctx;
    char path[256];
    save_path(path, sizeof(path), rom, ".sav");
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    size_t got = fread(data, 1, size, f);
    fclose(f);
    return got == size;
}

static bool xb_save_battery_ram(GBContext *ctx, const char *rom, const void *data, size_t size)
{
    (void)ctx;
    mkdir("sdmc:/3ds", 0777);
    mkdir(SAVE_DIR, 0777);
    char path[256];
    save_path(path, sizeof(path), rom, ".sav");
    FILE *f = fopen(path, "wb");
    if (!f)
        return false;
    size_t put = fwrite(data, 1, size, f);
    fclose(f);
    return put == size;
}

/* --- Renderizado --------------------------------------------------------- */

void gb_platform_render_frame(const uint32_t *framebuffer)
{
    if (!framebuffer)
        return;
    audio_update();

    // Copia lineal directa y limpia
    for (int y = 0; y < 144; ++y)
    {
        memcpy(&g_linear_fb[y * TEX_WIDTH], &framebuffer[y * 160], 160 * sizeof(uint32_t));
    }
    GSPGPU_FlushDataCache(g_linear_fb, TEX_WIDTH * TEX_HEIGHT * sizeof(uint32_t));

    // Conversión DMA a formato Tiled
    GX_DisplayTransfer(
        (u32 *)g_linear_fb,
        GX_BUFFER_DIM(TEX_WIDTH, TEX_HEIGHT),
        (u32 *)g_fb_texture.data,
        GX_BUFFER_DIM(TEX_WIDTH, TEX_HEIGHT),
        GX_TRANSFER_FLIP_VERT(0) | GX_TRANSFER_OUT_TILED(1) | GX_TRANSFER_RAW_COPY(0) |
            GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8) | GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8) |
            GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO));
    // gspWaitForPPF();

    C3D_FrameBegin(0);
    {
        C3D_FrameDrawOn(g_target_top);
        C3D_RenderTargetClear(g_target_top, C3D_CLEAR_ALL, 0x181818FF, 0);

        C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_ALL);
        C3D_FVUnifMtx4x4(GPU_VERTEX_SHADER, g_uLoc_mvp, &g_projection);
        C3D_TexBind(0, &g_fb_texture);

        C3D_DrawArrays(GPU_TRIANGLES, 0, 6);
    }
    C3D_FrameEnd(0);
}

/* --- Lifecycle ---------------------------------------------------------- */

bool gb_platform_init(int scale)
{
    (void)scale;
    osSetSpeedupEnable(true);

    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);
    gpu_init();
    audio_init();
    return true;
}

void gb_platform_shutdown(void)
{
    if (g_vbo_data)
        linearFree(g_vbo_data);
    if (g_linear_fb)
        linearFree(g_linear_fb);
    C3D_TexDelete(&g_fb_texture);
    shaderProgramFree(&g_program);
    DVLB_Free(g_vshader_dvlb);
    C3D_Fini();

    if (g_audio_ok)
    {
        ndspExit();
        for (int i = 0; i < AWBUF_N; ++i)
            if (g_wbuf_mem[i])
                linearFree(g_wbuf_mem[i]);
    }
    gfxExit();
    g_ctx = NULL;
}

static uint8_t xb_get_joypad(GBContext *ctx)
{
    uint8_t p1 = ctx->io ? ctx->io[0x00] : 0x00;
    uint8_t res = 0x0F;

    if (!(p1 & 0x10))
        res &= (g_joypad_buttons & 0x0F);
    if (!(p1 & 0x20))
        res &= (g_joypad_dpad & 0x0F);

    return (p1 & 0xF0) | res;
}

void gb_platform_register_context(GBContext *ctx)
{
    g_ctx = ctx;
    GBPlatformCallbacks cbs;
    memset(&cbs, 0, sizeof(cbs));
    cbs.get_joypad = xb_get_joypad;
    cbs.on_audio_sample = xb_on_audio_sample;
    cbs.load_battery_ram = xb_load_battery_ram;
    cbs.save_battery_ram = xb_save_battery_ram;
    gb_set_platform_callbacks(ctx, &cbs);
}

/* --- Input -------------------------------------------------------------- */

bool gb_platform_poll_events(GBContext *ctx)
{
    (void)ctx;
    if (g_quit || !aptMainLoop())
        return false;

    hidScanInput();
    uint32_t k = hidKeysHeld();

    uint8_t btns = 0xFF, dpad = 0xFF;

    if (k & (KEY_DRIGHT | KEY_CPAD_RIGHT))
        dpad &= ~(1 << 0);
    if (k & (KEY_DLEFT | KEY_CPAD_LEFT))
        dpad &= ~(1 << 1);
    if (k & (KEY_DUP | KEY_CPAD_UP))
        dpad &= ~(1 << 2);
    if (k & (KEY_DDOWN | KEY_CPAD_DOWN))
        dpad &= ~(1 << 3);

    if (k & KEY_A)
        btns &= ~(1 << 0);
    if (k & KEY_B)
        btns &= ~(1 << 1);
    if (k & KEY_SELECT)
        btns &= ~(1 << 2);
    if (k & KEY_START)
        btns &= ~(1 << 3);

    g_joypad_buttons = btns;
    g_joypad_dpad = dpad;
    return true;
}

uint8_t gb_platform_get_joypad(void)
{
    return g_joypad_buttons & g_joypad_dpad;
}

/* --- FPS Counter & VSync ------------------------------------------------ */

static u64 g_last_time = 0;
static int g_frames = 0;
static float g_fps = 0.0f;

void gb_platform_vsync(void)
{
    audio_update();

    g_frames++;
    u64 now = osGetTime();
    if (now - g_last_time >= 1000)
    {
        g_fps = (g_frames * 1000.0f) / (float)(now - g_last_time);
        g_frames = 0;
        g_last_time = now;
        printf("\x1b[10;1HFPS: %.2f  ", g_fps);
    }
}

void gb_platform_set_title(const char *title) { (void)title; }

#define STATE_PATH SAVE_DIR "/state.bin"
#define STATE_MAGIC 0x4C413344u
#define STATE_VERSION 1u

#define SS_WRAM 0x8000
#define SS_VRAM 0x4000
#define SS_OAM 0xA0
#define SS_HRAM 0x7F
#define SS_IO 0x80

void gb_platform_save_state(GBContext *ctx)
{
    if (!ctx)
        return;
    mkdir("sdmc:/3ds", 0777);
    mkdir(SAVE_DIR, 0777);
    FILE *f = fopen(STATE_PATH, "wb");
    if (!f)
        return;

    uint32_t magic = STATE_MAGIC, ver = STATE_VERSION;
    fwrite(&magic, 4, 1, f);
    fwrite(&ver, 4, 1, f);
    fwrite(&ctx->af, 2, 1, f);
    fwrite(&ctx->bc, 2, 1, f);
    fwrite(&ctx->de, 2, 1, f);
    fwrite(&ctx->hl, 2, 1, f);
    fwrite(&ctx->sp, 2, 1, f);
    fwrite(&ctx->pc, 2, 1, f);
    fwrite(&ctx->f_z, 1, 1, f);
    fwrite(&ctx->f_n, 1, 1, f);
    fwrite(&ctx->f_h, 1, 1, f);
    fwrite(&ctx->f_c, 1, 1, f);
    fwrite(&ctx->ime, 1, 1, f);
    fwrite(&ctx->halted, 1, 1, f);
    fwrite(&ctx->rom_bank, 2, 1, f);
    fwrite(&ctx->ram_bank, 1, 1, f);
    fwrite(&ctx->wram_bank, 1, 1, f);
    fwrite(&ctx->vram_bank, 1, 1, f);
    fwrite(&ctx->cycles, 4, 1, f);
    if (ctx->wram)
        fwrite(ctx->wram, 1, SS_WRAM, f);
    if (ctx->vram)
        fwrite(ctx->vram, 1, SS_VRAM, f);
    if (ctx->oam)
        fwrite(ctx->oam, 1, SS_OAM, f);
    if (ctx->hram)
        fwrite(ctx->hram, 1, SS_HRAM, f);
    if (ctx->io)
        fwrite(ctx->io, 1, SS_IO, f);
    uint32_t esz = ctx->eram ? (uint32_t)ctx->eram_size : 0;
    fwrite(&esz, 4, 1, f);
    if (ctx->eram && esz)
        fwrite(ctx->eram, 1, esz, f);
    if (ctx->ppu)
        fwrite(ctx->ppu, sizeof(GBPPU), 1, f);
    fclose(f);
}

void gb_platform_load_state(GBContext *ctx)
{
    if (!ctx)
        return;
    FILE *f = fopen(STATE_PATH, "rb");
    if (!f)
        return;
    uint32_t magic = 0, ver = 0;
    fread(&magic, 4, 1, f);
    fread(&ver, 4, 1, f);
    if (magic != STATE_MAGIC || ver != STATE_VERSION)
    {
        fclose(f);
        return;
    }
    fread(&ctx->af, 2, 1, f);
    fread(&ctx->bc, 2, 1, f);
    fread(&ctx->de, 2, 1, f);
    fread(&ctx->hl, 2, 1, f);
    fread(&ctx->sp, 2, 1, f);
    fread(&ctx->pc, 2, 1, f);
    fread(&ctx->f_z, 1, 1, f);
    fread(&ctx->f_n, 1, 1, f);
    fread(&ctx->f_h, 1, 1, f);
    fread(&ctx->f_c, 1, 1, f);
    fread(&ctx->ime, 1, 1, f);
    fread(&ctx->halted, 1, 1, f);
    fread(&ctx->rom_bank, 2, 1, f);
    fread(&ctx->ram_bank, 1, 1, f);
    fread(&ctx->wram_bank, 1, 1, f);
    fread(&ctx->vram_bank, 1, 1, f);
    fread(&ctx->cycles, 4, 1, f);
    if (ctx->wram)
        fread(ctx->wram, 1, SS_WRAM, f);
    if (ctx->vram)
        fread(ctx->vram, 1, SS_VRAM, f);
    if (ctx->oam)
        fread(ctx->oam, 1, SS_OAM, f);
    if (ctx->hram)
        fread(ctx->hram, 1, SS_HRAM, f);
    if (ctx->io)
        fread(ctx->io, 1, SS_IO, f);
    uint32_t esz = 0;
    fread(&esz, 4, 1, f);
    if (ctx->eram && esz <= ctx->eram_size)
        fread(ctx->eram, 1, esz, f);
    if (ctx->ppu)
        fread(ctx->ppu, sizeof(GBPPU), 1, f);
    fclose(f);
}

void gb_platform_set_input_script(const char *path) { (void)path; }
void gb_platform_set_dump_frames(const char *dir) { (void)dir; }
void gb_platform_set_screenshot_prefix(const char *prefix) { (void)prefix; }