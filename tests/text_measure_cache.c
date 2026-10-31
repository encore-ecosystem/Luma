#include <assert.h>
#include "sdl_mock.h"
#include "../workspace/ui_native/backend.c"

static unsigned measurements;
static unsigned atlas_creations, atlas_updates, atlas_destroys;
size_t luma_vk_ui_texture_create_empty(size_t h, uint32_t w, uint32_t v, uint32_t f) {
    (void)h; assert(w == 2048 && v == 2048 && f == 44);
    ++atlas_creations; return 1;
}
bool luma_vk_ui_texture_update(size_t h, const void *pixels, uint32_t w,
    uint32_t v, uint32_t pitch, uint32_t x, uint32_t y) {
    assert(h == 1 && pixels && w == 2 && v == 1 && pitch == 8);
    assert(x == atlas_updates * 3 && y == 0);
    ++atlas_updates; return true;
}
bool luma_vk_ui_texture_destroy(size_t h) { assert(h == 1); ++atlas_destroys; return true; }
static bool measure(UiTtfFont *font, const char *text, size_t length,
        int *width, int *height) {
    (void)font; (void)text;
    ++measurements;
    *width = (int)length * 8;
    *height = 16;
    return true;
}

int main(void) {
    UiWindow *state = calloc(1, sizeof(*state));
    assert(state != NULL);
    state->font_size = 16;
    state->font_weight = 400;
    state->pixel_density = 1;
    test_ttf.GetStringSize = measure;
    int width, height;
    assert(ui_measure_text(state, "Icons", 5, &width, &height));
    assert(width == 40 && height == 16 && measurements == 1);
    assert(ui_measure_text(state, "Icons", 5, &width, &height));
    assert(measurements == 1);
    state->font_weight = 700;
    assert(ui_measure_text(state, "Icons", 5, &width, &height));
    state->font_size = 20;
    assert(ui_measure_text(state, "Icons", 5, &width, &height));
    state->current_face = 1;
    assert(ui_measure_text(state, "Icons", 5, &width, &height));
    state->pixel_density = 2;
    assert(ui_measure_text(state, "Icons", 5, &width, &height));
    assert(measurements == 5);
    ui_clear_text_measure_cache(state);
    assert(ui_measure_text(state, "Icons", 5, &width, &height));
    assert(measurements == 6);
    ui_clear_text_measure_cache(state);
    state->renderer_kind = 3;
    uint32_t pixels[2] = {0xffffffff, 0xffffffff};
    UiSdlSurface surface = {0};
    surface.width = 2; surface.height = 1; surface.pitch = 8;
    surface.pixels = pixels;
    int x, y;
    uint32_t generation;
    assert(ui_text_atlas_upload(state, &surface, &x, &y, &generation));
    assert(generation != 0 && x == 0 && y == 0);
    assert(ui_text_atlas_upload(state, &surface, &x, &y, &generation));
    assert(generation != 0 && x == 3 && y == 0);
    assert(atlas_creations == 1 && atlas_updates == 2);
    ui_clear_text_cache(state);
    assert(atlas_destroys == 1);
    free(state);
    return 0;
}
