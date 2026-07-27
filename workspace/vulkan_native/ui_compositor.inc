/* Retained 2D geometry compositor for Luma's direct Vulkan backend. */
#include "ui_shaders.inc"

typedef struct {
    float x, y;
    float r, g, b, a;
    float u, v;
} LumaVkUiVertex;

typedef struct {
    uint32_t first_index, index_count;
    int32_t clip_x, clip_y;
    uint32_t clip_width, clip_height;
    struct LumaVkUiTexture *texture;
} LumaVkUiBatch;

typedef struct LumaVkUiUpload {
    GrapheneVkBuffer *staging;
    GrapheneVkSubmission *submission;
    struct LumaVkUiUpload *next;
} LumaVkUiUpload;

typedef struct LumaVkUiTexture {
    struct LumaVkUiCompositor *owner;
    GrapheneVkTexture *texture;
    GrapheneVkBindGroup *group;
    LumaVkUiUpload *uploads;
    unsigned char *atlas_pixels;
    uint32_t dirty_x, dirty_y, dirty_right, dirty_bottom;
    uint32_t width, height;
    uint8_t retire_slots;
    bool atlas_dirty;
    bool destroy_pending;
} LumaVkUiTexture;

typedef struct LumaVkUiCompositor {
    GrapheneVkDevice *device;
    GrapheneVkSwapchain *swapchain;
    GrapheneVkShader *vertex_shader;
    GrapheneVkShader *fragment_shader;
    GrapheneVkBindGroupLayout *texture_layout;
    GrapheneVkSampler *sampler;
    LumaVkUiTexture *white_texture;
    GrapheneVkPipeline *pipeline;
    GrapheneVkPipeline *load_pipeline;
    GrapheneVkBuffer *vertex_buffers[2];
    GrapheneVkBuffer *index_buffers[2];
    LumaVkUiVertex *vertices;
    uint32_t *indices;
    LumaVkUiBatch *batches;
    LumaVkUiTexture **retired_textures;
    size_t vertex_count, vertex_capacity;
    size_t index_count, index_capacity;
    size_t batch_count, batch_capacity;
    size_t composition_split_batch;
    size_t retained_vertex_count, retained_index_count, retained_batch_count;
    size_t retired_texture_count, retired_texture_capacity;
    bool retained_ready;
    bool force_new_batch;
    int32_t clip_x, clip_y;
    uint32_t clip_width, clip_height;
    float logical_width, logical_height;
    float clear[4];
    char error[256];
    bool available;
} LumaVkUiCompositor;

static void luma_vk_ui_error(LumaVkUiCompositor *ui, const char *message) {
    if (ui == NULL) return;
    snprintf(ui->error, sizeof(ui->error), "%s", message == NULL ? "Direct UI error" : message);
}

static bool luma_vk_ui_reserve_batches(LumaVkUiCompositor *ui, size_t count) {
    if (count <= ui->batch_capacity) return true;
    size_t capacity = ui->batch_capacity == 0 ? 256 : ui->batch_capacity;
    while (capacity < count) capacity *= 2;
    LumaVkUiBatch *fresh = (LumaVkUiBatch *)realloc(
        ui->batches, capacity * sizeof(LumaVkUiBatch));
    if (fresh == NULL) return false;
    ui->batches = fresh;
    ui->batch_capacity = capacity;
    return true;
}

static bool luma_vk_ui_reserve_cpu(LumaVkUiCompositor *ui,
    size_t vertices, size_t indices) {
    if (vertices > ui->vertex_capacity) {
        size_t capacity = ui->vertex_capacity == 0 ? 4096 : ui->vertex_capacity;
        while (capacity < vertices) capacity *= 2;
        LumaVkUiVertex *fresh = (LumaVkUiVertex *)realloc(
            ui->vertices, capacity * sizeof(LumaVkUiVertex));
        if (fresh == NULL) return false;
        ui->vertices = fresh;
        ui->vertex_capacity = capacity;
    }
    if (indices > ui->index_capacity) {
        size_t capacity = ui->index_capacity == 0 ? 8192 : ui->index_capacity;
        while (capacity < indices) capacity *= 2;
        uint32_t *fresh = (uint32_t *)realloc(ui->indices, capacity * sizeof(uint32_t));
        if (fresh == NULL) return false;
        ui->indices = fresh;
        ui->index_capacity = capacity;
    }
    return true;
}

static bool luma_vk_ui_upload(GrapheneVkBuffer *buffer, const void *data, size_t bytes) {
    if (buffer == NULL || !buffer->available || !buffer->host_visible ||
        bytes > buffer->size) return false;
    void *mapped = NULL;
    if (buffer->owner->map_memory(buffer->owner->device, buffer->memory,
            0, bytes, 0, &mapped) != VK_SUCCESS || mapped == NULL) return false;
    memcpy(mapped, data, bytes);
    buffer->owner->unmap_memory(buffer->owner->device, buffer->memory);
    return true;
}

static bool luma_vk_ui_track_upload(LumaVkUiTexture *texture,
    GrapheneVkBuffer *staging, GrapheneVkSubmission *submission) {
    LumaVkUiUpload *upload = (LumaVkUiUpload *)malloc(sizeof(LumaVkUiUpload));
    if (upload == NULL) return false;
    *upload = (LumaVkUiUpload){staging, submission, texture->uploads};
    texture->uploads = upload;
    return true;
}

