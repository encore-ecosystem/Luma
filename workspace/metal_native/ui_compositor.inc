#include "ui_metallib.inc"

typedef struct {
    float x, y;
    float r, g, b, a;
    float u, v;
} LumaMetalUiVertex;

struct LumaMetalUiTexture;

typedef struct {
    uint32_t first_index;
    uint32_t index_count;
    int32_t clip_x;
    int32_t clip_y;
    uint32_t clip_width;
    uint32_t clip_height;
    struct LumaMetalUiTexture *texture;
} LumaMetalUiBatch;

typedef struct {
    uint64_t key;
    LumaMetalUiVertex *vertices;
    uint32_t *indices;
    LumaMetalUiBatch *batches;
    size_t vertex_count;
    size_t index_count;
    size_t batch_count;
} LumaMetalUiPatchSegment;

typedef struct LumaMetalUiCompositor {
    GrapheneMetalDevice *device;
    GrapheneMetalSwapchain *swapchain;
    GrapheneMetalShader *vertex_shader;
    GrapheneMetalShader *fragment_shader;
    GrapheneMetalBindGroupLayout *texture_layout;
    GrapheneMetalSampler *sampler;
    struct LumaMetalUiTexture *white_texture;
    GrapheneMetalPipeline *pipeline;
    GrapheneMetalPipeline *load_pipeline;
    GrapheneMetalBuffer *vertex_buffers[2];
    GrapheneMetalBuffer *index_buffers[2];
    LumaMetalUiVertex *vertices;
    uint32_t *indices;
    LumaMetalUiBatch *batches;
    struct LumaMetalUiTexture **textures;
    size_t vertex_count;
    size_t vertex_capacity;
    size_t index_count;
    size_t index_capacity;
    size_t batch_count;
    size_t batch_capacity;
    size_t texture_count;
    size_t texture_capacity;
    size_t retained_vertex_count;
    size_t retained_index_count;
    size_t retained_batch_count;
    size_t retained_base_vertex_count;
    size_t retained_base_index_count;
    size_t retained_base_batch_count;
    LumaMetalUiPatchSegment *patches;
    size_t patch_count;
    size_t patch_capacity;
    size_t patch_vertex_start;
    size_t patch_index_start;
    size_t patch_batch_start;
    uint64_t patch_key;
    uint64_t segmented_commit_count;
    size_t composition_split_batch;
    int32_t clip_x;
    int32_t clip_y;
    uint32_t clip_width;
    uint32_t clip_height;
    float logical_width;
    float logical_height;
    float clear[4];
    bool retained_ready;
    bool patch_recording;
    bool force_new_batch;
    bool available;
    char error[512];
} LumaMetalUiCompositor;

typedef struct LumaMetalUiTexture {
    LumaMetalUiCompositor *owner;
    GrapheneMetalTexture *texture;
    GrapheneMetalBindGroup *group;
    uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    bool external;
    bool destroy_pending;
} LumaMetalUiTexture;

static bool luma_metal_ui_geometry_bound(size_t handle, const float *vertices,
    size_t vertex_count, size_t vertex_stride_floats, size_t color_offset_floats,
    const int *indices, size_t index_count, LumaMetalUiTexture *texture);

static void luma_metal_ui_patch_release(LumaMetalUiPatchSegment *patch) {
    if (patch == NULL) return;
    free(patch->vertices);
    free(patch->indices);
    free(patch->batches);
    memset(patch, 0, sizeof(*patch));
}

static void luma_metal_ui_patches_clear(LumaMetalUiCompositor *ui) {
    if (ui == NULL) return;
    for (size_t index = 0; index < ui->patch_count; ++index)
        luma_metal_ui_patch_release(&ui->patches[index]);
    ui->patch_count = 0;
}

static bool luma_metal_ui_patch_reserve(LumaMetalUiCompositor *ui, size_t count) {
    if (count <= ui->patch_capacity) return true;
    size_t capacity = ui->patch_capacity == 0 ? 16 : ui->patch_capacity;
    while (capacity < count) capacity *= 2;
    LumaMetalUiPatchSegment *fresh = realloc(ui->patches,
        capacity * sizeof(LumaMetalUiPatchSegment));
    if (fresh == NULL) return false;
    memset(fresh + ui->patch_capacity, 0,
        (capacity - ui->patch_capacity) * sizeof(LumaMetalUiPatchSegment));
    ui->patches = fresh;
    ui->patch_capacity = capacity;
    return true;
}

