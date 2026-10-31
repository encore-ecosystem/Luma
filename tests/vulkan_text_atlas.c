/* Exercise deferred atlas binding without a GPU. The mock enforces the same
 * image-layout contract as the Vulkan package, so premature binding fails. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../workspace/ui_native/vulkan_compositor.inc"

static uint32_t image_layout;
static unsigned binds, submits, copies, waits, groups_destroyed;
static bool reject_group;
static unsigned char staging[8 * 8 * 4];
uint32_t encore_vk_swapchain_width(size_t h) { (void)h; return 100; }
uint32_t encore_vk_swapchain_height(size_t h) { (void)h; return 100; }

size_t encore_vk_texture_create(size_t d, uint32_t w, uint32_t h,
    uint32_t m, uint32_t f, uint32_t u) {
    (void)d; (void)m; (void)f; (void)u;
    assert(w == 8 && h == 8);
    image_layout = VK_IMAGE_LAYOUT_UNDEFINED;
    return 1;
}
bool encore_vk_texture_available(size_t h) { return h == 1; }
bool encore_vk_texture_destroy(size_t h) { assert(h == 1); return true; }
size_t encore_vk_texture_bind_group_create(size_t d, size_t l, size_t t, size_t s) {
    (void)d; (void)l; (void)t; (void)s;
    assert(image_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    assert(waits > 0);
    ++binds;
    return reject_group ? 9 : 2;
}
bool encore_vk_bind_group_available(size_t h) { return h == 2; }
bool encore_vk_bind_group_destroy(size_t h) { assert(h == 2 || h == 9); ++groups_destroyed; return true; }
size_t encore_vk_buffer_create(size_t d, uint64_t n, uint32_t u, bool v) {
    (void)d; (void)u; assert(v && n <= sizeof staging); return 3;
}
bool encore_vk_buffer_available(size_t h) { return h == 3; }
void *encore_vk_buffer_map(size_t h, uint64_t n) { assert(h == 3 && n <= sizeof staging); return staging; }
void encore_vk_buffer_unmap(size_t h) { assert(h == 3); }
bool encore_vk_buffer_destroy(size_t h) { assert(h == 3); return true; }
size_t encore_vk_encoder_create(size_t d) { (void)d; return 4; }
bool encore_vk_encoder_available(size_t h) { return h == 4; }
bool encore_vk_encoder_destroy(size_t h) { assert(h == 4); return true; }
bool encore_vk_encoder_transition_texture(size_t e, size_t t, uint32_t layout) {
    assert(e == 4 && t == 1); image_layout = layout; return true;
}
bool encore_vk_encoder_copy_buffer_to_texture_region(size_t e, size_t b, size_t t,
    uint32_t w, uint32_t h, uint32_t m, uint32_t x, uint32_t y) {
    assert(e == 4 && b == 3 && t == 1 && m == 0);
    assert(image_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    if (copies == 0) {
        assert(w == 8 && h == 8 && x == 0 && y == 0);
        /* Initial upload includes zeroed gutters, not uninitialized GPU memory. */
        assert(staging[0] == 0 && staging[sizeof staging - 1] == 0);
        assert(staging[(2 * 8 + 2) * 4] == 255);
    } else if (copies == 1) {
        assert(w == 1 && h == 1 && x == 4 && y == 5);
    }
    ++copies; return true;
}
size_t encore_vk_encoder_submit(size_t h) { assert(h == 4); ++submits; return 5; }
bool encore_vk_submission_available(size_t h) { return h == 5; }
bool encore_vk_submission_wait(size_t h, uint64_t n) { assert(h == 5 && n == UINT64_MAX); ++waits; return true; }
bool encore_vk_submission_destroy(size_t h) { assert(h == 5); return true; }

int main(void) {
    LumaVkUiCompositor ui = {0};
    ui.available = true;
    LumaVkUiTexture *page = luma_vk_ui_texture_empty(&ui, 8, 8, 44);
    assert(page && page->group == NULL && binds == 0 && submits == 0);
    uint32_t pixel = 0xffffffff;
    assert(luma_vk_ui_texture_update((size_t)page, &pixel, 1, 1, 4, 2, 2));
    assert(luma_vk_ui_texture_update((size_t)page, &pixel, 1, 1, 4, 3, 3));
    /* Recording is legal before the single, aggregated upload. */
    assert(luma_vk_ui_texture_draw_region((size_t)&ui, (size_t)page, 0, 0, 1, 1, 2, 2, 1, 1));
    assert(luma_vk_ui_texture_flush(page));
    assert(binds == 1 && submits == 1 && copies == 1);
    assert(luma_vk_ui_texture_flush(page));
    assert(submits == 1);
    assert(luma_vk_ui_texture_update((size_t)page, &pixel, 1, 1, 4, 4, 5));
    assert(luma_vk_ui_texture_flush(page));
    assert(binds == 1 && submits == 2 && copies == 2);
    luma_vk_ui_texture_release(page);
    page = luma_vk_ui_texture_empty(&ui, 8, 8, 44);
    reject_group = true;
    assert(!luma_vk_ui_texture_flush(page));
    assert(page->group == NULL && page->atlas_dirty);
    reject_group = false;
    assert(luma_vk_ui_texture_flush(page));
    luma_vk_ui_texture_release(page);
    assert(groups_destroyed == 3);
    free(ui.vertices); free(ui.indices); free(ui.batches);
    puts("Vulkan text atlas: passed");
}