static LumaVkUiTexture *luma_vk_ui_texture_build(LumaVkUiCompositor *ui,
    const void *pixels, uint32_t width, uint32_t height, uint32_t pitch,
    uint32_t format) {
    if (ui == NULL || pixels == NULL || width == 0 || height == 0 ||
        pitch < width * 4u) return NULL;
    LumaVkUiTexture *result = (LumaVkUiTexture *)calloc(1, sizeof(LumaVkUiTexture));
    if (result == NULL) return NULL;
    result->owner = ui;
    result->width = width;
    result->height = height;
    result->texture = (GrapheneVkTexture *)graphene_vk_texture_create(
        (size_t)ui->device, width, height, 1u, format, 6u);
    GrapheneVkBuffer *staging = (GrapheneVkBuffer *)graphene_vk_buffer_create(
        (size_t)ui->device, (uint64_t)width * height * 4u, 1u, true);
    if (result->texture == NULL || !result->texture->available ||
        staging == NULL || !staging->available) goto failed;
    void *mapped = NULL;
    size_t packed_pitch = (size_t)width * 4u;
    if (ui->device->map_memory(ui->device->device, staging->memory, 0,
            packed_pitch * height, 0, &mapped) != VK_SUCCESS || mapped == NULL)
        goto failed;
    for (uint32_t row = 0; row < height; ++row)
        memcpy((unsigned char *)mapped + (size_t)row * packed_pitch,
            (const unsigned char *)pixels + (size_t)row * pitch, packed_pitch);
    ui->device->unmap_memory(ui->device->device, staging->memory);
    GrapheneVkEncoder *encoder = (GrapheneVkEncoder *)graphene_vk_encoder_create(
        (size_t)ui->device);
    if (encoder == NULL || !graphene_vk_encoder_available((size_t)encoder) ||
        !graphene_vk_encoder_transition_texture((size_t)encoder,
            (size_t)result->texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
        !graphene_vk_encoder_copy_buffer_to_texture((size_t)encoder,
            (size_t)staging, (size_t)result->texture, width, height, 0u) ||
        !graphene_vk_encoder_transition_texture((size_t)encoder,
            (size_t)result->texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
        if (encoder != NULL) graphene_vk_encoder_destroy((size_t)encoder);
        goto failed;
    }
    GrapheneVkSubmission *submission = (GrapheneVkSubmission *)
        graphene_vk_encoder_submit((size_t)encoder);
    graphene_vk_encoder_destroy((size_t)encoder);
    if (submission == NULL ||
        !graphene_vk_submission_available((size_t)submission)) {
        if (submission != NULL)
            graphene_vk_submission_destroy((size_t)submission);
        goto failed;
    }
    if (!luma_vk_ui_track_upload(result, staging, submission)) {
        graphene_vk_submission_destroy((size_t)submission);
        graphene_vk_buffer_destroy((size_t)staging);
        goto failed_without_staging;
    }
    result->group = (GrapheneVkBindGroup *)graphene_vk_texture_bind_group_create(
        (size_t)ui->device, (size_t)ui->texture_layout,
        (size_t)result->texture, (size_t)ui->sampler);
    if (result->group == NULL || !result->group->available) goto failed_without_staging;
    return result;

failed:
    if (staging != NULL && result->uploads == NULL)
        graphene_vk_buffer_destroy((size_t)staging);
failed_without_staging:
    while (result->uploads != NULL) {
        LumaVkUiUpload *upload = result->uploads;
        result->uploads = upload->next;
        graphene_vk_submission_destroy((size_t)upload->submission);
        graphene_vk_buffer_destroy((size_t)upload->staging);
        free(upload);
    }
    if (result->group != NULL) graphene_vk_bind_group_destroy((size_t)result->group);
    if (result->texture != NULL) graphene_vk_texture_destroy((size_t)result->texture);
    free(result);
    return NULL;
}

static void luma_vk_ui_texture_release(LumaVkUiTexture *texture) {
    if (texture == NULL) return;
    while (texture->uploads != NULL) {
        LumaVkUiUpload *upload = texture->uploads;
        texture->uploads = upload->next;
        graphene_vk_submission_destroy((size_t)upload->submission);
        graphene_vk_buffer_destroy((size_t)upload->staging);
        free(upload);
    }
    if (texture->group != NULL) graphene_vk_bind_group_destroy((size_t)texture->group);
    if (texture->texture != NULL) graphene_vk_texture_destroy((size_t)texture->texture);
    free(texture->atlas_pixels);
    free(texture);
}

static void luma_vk_ui_texture_collect_upload(LumaVkUiTexture *texture) {
    if (texture == NULL) return;
    LumaVkUiUpload **link = &texture->uploads;
    while (*link != NULL) {
        LumaVkUiUpload *upload = *link;
        if (!graphene_vk_submission_complete((size_t)upload->submission)) {
            link = &upload->next;
            continue;
        }
        *link = upload->next;
        graphene_vk_submission_destroy((size_t)upload->submission);
        graphene_vk_buffer_destroy((size_t)upload->staging);
        free(upload);
    }
}

static LumaVkUiTexture *luma_vk_ui_texture_empty(
    LumaVkUiCompositor *ui, uint32_t width, uint32_t height,
    uint32_t format) {
    if (ui == NULL || width == 0 || height == 0 ||
        (size_t)width * (size_t)height > SIZE_MAX / 4u) return NULL;
    LumaVkUiTexture *result =
        (LumaVkUiTexture *)calloc(1, sizeof(LumaVkUiTexture));
    if (result == NULL) return NULL;
    result->owner = ui;
    result->width = width;
    result->height = height;
    result->atlas_pixels =
        (unsigned char *)calloc((size_t)width * height, 4u);
    result->texture = (GrapheneVkTexture *)graphene_vk_texture_create(
        (size_t)ui->device, width, height, 1u, format, 6u);
    if (result->atlas_pixels == NULL || result->texture == NULL ||
        !result->texture->available) {
        luma_vk_ui_texture_release(result);
        return NULL;
    }
    result->group = (GrapheneVkBindGroup *)
        graphene_vk_texture_bind_group_create((size_t)ui->device,
            (size_t)ui->texture_layout, (size_t)result->texture,
            (size_t)ui->sampler);
    if (result->group == NULL || !result->group->available) {
        luma_vk_ui_texture_release(result);
        return NULL;
    }
    return result;
}

static bool luma_vk_ui_texture_flush(LumaVkUiTexture *texture) {
    if (texture == NULL || !texture->atlas_dirty ||
        texture->atlas_pixels == NULL) return true;
    uint32_t width = texture->dirty_right - texture->dirty_x;
    uint32_t height = texture->dirty_bottom - texture->dirty_y;
    uint64_t bytes = (uint64_t)width * height * 4u;
    GrapheneVkBuffer *staging = (GrapheneVkBuffer *)
        graphene_vk_buffer_create((size_t)texture->owner->device,
            bytes, 1u, true);
    if (staging == NULL || !staging->available) {
        if (staging != NULL) graphene_vk_buffer_destroy((size_t)staging);
        return false;
    }
    void *mapped = NULL;
    if (texture->owner->device->map_memory(
            texture->owner->device->device, staging->memory, 0, bytes, 0,
            &mapped) != VK_SUCCESS || mapped == NULL) {
        graphene_vk_buffer_destroy((size_t)staging);
        return false;
    }
    for (uint32_t row = 0; row < height; ++row)
        memcpy((unsigned char *)mapped + (size_t)row * width * 4u,
            texture->atlas_pixels +
                ((size_t)(texture->dirty_y + row) * texture->width +
                 texture->dirty_x) * 4u,
            (size_t)width * 4u);
    texture->owner->device->unmap_memory(
        texture->owner->device->device, staging->memory);
    GrapheneVkEncoder *encoder = (GrapheneVkEncoder *)
        graphene_vk_encoder_create((size_t)texture->owner->device);
    if (encoder == NULL ||
        !graphene_vk_encoder_available((size_t)encoder) ||
        !graphene_vk_encoder_transition_texture((size_t)encoder,
            (size_t)texture->texture,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
        !graphene_vk_encoder_copy_buffer_to_texture_region(
            (size_t)encoder, (size_t)staging, (size_t)texture->texture,
            width, height, 0u, texture->dirty_x, texture->dirty_y) ||
        !graphene_vk_encoder_transition_texture((size_t)encoder,
            (size_t)texture->texture,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
        if (encoder != NULL) graphene_vk_encoder_destroy((size_t)encoder);
        graphene_vk_buffer_destroy((size_t)staging);
        return false;
    }
    GrapheneVkSubmission *submission = (GrapheneVkSubmission *)
        graphene_vk_encoder_submit((size_t)encoder);
    graphene_vk_encoder_destroy((size_t)encoder);
    if (submission == NULL ||
        !graphene_vk_submission_available((size_t)submission) ||
        !luma_vk_ui_track_upload(texture, staging, submission)) {
        if (submission != NULL)
            graphene_vk_submission_destroy((size_t)submission);
        graphene_vk_buffer_destroy((size_t)staging);
        return false;
    }
    texture->atlas_dirty = false;
    return true;
}

static bool luma_vk_ui_reserve_retired_textures(LumaVkUiCompositor *ui, size_t count) {
    if (count <= ui->retired_texture_capacity) return true;
    size_t capacity = ui->retired_texture_capacity == 0 ? 32 : ui->retired_texture_capacity;
    while (capacity < count) capacity *= 2;
    LumaVkUiTexture **fresh = (LumaVkUiTexture **)realloc(
        ui->retired_textures, capacity * sizeof(LumaVkUiTexture *));
    if (fresh == NULL) return false;
    ui->retired_textures = fresh;
    ui->retired_texture_capacity = capacity;
    return true;
}

/* The corresponding frame fence has completed before this is called. */
static void luma_vk_ui_collect_retired(LumaVkUiCompositor *ui, uint32_t slot) {
    if (ui == NULL || slot >= 2) return;
    size_t write = 0;
    for (size_t i = 0; i < ui->retired_texture_count; ++i) {
        LumaVkUiTexture *texture = ui->retired_textures[i];
        texture->retire_slots &= (uint8_t)~(1u << slot);
        if (texture->retire_slots == 0)
            luma_vk_ui_texture_release(texture);
        else
            ui->retired_textures[write++] = texture;
    }
    ui->retired_texture_count = write;
}

static bool luma_vk_ui_ensure_gpu(LumaVkUiCompositor *ui, uint32_t slot,
    size_t vertex_bytes, size_t index_bytes) {
    if (slot >= 2) return false;
    if (ui->vertex_buffers[slot] == NULL ||
        ui->vertex_buffers[slot]->size < vertex_bytes) {
        if (ui->vertex_buffers[slot] != NULL)
            graphene_vk_buffer_destroy((size_t)ui->vertex_buffers[slot]);
        size_t capacity = 128 * 1024;
        while (capacity < vertex_bytes) capacity *= 2;
        ui->vertex_buffers[slot] = (GrapheneVkBuffer *)graphene_vk_buffer_create(
            (size_t)ui->device, capacity, 128u, true);
    }
    if (ui->index_buffers[slot] == NULL ||
        ui->index_buffers[slot]->size < index_bytes) {
        if (ui->index_buffers[slot] != NULL)
            graphene_vk_buffer_destroy((size_t)ui->index_buffers[slot]);
        size_t capacity = 64 * 1024;
        while (capacity < index_bytes) capacity *= 2;
        ui->index_buffers[slot] = (GrapheneVkBuffer *)graphene_vk_buffer_create(
            (size_t)ui->device, capacity, 64u, true);
    }
    return ui->vertex_buffers[slot] != NULL && ui->vertex_buffers[slot]->available &&
        ui->index_buffers[slot] != NULL && ui->index_buffers[slot]->available;
}

size_t luma_vk_ui_create(size_t device_handle, size_t swapchain_handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)calloc(1, sizeof(LumaVkUiCompositor));
    if (ui == NULL) return 0;
    ui->device = (GrapheneVkDevice *)device_handle;
    ui->swapchain = (GrapheneVkSwapchain *)swapchain_handle;
    if (ui->device == NULL || !ui->device->available || ui->swapchain == NULL ||
        !ui->swapchain->available || ui->swapchain->owner != ui->device) {
        luma_vk_ui_error(ui, "Invalid direct Vulkan device or swapchain");
        return (size_t)ui;
    }
    ui->vertex_shader = (GrapheneVkShader *)graphene_vk_shader_create(
        device_handle, luma_ui_vertex_spirv,
        sizeof(luma_ui_vertex_spirv) / sizeof(uint32_t),
        sizeof(luma_ui_vertex_spirv) / sizeof(uint32_t), VK_SHADER_STAGE_VERTEX_BIT);
    ui->fragment_shader = (GrapheneVkShader *)graphene_vk_shader_create(
        device_handle, luma_ui_fragment_spirv,
        sizeof(luma_ui_fragment_spirv) / sizeof(uint32_t),
        sizeof(luma_ui_fragment_spirv) / sizeof(uint32_t), VK_SHADER_STAGE_FRAGMENT_BIT);
    uint32_t bindings[1] = {0u}, types[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    uint32_t stages[1] = {VK_SHADER_STAGE_FRAGMENT_BIT};
    ui->texture_layout = (GrapheneVkBindGroupLayout *)
        graphene_vk_bind_group_layout_create_entries(device_handle,
            bindings, 1, 1, types, 1, 1, stages, 1, 1);
    ui->sampler = (GrapheneVkSampler *)graphene_vk_sampler_create(device_handle);
    uint32_t locations[3] = {0u, 1u, 2u};
    uint32_t formats[3] = {103u, 109u, 103u};
    uint32_t offsets[3] = {0u, 8u, 24u};
    ui->pipeline = (GrapheneVkPipeline *)graphene_vk_pipeline_create(
        device_handle, swapchain_handle, (size_t)ui->vertex_shader,
        (size_t)ui->fragment_shader, (size_t)ui->texture_layout,
        false, VK_CULL_MODE_NONE, true, false, sizeof(LumaVkUiVertex),
        locations, 3, 3, formats, 3, 3, offsets, 3, 3);
    ui->load_pipeline = (GrapheneVkPipeline *)graphene_vk_pipeline_create(
        device_handle, swapchain_handle, (size_t)ui->vertex_shader,
        (size_t)ui->fragment_shader, (size_t)ui->texture_layout,
        false, VK_CULL_MODE_NONE, true, true, sizeof(LumaVkUiVertex),
        locations, 3, 3, formats, 3, 3, offsets, 3, 3);
    if (ui->vertex_shader == NULL || !ui->vertex_shader->available ||
        ui->fragment_shader == NULL || !ui->fragment_shader->available ||
        ui->texture_layout == NULL || !ui->texture_layout->available ||
        ui->sampler == NULL || !ui->sampler->available ||
        ui->pipeline == NULL || !ui->pipeline->available ||
        ui->load_pipeline == NULL || !ui->load_pipeline->available) {
        luma_vk_ui_error(ui, ui->pipeline == NULL ? "Unable to allocate direct UI pipeline" :
            !ui->pipeline->available ? ui->pipeline->error :
            ui->load_pipeline == NULL ? "Unable to allocate direct UI load pipeline" :
            ui->load_pipeline->error);
        return (size_t)ui;
    }
    /* Build render-pass framebuffers before any image is acquired. Waiting
       for queue idle after vkAcquireNextImage can deadlock some WSI drivers. */
    if (!graphene_vk_swapchain_prepare_pipeline(ui->swapchain, ui->pipeline)) {
        luma_vk_ui_error(ui, ui->swapchain->error);
        return (size_t)ui;
    }
    if (!graphene_vk_swapchain_prepare_pipeline(ui->swapchain, ui->load_pipeline)) {
        luma_vk_ui_error(ui, ui->swapchain->error);
        return (size_t)ui;
    }
    uint32_t white = 0xffffffffu;
    ui->white_texture = luma_vk_ui_texture_build(ui, &white, 1u, 1u, 4u, 37u);
    if (ui->white_texture == NULL) {
        luma_vk_ui_error(ui, "Unable to create direct UI white texture");
        return (size_t)ui;
    }
    ui->available = true;
    return (size_t)ui;
}

bool luma_vk_ui_available(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return ui != NULL && ui->available;
}

bool luma_vk_ui_set_logical_extent(size_t handle, uint32_t width, uint32_t height) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available || width == 0 || height == 0) return false;
    ui->logical_width = (float)width;
    ui->logical_height = (float)height;
    return true;
}

bool luma_vk_ui_set_present_mode(size_t handle, uint32_t mode) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available || ui->swapchain == NULL || mode > 1u)
        return false;
    uint32_t requested = mode == 0u
        ? VK_PRESENT_MODE_FIFO_KHR : VK_PRESENT_MODE_IMMEDIATE_KHR;
    if (ui->swapchain->requested_present_mode != requested) {
        ui->swapchain->requested_present_mode = requested;
        ui->swapchain->needs_rebuild = true;
    }
    return true;
}