static uint64_t luma_metal_ui_patch_bounds_key(float x, float y,
    float width, float height) {
    uint32_t values[4];
    memcpy(&values[0], &x, sizeof(float));
    memcpy(&values[1], &y, sizeof(float));
    memcpy(&values[2], &width, sizeof(float));
    memcpy(&values[3], &height, sizeof(float));
    uint64_t hash = 1469598103934665603ull;
    for (size_t index = 0; index < 4; ++index) {
        hash ^= values[index];
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

static void luma_metal_ui_error(LumaMetalUiCompositor *ui, const char *message) {
    if (ui == NULL) return;
    metal_error(ui->error, sizeof(ui->error),
        message == NULL ? "Direct Metal UI error" : message);
    fprintf(stderr, "Luma Metal UI error: %s\n", ui->error);
}

static bool luma_metal_ui_reserve_geometry(LumaMetalUiCompositor *ui,
    size_t vertices, size_t indices, size_t batches) {
    if (vertices > ui->vertex_capacity) {
        size_t capacity = ui->vertex_capacity == 0 ? 4096 : ui->vertex_capacity;
        while (capacity < vertices) capacity *= 2;
        LumaMetalUiVertex *fresh = realloc(ui->vertices,
            capacity * sizeof(LumaMetalUiVertex));
        if (fresh == NULL) return false;
        ui->vertices = fresh;
        ui->vertex_capacity = capacity;
    }
    if (indices > ui->index_capacity) {
        size_t capacity = ui->index_capacity == 0 ? 8192 : ui->index_capacity;
        while (capacity < indices) capacity *= 2;
        uint32_t *fresh = realloc(ui->indices, capacity * sizeof(uint32_t));
        if (fresh == NULL) return false;
        ui->indices = fresh;
        ui->index_capacity = capacity;
    }
    if (batches > ui->batch_capacity) {
        size_t capacity = ui->batch_capacity == 0 ? 256 : ui->batch_capacity;
        while (capacity < batches) capacity *= 2;
        LumaMetalUiBatch *fresh = realloc(ui->batches,
            capacity * sizeof(LumaMetalUiBatch));
        if (fresh == NULL) return false;
        ui->batches = fresh;
        ui->batch_capacity = capacity;
    }
    return true;
}

static bool luma_metal_ui_flatten_patches(LumaMetalUiCompositor *ui) {
    size_t vertices = ui->retained_base_vertex_count;
    size_t indices = ui->retained_base_index_count;
    size_t batches = ui->retained_base_batch_count;
    for (size_t index = 0; index < ui->patch_count; ++index) {
        vertices += ui->patches[index].vertex_count;
        indices += ui->patches[index].index_count;
        batches += ui->patches[index].batch_count;
    }
    if (!luma_metal_ui_reserve_geometry(ui, vertices, indices, batches)) return false;
    ui->vertex_count = ui->retained_base_vertex_count;
    ui->index_count = ui->retained_base_index_count;
    ui->batch_count = ui->retained_base_batch_count;
    for (size_t index = 0; index < ui->patch_count; ++index) {
        LumaMetalUiPatchSegment *patch = &ui->patches[index];
        size_t vertex_base = ui->vertex_count;
        size_t index_base = ui->index_count;
        if (patch->vertex_count > 0)
            memcpy(ui->vertices + vertex_base, patch->vertices,
                patch->vertex_count * sizeof(LumaMetalUiVertex));
        for (size_t item = 0; item < patch->index_count; ++item)
            ui->indices[index_base + item] =
                (uint32_t)(vertex_base + patch->indices[item]);
        for (size_t item = 0; item < patch->batch_count; ++item) {
            LumaMetalUiBatch batch = patch->batches[item];
            batch.first_index += (uint32_t)index_base;
            ui->batches[ui->batch_count + item] = batch;
        }
        ui->vertex_count += patch->vertex_count;
        ui->index_count += patch->index_count;
        ui->batch_count += patch->batch_count;
    }
    ui->retained_vertex_count = ui->vertex_count;
    ui->retained_index_count = ui->index_count;
    ui->retained_batch_count = ui->batch_count;
    return true;
}

static bool luma_metal_ui_track_texture(LumaMetalUiCompositor *ui,
    LumaMetalUiTexture *texture) {
    if (ui->texture_count == ui->texture_capacity) {
        size_t capacity = ui->texture_capacity == 0 ? 64 : ui->texture_capacity * 2;
        LumaMetalUiTexture **fresh = realloc(ui->textures,
            capacity * sizeof(LumaMetalUiTexture *));
        if (fresh == NULL) return false;
        ui->textures = fresh;
        ui->texture_capacity = capacity;
    }
    ui->textures[ui->texture_count++] = texture;
    return true;
}

static GrapheneMetalBindGroup *luma_metal_ui_texture_group(
    LumaMetalUiCompositor *ui, GrapheneMetalTexture *texture) {
    GrapheneMetalBindGroup *group = metal_bind_group_new((size_t)ui->texture_layout);
    if (group == NULL) return NULL;
    group->texture_binding = 0;
    group->texture = texture;
    group->sampler = ui->sampler->sampler;
    return group;
}

static LumaMetalUiTexture *luma_metal_ui_texture_build(
    LumaMetalUiCompositor *ui, const void *pixels, uint32_t width,
    uint32_t height, uint32_t pitch, uint32_t format) {
    if (ui == NULL || !ui->available || width == 0 || height == 0 ||
        pitch < width * 4u || (format != 37u && format != 44u)) return NULL;
    LumaMetalUiTexture *texture = calloc(1, sizeof(LumaMetalUiTexture));
    if (texture == NULL) return NULL;
    texture->owner = ui;
    texture->width = width;
    texture->height = height;
    texture->pixels = calloc((size_t)width * height, 4u);
    if (texture->pixels == NULL) { free(texture); return NULL; }
    if (pixels != NULL) {
        for (uint32_t row = 0; row < height; ++row)
            memcpy(texture->pixels + (size_t)row * width * 4u,
                (const uint8_t *)pixels + (size_t)row * pitch, (size_t)width * 4u);
    }
    texture->texture = (GrapheneMetalTexture *)graphene_metal_texture_create(
        (size_t)ui->device, width, height, 1, format, 4u);
    if (texture->texture == NULL || !texture->texture->available) goto failed;
    [texture->texture->texture replaceRegion:MTLRegionMake2D(0, 0, width, height)
        mipmapLevel:0 withBytes:texture->pixels bytesPerRow:width * 4u];
    texture->group = luma_metal_ui_texture_group(ui, texture->texture);
    if (texture->group == NULL || !texture->group->available ||
        !luma_metal_ui_track_texture(ui, texture)) goto failed;
    return texture;
failed:
    if (texture->group != NULL) graphene_metal_bind_group_destroy((size_t)texture->group);
    if (texture->texture != NULL) graphene_metal_texture_destroy((size_t)texture->texture);
    free(texture->pixels);
    free(texture);
    return NULL;
}

static void luma_metal_ui_texture_release(LumaMetalUiTexture *texture) {
    if (texture == NULL) return;
    if (texture->group != NULL) graphene_metal_bind_group_destroy((size_t)texture->group);
    if (texture->texture != NULL) {
        if (texture->external) {
            [texture->texture->texture release];
            free(texture->texture);
        } else {
            graphene_metal_texture_destroy((size_t)texture->texture);
        }
    }
    free(texture->pixels);
    free(texture);
}

size_t luma_metal_ui_create(size_t device_handle, size_t swapchain_handle) {
    LumaMetalUiCompositor *ui = calloc(1, sizeof(LumaMetalUiCompositor));
    if (ui == NULL) return 0;
    ui->device = (GrapheneMetalDevice *)device_handle;
    ui->swapchain = (GrapheneMetalSwapchain *)swapchain_handle;
    if (!graphene_metal_device_available(device_handle) ||
        !graphene_metal_swapchain_available(swapchain_handle) ||
        ui->swapchain->device != ui->device) {
        luma_metal_ui_error(ui, "Invalid direct Metal device or swapchain");
        return (size_t)ui;
    }
    ui->vertex_shader = (GrapheneMetalShader *)graphene_metal_shader_create(device_handle,
        luma_ui_vertex_metallib, sizeof(luma_ui_vertex_metallib) / sizeof(uint32_t),
        GRAPHENE_METAL_STAGE_VERTEX);
    ui->fragment_shader = (GrapheneMetalShader *)graphene_metal_shader_create(device_handle,
        luma_ui_fragment_metallib, sizeof(luma_ui_fragment_metallib) / sizeof(uint32_t),
        GRAPHENE_METAL_STAGE_FRAGMENT);
    uint32_t binding = 0, kind = 1, stage = GRAPHENE_METAL_STAGE_FRAGMENT;
    ui->texture_layout = (GrapheneMetalBindGroupLayout *)
        graphene_metal_bind_group_layout_create_entries(device_handle,
            &binding, 1, &kind, 1, &stage, 1);
    ui->sampler = (GrapheneMetalSampler *)graphene_metal_sampler_create(device_handle);
    uint32_t locations[3] = {0, 1, 2};
    uint32_t formats[3] = {103, 109, 103};
    uint32_t offsets[3] = {0, 8, 24};
    ui->pipeline = (GrapheneMetalPipeline *)graphene_metal_pipeline_create(
        device_handle, swapchain_handle, (size_t)ui->vertex_shader,
        (size_t)ui->fragment_shader, (size_t)ui->texture_layout,
        true, false, false, 0, true, false, false,
        sizeof(LumaMetalUiVertex), locations, 3, formats, 3, offsets, 3);
    ui->load_pipeline = (GrapheneMetalPipeline *)graphene_metal_pipeline_create(
        device_handle, swapchain_handle, (size_t)ui->vertex_shader,
        (size_t)ui->fragment_shader, (size_t)ui->texture_layout,
        true, false, false, 0, true, false, true,
        sizeof(LumaMetalUiVertex), locations, 3, formats, 3, offsets, 3);
    if (!graphene_metal_shader_available((size_t)ui->vertex_shader) ||
        !graphene_metal_shader_available((size_t)ui->fragment_shader) ||
        !graphene_metal_bind_group_layout_available((size_t)ui->texture_layout) ||
        !graphene_metal_sampler_available((size_t)ui->sampler) ||
        !graphene_metal_pipeline_available((size_t)ui->pipeline) ||
        !graphene_metal_pipeline_available((size_t)ui->load_pipeline)) {
        const char *message = ui->vertex_shader != NULL && !ui->vertex_shader->available
            ? ui->vertex_shader->error : ui->fragment_shader != NULL && !ui->fragment_shader->available
            ? ui->fragment_shader->error : ui->pipeline != NULL && !ui->pipeline->available
            ? ui->pipeline->error : "Unable to create the direct Metal UI pipeline";
        luma_metal_ui_error(ui, message);
        return (size_t)ui;
    }
    ui->available = true;
    uint32_t white = 0xffffffffu;
    ui->white_texture = luma_metal_ui_texture_build(ui, &white, 1, 1, 4, 37);
    if (ui->white_texture == NULL) {
        ui->available = false;
        luma_metal_ui_error(ui, "Unable to create the direct Metal UI white texture");
    }
    return (size_t)ui;
}

bool luma_metal_ui_available(size_t handle) { LumaMetalUiCompositor *ui = (void *)handle; return ui != NULL && ui->available; }
encore_str luma_metal_ui_error_string(size_t handle) { LumaMetalUiCompositor *ui = (void *)handle; return metal_string(ui == NULL ? "Invalid direct Metal UI compositor" : ui->error); }

bool luma_metal_ui_set_logical_extent(size_t handle, uint32_t width, uint32_t height) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    if (!luma_metal_ui_available(handle) || width == 0 || height == 0) return false;
    ui->logical_width = width;
    ui->logical_height = height;
    return true;
}

bool luma_metal_ui_set_present_mode(size_t handle, uint32_t mode) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    return luma_metal_ui_available(handle) && mode <= 1u &&
        graphene_metal_swapchain_set_present_mode((size_t)ui->swapchain, mode == 1u);
}

