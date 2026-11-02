#include "vulkan_api.h"
#include "raster_bounds.h"
#include "sdl_api.h"
/* Retained 2D geometry compositor for Luma's direct Vulkan backend. */
#include "ui_shaders.inc"

typedef struct {
    float x, y;
    float r, g, b, a;
    float u, v;
} LumaVkUiVertex;

typedef struct LumaVkUiMesh {
    struct LumaVkUiMesh *next;
    char *key;
    size_t key_length, bytes, vertex_count, index_count;
    uint64_t hash, used;
    float scale, stroke, density;
    LumaVkUiVertex *vertices;
    uint32_t *indices;
    EncoreVkBuffer *vertex_buffer, *index_buffer;
    uint32_t pending_slots;
    bool transient;
    struct LumaVkUiCompositor *owner;
    size_t arena_offset, arena_bytes, index_offset;
} LumaVkUiMesh;

typedef struct LumaVkUiMeshBlock {
    size_t offset, bytes;
    struct LumaVkUiMeshBlock *next;
} LumaVkUiMeshBlock;

typedef struct {
    uint32_t first_index, index_count;
    int32_t clip_x, clip_y;
    uint32_t clip_width, clip_height;
    struct LumaVkUiTexture *texture;
    LumaVkUiMesh *mesh;
    float x, y, density;
    uint32_t color;
} LumaVkUiBatch;

typedef struct {
    uint64_t key;
    LumaVkUiVertex *vertices;
    uint32_t *indices;
    LumaVkUiBatch *batches;
    size_t vertex_count, index_count, batch_count;
} LumaVkUiPatchSegment;

typedef struct LumaVkUiUpload {
    EncoreVkBuffer *staging;
    EncoreVkSubmission *submission;
    struct LumaVkUiUpload *next;
} LumaVkUiUpload;

typedef struct LumaVkUiTexture {
    struct LumaVkUiCompositor *owner;
    EncoreVkTexture *texture;
    EncoreVkBindGroup *group;
    LumaVkUiUpload *uploads;
    unsigned char *atlas_pixels;
    uint32_t dirty_x, dirty_y, dirty_right, dirty_bottom;
    uint32_t width, height;
    uint8_t retire_slots;
    bool atlas_dirty;
    bool destroy_pending;
} LumaVkUiTexture;

typedef struct LumaVkUiCompositor {
    EncoreVkDevice *device;
    EncoreVkSwapchain *swapchain;
    EncoreVkShader *vertex_shader;
    EncoreVkShader *fragment_shader;
    EncoreVkBindGroupLayout *texture_layout;
    EncoreVkSampler *sampler;
    LumaVkUiTexture *white_texture;
    EncoreVkPipeline *pipeline;
    EncoreVkPipeline *load_pipeline;
    EncoreVkBuffer *vertex_buffers[2];
    EncoreVkBuffer *index_buffers[2];
    uint64_t geometry_revision, uploaded_revision[2];
    size_t uploaded_vertices[2], uploaded_indices[2];
    uint64_t geometry_upload_bytes;
    uint64_t presentation_ns;
    uint64_t acquire_ns, present_ns, gpu_preparation_ns;
    LumaVkUiVertex *vertices;
    uint32_t *indices;
    LumaVkUiBatch *batches;
    LumaVkUiTexture **retired_textures;
    size_t vertex_count, vertex_capacity;
    size_t index_count, index_capacity;
    size_t batch_count, batch_capacity;
    size_t composition_split_batch;
    size_t retained_base_vertex_count, retained_base_index_count;
    size_t retained_base_batch_count;
    size_t retained_vertex_count, retained_index_count, retained_batch_count;
    LumaVkUiPatchSegment *patches;
    size_t patch_count, patch_capacity;
    size_t patch_vertex_start, patch_index_start, patch_batch_start;
    uint64_t patch_key;
    uint64_t segmented_commit_count;
    bool patch_recording;
    size_t retired_texture_count, retired_texture_capacity;
    bool retained_ready;
    bool force_new_batch;
    int32_t clip_x, clip_y;
    uint32_t clip_width, clip_height;
    float logical_width, logical_height;
    float clear[4];
    char error[256];
    bool available;
    LumaVkUiMesh *meshes, *capturing_mesh;
    size_t mesh_bytes, mesh_vertex_start, mesh_index_start, mesh_batch_start;
    float mesh_x, mesh_y;
    uint32_t mesh_color, acquire_status;
    uint64_t mesh_clock, mesh_builds, mesh_hits, mesh_upload_bytes;
    EncoreVkBuffer *mesh_arena;
    unsigned char *mesh_mapping;
    LumaVkUiMeshBlock *mesh_free;
    EncoreVkBuffer *capture_buffer;
    uint32_t capture_width, capture_height, capture_format;
    bool capture_requested, capture_ready;
} LumaVkUiCompositor;

static uint64_t luma_vk_ui_clock_ns(void) {
    const UiSdlApi *api = encore_sdl_api_v1();
    return api && api->GetTicksNS ? api->GetTicksNS() : 0;
}

uint64_t luma_vk_ui_presentation_ns(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return ui ? ui->presentation_ns : 0;
}
uint32_t luma_vk_ui_acquire_status(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return ui ? ui->acquire_status : ENCORE_VK_FRAME_FAILED;
}
void luma_vk_ui_wake(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui && ui->acquire_status == ENCORE_VK_FRAME_SUSPENDED)
        ui->acquire_status = ENCORE_VK_FRAME_READY;
}

uint64_t luma_vk_ui_upload_bytes(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return ui ? ui->geometry_upload_bytes : 0;
}

static EncoreVkFrame *luma_vk_ui_acquire_timed(LumaVkUiCompositor *ui) {
    if (!ui) return NULL;
    uint64_t started = luma_vk_ui_clock_ns();
    EncoreVkFrame *frame = (EncoreVkFrame *)encore_vk_frame_try_acquire((size_t)ui->swapchain);
    ui->acquire_status = encore_vk_frame_acquire_status((size_t)frame);
    uint64_t elapsed = luma_vk_ui_clock_ns() - started;
    ui->presentation_ns += elapsed; ui->acquire_ns += elapsed;
    if (encore_vk_frame_available((size_t)frame)) encore_vk_frame_timing_mark((size_t)frame, 8, true);
    return frame;
}

static bool luma_vk_ui_geometry_bound(size_t handle, const float *vertices,
    size_t vertex_count, size_t vertex_stride_floats,
    size_t color_offset_floats, const int *indices, size_t index_count,
    LumaVkUiTexture *texture);

static void luma_vk_ui_patch_release(LumaVkUiPatchSegment *patch) {
    if (patch == NULL) return;
    free(patch->vertices);
    free(patch->indices);
    free(patch->batches);
    memset(patch, 0, sizeof(*patch));
}

static void luma_vk_ui_patches_clear(LumaVkUiCompositor *ui) {
    if (ui == NULL) return;
    for (size_t i = 0; i < ui->patch_count; ++i)
        luma_vk_ui_patch_release(&ui->patches[i]);
    ui->patch_count = 0;
}