encore_str luma_vk_ui_error_string(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return graphene_string(ui == NULL ? "Invalid direct UI compositor" : ui->error);
}

bool luma_vk_ui_begin(size_t handle, uint32_t clear_color) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available) return false;
    ui->vertex_count = 0;
    ui->index_count = 0;
    ui->batch_count = 0;
    ui->composition_split_batch = 0;
    ui->force_new_batch = false;
    ui->clip_x = 0;
    ui->clip_y = 0;
    ui->clip_width = (uint32_t)(ui->logical_width > 0.0f
        ? ui->logical_width : (float)ui->swapchain->width);
    ui->clip_height = (uint32_t)(ui->logical_height > 0.0f
        ? ui->logical_height : (float)ui->swapchain->height);
    ui->clear[0] = (float)((clear_color >> 24) & 255u) / 255.0f;
    ui->clear[1] = (float)((clear_color >> 16) & 255u) / 255.0f;
    ui->clear[2] = (float)((clear_color >> 8) & 255u) / 255.0f;
    ui->clear[3] = (float)(clear_color & 255u) / 255.0f;
    return true;
}

bool luma_vk_ui_cache_begin(size_t handle, uint32_t clear_color) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (!luma_vk_ui_begin(handle, clear_color)) return false;
    ui->retained_ready = false;
    return true;
}