static bool luma_metal_ui_begin_state(LumaMetalUiCompositor *ui,
    uint32_t clear_color, bool reset) {
    if (ui == NULL || !ui->available) return false;
    if (reset) { ui->vertex_count = 0; ui->index_count = 0; ui->batch_count = 0; }
    ui->composition_split_batch = 0;
    ui->force_new_batch = false;
    ui->clip_x = 0;
    ui->clip_y = 0;
    ui->clip_width = (uint32_t)(ui->logical_width > 0 ? ui->logical_width : ui->swapchain->width);
    ui->clip_height = (uint32_t)(ui->logical_height > 0 ? ui->logical_height : ui->swapchain->height);
    ui->clear[0] = (float)((clear_color >> 24) & 255u) / 255.0f;
    ui->clear[1] = (float)((clear_color >> 16) & 255u) / 255.0f;
    ui->clear[2] = (float)((clear_color >> 8) & 255u) / 255.0f;
    ui->clear[3] = (float)(clear_color & 255u) / 255.0f;
    return true;
}

bool luma_metal_ui_begin(size_t handle, uint32_t clear_color) { return luma_metal_ui_begin_state((void *)handle, clear_color, true); }
bool luma_metal_ui_cache_begin(size_t handle, uint32_t clear_color) {
    LumaMetalUiCompositor *ui = (void *)handle;
    if (!luma_metal_ui_begin_state(ui, clear_color, true)) return false;
    luma_metal_ui_patches_clear(ui);
    ui->patch_recording = false;
    ui->retained_ready = false;
    return true;
}