static bool luma_vk_ui_patch_reserve(LumaVkUiCompositor *ui, size_t count) {
    if (count <= ui->patch_capacity) return true;
    size_t capacity = ui->patch_capacity == 0 ? 16 : ui->patch_capacity;
    while (capacity < count) capacity *= 2;
    LumaVkUiPatchSegment *fresh = (LumaVkUiPatchSegment *)realloc(
        ui->patches, capacity * sizeof(LumaVkUiPatchSegment));
    if (fresh == NULL) return false;
    memset(fresh + ui->patch_capacity, 0,
        (capacity - ui->patch_capacity) * sizeof(LumaVkUiPatchSegment));
    ui->patches = fresh;
    ui->patch_capacity = capacity;
    return true;
}

static void luma_vk_ui_error(LumaVkUiCompositor *ui, const char *message) {
    if (ui == NULL) return;
    ui->acquire_status = ENCORE_VK_FRAME_FAILED;
    snprintf(ui->error, sizeof(ui->error), "%s", message == NULL ? "Direct UI error" : message);
    fprintf(stderr, "Luma Vulkan UI error: %s\n", ui->error);
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

static uint64_t luma_vk_ui_patch_bounds_key(float x, float y,
    float width, float height) {
    uint32_t values[4];
    memcpy(&values[0], &x, sizeof(float));
    memcpy(&values[1], &y, sizeof(float));
    memcpy(&values[2], &width, sizeof(float));
    memcpy(&values[3], &height, sizeof(float));
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < 4; ++i) {
        hash ^= values[i];
        hash *= 1099511628211ull;
    }
    return hash == 0 ? 1 : hash;
}