bool luma_vk_ui_cache_end(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available) return false;
    ui->retained_vertex_count = ui->vertex_count;
    ui->retained_index_count = ui->index_count;
    ui->retained_batch_count = ui->batch_count;
    ui->retained_ready = true;
    return true;
}

bool luma_vk_ui_cached_begin(size_t handle, uint32_t clear_color) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available || !ui->retained_ready) return false;
    ui->vertex_count = ui->retained_vertex_count;
    ui->index_count = ui->retained_index_count;
    ui->batch_count = ui->retained_batch_count;
    ui->clip_x = 0;
    ui->clip_y = 0;
    ui->clip_width = (uint32_t)(ui->logical_width > 0.0f
        ? ui->logical_width : (float)ui->swapchain->width);
    ui->clip_height = (uint32_t)(ui->logical_height > 0.0f
        ? ui->logical_height : (float)ui->swapchain->height);
    ui->clear[0] = (float)((clear_color >> 24) & 255u) / 255.0f;
    ui->clear[1] = (float)((clear_color >> 16) & 255u) / 255.0f;
    ui->clear[2] = (float)((clear_color >> 8) & 255u) / 255.0f;
    ui->clear[3] = (float)(clear_color & 255u) / 255.0f;
    return true;
}

bool luma_vk_ui_cache_ready(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return ui != NULL && ui->available && ui->retained_ready;
}

