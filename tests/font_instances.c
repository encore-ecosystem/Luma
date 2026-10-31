/* Run from Luma root; real SDL_ttf/HarfBuzz, no window or display required. */
#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <hb-ot.h>
#include <assert.h>
#include <dlfcn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef TTF_Font UiTtfFont;
typedef struct UiFontInstanceCache UiFontInstanceCache;
static struct { SDL_IOStream *(*IOFromConstMem)(const void *, size_t); const char *(*GetError)(void); bool (*CloseIO)(SDL_IOStream *); }
    g_sdl = {SDL_IOFromConstMem, SDL_GetError, SDL_CloseIO};
static struct { TTF_Font *(*OpenFontIO)(SDL_IOStream *, bool, float); void (*SetFontStyle)(TTF_Font *, TTF_FontStyleFlags); }
    g_ttf = {TTF_OpenFontIO, TTF_SetFontStyle};
static void *ui_symbol(void *lib, const char *name) { return dlsym(lib, name); }
static void ui_set_error(const char *error) { fprintf(stderr, "font: %s\n", error); }
static char *ui_copy_cstr(const char *text) {
    char *copy = malloc(strlen(text) + 1);
    if (copy) strcpy(copy, text);
    return copy;
}
#include "../workspace/ui_native/font_instances.inc"
_Static_assert(sizeof(UiHbAxis) == sizeof(hb_ot_var_axis_info_t), "HarfBuzz axis ABI");
_Static_assert(offsetof(UiHbAxis, reserved) == offsetof(hb_ot_var_axis_info_t, reserved), "HarfBuzz axis offsets");

static uint64_t ink(SDL_Surface *surface) {
    uint64_t sum = 0;
    for (int y = 0; y < surface->h; ++y) for (int x = 0; x < surface->w; ++x) {
        Uint8 r, g, b, a;
        assert(SDL_ReadSurfacePixel(surface, x, y, &r, &g, &b, &a));
        sum += a;
    }
    return sum;
}

int main(void) {
    assert(TTF_Init());
    UiFontInstanceCache *cache = NULL;
    const char *paths[] = {
        "assets/fonts/Inter/Inter-VariableFont_opsz,wght.ttf",
        "assets/fonts/OpenSans/OpenSans-VariableFont_wdth,wght.ttf",
        "assets/fonts/Roboto/Roboto-VariableFont_wdth,wght.ttf",
        "assets/fonts/RobotoMono/RobotoMono-VariableFont_wght.ttf",
        "assets/fonts/Inter/Inter-Italic-VariableFont_opsz,wght.ttf",
        "assets/fonts/OpenSans/OpenSans-Italic-VariableFont_wdth,wght.ttf",
        "assets/fonts/Roboto/Roboto-Italic-VariableFont_wdth,wght.ttf"
    };
    SDL_Surface *preview = SDL_CreateSurface(1050, 620, SDL_PIXELFORMAT_RGBA32);
    assert(preview);
    assert(SDL_FillSurfaceRect(preview, NULL, SDL_MapSurfaceRGBA(preview, 18, 18, 20, 255)));
    for (size_t f = 0; f < sizeof(paths) / sizeof(paths[0]); ++f) {
        uint64_t previous = 0;
        for (unsigned weight = 400; weight <= 700; weight += 50) {
            void *backing = NULL;
            TTF_Font *font = ui_open_weighted_font(&cache, paths[f], 42, weight, &backing);
            assert(font && backing);
            assert(!(TTF_GetFontStyle(font) & TTF_STYLE_BOLD));
            char text[160];
            snprintf(text, sizeof(text), "%u  Hamburgefonts — Привет ffi 012345", weight);
            SDL_Surface *surface = TTF_RenderText_Blended(font, "Hamburgefonts Привет ffi 012345", 0, (SDL_Color){255,255,255,255});
            assert(surface);
            uint64_t current = ink(surface);
            assert(current > previous); previous = current;
            SDL_DestroySurface(surface);
            if (f == 0) {
                surface = TTF_RenderText_Blended(font, text, 0, (SDL_Color){235,235,240,255});
                assert(surface);
                SDL_Rect destination = {20, 20 + (int)(weight - 400) * 80 / 50, surface->w, surface->h};
                assert(SDL_BlitSurface(surface, NULL, preview, &destination));
                SDL_DestroySurface(surface);
            }
            void *second = NULL;
            TTF_Font *resized = ui_open_weighted_font(&cache, paths[f], 21, weight, &second);
            assert(resized && ((UiFontBacking *)second)->blob == ((UiFontBacking *)backing)->blob);
            TTF_CloseFont(resized); ui_font_backing_release(second);
            TTF_CloseFont(font); ui_font_backing_release(backing);
        }
    }
    assert(SDL_SaveBMP(preview, "target/variable-font-weights.bmp"));
    SDL_DestroySurface(preview);
    /* A live font must survive both LRU eviction and complete cache cleanup. */
    void *held = NULL;
    TTF_Font *font = ui_open_weighted_font(&cache, paths[0], 24, 450, &held);
    assert(font);
    for (unsigned weight = 200; weight < 900; weight += 17) {
        bool variable = false;
        void *blob = ui_font_instance(&cache, paths[1], weight, &variable);
        assert(blob && variable && cache->count <= 16 && cache->bytes <= 16u * 1024u * 1024u);
        ui_font_blob_release(blob);
    }
    ui_font_instances_clear(&cache);
    SDL_Surface *surface = TTF_RenderText_Blended(font, "Still alive", 0, (SDL_Color){255,255,255,255});
    assert(surface && ink(surface));
    SDL_DestroySurface(surface); TTF_CloseFont(font); ui_font_backing_release(held);
    /* Out-of-range weights are clamped to the font's supported range. */
    bool variable = false;
    void *low = ui_font_instance(&cache, paths[0], 1, &variable);
    void *minimum = ui_font_instance(&cache, paths[0], 100, &variable);
    unsigned a = 0, b = 0;
    const char *first = g_hb.blob_data(low, &a), *second = g_hb.blob_data(minimum, &b);
    assert(a == b && memcmp(first, second, a) == 0);
    ui_font_blob_release(low); ui_font_blob_release(minimum);
    void *static_blob = ui_font_instance(&cache, paths[0], 425, &variable);
    assert(static_blob && variable);
    unsigned static_size = 0;
    const char *static_data = g_hb.blob_data(static_blob, &static_size);
    FILE *fixture = fopen("target/font-static-fixture.ttf", "wb");
    assert(fixture && fwrite(static_data, 1, static_size, fixture) == static_size);
    assert(fclose(fixture) == 0);
    ui_font_blob_release(static_blob);
    void *static_backing = NULL;
    TTF_Font *static_font = ui_open_weighted_font(&cache,
        "target/font-static-fixture.ttf", 24, 700, &static_backing);
    assert(static_font && (TTF_GetFontStyle(static_font) & TTF_STYLE_BOLD));
    TTF_CloseFont(static_font); ui_font_backing_release(static_backing);
    void *bad = NULL;
    assert(!ui_open_weighted_font(&cache, "encore.toml", 24, 400, &bad) && !bad);
    ui_font_instances_clear(&cache);
    g_hb.ready = false;
    assert(!ui_open_weighted_font(&cache, paths[0], 24, 425, &bad) && !bad);
    g_hb.ready = true;
    TTF_Quit();
    puts("Variable weights: 7 faces, 400..700 by 50, size reuse, bounds, LRU and ownership passed");
    return 0;
}