bool luma_metal_ui_cache_end(size_t handle) {
    LumaMetalUiCompositor *ui = (void *)handle;
    if (!luma_metal_ui_available(handle)) return false;
    if (ui->patch_recording) {
        size_t vertex_count = ui->vertex_count - ui->patch_vertex_start;
        size_t index_count = ui->index_count - ui->patch_index_start;
        size_t batch_count = ui->batch_count - ui->patch_batch_start;
        LumaMetalUiPatchSegment patch = {0};
        patch.key = ui->patch_key;
        patch.vertex_count = vertex_count;
        patch.index_count = index_count;
        patch.batch_count = batch_count;
        if (vertex_count > 0) {
            patch.vertices = malloc(vertex_count * sizeof(LumaMetalUiVertex));
            if (patch.vertices == NULL) goto patch_failed;
            memcpy(patch.vertices, ui->vertices + ui->patch_vertex_start,
                vertex_count * sizeof(LumaMetalUiVertex));
        }
        if (index_count > 0) {
            patch.indices = malloc(index_count * sizeof(uint32_t));
            if (patch.indices == NULL) goto patch_failed;
            for (size_t index = 0; index < index_count; ++index)
                patch.indices[index] = ui->indices[ui->patch_index_start + index] -
                    (uint32_t)ui->patch_vertex_start;
        }
        if (batch_count > 0) {
            patch.batches = malloc(batch_count * sizeof(LumaMetalUiBatch));
            if (patch.batches == NULL) goto patch_failed;
            for (size_t index = 0; index < batch_count; ++index) {
                patch.batches[index] = ui->batches[ui->patch_batch_start + index];
                patch.batches[index].first_index -= (uint32_t)ui->patch_index_start;
            }
        }
        size_t destination = ui->patch_count;
        for (size_t index = 0; index < ui->patch_count; ++index) {
            if (ui->patches[index].key == patch.key) {
                destination = index;
                break;
            }
        }
        if (destination == ui->patch_count &&
            !luma_metal_ui_patch_reserve(ui, ui->patch_count + 1)) goto patch_failed;
        if (destination < ui->patch_count)
            luma_metal_ui_patch_release(&ui->patches[destination]);
        else
            ui->patch_count++;
        ui->patches[destination] = patch;
        ui->patch_recording = false;
        ui->segmented_commit_count++;
        if (!luma_metal_ui_flatten_patches(ui)) {
            ui->retained_ready = false;
            return false;
        }
        ui->retained_ready = true;
        return true;
patch_failed:
        luma_metal_ui_patch_release(&patch);
        ui->patch_recording = false;
        ui->vertex_count = ui->retained_vertex_count;
        ui->index_count = ui->retained_index_count;
        ui->batch_count = ui->retained_batch_count;
        return false;
    }
    ui->retained_base_vertex_count = ui->vertex_count;
    ui->retained_base_index_count = ui->index_count;
    ui->retained_base_batch_count = ui->batch_count;
    ui->retained_vertex_count = ui->vertex_count;
    ui->retained_index_count = ui->index_count;
    ui->retained_batch_count = ui->batch_count;
    ui->retained_ready = true;
    return true;
}
bool luma_metal_ui_cached_begin(size_t handle, uint32_t clear_color) { LumaMetalUiCompositor *ui = (void *)handle; if (!luma_metal_ui_available(handle) || !ui->retained_ready) return false; ui->vertex_count = ui->retained_vertex_count; ui->index_count = ui->retained_index_count; ui->batch_count = ui->retained_batch_count; return luma_metal_ui_begin_state(ui, clear_color, false); }
bool luma_metal_ui_cache_ready(size_t handle) { LumaMetalUiCompositor *ui = (void *)handle; return luma_metal_ui_available(handle) && ui->retained_ready; }
bool luma_metal_ui_cache_patch_begin(size_t handle, uint32_t clear_color,
    float x, float y, float width, float height) {
    LumaMetalUiCompositor *ui = (void *)handle;
    if (!luma_metal_ui_available(handle) || !ui->retained_ready ||
        ui->patch_recording || width <= 0.0f || height <= 0.0f) return false;
    ui->vertex_count = ui->retained_vertex_count;
    ui->index_count = ui->retained_index_count;
    ui->batch_count = ui->retained_batch_count;
    ui->patch_vertex_start = ui->vertex_count;
    ui->patch_index_start = ui->index_count;
    ui->patch_batch_start = ui->batch_count;
    ui->patch_key = luma_metal_ui_patch_bounds_key(x, y, width, height);
    ui->patch_recording = true;
    ui->clip_x = x < 0.0f ? 0 : (int32_t)x;
    ui->clip_y = y < 0.0f ? 0 : (int32_t)y;
    ui->clip_width = (uint32_t)(width + 1.0f);
    ui->clip_height = (uint32_t)(height + 1.0f);
    float red = (float)((clear_color >> 24) & 255u) / 255.0f;
    float green = (float)((clear_color >> 16) & 255u) / 255.0f;
    float blue = (float)((clear_color >> 8) & 255u) / 255.0f;
    float alpha = (float)(clear_color & 255u) / 255.0f;
    float vertices[4][8] = {
        {x,y,red,green,blue,alpha,0,0},
        {x+width,y,red,green,blue,alpha,1,0},
        {x+width,y+height,red,green,blue,alpha,1,1},
        {x,y+height,red,green,blue,alpha,0,1}
    };
    int indices[6] = {0,1,2,0,2,3};
    if (!luma_metal_ui_geometry_bound(handle, &vertices[0][0], 4, 8, 2,
            indices, 6, ui->white_texture)) {
        ui->patch_recording = false;
        return false;
    }
    return true;
}

