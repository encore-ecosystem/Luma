/* Real Vulkan readback test. Link against the development vulkan runtime. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../runtime.c"

static UiSdlApi mock_sdl;
const UiSdlApi *encore_sdl_api_v1(void) { return &mock_sdl; }
void *encore_str_from_cstr(const char *text) {
    size_t size = strlen(text);
    encore_str_object *s = malloc(sizeof(*s) + size + 1);
    s->ref_count = 1; s->len = size; memcpy(s->data, text, size + 1); return s;
}
static void present(size_t ui) {
    for (int attempt = 0; attempt < 1000; ++attempt) {
        if (luma_vk_ui_end(ui)) return;
        assert(luma_vk_ui_acquire_status(ui) == ENCORE_VK_FRAME_PENDING);
        struct timespec delay = {0, 1000000}; nanosleep(&delay, NULL);
    }
    assert(!"frame remained pending");
}
int main(void) {
    if(getenv("LUMA_TEST_PARALLEL"))assert(encore_vk_parallel_configure(4));
    size_t context = encore_vk_create(false);
    assert(encore_vk_available(context));
    size_t device = encore_vk_device_create(context, 0);
    assert(encore_vk_device_available(device));
    size_t swapchain = encore_vk_offscreen_create(device, 128, 128);
    size_t compositor = luma_vk_ui_create(device, swapchain);
    assert(luma_vk_ui_available(compositor));
    UiWindow window = {0}; window.open = true; window.renderer_kind = 3;
    window.direct_compositor = compositor; window.pixel_density = 1;
    assert(ui_register_window(&window));
    size_t handle = (size_t)&window;
    encore_str source = {(encore_str_object *)encore_str_from_cstr("line")};
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)compositor;
    for (int density_index = 0; density_index < 3; ++density_index) {
        float density = 1.0f + (float)density_index * 0.5f;
        window.pixel_density = density;
        assert(luma_vk_ui_set_logical_extent(compositor, (uint32_t)(128 / density), (uint32_t)(128 / density)));
        for (int frame = 0; frame < 100; ++frame) {
            assert(luma_vk_ui_cache_begin(compositor, 0xffffffff));
            float x = 8.0f + (float)(frame % 20) * 0.25f;
            uint32_t result = luma_vector_cache_begin(handle, source, 1, 2, x, 8, 0x000000ff);
            if (frame == 0) {
                assert(result == 2);
                assert(luma_vector_path_begin(handle, 8, 8));
                assert(luma_vector_path_point(handle, 32, 8));
                assert(luma_vector_path_stroke(handle, 2, 0xffffffff, false));
                assert(luma_vector_cache_end(handle));
            } else assert(result == 1);
            assert(luma_vk_ui_cache_end(compositor));
            uint64_t before = ui->mesh_upload_bytes;
            present(compositor);
            assert(encore_vk_swapchain_wait(swapchain));
            if (frame != 0) assert(ui->mesh_upload_bytes == before);
            const unsigned char *pixels = (const unsigned char *)encore_vk_swapchain_pixels(swapchain);
            assert(pixels && pixels[0] == 255);
            uint32_t stride = encore_vk_swapchain_pitch(swapchain);
            float actual = 128.0f / (float)(uint32_t)(128 / density);
            size_t y = (size_t)(16.0f * actual);
            size_t left = (size_t)((x + 12) * actual), right = (size_t)((x + 28) * actual);
            /* Horizontal stroke stays straight at quarter-pixel translations. */
            for (size_t px = left; px <= right; ++px)
                assert(pixels[y * stride + px * 4] < 80);
        }
    }
    assert(ui->mesh_builds == 3 && ui->mesh_hits == 297);
    /* Evict an atlas page still used by the retained base. Replaying that
     * immutable snapshot must remain valid until a repaint drops its refs. */
    assert(luma_vk_ui_set_logical_extent(compositor,128,128));
    uint32_t red = 0xff0000ffu;
    size_t texture = luma_vk_ui_texture_create(compositor,&red,1,1,4,37);
    assert(texture);
    assert(luma_vk_ui_cache_begin(compositor,0xffffffff));
    assert(luma_vk_ui_texture_draw(compositor,texture,0,0,128,128));
    assert(luma_vk_ui_cache_end(compositor));
    present(compositor); assert(encore_vk_swapchain_wait(swapchain));
    assert(luma_vk_ui_texture_destroy(texture));
    assert(ui->retired_texture_count == 1);
    for (int frame=0;frame<4;++frame) {
        assert(luma_vk_ui_cached_begin(compositor,0xffffffff));
        present(compositor); assert(encore_vk_swapchain_wait(swapchain));
        const unsigned char *pixel=(void *)encore_vk_swapchain_pixels(swapchain);
        assert(pixel);
        assert(pixel[0]==0 && pixel[1]==0 && pixel[2]==255);
        assert(ui->retired_texture_count==1);
    }
    assert(luma_vk_ui_cache_begin(compositor,0xffffffff));
    assert(luma_vk_ui_cache_end(compositor));
    for(int frame=0;frame<2;++frame){
        assert(luma_vk_ui_cached_begin(compositor,0xffffffff));
        present(compositor); assert(encore_vk_swapchain_wait(swapchain));
    }
    assert(ui->retired_texture_count==0);
    puts("Retained atlas eviction: cached pixels preserved, both slots retire, texture reclaimed");
    /* Exercise the public RGBA payload path, including its format selection.
     * Reading both primary colors catches an RGBA/BGRA upload mismatch. */
    unsigned char rgba_payload[] = {2,0,0,0,1,0,0,0,255,0,0,255,0,0,255,255};
    encore_str_object *rgba_object = malloc(sizeof(*rgba_object) + sizeof(rgba_payload));
    assert(rgba_object); rgba_object->ref_count = 1; rgba_object->len = sizeof(rgba_payload);
    memcpy(rgba_object->data, rgba_payload, sizeof(rgba_payload));
    encore_str rgba = {rgba_object};
    assert(luma_vk_ui_cache_begin(compositor, 0x000000ff));
    assert(luma_image_rgba(handle, 0, 0, 128, 128, rgba));
    assert(luma_vk_ui_cache_end(compositor));
    present(compositor); assert(encore_vk_swapchain_wait(swapchain));
    const unsigned char *rgba_pixels = (void *)encore_vk_swapchain_pixels(swapchain);
    uint32_t rgba_pitch = encore_vk_swapchain_pitch(swapchain);
    const unsigned char *left = rgba_pixels + 64u * rgba_pitch + 16u * 4u;
    const unsigned char *right = rgba_pixels + 64u * rgba_pitch + 112u * 4u;
    assert(left[2] > 240 && left[1] < 15 && left[0] < 15);
    assert(right[0] > 240 && right[1] < 15 && right[2] < 15);
    ui_clear_raster_cache(&window); free(rgba_object);
    puts("RGBA payload readback: red and blue channels preserved");
    ui_unregister_window(&window);
    /* The fixture owns a stack UiWindow rather than a normal native window.
       Mirror its vector-buffer teardown without freeing the stack object. */
    free(window.vector_vertices);
    free(window.vector_indices);
    free(source.object);
    assert(luma_vk_ui_destroy(compositor));
    assert(encore_vk_swapchain_destroy(swapchain));
    assert(encore_vk_device_destroy(device)); assert(encore_vk_destroy(context));
    puts("Vulkan vector readback: 300 frames, 1/1.5/2x DPI, subpixel translation, zero warm mesh uploads passed");
}