bool luma_vk_ui_clip(size_t handle, bool enabled, int32_t x, int32_t y,
    uint32_t width, uint32_t height) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available) return false;
    if (!enabled) {
        ui->clip_x = 0;
        ui->clip_y = 0;
        ui->clip_width = (uint32_t)(ui->logical_width > 0.0f
            ? ui->logical_width : (float)ui->swapchain->width);
        ui->clip_height = (uint32_t)(ui->logical_height > 0.0f
            ? ui->logical_height : (float)ui->swapchain->height);
        return true;
    }
    if (x < 0) { width = width > (uint32_t)(-x) ? width - (uint32_t)(-x) : 0; x = 0; }
    if (y < 0) { height = height > (uint32_t)(-y) ? height - (uint32_t)(-y) : 0; y = 0; }
    uint32_t logical_width = (uint32_t)(ui->logical_width > 0.0f
        ? ui->logical_width : (float)ui->swapchain->width);
    uint32_t logical_height = (uint32_t)(ui->logical_height > 0.0f
        ? ui->logical_height : (float)ui->swapchain->height);
    if ((uint32_t)x >= logical_width || (uint32_t)y >= logical_height) {
        width = 0; height = 0;
    } else {
        if (width > logical_width - (uint32_t)x)
            width = logical_width - (uint32_t)x;
        if (height > logical_height - (uint32_t)y)
            height = logical_height - (uint32_t)y;
    }
    ui->clip_x = x;
    ui->clip_y = y;
    ui->clip_width = width;
    ui->clip_height = height;
    return true;
}

static bool luma_vk_ui_geometry_bound(size_t handle, const float *vertices, size_t vertex_count,
    size_t vertex_stride_floats, size_t color_offset_floats,
    const int *indices, size_t index_count, LumaVkUiTexture *texture) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available || vertices == NULL || indices == NULL ||
        vertex_stride_floats < color_offset_floats + 4 || index_count == 0) return false;
    size_t first_vertex = ui->vertex_count;
    size_t first_index = ui->index_count;
    if (!luma_vk_ui_reserve_cpu(ui, first_vertex + vertex_count,
            first_index + index_count) ||
        !luma_vk_ui_reserve_batches(ui, ui->batch_count + 1)) {
        luma_vk_ui_error(ui, "Unable to grow retained UI geometry");
        return false;
    }
    float width = ui->logical_width > 0.0f
        ? ui->logical_width : (float)ui->swapchain->width;
    float height = ui->logical_height > 0.0f
        ? ui->logical_height : (float)ui->swapchain->height;
    for (size_t i = 0; i < vertex_count; ++i) {
        const float *source = vertices + i * vertex_stride_floats;
        LumaVkUiVertex *target = &ui->vertices[first_vertex + i];
        target->x = source[0] * 2.0f / width - 1.0f;
        target->y = source[1] * 2.0f / height - 1.0f;
        target->r = source[color_offset_floats + 0];
        target->g = source[color_offset_floats + 1];
        target->b = source[color_offset_floats + 2];
        target->a = source[color_offset_floats + 3];
        target->u = source[vertex_stride_floats - 2];
        target->v = source[vertex_stride_floats - 1];
    }
    for (size_t i = 0; i < index_count; ++i) {
        if (indices[i] < 0 || (size_t)indices[i] >= vertex_count) return false;
        ui->indices[ui->index_count + i] = (uint32_t)(first_vertex + (size_t)indices[i]);
    }
    ui->vertex_count += vertex_count;
    ui->index_count += index_count;
    LumaVkUiBatch *batch = ui->batch_count == 0 ? NULL : &ui->batches[ui->batch_count - 1];
    if (!ui->force_new_batch && batch != NULL &&
        batch->first_index + batch->index_count == first_index &&
        batch->clip_x == ui->clip_x && batch->clip_y == ui->clip_y &&
        batch->clip_width == ui->clip_width && batch->clip_height == ui->clip_height &&
        batch->texture == texture) {
        batch->index_count += (uint32_t)index_count;
    } else {
        ui->batches[ui->batch_count++] = (LumaVkUiBatch){
            (uint32_t)first_index, (uint32_t)index_count,
            ui->clip_x, ui->clip_y, ui->clip_width, ui->clip_height, texture};
    }
    ui->force_new_batch = false;
    return true;
}