static bool luma_vk_ui_flatten_patches(LumaVkUiCompositor *ui) {
    size_t vertices = ui->retained_base_vertex_count;
    size_t indices = ui->retained_base_index_count;
    size_t batches = ui->retained_base_batch_count;
    for (size_t i = 0; i < ui->patch_count; ++i) {
        vertices += ui->patches[i].vertex_count;
        indices += ui->patches[i].index_count;
        batches += ui->patches[i].batch_count;
    }
    if (!luma_vk_ui_reserve_cpu(ui, vertices, indices) ||
        !luma_vk_ui_reserve_batches(ui, batches)) return false;
    ui->vertex_count = ui->retained_base_vertex_count;
    ui->index_count = ui->retained_base_index_count;
    ui->batch_count = ui->retained_base_batch_count;
    for (size_t i = 0; i < ui->patch_count; ++i) {
        LumaVkUiPatchSegment *patch = &ui->patches[i];
        size_t vertex_base = ui->vertex_count;
        size_t index_base = ui->index_count;
        if (patch->vertex_count > 0)
            memcpy(ui->vertices + vertex_base, patch->vertices,
                patch->vertex_count * sizeof(LumaVkUiVertex));
        for (size_t j = 0; j < patch->index_count; ++j)
            ui->indices[index_base + j] =
                (uint32_t)(vertex_base + patch->indices[j]);
        for (size_t j = 0; j < patch->batch_count; ++j) {
            LumaVkUiBatch batch = patch->batches[j];
            if (batch.mesh == NULL) batch.first_index += (uint32_t)index_base;
            ui->batches[ui->batch_count + j] = batch;
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

static bool luma_vk_ui_upload(EncoreVkBuffer *buffer, const void *data, size_t bytes) {
    void *mapped=encore_vk_buffer_map((size_t)buffer,bytes);
    if (!mapped) return false;
    memcpy(mapped,data,bytes);
    encore_vk_buffer_unmap((size_t)buffer); return true;
}

static bool luma_vk_ui_geometry_resident(const LumaVkUiCompositor *ui, uint32_t slot) {
    return slot < 2 && ui->geometry_revision != 0 &&
        ui->uploaded_revision[slot] == ui->geometry_revision &&
        ui->uploaded_vertices[slot] == ui->vertex_count &&
        ui->uploaded_indices[slot] == ui->index_count;
}

static bool luma_vk_ui_track_upload(LumaVkUiTexture *texture,
    EncoreVkBuffer *staging, EncoreVkSubmission *submission) {
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
    result->texture = (EncoreVkTexture *)encore_vk_texture_create(
        (size_t)ui->device, width, height, 1u, format, 6u);
    EncoreVkBuffer *staging = (EncoreVkBuffer *)encore_vk_buffer_create(
        (size_t)ui->device, (uint64_t)width * height * 4u, 1u, true);
    if (result->texture == NULL || !encore_vk_texture_available((size_t)result->texture) ||
        staging == NULL || !encore_vk_buffer_available((size_t)staging)) goto failed;
    void *mapped = NULL;
    size_t packed_pitch = (size_t)width * 4u;
    mapped = encore_vk_buffer_map((size_t)staging, packed_pitch * height);
    if (mapped == NULL) goto failed;
    for (uint32_t row = 0; row < height; ++row)
        memcpy((unsigned char *)mapped + (size_t)row * packed_pitch,
            (const unsigned char *)pixels + (size_t)row * pitch, packed_pitch);
    encore_vk_buffer_unmap((size_t)staging);
    EncoreVkEncoder *encoder = (EncoreVkEncoder *)encore_vk_encoder_create(
        (size_t)ui->device);
    if (encoder == NULL || !encore_vk_encoder_available((size_t)encoder) ||
        !encore_vk_encoder_transition_texture((size_t)encoder,
            (size_t)result->texture, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
        !encore_vk_encoder_copy_buffer_to_texture((size_t)encoder,
            (size_t)staging, (size_t)result->texture, width, height, 0u) ||
        !encore_vk_encoder_transition_texture((size_t)encoder,
            (size_t)result->texture, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
        if (encoder != NULL) encore_vk_encoder_destroy((size_t)encoder);
        goto failed;
    }
    EncoreVkSubmission *submission = (EncoreVkSubmission *)
        encore_vk_encoder_submit((size_t)encoder);
    encore_vk_encoder_destroy((size_t)encoder);
    if (submission == NULL ||
        !encore_vk_submission_available((size_t)submission)) {
        if (submission != NULL)
            encore_vk_submission_destroy((size_t)submission);
        goto failed;
    }
    if (!luma_vk_ui_track_upload(result, staging, submission)) {
        encore_vk_submission_destroy((size_t)submission);
        encore_vk_buffer_destroy((size_t)staging);
        goto failed_without_staging;
    }
    result->group = (EncoreVkBindGroup *)encore_vk_texture_bind_group_create(
        (size_t)ui->device, (size_t)ui->texture_layout,
        (size_t)result->texture, (size_t)ui->sampler);
    if (result->group == NULL || !encore_vk_bind_group_available((size_t)result->group)) goto failed_without_staging;
    return result;

failed:
    if (staging != NULL && result->uploads == NULL)
        encore_vk_buffer_destroy((size_t)staging);
failed_without_staging:
    while (result->uploads != NULL) {
        LumaVkUiUpload *upload = result->uploads;
        result->uploads = upload->next;
        encore_vk_submission_destroy((size_t)upload->submission);
        encore_vk_buffer_destroy((size_t)upload->staging);
        free(upload);
    }
    if (result->group != NULL) encore_vk_bind_group_destroy((size_t)result->group);
    if (result->texture != NULL) encore_vk_texture_destroy((size_t)result->texture);
    free(result);
    return NULL;
}

static void luma_vk_ui_texture_release(LumaVkUiTexture *texture) {
    if (texture == NULL) return;
    while (texture->uploads != NULL) {
        LumaVkUiUpload *upload = texture->uploads;
        texture->uploads = upload->next;
        encore_vk_submission_destroy((size_t)upload->submission);
        encore_vk_buffer_destroy((size_t)upload->staging);
        free(upload);
    }
    if (texture->group != NULL) encore_vk_bind_group_destroy((size_t)texture->group);
    if (texture->texture != NULL) encore_vk_texture_destroy((size_t)texture->texture);
    free(texture->atlas_pixels);
    free(texture);
}

static void luma_vk_ui_texture_collect_upload(LumaVkUiTexture *texture) {
    if (texture == NULL) return;
    LumaVkUiUpload **link = &texture->uploads;
    while (*link != NULL) {
        LumaVkUiUpload *upload = *link;
        if (!encore_vk_submission_complete((size_t)upload->submission)) {
            link = &upload->next;
            continue;
        }
        *link = upload->next;
        encore_vk_submission_destroy((size_t)upload->submission);
        encore_vk_buffer_destroy((size_t)upload->staging);
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
    result->texture = (EncoreVkTexture *)encore_vk_texture_create(
        (size_t)ui->device, width, height, 1u, format, 6u);
    if (result->atlas_pixels == NULL || result->texture == NULL ||
        !encore_vk_texture_available((size_t)result->texture)) {
        luma_vk_ui_texture_release(result);
        return NULL;
    }
    /* Descriptors require SHADER_READ_ONLY_OPTIMAL. A fresh image is still
     * UNDEFINED: defer binding until the first aggregated upload transitions
     * it. Initialize the complete page, including transparent atlas gutters. */
    result->atlas_dirty = true;
    result->dirty_right = width;
    result->dirty_bottom = height;
    return result;
}

static bool luma_vk_ui_texture_flush(LumaVkUiTexture *texture) {
    if (texture == NULL || !texture->atlas_dirty ||
        texture->atlas_pixels == NULL) return true;
    luma_vk_ui_texture_collect_upload(texture);
    uint32_t width = texture->dirty_right - texture->dirty_x;
    uint32_t height = texture->dirty_bottom - texture->dirty_y;
    uint64_t bytes = (uint64_t)width * height * 4u;
    EncoreVkBuffer *staging = (EncoreVkBuffer *)
        encore_vk_buffer_create((size_t)texture->owner->device,
            bytes, 1u, true);
    if (staging == NULL || !encore_vk_buffer_available((size_t)staging)) {
        if (staging != NULL) encore_vk_buffer_destroy((size_t)staging);
        return false;
    }
    void *mapped = NULL;
    if ((mapped = encore_vk_buffer_map((size_t)staging, bytes)) == NULL) {
        encore_vk_buffer_destroy((size_t)staging);
        return false;
    }
    for (uint32_t row = 0; row < height; ++row)
        memcpy((unsigned char *)mapped + (size_t)row * width * 4u,
            texture->atlas_pixels +
                ((size_t)(texture->dirty_y + row) * texture->width +
                 texture->dirty_x) * 4u,
            (size_t)width * 4u);
    encore_vk_buffer_unmap((size_t)staging);
    EncoreVkEncoder *encoder = (EncoreVkEncoder *)
        encore_vk_encoder_create((size_t)texture->owner->device);
    if (encoder == NULL ||
        !encore_vk_encoder_available((size_t)encoder) ||
        !encore_vk_encoder_transition_texture((size_t)encoder,
            (size_t)texture->texture,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) ||
        !encore_vk_encoder_copy_buffer_to_texture_region(
            (size_t)encoder, (size_t)staging, (size_t)texture->texture,
            width, height, 0u, texture->dirty_x, texture->dirty_y) ||
        !encore_vk_encoder_transition_texture((size_t)encoder,
            (size_t)texture->texture,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)) {
        if (encoder != NULL) encore_vk_encoder_destroy((size_t)encoder);
        encore_vk_buffer_destroy((size_t)staging);
        return false;
    }
    EncoreVkSubmission *submission = (EncoreVkSubmission *)
        encore_vk_encoder_submit((size_t)encoder);
    encore_vk_encoder_destroy((size_t)encoder);
    /* Upload and draw use the same device queue. The recorded image barriers
     * order the copy with prior sampling and the following UI frame. Retain
     * staging/commands through the existing upload owner instead of stalling
     * the CPU whenever a dynamic text run changes. */
    bool uploaded = submission != NULL &&
        encore_vk_submission_available((size_t)submission) &&
        luma_vk_ui_track_upload(texture, staging, submission);
    if (!uploaded) {
        if (submission != NULL)
            encore_vk_submission_destroy((size_t)submission);
        encore_vk_buffer_destroy((size_t)staging);
        return false;
    }
    if (texture->group == NULL) {
        EncoreVkBindGroup *group = (EncoreVkBindGroup *)
            encore_vk_texture_bind_group_create((size_t)texture->owner->device,
                (size_t)texture->owner->texture_layout,
                (size_t)texture->texture, (size_t)texture->owner->sampler);
        if (group == NULL || !encore_vk_bind_group_available((size_t)group)) {
            if (group != NULL) encore_vk_bind_group_destroy((size_t)group);
            return false;
        }
        texture->group = group;
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

static bool luma_vk_ui_texture_referenced(
    const LumaVkUiCompositor *ui, const LumaVkUiTexture *texture) {
    if (ui == NULL || texture == NULL) return false;
    for (size_t i = 0; i < ui->batch_count; ++i) {
        if (ui->batches[i].texture == texture) return true;
    }
    for (size_t i = 0; i < ui->patch_count; ++i) {
        const LumaVkUiPatchSegment *patch = &ui->patches[i];
        for (size_t j = 0; j < patch->batch_count; ++j) {
            if (patch->batches[j].texture == texture) return true;
        }
    }
    return false;
}

/* The corresponding frame fence has completed before this is called. */
static void luma_vk_ui_collect_retired(LumaVkUiCompositor *ui, uint32_t slot) {
    if (ui == NULL || slot >= 2) return;
    size_t write = 0;
    for (size_t i = 0; i < ui->retired_texture_count; ++i) {
        LumaVkUiTexture *texture = ui->retired_textures[i];
        texture->retire_slots &= (uint8_t)~(1u << slot);
        if (texture->retire_slots == 0 &&
            !luma_vk_ui_texture_referenced(ui, texture))
            luma_vk_ui_texture_release(texture);
        else
            ui->retired_textures[write++] = texture;
    }
    ui->retired_texture_count = write;
}

/* A producer may replace its image without waiting for two more visible
   frames. Poll the consumer fence; never wait for the queue to become idle. */
bool luma_vk_ui_external_release_ready(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (!ui || !encore_vk_swapchain_last_frame_complete((size_t)ui->swapchain))
        return false;
    ui->vertex_count = ui->retained_vertex_count;
    ui->index_count = ui->retained_index_count;
    ui->batch_count = ui->retained_batch_count;
    return true;
}

void luma_vk_ui_collect_idle_textures(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    luma_vk_ui_collect_retired(ui, 0);
    luma_vk_ui_collect_retired(ui, 1);
}

static bool luma_vk_ui_ensure_gpu(LumaVkUiCompositor *ui, uint32_t slot,
    size_t vertex_bytes, size_t index_bytes) {
    if (slot >= 2) return false;
    if (ui->vertex_buffers[slot] == NULL ||
        encore_vk_buffer_size((size_t)ui->vertex_buffers[slot]) < vertex_bytes) {
        if (ui->vertex_buffers[slot] != NULL)
            encore_vk_buffer_destroy((size_t)ui->vertex_buffers[slot]);
        size_t capacity = 128 * 1024;
        while (capacity < vertex_bytes) capacity *= 2;
        ui->vertex_buffers[slot] = (EncoreVkBuffer *)encore_vk_buffer_create(
            (size_t)ui->device, capacity, 128u, true);
    }
    if (ui->index_buffers[slot] == NULL ||
        encore_vk_buffer_size((size_t)ui->index_buffers[slot]) < index_bytes) {
        if (ui->index_buffers[slot] != NULL)
            encore_vk_buffer_destroy((size_t)ui->index_buffers[slot]);
        size_t capacity = 64 * 1024;
        while (capacity < index_bytes) capacity *= 2;
        ui->index_buffers[slot] = (EncoreVkBuffer *)encore_vk_buffer_create(
            (size_t)ui->device, capacity, 64u, true);
    }
    return ui->vertex_buffers[slot] != NULL && encore_vk_buffer_available((size_t)ui->vertex_buffers[slot]) &&
        ui->index_buffers[slot] != NULL && encore_vk_buffer_available((size_t)ui->index_buffers[slot]);
}

size_t luma_vk_ui_create(size_t device_handle, size_t swapchain_handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)calloc(1, sizeof(LumaVkUiCompositor));
    if (ui == NULL) return 0;
    ui->device = (EncoreVkDevice *)device_handle;
    ui->swapchain = (EncoreVkSwapchain *)swapchain_handle;
    if (ui->device == NULL || !encore_vk_device_available((size_t)ui->device) || ui->swapchain == NULL ||
        !encore_vk_swapchain_available((size_t)ui->swapchain) || encore_vk_swapchain_device((size_t)ui->swapchain) != (size_t)ui->device) {
        luma_vk_ui_error(ui, "Invalid direct Vulkan device or swapchain");
        return (size_t)ui;
    }
    ui->vertex_shader = (EncoreVkShader *)encore_vk_shader_create(
        device_handle, luma_ui_vertex_spirv,
        sizeof(luma_ui_vertex_spirv) / sizeof(uint32_t),
        VK_SHADER_STAGE_VERTEX_BIT);
    ui->fragment_shader = (EncoreVkShader *)encore_vk_shader_create(
        device_handle, luma_ui_fragment_spirv,
        sizeof(luma_ui_fragment_spirv) / sizeof(uint32_t),
        VK_SHADER_STAGE_FRAGMENT_BIT);
    uint32_t bindings[1] = {0u}, types[1] = {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER};
    uint32_t stages[1] = {VK_SHADER_STAGE_FRAGMENT_BIT};
    ui->texture_layout = (EncoreVkBindGroupLayout *)
        encore_vk_bind_group_layout_create_entries(device_handle,
            bindings, 1, types, 1, stages, 1);
    ui->sampler = (EncoreVkSampler *)encore_vk_sampler_create(device_handle);
    uint32_t locations[3] = {0u, 1u, 2u};
    uint32_t formats[3] = {103u, 109u, 103u};
    uint32_t offsets[3] = {0u, 8u, 24u};
    ui->pipeline = (EncoreVkPipeline *)encore_vk_pipeline_create(
        device_handle, swapchain_handle, (size_t)ui->vertex_shader,
        (size_t)ui->fragment_shader, (size_t)ui->texture_layout,
        false, false, false, VK_CULL_MODE_NONE, true, false, false,
        sizeof(LumaVkUiVertex),
        locations, 3, formats, 3, offsets, 3);
    ui->load_pipeline = (EncoreVkPipeline *)encore_vk_pipeline_create(
        device_handle, swapchain_handle, (size_t)ui->vertex_shader,
        (size_t)ui->fragment_shader, (size_t)ui->texture_layout,
        false, false, false, VK_CULL_MODE_NONE, true, false, true,
        sizeof(LumaVkUiVertex),
        locations, 3, formats, 3, offsets, 3);
    if (ui->vertex_shader == NULL || !encore_vk_shader_available((size_t)ui->vertex_shader) ||
        ui->fragment_shader == NULL || !encore_vk_shader_available((size_t)ui->fragment_shader) ||
        ui->texture_layout == NULL || !encore_vk_bind_group_layout_available((size_t)ui->texture_layout) ||
        ui->sampler == NULL || !encore_vk_sampler_available((size_t)ui->sampler) ||
        ui->pipeline == NULL || !encore_vk_pipeline_available((size_t)ui->pipeline) ||
        ui->load_pipeline == NULL || !encore_vk_pipeline_available((size_t)ui->load_pipeline)) {
        luma_vk_ui_error(ui, ui->pipeline == NULL ? "Unable to allocate direct UI pipeline" :
            !encore_vk_pipeline_available((size_t)ui->pipeline) ? encore_vk_pipeline_error_cstr((size_t)ui->pipeline) :
            ui->load_pipeline == NULL ? "Unable to allocate direct UI load pipeline" :
            encore_vk_pipeline_error_cstr((size_t)ui->load_pipeline));
        return (size_t)ui;
    }
    /* Build render-pass framebuffers before any image is acquired. Waiting
       for queue idle after vkAcquireNextImage can deadlock some WSI drivers. */
    if (!encore_vk_swapchain_prepare((size_t)ui->swapchain, (size_t)ui->pipeline)) {
        luma_vk_ui_error(ui, encore_vk_swapchain_error_cstr((size_t)ui->swapchain));
        return (size_t)ui;
    }
    if (!encore_vk_swapchain_prepare((size_t)ui->swapchain, (size_t)ui->load_pipeline)) {
        luma_vk_ui_error(ui, encore_vk_swapchain_error_cstr((size_t)ui->swapchain));
        return (size_t)ui;
    }
    uint32_t white = 0xffffffffu;
    ui->white_texture = luma_vk_ui_texture_build(ui, &white, 1u, 1u, 4u, 37u);
    if (ui->white_texture == NULL) {
        luma_vk_ui_error(ui, "Unable to create direct UI white texture");
        return (size_t)ui;
    }
    ui->available = true;
    ui->acquire_status = ENCORE_VK_FRAME_READY;
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
    return encore_vk_swapchain_present_mode((size_t)ui->swapchain, requested);
}

const char *luma_vk_ui_error_cstr(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return ui ? ui->error : "Invalid direct UI compositor";
}

encore_str luma_vk_ui_error_string(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return (encore_str){(encore_str_object *)encore_str_from_cstr(ui == NULL ? "Invalid direct UI compositor" : ui->error)};
}

bool luma_vk_ui_begin(size_t handle, uint32_t clear_color) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available) return false;
    if (getenv("LUMA_TRACE_STARTUP"))
        fprintf(stderr, "LUMA_FRAME clear=%08x logical=%.0fx%.0f swapchain=%ux%u\n",
            clear_color, ui->logical_width, ui->logical_height,
            encore_vk_swapchain_width((size_t)ui->swapchain), encore_vk_swapchain_height((size_t)ui->swapchain));
    ++ui->geometry_revision;
    ui->vertex_count = 0;
    ui->index_count = 0;
    ui->batch_count = 0;
    ui->composition_split_batch = 0;
    ui->force_new_batch = false;
    ui->clip_x = 0;
    ui->clip_y = 0;
    ui->clip_width = (uint32_t)(ui->logical_width > 0.0f
        ? ui->logical_width : (float)encore_vk_swapchain_width((size_t)ui->swapchain));
    ui->clip_height = (uint32_t)(ui->logical_height > 0.0f
        ? ui->logical_height : (float)encore_vk_swapchain_height((size_t)ui->swapchain));
    ui->clear[0] = (float)((clear_color >> 24) & 255u) / 255.0f;
    ui->clear[1] = (float)((clear_color >> 16) & 255u) / 255.0f;
    ui->clear[2] = (float)((clear_color >> 8) & 255u) / 255.0f;
    ui->clear[3] = (float)(clear_color & 255u) / 255.0f;
    return true;
}

bool luma_vk_ui_cache_begin(size_t handle, uint32_t clear_color) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (!luma_vk_ui_begin(handle, clear_color)) return false;
    luma_vk_ui_patches_clear(ui);
    ui->patch_recording = false;
    ui->retained_ready = false;
    return true;
}

bool luma_vk_ui_cache_end(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available) return false;
    ++ui->geometry_revision;
    if (ui->patch_recording) {
        size_t vertex_count = ui->vertex_count - ui->patch_vertex_start;
        size_t index_count = ui->index_count - ui->patch_index_start;
        size_t batch_count = ui->batch_count - ui->patch_batch_start;
        LumaVkUiPatchSegment patch = {0};
        patch.key = ui->patch_key;
        patch.vertex_count = vertex_count;
        patch.index_count = index_count;
        patch.batch_count = batch_count;
        if (vertex_count > 0) {
            patch.vertices = (LumaVkUiVertex *)malloc(
                vertex_count * sizeof(LumaVkUiVertex));
            if (patch.vertices == NULL) goto patch_failed;
            memcpy(patch.vertices, ui->vertices + ui->patch_vertex_start,
                vertex_count * sizeof(LumaVkUiVertex));
        }
        if (index_count > 0) {
            patch.indices = (uint32_t *)malloc(
                index_count * sizeof(uint32_t));
            if (patch.indices == NULL) goto patch_failed;
            for (size_t i = 0; i < index_count; ++i)
                patch.indices[i] = ui->indices[ui->patch_index_start + i] -
                    (uint32_t)ui->patch_vertex_start;
        }
        if (batch_count > 0) {
            patch.batches = (LumaVkUiBatch *)malloc(
                batch_count * sizeof(LumaVkUiBatch));
            if (patch.batches == NULL) goto patch_failed;
            for (size_t i = 0; i < batch_count; ++i) {
                patch.batches[i] = ui->batches[ui->patch_batch_start + i];
                if (patch.batches[i].mesh == NULL)
                    patch.batches[i].first_index -= (uint32_t)ui->patch_index_start;
            }
        }
        {
            size_t destination = ui->patch_count;
            for (size_t i = 0; i < ui->patch_count; ++i) {
                if (ui->patches[i].key == patch.key) {
                    destination = i;
                    break;
                }
            }
            if (destination == ui->patch_count &&
                !luma_vk_ui_patch_reserve(ui, ui->patch_count + 1))
                goto patch_failed;
            if (destination < ui->patch_count) {
                luma_vk_ui_patch_release(&ui->patches[destination]);
                /* Damage snapshots overlap (for example a button and its
                 * hovered parent). A replacement is the newest commit, not
                 * the oldest slot: later stale snapshots must not cover it.
                 * Keep one entry per bounds while preserving commit order. */
                memmove(ui->patches + destination, ui->patches + destination + 1,
                    (ui->patch_count - destination - 1) * sizeof(*ui->patches));
                destination = ui->patch_count - 1;
            } else {
                ui->patch_count++;
            }
            ui->patches[destination] = patch;
        }
        ui->patch_recording = false;
        ui->segmented_commit_count++;
        if (!luma_vk_ui_flatten_patches(ui)) {
            ui->retained_ready = false;
            return false;
        }
        ui->retained_ready = true;
        return true;
patch_failed:
        luma_vk_ui_patch_release(&patch);
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

bool luma_vk_ui_cache_patch_begin(size_t handle, uint32_t clear_color,
    float x, float y, float width, float height) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available || !ui->retained_ready ||
        ui->patch_recording || width <= 0.0f || height <= 0.0f) return false;
    ui->vertex_count = ui->retained_vertex_count;
    ui->index_count = ui->retained_index_count;
    ui->batch_count = ui->retained_batch_count;
    ui->patch_vertex_start = ui->vertex_count;
    ui->patch_index_start = ui->index_count;
    ui->patch_batch_start = ui->batch_count;
    ui->patch_key = luma_vk_ui_patch_bounds_key(x, y, width, height);
    ui->patch_recording = true;
    /* A patch owns all its batches. Never append its first draw into the
     * last retained batch, which is outside the recorded patch range. */
    ui->force_new_batch = true;
    ui->clip_x = x < 0.0f ? 0 : luma_raster_floor(x);
    ui->clip_y = y < 0.0f ? 0 : luma_raster_floor(y);
    int32_t right = luma_raster_ceil(x + width);
    int32_t bottom = luma_raster_ceil(y + height);
    ui->clip_width = (uint32_t)luma_raster_extent(ui->clip_x, right);
    ui->clip_height = (uint32_t)luma_raster_extent(ui->clip_y, bottom);
    float r = (float)((clear_color >> 24) & 255u) / 255.0f;
    float g = (float)((clear_color >> 16) & 255u) / 255.0f;
    float b = (float)((clear_color >> 8) & 255u) / 255.0f;
    float a = (float)(clear_color & 255u) / 255.0f;
    float vertices[4][8] = {
        {x,y,r,g,b,a,0,0}, {x+width,y,r,g,b,a,1,0},
        {x+width,y+height,r,g,b,a,1,1}, {x,y+height,r,g,b,a,0,1}
    };
    int indices[6] = {0,1,2,0,2,3};
    if (!luma_vk_ui_geometry_bound(handle, &vertices[0][0], 4, 8, 2,
            indices, 6, ui->white_texture)) {
        ui->patch_recording = false;
        return false;
    }
    return true;
}

uint64_t luma_vk_ui_segmented_commit_count(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return ui == NULL ? 0 : ui->segmented_commit_count;
}

uint32_t luma_vk_ui_patch_count(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    return ui == NULL ? 0 : (uint32_t)ui->patch_count;
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
        ? ui->logical_width : (float)encore_vk_swapchain_width((size_t)ui->swapchain));
    ui->clip_height = (uint32_t)(ui->logical_height > 0.0f
        ? ui->logical_height : (float)encore_vk_swapchain_height((size_t)ui->swapchain));
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
            ? ui->logical_width : (float)encore_vk_swapchain_width((size_t)ui->swapchain));
        ui->clip_height = (uint32_t)(ui->logical_height > 0.0f
            ? ui->logical_height : (float)encore_vk_swapchain_height((size_t)ui->swapchain));
        return true;
    }
    if (x < 0) { width = width > (uint32_t)(-x) ? width - (uint32_t)(-x) : 0; x = 0; }
    if (y < 0) { height = height > (uint32_t)(-y) ? height - (uint32_t)(-y) : 0; y = 0; }
    uint32_t logical_width = (uint32_t)(ui->logical_width > 0.0f
        ? ui->logical_width : (float)encore_vk_swapchain_width((size_t)ui->swapchain));
    uint32_t logical_height = (uint32_t)(ui->logical_height > 0.0f
        ? ui->logical_height : (float)encore_vk_swapchain_height((size_t)ui->swapchain));
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
    for (size_t i = 0; i < vertex_count; ++i) {
        const float *source = vertices + i * vertex_stride_floats;
        LumaVkUiVertex *target = &ui->vertices[first_vertex + i];
        target->x = source[0];
        target->y = source[1];
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
    ++ui->geometry_revision;
    LumaVkUiBatch *batch = ui->batch_count == 0 ? NULL : &ui->batches[ui->batch_count - 1];
    if (!ui->force_new_batch && batch != NULL && batch->mesh == NULL &&
        batch->first_index + batch->index_count == first_index &&
        batch->clip_x == ui->clip_x && batch->clip_y == ui->clip_y &&
        batch->clip_width == ui->clip_width && batch->clip_height == ui->clip_height &&
        batch->texture == texture) {
        batch->index_count += (uint32_t)index_count;
    } else {
        ui->batches[ui->batch_count++] = (LumaVkUiBatch){
            .first_index = (uint32_t)first_index,
            .index_count = (uint32_t)index_count,
            .clip_x = ui->clip_x, .clip_y = ui->clip_y,
            .clip_width = ui->clip_width, .clip_height = ui->clip_height,
            .texture = texture};
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
    EncoreVkTexture *texture = (EncoreVkTexture *)encore_vk_texture_borrow(
        (size_t)ui->device,image,width,height,VK_FORMAT_B8G8R8A8_UNORM,layout);
    if (result == NULL || texture == NULL) {
        if (texture) encore_vk_texture_destroy((size_t)texture);
        free(result); return 0;
    }
    result->texture = texture;
    result->owner = ui;
    result->width = width;
    result->height = height;
    result->group = (EncoreVkBindGroup *)encore_vk_texture_bind_group_create(
        (size_t)ui->device, (size_t)ui->texture_layout,
        (size_t)texture, (size_t)ui->sampler);
    if (result->group == NULL || !encore_vk_bind_group_available((size_t)result->group)) {
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
    /* Existing retained batches own their immutable texture snapshot. Atlas
     * eviction prevents new draws from referencing this texture, but must not
     * invalidate the cached base: it may be replayed without a UI repaint.
     * collect_retired keeps the texture alive until both GPU slots complete
     * AND the last retained/current batch has been replaced. */
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
        texture->destroy_pending ||
        (texture->group == NULL && texture->atlas_pixels == NULL) ||
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
    /* Keep retained UI batches before the borrowed scene pass; transient
     * overlays recorded after this boundary are appended by frame_finish. */
    ui->composition_split_batch = ui->batch_count;
    ui->force_new_batch = true;
    return true;
}

#include "vulkan_mesh_cache.inc"

static bool luma_vk_ui_prepare_gpu_internal(LumaVkUiCompositor *ui,
    EncoreVkFrame *frame) {
    size_t vertex_bytes = ui->vertex_count * sizeof(LumaVkUiVertex);
    size_t index_bytes = ui->index_count * sizeof(uint32_t);
    for (size_t i = 0; i < ui->batch_count; ++i) {
        if (!luma_vk_ui_texture_flush(ui->batches[i].texture)) {
            luma_vk_ui_error(ui, "Unable to upload a direct Vulkan atlas page");
            return false;
        }
    }
    uint32_t slot = encore_vk_frame_slot((size_t)frame);
    if (slot >= 2) return false;
    luma_vk_ui_collect_retired(ui, slot);
    for (LumaVkUiMesh *mesh = ui->meshes; mesh; mesh = mesh->next)
        mesh->pending_slots &= ~(1u << slot);
    luma_vk_ui_mesh_trim(ui, 0);
    for (size_t i = 0; i < ui->batch_count; ++i) {
        LumaVkUiMesh *mesh = ui->batches[i].mesh;
        if (mesh != NULL && !luma_vk_ui_mesh_upload(ui, mesh)) return false;
    }
    if (ui->index_count > 0 && !luma_vk_ui_geometry_resident(ui, slot) &&
        (!luma_vk_ui_ensure_gpu(ui, slot, vertex_bytes, index_bytes) ||
         !luma_vk_ui_upload(ui->vertex_buffers[slot], ui->vertices, vertex_bytes) ||
         !luma_vk_ui_upload(ui->index_buffers[slot], ui->indices, index_bytes))) {
        luma_vk_ui_error(ui, "Unable to upload direct UI geometry");
        return false;
    }
    if (ui->index_count > 0 && !luma_vk_ui_geometry_resident(ui, slot)) {
        ui->geometry_upload_bytes += vertex_bytes + index_bytes;
        ui->uploaded_revision[slot] = ui->geometry_revision;
        ui->uploaded_vertices[slot] = ui->vertex_count;
        ui->uploaded_indices[slot] = ui->index_count;
    }
    return true;
}

static bool luma_vk_ui_prepare_gpu(LumaVkUiCompositor *ui, EncoreVkFrame *frame) {
    uint64_t started = luma_vk_ui_clock_ns();
    bool result = luma_vk_ui_prepare_gpu_internal(ui, frame);
    ui->gpu_preparation_ns += luma_vk_ui_clock_ns() - started;
    return result;
}

uint64_t luma_vk_ui_timing_value(size_t handle, uint32_t field) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (!ui) return 0;
    switch (field) { case 0:return ui->acquire_ns; case 1:return ui->present_ns;
        case 2:return ui->gpu_preparation_ns; case 3:return ui->geometry_upload_bytes; default:return 0; }
}

/* Async content can finish below the viewport. An empty intersection is a
 * successful no-op, not a failed Vulkan scissor/frame submission. */
static bool luma_vk_ui_visible_scissor(int32_t *x, int32_t *y,
    uint32_t right, uint32_t bottom) {
    if (*x < 0) *x = 0;
    if (*y < 0) *y = 0;
    return right > (uint32_t)*x && bottom > (uint32_t)*y;
}

static bool luma_vk_ui_draw_range(LumaVkUiCompositor *ui,
    EncoreVkFrame *frame, size_t first, size_t end) {
    if (first > ui->batch_count) first = ui->batch_count;
    if (end > ui->batch_count) end = ui->batch_count;
    if (end <= first) return true;
    uint32_t slot = encore_vk_frame_slot((size_t)frame);
    bool rendered = true;
    if (ui->batch_count > 0) {
        rendered = rendered && encore_vk_frame_set_viewport((size_t)frame,0,0,
            encore_vk_swapchain_width((size_t)ui->swapchain),encore_vk_swapchain_height((size_t)ui->swapchain));
        for (size_t i = first; i < end && rendered; ++i) {
            LumaVkUiBatch *batch = &ui->batches[i];
            if (batch->texture == NULL) {
                luma_vk_ui_error(ui, "Retained UI batch has no texture");
                rendered = false;
                break;
            }
            luma_vk_ui_texture_collect_upload(batch->texture);
            float logical_width = ui->logical_width > 0.0f
                ? ui->logical_width : (float)encore_vk_swapchain_width((size_t)ui->swapchain);
            float logical_height = ui->logical_height > 0.0f
                ? ui->logical_height : (float)encore_vk_swapchain_height((size_t)ui->swapchain);
            float scale_x = (float)encore_vk_swapchain_width((size_t)ui->swapchain) / logical_width;
            float scale_y = (float)encore_vk_swapchain_height((size_t)ui->swapchain) / logical_height;
            int32_t scissor_x = (int32_t)((float)batch->clip_x * scale_x);
            int32_t scissor_y = (int32_t)((float)batch->clip_y * scale_y);
            uint32_t scissor_right = (uint32_t)(
                ((float)batch->clip_x + (float)batch->clip_width) * scale_x + 0.9999f);
            uint32_t scissor_bottom = (uint32_t)(
                ((float)batch->clip_y + (float)batch->clip_height) * scale_y + 0.9999f);
            if (scissor_right > encore_vk_swapchain_width((size_t)ui->swapchain)) scissor_right = encore_vk_swapchain_width((size_t)ui->swapchain);
            if (scissor_bottom > encore_vk_swapchain_height((size_t)ui->swapchain)) scissor_bottom = encore_vk_swapchain_height((size_t)ui->swapchain);
            if (!luma_vk_ui_visible_scissor(&scissor_x, &scissor_y,
                    scissor_right, scissor_bottom)) continue;
            LumaVkUiMesh *mesh = batch->mesh;
            float constants[12] = {
                2.0f / logical_width, 2.0f / logical_height,
                mesh ? batch->x : 0.0f, mesh ? batch->y : 0.0f,
                1, 1, 1, 1, mesh ? scale_x : 0.0f, mesh ? scale_y : 0.0f, 0, 0};
            if (mesh) {
                constants[4] = (float)((batch->color >> 24) & 255u) / 255.0f;
                constants[5] = (float)((batch->color >> 16) & 255u) / 255.0f;
                constants[6] = (float)((batch->color >> 8) & 255u) / 255.0f;
                constants[7] = (float)(batch->color & 255u) / 255.0f;
                mesh->pending_slots |= 1u << slot;
            }
            rendered = rendered && encore_vk_frame_constants((size_t)frame, constants, 12) &&
                encore_vk_frame_bind_vertex_buffer((size_t)frame,
                    (size_t)(mesh ? mesh->vertex_buffer : ui->vertex_buffers[slot])) &&
                encore_vk_frame_bind_index_buffer((size_t)frame,
                    (size_t)(mesh ? mesh->index_buffer : ui->index_buffers[slot]));
            rendered = rendered && encore_vk_frame_scissor((size_t)frame,scissor_x,scissor_y,
                scissor_right > (uint32_t)scissor_x ? scissor_right-(uint32_t)scissor_x : 0,
                scissor_bottom > (uint32_t)scissor_y ? scissor_bottom-(uint32_t)scissor_y : 0);
            rendered = rendered && encore_vk_frame_bind_group((size_t)frame,
                (size_t)batch->texture->group) &&
                encore_vk_frame_draw_indexed((size_t)frame,
                batch->index_count, 1u,
                mesh && mesh->arena_bytes ? (uint32_t)(mesh->index_offset / sizeof(uint32_t)) : batch->first_index,
                mesh && mesh->arena_bytes ? (int32_t)(mesh->arena_offset / sizeof(LumaVkUiVertex)) : 0, 0u);
            if (!rendered)
                luma_vk_ui_error(ui, encore_vk_frame_error_cstr((size_t)frame));
        }
    }
    return rendered;
}

size_t luma_vk_ui_frame_prepare(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL || !ui->available) return 0;
    EncoreVkFrame *frame = luma_vk_ui_acquire_timed(ui);
    if (frame == NULL || !encore_vk_frame_available((size_t)frame)) {
        if (ui->acquire_status == ENCORE_VK_FRAME_FAILED) luma_vk_ui_error(ui,
            frame == NULL ? "Unable to allocate direct UI frame" : encore_vk_frame_error_cstr((size_t)frame));
        if (frame != NULL) encore_vk_frame_destroy((size_t)frame);
        return 0;
    }
    if (!luma_vk_ui_prepare_gpu(ui, frame)) {
        luma_vk_ui_error(ui, encore_vk_frame_error_cstr((size_t)frame));
        encore_vk_frame_destroy((size_t)frame);
        return 0;
    }
    return (size_t)frame;
}

bool luma_vk_ui_frame_begin_composition(size_t handle, size_t frame_handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    EncoreVkFrame *frame = (EncoreVkFrame *)frame_handle;
    if (ui == NULL || !ui->available || frame == NULL ||
        !encore_vk_frame_available((size_t)frame) || encore_vk_frame_in_pass((size_t)frame) ||
        encore_vk_frame_swapchain((size_t)frame) != (size_t)ui->swapchain) return false;
    return encore_vk_frame_begin_render_pass((size_t)frame,
            (size_t)ui->pipeline,
            ui->clear[0], ui->clear[1], ui->clear[2], ui->clear[3]) &&
        luma_vk_ui_draw_range(ui, frame, 0, ui->composition_split_batch);
}

size_t luma_vk_ui_frame_acquire(size_t handle) {
    size_t frame = luma_vk_ui_frame_prepare(handle);
    if (frame == 0) return 0;
    if (!luma_vk_ui_frame_begin_composition(handle, frame)) {
        luma_vk_ui_error((LumaVkUiCompositor *)handle, "Unable to begin direct UI composition");
        encore_vk_frame_destroy(frame);
        return 0;
    }
    return frame;
}

static bool luma_vk_ui_finish_frame(LumaVkUiCompositor *ui,
    EncoreVkFrame *frame, bool composed) {
    if (ui == NULL || !ui->available || frame == NULL ||
        !encore_vk_frame_available((size_t)frame) ||
        encore_vk_frame_swapchain((size_t)frame) != (size_t)ui->swapchain) {
        if (ui != NULL)
            luma_vk_ui_error(ui, "Invalid borrowed direct Vulkan frame");
        return false;
    }
    bool rendered = true;
    if (composed) {
        rendered = encore_vk_frame_in_pass((size_t)frame) &&
            encore_vk_frame_bind_pipeline(
                (size_t)frame, (size_t)ui->load_pipeline) &&
            luma_vk_ui_draw_range(ui, frame,
                ui->composition_split_batch, ui->batch_count);
    } else {
        rendered = !encore_vk_frame_in_pass((size_t)frame) &&
            luma_vk_ui_prepare_gpu(ui, frame) &&
            encore_vk_frame_begin_render_pass((size_t)frame,
                (size_t)ui->pipeline,
                ui->clear[0], ui->clear[1], ui->clear[2], ui->clear[3]) &&
            luma_vk_ui_draw_range(ui, frame, 0, ui->batch_count);
    }
    rendered = encore_vk_frame_end_render_pass((size_t)frame) && rendered;
    encore_vk_frame_timing_mark((size_t)frame, 8, false);
    if (ui->capture_requested && rendered) {
        uint32_t width = encore_vk_swapchain_width((size_t)ui->swapchain);
        uint32_t height = encore_vk_swapchain_height((size_t)ui->swapchain);
        uint64_t bytes = (uint64_t)width * height * 4;
        if (bytes && bytes <= 256u * 1024u * 1024u) {
            if (ui->capture_buffer && encore_vk_buffer_size((size_t)ui->capture_buffer) != bytes) {
                encore_vk_swapchain_wait((size_t)ui->swapchain);
                encore_vk_buffer_destroy((size_t)ui->capture_buffer); ui->capture_buffer = NULL;
            }
            if (!ui->capture_buffer) ui->capture_buffer = (EncoreVkBuffer *)encore_vk_buffer_create((size_t)ui->device,bytes,2u,true);
            ui->capture_format = encore_vk_frame_copy_color((size_t)frame,(size_t)ui->capture_buffer);
            ui->capture_width = width; ui->capture_height = height;
            ui->capture_ready = ui->capture_format != 0;
        }
        ui->capture_requested = false;
    }
    uint64_t present_started = luma_vk_ui_clock_ns();
    rendered = encore_vk_frame_present((size_t)frame) && rendered;
    uint64_t elapsed = luma_vk_ui_clock_ns() - present_started;
    ui->presentation_ns += elapsed; ui->present_ns += elapsed;
    if (!rendered) luma_vk_ui_error(ui, encore_vk_frame_error_cstr((size_t)frame));
    encore_vk_frame_destroy((size_t)frame);
    return rendered;
}

bool luma_vk_ui_frame_finish(size_t handle, size_t frame_handle) {
    return luma_vk_ui_finish_frame((LumaVkUiCompositor *)handle,
        (EncoreVkFrame *)frame_handle, true);
}

bool luma_vk_ui_end(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    EncoreVkFrame *frame = luma_vk_ui_acquire_timed(ui);
    if (frame == NULL || !encore_vk_frame_available((size_t)frame)) {
        if (ui != NULL && ui->acquire_status == ENCORE_VK_FRAME_FAILED)
            luma_vk_ui_error(ui, frame == NULL
                ? "Unable to allocate direct UI frame" : encore_vk_frame_error_cstr((size_t)frame));
        if (frame != NULL) encore_vk_frame_destroy((size_t)frame);
        return false;
    }
    return luma_vk_ui_finish_frame(ui, frame, false);
}

bool luma_vk_ui_destroy(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (ui == NULL) return false;
    if (ui->swapchain != NULL &&
        !encore_vk_swapchain_wait((size_t)ui->swapchain)) return false;
    if (ui->capture_buffer) encore_vk_buffer_destroy((size_t)ui->capture_buffer);
    const char *profile = getenv("LUMA_PROFILE");
    if (profile && strcmp(profile, "1") == 0)
        fprintf(stderr, "LUMA_MESH builds=%llu hits=%llu uploaded_bytes=%llu cached_bytes=%zu\n",
            (unsigned long long)ui->mesh_builds, (unsigned long long)ui->mesh_hits,
            (unsigned long long)ui->mesh_upload_bytes, ui->mesh_bytes);
    for (uint32_t slot = 0; slot < 2; ++slot) {
        if (ui->vertex_buffers[slot] != NULL)
            encore_vk_buffer_destroy((size_t)ui->vertex_buffers[slot]);
        if (ui->index_buffers[slot] != NULL)
            encore_vk_buffer_destroy((size_t)ui->index_buffers[slot]);
    }
    while (ui->meshes) {
        LumaVkUiMesh *mesh = ui->meshes;
        ui->meshes = mesh->next;
        luma_vk_ui_mesh_release(mesh);
    }
    if (ui->mesh_mapping) encore_vk_buffer_unmap((size_t)ui->mesh_arena);
    if (ui->mesh_arena) encore_vk_buffer_destroy((size_t)ui->mesh_arena);
    while (ui->mesh_free) {
        LumaVkUiMeshBlock *block = ui->mesh_free;
        ui->mesh_free = block->next;
        free(block);
    }
    if (ui->pipeline != NULL) encore_vk_pipeline_destroy((size_t)ui->pipeline);
    if (ui->load_pipeline != NULL)
        encore_vk_pipeline_destroy((size_t)ui->load_pipeline);
    for (size_t i = 0; i < ui->retired_texture_count; ++i)
        luma_vk_ui_texture_release(ui->retired_textures[i]);
    luma_vk_ui_texture_release(ui->white_texture);
    if (ui->sampler != NULL) encore_vk_sampler_destroy((size_t)ui->sampler);
    if (ui->texture_layout != NULL)
        encore_vk_bind_group_layout_destroy((size_t)ui->texture_layout);
    if (ui->vertex_shader != NULL) encore_vk_shader_destroy((size_t)ui->vertex_shader);
    if (ui->fragment_shader != NULL) encore_vk_shader_destroy((size_t)ui->fragment_shader);
    free(ui->vertices);
    free(ui->indices);
    free(ui->batches);
    luma_vk_ui_patches_clear(ui);
    free(ui->patches);
    free(ui->retired_textures);
    free(ui);
    return true;
}