bool luma_metal_ui_clip(size_t handle, bool enabled, int32_t x, int32_t y,
    uint32_t width, uint32_t height) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    if (!luma_metal_ui_available(handle)) return false;
    uint32_t logical_width = (uint32_t)(ui->logical_width > 0 ? ui->logical_width : ui->swapchain->width);
    uint32_t logical_height = (uint32_t)(ui->logical_height > 0 ? ui->logical_height : ui->swapchain->height);
    if (!enabled) { ui->clip_x = 0; ui->clip_y = 0; ui->clip_width = logical_width; ui->clip_height = logical_height; return true; }
    if (x < 0) { width = width > (uint32_t)-x ? width - (uint32_t)-x : 0; x = 0; }
    if (y < 0) { height = height > (uint32_t)-y ? height - (uint32_t)-y : 0; y = 0; }
    if ((uint32_t)x >= logical_width || (uint32_t)y >= logical_height) { width = 0; height = 0; }
    else { if (width > logical_width - (uint32_t)x) width = logical_width - (uint32_t)x; if (height > logical_height - (uint32_t)y) height = logical_height - (uint32_t)y; }
    ui->clip_x = x; ui->clip_y = y; ui->clip_width = width; ui->clip_height = height;
    return true;
}

static bool luma_metal_ui_geometry_bound(size_t handle, const float *vertices,
    size_t vertex_count, size_t vertex_stride_floats, size_t color_offset_floats,
    const int *indices, size_t index_count, LumaMetalUiTexture *texture) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    if (!luma_metal_ui_available(handle) || vertices == NULL || indices == NULL ||
        texture == NULL || texture->destroy_pending || vertex_stride_floats < color_offset_floats + 4 ||
        vertex_stride_floats < 2 || vertex_count == 0 || index_count == 0 ||
        !luma_metal_ui_reserve_geometry(ui, ui->vertex_count + vertex_count,
            ui->index_count + index_count, ui->batch_count + 1)) return false;
    size_t first_vertex = ui->vertex_count;
    size_t first_index = ui->index_count;
    float width = ui->logical_width > 0 ? ui->logical_width : ui->swapchain->width;
    float height = ui->logical_height > 0 ? ui->logical_height : ui->swapchain->height;
    for (size_t index = 0; index < vertex_count; ++index) {
        const float *source = vertices + index * vertex_stride_floats;
        LumaMetalUiVertex *target = &ui->vertices[first_vertex + index];
        target->x = source[0] * 2.0f / width - 1.0f;
        target->y = 1.0f - source[1] * 2.0f / height;
        target->r = source[color_offset_floats];
        target->g = source[color_offset_floats + 1];
        target->b = source[color_offset_floats + 2];
        target->a = source[color_offset_floats + 3];
        target->u = source[vertex_stride_floats - 2];
        target->v = source[vertex_stride_floats - 1];
    }
    for (size_t index = 0; index < index_count; ++index) {
        if (indices[index] < 0 || (size_t)indices[index] >= vertex_count) return false;
        ui->indices[first_index + index] = (uint32_t)(first_vertex + (size_t)indices[index]);
    }
    ui->vertex_count += vertex_count;
    ui->index_count += index_count;
    LumaMetalUiBatch *batch = ui->batch_count == 0 ? NULL : &ui->batches[ui->batch_count - 1];
    if (!ui->force_new_batch && batch != NULL && batch->first_index + batch->index_count == first_index &&
        batch->clip_x == ui->clip_x && batch->clip_y == ui->clip_y &&
        batch->clip_width == ui->clip_width && batch->clip_height == ui->clip_height &&
        batch->texture == texture) batch->index_count += (uint32_t)index_count;
    else ui->batches[ui->batch_count++] = (LumaMetalUiBatch){(uint32_t)first_index,
        (uint32_t)index_count, ui->clip_x, ui->clip_y, ui->clip_width, ui->clip_height, texture};
    ui->force_new_batch = false;
    return true;
}

