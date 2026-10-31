#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../workspace/ui_native/vulkan_compositor.inc"
uint32_t encore_vk_swapchain_width(size_t h) { (void)h; return 100; }
uint32_t encore_vk_swapchain_height(size_t h) { (void)h; return 100; }
bool encore_vk_buffer_destroy(size_t h) { (void)h; return true; }

int main(void) {
    LumaVkUiCompositor ui = {0}; ui.available = true;
    luma_vk_ui_mesh_free_range(&ui, 0, 1024);
    size_t a = 0, b = 0;
    assert(luma_vk_ui_mesh_allocate(&ui, 256, &a) && a == 0);
    assert(luma_vk_ui_mesh_allocate(&ui, 512, &b) && b == 256);
    luma_vk_ui_mesh_free_range(&ui, a, 256);
    luma_vk_ui_mesh_free_range(&ui, b, 512);
    assert(ui.mesh_free->offset == 0 && ui.mesh_free->bytes == 1024 && !ui.mesh_free->next);
    assert(luma_vk_ui_mesh_allocate(&ui, 1024, &a) && !ui.mesh_free);
    ui.logical_width = ui.logical_height = 100;
    encore_str key = {malloc(sizeof(encore_str_object) + 4)};
    key.object->len = 3; memcpy(key.object->data, "svg", 4);
    const float vertices[3][8] = {{0,0,1,1,1,1,-1e30f,-1e30f},
        {10,0,1,1,1,1,-1e30f,-1e30f},{0,10,1,1,1,1,-1e30f,-1e30f}};
    int indices[3] = {0,1,2};
    assert(luma_vk_ui_cache_begin((size_t)&ui, 0));
    assert(luma_vk_ui_mesh_begin((size_t)&ui,key,1,2,1.5f,20,30,0xffffffff) == 2);
    assert(luma_vk_ui_geometry((size_t)&ui, &vertices[0][0],3,8,2,indices,3));
    assert(luma_vk_ui_mesh_end((size_t)&ui));
    LumaVkUiMesh *mesh = ui.meshes;
    assert(mesh && mesh->vertices[1].x == 10 && mesh->indices[2] == 2);
    assert(ui.vertex_count == 0 && ui.index_count == 0 && ui.batch_count == 1);
    assert(luma_vk_ui_cache_end((size_t)&ui));
    size_t bytes = ui.mesh_bytes;
    for (int i = 0; i < 1000; ++i) {
        assert(luma_vk_ui_cache_begin((size_t)&ui, 0));
        assert(luma_vk_ui_set_logical_extent((size_t)&ui, 200 + i, 300 + i));
        assert(luma_vk_ui_mesh_begin((size_t)&ui,key,1,2,1.5f,(float)i/3,30,0x101010ff) == 1);
        assert(luma_vk_ui_cache_end((size_t)&ui));
    }
    assert(ui.mesh_builds == 1 && ui.mesh_hits == 1000 && ui.mesh_bytes == bytes);
    assert(ui.batches[0].mesh == mesh && ui.batches[0].color == 0x101010ff);
    luma_vk_ui_mesh_trim(&ui, LUMA_VECTOR_CACHE_BYTES);
    assert(ui.meshes == mesh); /* retained commands pin it */
    assert(luma_vk_ui_cache_begin((size_t)&ui, 0));
    mesh->pending_slots = 1;
    luma_vk_ui_mesh_trim(&ui, LUMA_VECTOR_CACHE_BYTES);
    assert(ui.meshes == mesh); /* an in-flight frame pins it */
    mesh->pending_slots = 0;
    luma_vk_ui_mesh_trim(&ui, LUMA_VECTOR_CACHE_BYTES);
    assert(ui.meshes == NULL && ui.mesh_bytes == 0);
    for (int variant = 0; variant < 3; ++variant) {
        assert(luma_vk_ui_mesh_begin((size_t)&ui,key,1 + variant,2,1,0,0,0xffffffff) == 2);
        assert(luma_vk_ui_geometry((size_t)&ui, &vertices[0][0],3,8,2,indices,3));
        assert(luma_vk_ui_mesh_end((size_t)&ui));
    }
    assert(ui.mesh_builds == 4);
    /* Density and stroke changes invalidate independently of position. */
    for (int density = 1; density <= 3; ++density) {
        assert(luma_vk_ui_mesh_begin((size_t)&ui,key,1,3,(float)density/2 + 0.5f,0,0,0xffffffff) == 2);
        assert(luma_vk_ui_geometry((size_t)&ui, &vertices[0][0],3,8,2,indices,3));
        assert(luma_vk_ui_mesh_end((size_t)&ui));
    }
    assert(ui.mesh_builds == 7);
    assert(luma_vk_ui_cache_begin((size_t)&ui, 0));
    while (ui.meshes) { mesh = ui.meshes; ui.meshes = mesh->next; luma_vk_ui_mesh_release(mesh); }
    free(ui.vertices); free(ui.indices); free(ui.batches); free(ui.patches); free(key.object);
    puts("vector mesh cache: translations, viewport changes, tint, invalidation and lifetime passed");
}