bool luma_vk_ui_geometry(size_t handle, const float *vertices, size_t vertex_count,
    size_t vertex_stride_floats, size_t color_offset_floats,
    const int *indices, size_t index_count) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return luma_vk_ui_geometry_bound(handle, vertices, vertex_count,
        vertex_stride_floats, color_offset_floats, indices, index_count,
        ui == NULL ? NULL : ui->white_texture);
}

size_t luma_vk_ui_texture_create(size_t handle, const void *pixels,
    uint32_t width, uint32_t height, uint32_t pitch, uint32_t format) {
    return (size_t)luma_vk_ui_texture_build((LumaVkUiCompositor *)handle,
        pixels, width, height, pitch, format);
}

size_t luma_vk_ui_texture_create_empty(size_t handle, uint32_t width,
    uint32_t height, uint32_t format) {
    return (size_t)luma_vk_ui_texture_empty(
        (LumaVkUiCompositor *)handle, width, height, format);
}

bool luma_vk_ui_texture_update(size_t handle, const void *pixels,
    uint32_t width, uint32_t height, uint32_t pitch, uint32_t x,
    uint32_t y) {
    LumaVkUiTexture *texture = (LumaVkUiTexture *)handle;
    if (texture == NULL || texture->destroy_pending ||
        texture->atlas_pixels == NULL || pixels == NULL || width == 0 ||
        height == 0 || pitch < width * 4u || x > texture->width ||
        width > texture->width - x || y > texture->height ||
        height > texture->height - y) return false;
    for (uint32_t row = 0; row < height; ++row)
        memcpy(texture->atlas_pixels +
                ((size_t)(y + row) * texture->width + x) * 4u,
            (const unsigned char *)pixels + (size_t)row * pitch,
            (size_t)width * 4u);
    if (!texture->atlas_dirty) {
        texture->dirty_x = x;
        texture->dirty_y = y;
        texture->dirty_right = x + width;
        texture->dirty_bottom = y + height;
        texture->atlas_dirty = true;
    } else {
        if (x < texture->dirty_x) texture->dirty_x = x;
        if (y < texture->dirty_y) texture->dirty_y = y;
        if (x + width > texture->dirty_right)
            texture->dirty_right = x + width;
        if (y + height > texture->dirty_bottom)
            texture->dirty_bottom = y + height;
    }
    return true;
}

size_t luma_vk_ui_external_texture_create(size_t handle, uint64_t image,
    uint32_t width, uint32_t height, uint32_t layout) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available || image == 0 || width == 0 || height == 0 ||
        layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) return 0;
    LumaVkUiTexture *result = (LumaVkUiTexture *)calloc(1, sizeof(LumaVkUiTexture));
    GrapheneVkTexture *texture = (GrapheneVkTexture *)calloc(1, sizeof(GrapheneVkTexture));
    if (result == NULL || texture == NULL) {
        free(result); free(texture); return 0;
    }
    texture->owner = ui->device;
    texture->image = (void *)(uintptr_t)image;
    texture->width = width;
    texture->height = height;
    texture->mip_levels = 1;
    texture->format = VK_FORMAT_B8G8R8A8_UNORM;
    texture->layout = layout;
    texture->aspect = VK_IMAGE_ASPECT_COLOR_BIT;
    VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO, NULL, 0,
        texture->image, 1, texture->format, {0, 0, 0, 0},
        {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1}};
    if (ui->device->create_image_view(ui->device->device, &view, NULL,
            &texture->view) != VK_SUCCESS) {
        free(texture); free(result); return 0;
    }
    texture->available = true;
    result->texture = texture;
    result->owner = ui;
    result->width = width;
    result->height = height;
    result->group = (GrapheneVkBindGroup *)graphene_vk_texture_bind_group_create(
        (size_t)ui->device, (size_t)ui->texture_layout,
        (size_t)texture, (size_t)ui->sampler);
    if (result->group == NULL || !result->group->available) {
        luma_vk_ui_texture_release(result);
        return 0;
    }
    return (size_t)result;
}