bool luma_metal_ui_geometry(size_t handle, const float *vertices, size_t vertex_count,
    size_t vertex_stride_floats, size_t color_offset_floats,
    const int *indices, size_t index_count) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    LumaMetalUiTexture *white = ui != NULL && ui->texture_count > 0 ? ui->textures[0] : NULL;
    return luma_metal_ui_geometry_bound(handle, vertices, vertex_count,
        vertex_stride_floats, color_offset_floats, indices, index_count, white);
}

size_t luma_metal_ui_texture_create(size_t handle, const void *pixels,
    uint32_t width, uint32_t height, uint32_t pitch, uint32_t format) {
    return (size_t)luma_metal_ui_texture_build((void *)handle, pixels, width, height, pitch, format);
}
size_t luma_metal_ui_texture_create_empty(size_t handle, uint32_t width,
    uint32_t height, uint32_t format) {
    return (size_t)luma_metal_ui_texture_build((void *)handle, NULL, width, height, width * 4u, format);
}

bool luma_metal_ui_texture_update(size_t handle, const void *pixels,
    uint32_t width, uint32_t height, uint32_t pitch, uint32_t x, uint32_t y) {
    LumaMetalUiTexture *texture = (LumaMetalUiTexture *)handle;
    if (texture == NULL || texture->destroy_pending || texture->pixels == NULL || pixels == NULL ||
        width == 0 || height == 0 || pitch < width * 4u || x > texture->width ||
        width > texture->width - x || y > texture->height || height > texture->height - y ||
        !metal_texture_prepare_host_access(texture->texture)) return false;
    for (uint32_t row = 0; row < height; ++row)
        memcpy(texture->pixels + ((size_t)(y + row) * texture->width + x) * 4u,
            (const uint8_t *)pixels + (size_t)row * pitch, (size_t)width * 4u);
    [texture->texture->texture replaceRegion:MTLRegionMake2D(x, y, width, height)
        mipmapLevel:0 withBytes:texture->pixels + ((size_t)y * texture->width + x) * 4u
        bytesPerRow:texture->width * 4u];
    return true;
}

size_t luma_metal_ui_external_texture_create(size_t handle, uint64_t image,
    uint32_t width, uint32_t height, uint32_t layout) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    if (!luma_metal_ui_available(handle) || image == 0 || width == 0 || height == 0 || layout != 5u)
        return 0;
    LumaMetalUiTexture *texture = calloc(1, sizeof(LumaMetalUiTexture));
    GrapheneMetalTexture *native = calloc(1, sizeof(GrapheneMetalTexture));
    if (texture == NULL || native == NULL) { free(texture); free(native); return 0; }
    native->owner = ui->device;
    native->texture = [(id<MTLTexture>)(void *)(uintptr_t)image retain];
    native->width = width; native->height = height; native->mip_levels = 1;
    native->format = 44; native->layout = layout; native->available = true;
    texture->owner = ui; texture->texture = native; texture->width = width;
    texture->height = height; texture->external = true;
    texture->group = luma_metal_ui_texture_group(ui, native);
    if (texture->group == NULL || !luma_metal_ui_track_texture(ui, texture)) {
        luma_metal_ui_texture_release(texture); return 0;
    }
    return (size_t)texture;
}

