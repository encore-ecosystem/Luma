/* CPU-side retained Vulkan command tests; no GPU/window required.
 * cc -O1 -g -ffunction-sections -fdata-sections -fsanitize=address,undefined
 *    tests/vulkan_retained_patches.c -Wl,--gc-sections -lm -o target/vulkan-retained-patches
 */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../workspace/ui_native/vulkan_compositor.inc"

uint32_t encore_vk_swapchain_width(size_t handle) { (void)handle; return 100; }
uint32_t encore_vk_swapchain_height(size_t handle) { (void)handle; return 100; }

static void patch(LumaVkUiCompositor *ui, float extent, uint32_t color) {
    assert(luma_vk_ui_cache_patch_begin((size_t)ui, color, 0, 0, extent, extent));
    assert(luma_vk_ui_cache_end((size_t)ui));
}

static void check_batches(LumaVkUiCompositor *ui) {
    size_t covered = 0;
    for (size_t i = 0; i < ui->batch_count; ++i) {
        LumaVkUiBatch *batch = &ui->batches[i];
        assert(batch->first_index == covered);
        covered += batch->index_count;
        assert(covered <= ui->index_count);
    }
    assert(covered == ui->index_count);
    for (size_t i = 0; i < ui->index_count; ++i)
        assert(ui->indices[i] < ui->vertex_count);
}

int main(void) {
    int32_t x = 10, y = 120;
    assert(!luma_vk_ui_visible_scissor(&x, &y, 90, 100));
    x = 100; y = 10;
    assert(!luma_vk_ui_visible_scissor(&x, &y, 100, 90));
    x = -10; y = -20;
    assert(luma_vk_ui_visible_scissor(&x, &y, 20, 30));
    assert(x == 0 && y == 0);
    LumaVkUiCompositor ui = {0};
    ui.available = true;
    ui.logical_width = ui.logical_height = 100;
    assert(luma_vk_ui_cache_begin((size_t)&ui, 0xffffffff));
    assert(luma_vk_ui_cache_end((size_t)&ui));

    /* Hover the button, hover its parent, then update the button while the
     * pointer stays on it. The newest button must be painted AFTER the old
     * parent snapshot, even though its bounds already have a cache entry. */
    patch(&ui, 20, 0xff0000ff);
    patch(&ui, 100, 0x00ff00ff);
    patch(&ui, 20, 0x0000ffff);
    assert(ui.patch_count == 2);
    assert(ui.patches[1].key == luma_vk_ui_patch_bounds_key(0, 0, 20, 20));
    assert(ui.patches[1].vertices[0].b == 1.0f);
    check_batches(&ui);

    /* Each in-flight slot has independent residency. Trimming transient
     * overlays or replacing equally-sized geometry invalidates it as well. */
    assert(!luma_vk_ui_geometry_resident(&ui, 0));
    ui.uploaded_revision[0] = ui.geometry_revision;
    ui.uploaded_vertices[0] = ui.vertex_count;
    ui.uploaded_indices[0] = ui.index_count;
    assert(luma_vk_ui_geometry_resident(&ui, 0));
    assert(!luma_vk_ui_geometry_resident(&ui, 1));
    assert(!luma_vk_ui_geometry_resident(&ui, 2));
    assert(luma_vk_ui_cached_begin((size_t)&ui, 0xffffffff));
    assert(luma_vk_ui_geometry_resident(&ui, 0));
    patch(&ui, 20, 0x0000ffff);
    assert(!luma_vk_ui_geometry_resident(&ui, 0));

    /* Repeating a region must not merge its first batch into the previous
     * retained segment, drop its draw commands, or grow the cache forever. */
    for (size_t i = 0; i < 100; ++i) {
        patch(&ui, 20, 0x0000ffff);
        assert(ui.patch_count == 2);
        assert(ui.patches[1].batch_count == 1);
        check_batches(&ui);
    }
    patch(&ui, 100, 0xffffffff);
    assert(ui.patches[ui.patch_count - 1].key ==
        luma_vk_ui_patch_bounds_key(0, 0, 100, 100));
    check_batches(&ui);

    /* Fractional logical damage must use outward-rounded clipping, without
     * clearing a wider strip than subsequent scene replay can repaint. */
    assert(luma_vk_ui_cache_patch_begin((size_t)&ui, 0xffffffff,
        0.5f, 1.5f, 10.0f, 10.0f));
    assert(ui.clip_x == 0 && ui.clip_y == 1);
    assert(ui.clip_width == 11 && ui.clip_height == 11);
    assert(luma_vk_ui_cache_end((size_t)&ui));
    check_batches(&ui);

    /* A resized viewport uses a useful region of a larger allocation, not
     * UVs covering the entire capacity (which would stretch padded pixels). */
    unsigned char resident = 0;
    LumaVkUiTexture image = {.owner = &ui, .width = 128, .height = 64,
        .atlas_pixels = &resident};
    size_t first_vertex = ui.vertex_count;
    assert(luma_vk_ui_texture_draw_region((size_t)&ui, (size_t)&image,
        10, 20, 64, 32, 16, 8, 64, 32));
    assert(ui.vertices[first_vertex].x == 10);
    assert(ui.vertices[first_vertex].y == 20);
    assert(ui.vertices[first_vertex].u == 0.125f);
    assert(ui.vertices[first_vertex].v == 0.125f);
    assert(ui.vertices[first_vertex + 2].x == 74);
    assert(ui.vertices[first_vertex + 2].y == 52);
    assert(ui.vertices[first_vertex + 2].u == 0.625f);
    assert(ui.vertices[first_vertex + 2].v == 0.625f);
    size_t count = ui.vertex_count;
    assert(!luma_vk_ui_texture_draw_region((size_t)&ui, (size_t)&image,
        0, 0, 64, 32, 65, 0, 64, 32));
    assert(!luma_vk_ui_texture_draw_region((size_t)&ui, (size_t)&image,
        0, 0, 64, 32, UINT32_MAX, 0, 64, 32));
    assert(!luma_vk_ui_texture_draw_region((size_t)&ui, (size_t)&image,
        0, 0, 64, 32, 0, 0, 0, 32));
    assert(ui.vertex_count == count);
    check_batches(&ui);

    luma_vk_ui_patches_clear(&ui);
    free(ui.patches);
    free(ui.vertices);
    free(ui.indices);
    free(ui.batches);
    puts("retained Vulkan patches: passed");
    return 0;
}