bool luma_vk_ui_texture_destroy(size_t handle) {
    if (handle == 0) return false;
    LumaVkUiTexture *texture = (LumaVkUiTexture *)handle;
    LumaVkUiCompositor *ui = texture->owner;
    if (ui == NULL || texture->destroy_pending ||
        !luma_vk_ui_reserve_retired_textures(ui, ui->retired_texture_count + 1))
        return false;
    texture->destroy_pending = true;
    texture->retire_slots = 3u;
    for (size_t i = 0; i < ui->retained_batch_count; ++i) {
        if (ui->batches[i].texture == texture) {
            ui->retained_ready = false;
            ui->retained_vertex_count = 0;
            ui->retained_index_count = 0;
            ui->retained_batch_count = 0;
            break;
        }
    }
    ui->retired_textures[ui->retired_texture_count++] = texture;
    return true;
}

bool luma_vk_ui_texture_draw_region(size_t handle, size_t texture_handle,
    float x, float y, float width, float height, uint32_t source_x,
    uint32_t source_y, uint32_t source_width, uint32_t source_height);

bool luma_vk_ui_texture_draw(size_t handle, size_t texture_handle,
    float x, float y, float width, float height) {
    LumaVkUiTexture *texture = (LumaVkUiTexture *)texture_handle;
    return luma_vk_ui_texture_draw_region(handle, texture_handle, x, y,
        width, height, 0u, 0u, texture == NULL ? 0u : texture->width,
        texture == NULL ? 0u : texture->height);
}

bool luma_vk_ui_texture_draw_region(size_t handle, size_t texture_handle,
    float x, float y, float width, float height, uint32_t source_x,
    uint32_t source_y, uint32_t source_width, uint32_t source_height) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    LumaVkUiTexture *texture = (LumaVkUiTexture *)texture_handle;
    if (ui == NULL || texture == NULL || texture->owner != ui ||
        texture->destroy_pending || texture->group == NULL ||
        width <= 0.0f || height <= 0.0f || source_width == 0 ||
        source_height == 0 || source_x > texture->width ||
        source_width > texture->width - source_x ||
        source_y > texture->height ||
        source_height > texture->height - source_y) return false;
    float u0 = (float)source_x / (float)texture->width;
    float v0 = (float)source_y / (float)texture->height;
    float u1 = (float)(source_x + source_width) / (float)texture->width;
    float v1 = (float)(source_y + source_height) / (float)texture->height;
    float vertices[4][8] = {
        {x,y,1,1,1,1,u0,v0}, {x+width,y,1,1,1,1,u1,v0},
        {x+width,y+height,1,1,1,1,u1,v1},
        {x,y+height,1,1,1,1,u0,v1}
    };
    int indices[6] = {0,1,2,0,2,3};
    return luma_vk_ui_geometry_bound(handle, &vertices[0][0], 4, 8, 2,
        indices, 6, texture);
}

bool luma_vk_ui_composition_split(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available) return false;
    ui->composition_split_batch = ui->batch_count;
    ui->force_new_batch = true;
    return true;
}

static bool luma_vk_ui_prepare_gpu(LumaVkUiCompositor *ui,
    GrapheneVkFrame *frame) {
    size_t vertex_bytes = ui->vertex_count * sizeof(LumaVkUiVertex);
    size_t index_bytes = ui->index_count * sizeof(uint32_t);
    for (size_t i = 0; i < ui->batch_count; ++i) {
        if (!luma_vk_ui_texture_flush(ui->batches[i].texture)) {
            luma_vk_ui_error(ui, "Unable to upload a direct Vulkan atlas page");
            return false;
        }
    }
    uint32_t slot = frame->frame_index;
    luma_vk_ui_collect_retired(ui, slot);
    if (ui->index_count > 0 &&
        (!luma_vk_ui_ensure_gpu(ui, slot, vertex_bytes, index_bytes) ||
         !luma_vk_ui_upload(ui->vertex_buffers[slot], ui->vertices, vertex_bytes) ||
         !luma_vk_ui_upload(ui->index_buffers[slot], ui->indices, index_bytes))) {
        luma_vk_ui_error(ui, "Unable to upload direct UI geometry");
        return false;
    }
    return true;
}

static bool luma_vk_ui_draw_range(LumaVkUiCompositor *ui,
    GrapheneVkFrame *frame, size_t first, size_t end) {
    if (first > ui->batch_count) first = ui->batch_count;
    if (end > ui->batch_count) end = ui->batch_count;
    if (end <= first) return true;
    uint32_t slot = frame->frame_index;
    bool rendered = true;
    if (ui->index_count > 0) {
        rendered = graphene_vk_frame_bind_vertex_buffer((size_t)frame,
                (size_t)ui->vertex_buffers[slot]) &&
            graphene_vk_frame_bind_index_buffer((size_t)frame,
                (size_t)ui->index_buffers[slot]);
        for (size_t i = first; i < end && rendered; ++i) {
            LumaVkUiBatch *batch = &ui->batches[i];
            luma_vk_ui_texture_collect_upload(batch->texture);
            float logical_width = ui->logical_width > 0.0f
                ? ui->logical_width : (float)ui->swapchain->width;
            float logical_height = ui->logical_height > 0.0f
                ? ui->logical_height : (float)ui->swapchain->height;
            float scale_x = (float)ui->swapchain->width / logical_width;
            float scale_y = (float)ui->swapchain->height / logical_height;
            int32_t scissor_x = (int32_t)((float)batch->clip_x * scale_x);
            int32_t scissor_y = (int32_t)((float)batch->clip_y * scale_y);
            uint32_t scissor_right = (uint32_t)(
                ((float)batch->clip_x + (float)batch->clip_width) * scale_x + 0.9999f);
            uint32_t scissor_bottom = (uint32_t)(
                ((float)batch->clip_y + (float)batch->clip_height) * scale_y + 0.9999f);
            if (scissor_right > ui->swapchain->width) scissor_right = ui->swapchain->width;
            if (scissor_bottom > ui->swapchain->height) scissor_bottom = ui->swapchain->height;
            VkRect2D scissor = {{scissor_x, scissor_y},
                {scissor_right > (uint32_t)scissor_x
                    ? scissor_right - (uint32_t)scissor_x : 0,
                 scissor_bottom > (uint32_t)scissor_y
                    ? scissor_bottom - (uint32_t)scissor_y : 0}};
            ui->swapchain->cmd_set_scissor(frame->command_buffer, 0, 1, &scissor);
            rendered = batch->texture != NULL && !batch->texture->destroy_pending &&
                graphene_vk_frame_bind_group((size_t)frame,
                (size_t)batch->texture->group) &&
                graphene_vk_frame_draw_indexed((size_t)frame,
                batch->index_count, 1u, batch->first_index, 0, 0u);
        }
    }
    return rendered;
}