bool luma_metal_ui_texture_destroy(size_t handle) {
    LumaMetalUiTexture *texture = (LumaMetalUiTexture *)handle;
    if (texture == NULL || texture->destroy_pending) return false;
    texture->destroy_pending = true;
    if (texture->owner != NULL) texture->owner->retained_ready = false;
    return true;
}

bool luma_metal_ui_texture_draw_region(size_t handle, size_t texture_handle,
    float x, float y, float width, float height, uint32_t source_x,
    uint32_t source_y, uint32_t source_width, uint32_t source_height) {
    LumaMetalUiTexture *texture = (LumaMetalUiTexture *)texture_handle;
    if (texture == NULL || texture->destroy_pending || width <= 0 || height <= 0 ||
        source_width == 0 || source_height == 0 || source_x > texture->width ||
        source_width > texture->width - source_x || source_y > texture->height ||
        source_height > texture->height - source_y) return false;
    float u0 = (float)source_x / texture->width, v0 = (float)source_y / texture->height;
    float u1 = (float)(source_x + source_width) / texture->width;
    float v1 = (float)(source_y + source_height) / texture->height;
    float vertices[4][8] = {
        {x,y,1,1,1,1,u0,v0}, {x+width,y,1,1,1,1,u1,v0},
        {x+width,y+height,1,1,1,1,u1,v1}, {x,y+height,1,1,1,1,u0,v1}};
    int indices[6] = {0,1,2,0,2,3};
    return luma_metal_ui_geometry_bound(handle, &vertices[0][0], 4, 8, 2,
        indices, 6, texture);
}

bool luma_metal_ui_texture_draw(size_t handle, size_t texture_handle,
    float x, float y, float width, float height) {
    LumaMetalUiTexture *texture = (LumaMetalUiTexture *)texture_handle;
    return luma_metal_ui_texture_draw_region(handle, texture_handle, x, y, width, height,
        0, 0, texture == NULL ? 0 : texture->width, texture == NULL ? 0 : texture->height);
}

bool luma_metal_ui_composition_split(size_t handle) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    if (!luma_metal_ui_available(handle)) return false;
    ui->composition_split_batch = ui->batch_count;
    ui->force_new_batch = true;
    return true;
}

static bool luma_metal_ui_ensure_buffers(LumaMetalUiCompositor *ui,
    uint32_t slot) {
    size_t vertex_bytes = ui->vertex_count * sizeof(LumaMetalUiVertex);
    size_t index_bytes = ui->index_count * sizeof(uint32_t);
    if (vertex_bytes == 0 || index_bytes == 0) return true;
    if (ui->vertex_buffers[slot] == NULL || ui->vertex_buffers[slot]->size < vertex_bytes) {
        if (ui->vertex_buffers[slot] != NULL) graphene_metal_buffer_destroy((size_t)ui->vertex_buffers[slot]);
        size_t capacity = 64 * 1024; while (capacity < vertex_bytes) capacity *= 2;
        ui->vertex_buffers[slot] = (GrapheneMetalBuffer *)graphene_metal_buffer_create(
            (size_t)ui->device, capacity, 128u, true);
    }
    if (ui->index_buffers[slot] == NULL || ui->index_buffers[slot]->size < index_bytes) {
        if (ui->index_buffers[slot] != NULL) graphene_metal_buffer_destroy((size_t)ui->index_buffers[slot]);
        size_t capacity = 64 * 1024; while (capacity < index_bytes) capacity *= 2;
        ui->index_buffers[slot] = (GrapheneMetalBuffer *)graphene_metal_buffer_create(
            (size_t)ui->device, capacity, 64u, true);
    }
    return metal_buffer_write(ui->vertex_buffers[slot], 0, ui->vertices, vertex_bytes) &&
        metal_buffer_write(ui->index_buffers[slot], 0, ui->indices, index_bytes);
}

static bool luma_metal_ui_draw_range(LumaMetalUiCompositor *ui,
    GrapheneMetalFrame *frame, size_t first, size_t end) {
    if (first > ui->batch_count) first = ui->batch_count;
    if (end > ui->batch_count) end = ui->batch_count;
    if (end <= first) return true;
    uint32_t slot = frame->slot;
    if (!graphene_metal_frame_bind_vertex_buffer((size_t)frame,
            (size_t)ui->vertex_buffers[slot]) ||
        !graphene_metal_frame_bind_index_buffer((size_t)frame,
            (size_t)ui->index_buffers[slot])) return false;
    float logical_width = ui->logical_width > 0 ? ui->logical_width : ui->swapchain->width;
    float logical_height = ui->logical_height > 0 ? ui->logical_height : ui->swapchain->height;
    float scale_x = (float)ui->swapchain->width / logical_width;
    float scale_y = (float)ui->swapchain->height / logical_height;
    [frame->render setViewport:(MTLViewport){0, 0, ui->swapchain->width,
        ui->swapchain->height, 0.0, 1.0}];
    for (size_t index = first; index < end; ++index) {
        LumaMetalUiBatch *batch = &ui->batches[index];
        if (batch->texture == NULL || batch->texture->destroy_pending) return false;
        uint32_t sx = (uint32_t)((float)(batch->clip_x < 0 ? 0 : batch->clip_x) * scale_x);
        uint32_t sy = (uint32_t)((float)(batch->clip_y < 0 ? 0 : batch->clip_y) * scale_y);
        uint32_t right = (uint32_t)(((float)batch->clip_x + batch->clip_width) * scale_x + 0.9999f);
        uint32_t bottom = (uint32_t)(((float)batch->clip_y + batch->clip_height) * scale_y + 0.9999f);
        if (right > ui->swapchain->width) right = ui->swapchain->width;
        if (bottom > ui->swapchain->height) bottom = ui->swapchain->height;
        if (right <= sx || bottom <= sy) continue;
        [frame->render setScissorRect:(MTLScissorRect){sx, sy, right - sx, bottom - sy}];
        if (!graphene_metal_frame_bind_group((size_t)frame, (size_t)batch->texture->group) ||
            !graphene_metal_frame_draw_indexed((size_t)frame, batch->index_count, 1,
                batch->first_index, 0, 0)) return false;
    }
    return true;
}