size_t luma_vk_ui_frame_acquire(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available) return 0;
    GrapheneVkFrame *frame = (GrapheneVkFrame *)
        graphene_vk_frame_acquire((size_t)ui->swapchain);
    if (frame == NULL || !frame->recording) {
        luma_vk_ui_error(ui,
            frame == NULL ? "Unable to allocate direct UI frame" : frame->error);
        if (frame != NULL) graphene_vk_frame_destroy((size_t)frame);
        return 0;
    }
    if (!luma_vk_ui_prepare_gpu(ui, frame) ||
        !graphene_vk_frame_begin_render_pass((size_t)frame,
            (size_t)ui->pipeline,
            ui->clear[0], ui->clear[1], ui->clear[2], ui->clear[3]) ||
        !luma_vk_ui_draw_range(ui, frame, 0, ui->composition_split_batch)) {
        luma_vk_ui_error(ui, frame->error);
        graphene_vk_frame_destroy((size_t)frame);
        return 0;
    }
    return (size_t)frame;
}

static bool luma_vk_ui_finish_frame(LumaVkUiCompositor *ui,
    GrapheneVkFrame *frame, bool composed) {
    if (ui == NULL || !ui->available || frame == NULL ||
        !frame->recording || frame->complete ||
        frame->owner != ui->swapchain) {
        if (ui != NULL)
            luma_vk_ui_error(ui, "Invalid borrowed direct Vulkan frame");
        return false;
    }
    bool rendered = true;
    if (composed) {
        rendered = frame->in_render_pass &&
            graphene_vk_frame_bind_pipeline(
                (size_t)frame, (size_t)ui->load_pipeline) &&
            luma_vk_ui_draw_range(ui, frame,
                ui->composition_split_batch, ui->batch_count);
    } else {
        rendered = !frame->in_render_pass &&
            luma_vk_ui_prepare_gpu(ui, frame) &&
            graphene_vk_frame_begin_render_pass((size_t)frame,
                (size_t)ui->pipeline,
                ui->clear[0], ui->clear[1], ui->clear[2], ui->clear[3]) &&
            luma_vk_ui_draw_range(ui, frame, 0, ui->batch_count);
    }
    rendered = graphene_vk_frame_end_render_pass((size_t)frame) && rendered;
    rendered = graphene_vk_frame_present((size_t)frame) && rendered;
    graphene_vk_frame_destroy((size_t)frame);
    return rendered;
}

bool luma_vk_ui_frame_finish(size_t handle, size_t frame_handle) {
    return luma_vk_ui_finish_frame((LumaVkUiCompositor *)handle,
        (GrapheneVkFrame *)frame_handle, true);
}

bool luma_vk_ui_end(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    GrapheneVkFrame *frame = ui == NULL ? NULL : (GrapheneVkFrame *)
        graphene_vk_frame_acquire((size_t)ui->swapchain);
    if (frame == NULL || !frame->recording) {
        if (ui != NULL)
            luma_vk_ui_error(ui, frame == NULL
                ? "Unable to allocate direct UI frame" : frame->error);
        if (frame != NULL) graphene_vk_frame_destroy((size_t)frame);
        return false;
    }
    return luma_vk_ui_finish_frame(ui, frame, false);
}

bool luma_vk_ui_destroy(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL) return false;
    if (ui->swapchain != NULL &&
        !graphene_vk_swapchain_wait((size_t)ui->swapchain)) return false;
    for (uint32_t slot = 0; slot < 2; ++slot) {
        if (ui->vertex_buffers[slot] != NULL)
            graphene_vk_buffer_destroy((size_t)ui->vertex_buffers[slot]);
        if (ui->index_buffers[slot] != NULL)
            graphene_vk_buffer_destroy((size_t)ui->index_buffers[slot]);
    }
    if (ui->pipeline != NULL) graphene_vk_pipeline_destroy((size_t)ui->pipeline);
    if (ui->load_pipeline != NULL)
        graphene_vk_pipeline_destroy((size_t)ui->load_pipeline);
    for (size_t i = 0; i < ui->retired_texture_count; ++i)
        luma_vk_ui_texture_release(ui->retired_textures[i]);
    luma_vk_ui_texture_release(ui->white_texture);
    if (ui->sampler != NULL) graphene_vk_sampler_destroy((size_t)ui->sampler);
    if (ui->texture_layout != NULL)
        graphene_vk_bind_group_layout_destroy((size_t)ui->texture_layout);
    if (ui->vertex_shader != NULL) graphene_vk_shader_destroy((size_t)ui->vertex_shader);
    if (ui->fragment_shader != NULL) graphene_vk_shader_destroy((size_t)ui->fragment_shader);
    free(ui->vertices);
    free(ui->indices);
    free(ui->batches);
    free(ui->retired_textures);
    free(ui);
    return true;
}