size_t luma_metal_ui_frame_prepare(size_t handle) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    if (!luma_metal_ui_available(handle)) return 0;
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)graphene_metal_frame_acquire((size_t)ui->swapchain);
    if (frame == NULL || !frame->available || !luma_metal_ui_ensure_buffers(ui, frame->slot)) {
        if (frame != NULL) graphene_metal_frame_destroy((size_t)frame);
        luma_metal_ui_error(ui, "Unable to prepare the direct Metal UI frame");
        return 0;
    }
    return (size_t)frame;
}

bool luma_metal_ui_frame_begin_composition(size_t handle, size_t frame_handle) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)frame_handle;
    return luma_metal_ui_available(handle) && frame != NULL && frame->swapchain == ui->swapchain &&
        graphene_metal_frame_begin_render_pass(frame_handle, (size_t)ui->pipeline,
            ui->clear[0], ui->clear[1], ui->clear[2], ui->clear[3]) &&
        luma_metal_ui_draw_range(ui, frame, 0, ui->composition_split_batch);
}

size_t luma_metal_ui_frame_acquire(size_t handle) {
    size_t frame = luma_metal_ui_frame_prepare(handle);
    if (frame != 0 && luma_metal_ui_frame_begin_composition(handle, frame)) return frame;
    if (frame != 0) graphene_metal_frame_destroy(frame);
    return 0;
}

static bool luma_metal_ui_finish_frame(LumaMetalUiCompositor *ui,
    GrapheneMetalFrame *frame, bool composed) {
    if (ui == NULL || frame == NULL || !frame->available || frame->swapchain != ui->swapchain)
        return false;
    bool rendered = true;
    if (composed) {
        rendered = frame->render != nil &&
            graphene_metal_frame_bind_pipeline((size_t)frame, (size_t)ui->load_pipeline) &&
            luma_metal_ui_draw_range(ui, frame, ui->composition_split_batch, ui->batch_count);
    } else {
        rendered = graphene_metal_frame_begin_render_pass((size_t)frame, (size_t)ui->pipeline,
            ui->clear[0], ui->clear[1], ui->clear[2], ui->clear[3]) &&
            luma_metal_ui_draw_range(ui, frame, 0, ui->batch_count);
    }
    if (frame->render != nil) rendered = graphene_metal_frame_end_render_pass((size_t)frame) && rendered;
    rendered = graphene_metal_frame_present((size_t)frame) && rendered;
    graphene_metal_frame_destroy((size_t)frame);
    return rendered;
}

bool luma_metal_ui_frame_finish(size_t handle, size_t frame_handle) {
    return luma_metal_ui_finish_frame((void *)handle, (void *)frame_handle, true);
}

bool luma_metal_ui_end(size_t handle) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    size_t frame = luma_metal_ui_frame_prepare(handle);
    return frame != 0 && luma_metal_ui_finish_frame(ui, (void *)frame, false);
}

bool luma_metal_ui_destroy(size_t handle) {
    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)handle;
    if (ui == NULL) return false;
    for (size_t slot = 0; slot < 2; ++slot) {
        if (ui->vertex_buffers[slot] != NULL) graphene_metal_buffer_destroy((size_t)ui->vertex_buffers[slot]);
        if (ui->index_buffers[slot] != NULL) graphene_metal_buffer_destroy((size_t)ui->index_buffers[slot]);
    }
    for (size_t index = 0; index < ui->texture_count; ++index)
        luma_metal_ui_texture_release(ui->textures[index]);
    if (ui->pipeline != NULL) graphene_metal_pipeline_destroy((size_t)ui->pipeline);
    if (ui->load_pipeline != NULL) graphene_metal_pipeline_destroy((size_t)ui->load_pipeline);
    if (ui->sampler != NULL) graphene_metal_sampler_destroy((size_t)ui->sampler);
    if (ui->texture_layout != NULL) graphene_metal_bind_group_layout_destroy((size_t)ui->texture_layout);
    if (ui->vertex_shader != NULL) graphene_metal_shader_destroy((size_t)ui->vertex_shader);
    if (ui->fragment_shader != NULL) graphene_metal_shader_destroy((size_t)ui->fragment_shader);
    luma_metal_ui_patches_clear(ui);
    free(ui->patches);
    free(ui->vertices); free(ui->indices); free(ui->batches); free(ui->textures); free(ui);
    return true;
}
