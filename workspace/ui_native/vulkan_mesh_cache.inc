/* Immutable local-coordinate vector meshes. Retained batches own references
 * implicitly; in-flight slots pin GPU storage across swapchain rebuilds. */
#define LUMA_VECTOR_CACHE_BYTES (64u * 1024u * 1024u)

static void luma_vk_ui_mesh_free_range(LumaVkUiCompositor *ui, size_t offset, size_t bytes) {
    LumaVkUiMeshBlock **link = &ui->mesh_free;
    while (*link && (*link)->offset < offset) link = &(*link)->next;
    LumaVkUiMeshBlock *block = malloc(sizeof(*block));
    if (!block) return; /* Lose capacity on OOM, never reuse a live range. */
    *block = (LumaVkUiMeshBlock){offset, bytes, *link}; *link = block;
    for (block = ui->mesh_free; block && block->next;) {
        if (block->offset + block->bytes == block->next->offset) {
            LumaVkUiMeshBlock *next = block->next;
            block->bytes += next->bytes; block->next = next->next; free(next);
        } else block = block->next;
    }
}

static bool luma_vk_ui_mesh_allocate(LumaVkUiCompositor *ui, size_t bytes, size_t *offset) {
    for (LumaVkUiMeshBlock **link = &ui->mesh_free; *link; link = &(*link)->next) {
        LumaVkUiMeshBlock *block = *link;
        if (block->bytes < bytes) continue;
        *offset = block->offset; block->offset += bytes; block->bytes -= bytes;
        if (!block->bytes) { *link = block->next; free(block); }
        return true;
    }
    return false;
}

static void luma_vk_ui_mesh_release(LumaVkUiMesh *mesh) {
    if (!mesh) return;
    if (mesh->arena_bytes) {
        luma_vk_ui_mesh_free_range(mesh->owner, mesh->arena_offset, mesh->arena_bytes);
    } else {
        if (mesh->vertex_buffer) encore_vk_buffer_destroy((size_t)mesh->vertex_buffer);
        if (mesh->index_buffer) encore_vk_buffer_destroy((size_t)mesh->index_buffer);
    }
    free(mesh->key); free(mesh->vertices); free(mesh->indices); free(mesh);
}

static bool luma_vk_ui_mesh_referenced(LumaVkUiCompositor *ui, LumaVkUiMesh *mesh) {
    if (ui->capturing_mesh == mesh || mesh->pending_slots) return true;
    for (size_t i = 0; i < ui->batch_count; ++i)
        if (ui->batches[i].mesh == mesh) return true;
    for (size_t i = 0; i < ui->patch_count; ++i)
        for (size_t j = 0; j < ui->patches[i].batch_count; ++j)
            if (ui->patches[i].batches[j].mesh == mesh) return true;
    return false;
}

static void luma_vk_ui_mesh_trim(LumaVkUiCompositor *ui, size_t incoming) {
    for (;;) {
        LumaVkUiMesh **candidate = NULL;
        for (LumaVkUiMesh **link = &ui->meshes; *link; link = &(*link)->next) {
            LumaVkUiMesh *mesh = *link;
            if ((!mesh->transient && ui->mesh_bytes + incoming <= LUMA_VECTOR_CACHE_BYTES) ||
                luma_vk_ui_mesh_referenced(ui, mesh)) continue;
            if (!candidate || mesh->used < (*candidate)->used) candidate = link;
        }
        if (!candidate) return;
        LumaVkUiMesh *mesh = *candidate;
        *candidate = mesh->next;
        if (!mesh->transient) ui->mesh_bytes -= mesh->bytes;
        luma_vk_ui_mesh_release(mesh);
    }
}

static bool luma_vk_ui_mesh_draw(LumaVkUiCompositor *ui, LumaVkUiMesh *mesh,
    float x, float y, uint32_t color) {
    if (mesh->index_count == 0) return true;
    if (!luma_vk_ui_reserve_batches(ui, ui->batch_count + 1)) return false;
    ui->batches[ui->batch_count++] = (LumaVkUiBatch){
        0, (uint32_t)mesh->index_count, ui->clip_x, ui->clip_y,
        ui->clip_width, ui->clip_height, ui->white_texture, mesh,
        x, y, mesh->density, color};
    mesh->used = ++ui->mesh_clock;
    ui->force_new_batch = true;
    return true;
}

/* 0 unavailable, 1 replayed, 2 capture local white geometry then call end. */
uint32_t luma_vk_ui_mesh_begin(size_t handle, encore_str key,
    float scale, float stroke, float density, float x, float y, uint32_t color) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    if (!ui || !ui->available || ui->capturing_mesh || !key.object ||
        !key.object->len || scale <= 0 || stroke <= 0 || density <= 0) return 0;
    uint64_t hash = 1469598103934665603ull;
    for (size_t i = 0; i < key.object->len; ++i) {
        hash ^= (unsigned char)key.object->data[i]; hash *= 1099511628211ull;
    }
    for (LumaVkUiMesh *mesh = ui->meshes; mesh; mesh = mesh->next) {
        if (!mesh->transient && mesh->hash == hash && mesh->scale == scale &&
            mesh->stroke == stroke && mesh->density == density &&
            mesh->key_length == key.object->len &&
            memcmp(mesh->key, key.object->data, mesh->key_length) == 0) {
            if (!luma_vk_ui_mesh_draw(ui, mesh, x, y, color)) return 0;
            ++ui->mesh_hits;
            return 1;
        }
    }
    LumaVkUiMesh *mesh = calloc(1, sizeof(*mesh));
    if (!mesh) return 0;
    mesh->owner = ui;
    mesh->key = malloc(key.object->len);
    if (!mesh->key) { free(mesh); return 0; }
    memcpy(mesh->key, key.object->data, key.object->len);
    mesh->key_length = key.object->len; mesh->hash = hash;
    mesh->scale = scale; mesh->stroke = stroke; mesh->density = density;
    ui->capturing_mesh = mesh;
    ui->mesh_vertex_start = ui->vertex_count;
    ui->mesh_index_start = ui->index_count;
    ui->mesh_batch_start = ui->batch_count;
    ui->mesh_x = x; ui->mesh_y = y; ui->mesh_color = color;
    ui->force_new_batch = true;
    return 2;
}

bool luma_vk_ui_mesh_end(size_t handle) {
    LumaVkUiCompositor *ui = (LumaVkUiCompositor *)handle;
    LumaVkUiMesh *mesh = ui ? ui->capturing_mesh : NULL;
    if (!mesh) return false;
    mesh->vertex_count = ui->vertex_count - ui->mesh_vertex_start;
    mesh->index_count = ui->index_count - ui->mesh_index_start;
    size_t vertex_bytes = mesh->vertex_count * sizeof(LumaVkUiVertex);
    size_t index_bytes = mesh->index_count * sizeof(uint32_t);
    mesh->vertices = malloc(vertex_bytes ? vertex_bytes : 1);
    mesh->indices = malloc(index_bytes ? index_bytes : 1);
    bool valid = mesh->vertices && mesh->indices;
    if (valid) {
        if (vertex_bytes) memcpy(mesh->vertices, ui->vertices + ui->mesh_vertex_start, vertex_bytes);
        for (size_t i = 0; i < mesh->index_count; ++i)
            mesh->indices[i] = ui->indices[ui->mesh_index_start + i] - (uint32_t)ui->mesh_vertex_start;
    }
    ui->vertex_count = ui->mesh_vertex_start;
    ui->index_count = ui->mesh_index_start;
    ui->batch_count = ui->mesh_batch_start;
    ui->capturing_mesh = NULL;
    ui->force_new_batch = true;
    if (!valid) { luma_vk_ui_mesh_release(mesh); return false; }
    mesh->bytes = vertex_bytes + index_bytes + mesh->key_length + sizeof(*mesh);
    luma_vk_ui_mesh_trim(ui, mesh->bytes);
    mesh->transient = mesh->bytes > LUMA_VECTOR_CACHE_BYTES ||
        ui->mesh_bytes + mesh->bytes > LUMA_VECTOR_CACHE_BYTES;
    if (!mesh->transient) ui->mesh_bytes += mesh->bytes;
    mesh->next = ui->meshes; ui->meshes = mesh;
    ++ui->mesh_builds;
    return luma_vk_ui_mesh_draw(ui, mesh, ui->mesh_x, ui->mesh_y, ui->mesh_color);
}

static bool luma_vk_ui_mesh_upload(LumaVkUiCompositor *ui, LumaVkUiMesh *mesh) {
    if (mesh->vertex_buffer && mesh->index_buffer) return true;
    if (!mesh->index_count) return true;
    size_t vertices = mesh->vertex_count * sizeof(LumaVkUiVertex);
    size_t indices = mesh->index_count * sizeof(uint32_t);
    size_t bytes = (vertices + indices + sizeof(LumaVkUiVertex) - 1) & ~(sizeof(LumaVkUiVertex) - 1);
    if (!ui->mesh_arena) {
        ui->mesh_arena = (EncoreVkBuffer *)encore_vk_buffer_create(
            (size_t)ui->device, LUMA_VECTOR_CACHE_BYTES, 128u | 64u, true);
        if (encore_vk_buffer_available((size_t)ui->mesh_arena))
            ui->mesh_mapping = encore_vk_buffer_map((size_t)ui->mesh_arena, LUMA_VECTOR_CACHE_BYTES);
        if (!ui->mesh_mapping) {
            luma_vk_ui_error(ui, "Unable to allocate the vector mesh arena");
            return false;
        }
        luma_vk_ui_mesh_free_range(ui, 0, LUMA_VECTOR_CACHE_BYTES);
    }
    size_t offset = 0;
    if (luma_vk_ui_mesh_allocate(ui, bytes, &offset)) {
        memcpy(ui->mesh_mapping + offset, mesh->vertices, vertices);
        memcpy(ui->mesh_mapping + offset + vertices, mesh->indices, indices);
        mesh->arena_offset = offset; mesh->arena_bytes = bytes;
        mesh->index_offset = offset + vertices;
        mesh->vertex_buffer = mesh->index_buffer = ui->mesh_arena;
        free(mesh->vertices); free(mesh->indices);
        mesh->vertices = NULL; mesh->indices = NULL;
        ui->geometry_upload_bytes += vertices + indices;
        ui->mesh_upload_bytes += vertices + indices;
        return true;
    }
    /* A single oversized mesh or ranges pinned by in-flight frames may need
       transient storage. It is never admitted to the persistent cache. */
    if (!mesh->transient) { ui->mesh_bytes -= mesh->bytes; mesh->transient = true; }
    EncoreVkBuffer *vb = (EncoreVkBuffer *)encore_vk_buffer_create((size_t)ui->device, vertices, 128u, true);
    EncoreVkBuffer *ib = (EncoreVkBuffer *)encore_vk_buffer_create((size_t)ui->device, indices, 64u, true);
    if (!encore_vk_buffer_available((size_t)vb) || !encore_vk_buffer_available((size_t)ib) ||
        !luma_vk_ui_upload(vb, mesh->vertices, vertices) || !luma_vk_ui_upload(ib, mesh->indices, indices)) {
        if (vb) encore_vk_buffer_destroy((size_t)vb);
        if (ib) encore_vk_buffer_destroy((size_t)ib);
        luma_vk_ui_error(ui, "Unable to upload retained vector mesh");
        return false;
    }
    mesh->vertex_buffer = vb; mesh->index_buffer = ib;
    free(mesh->vertices); free(mesh->indices);
    mesh->vertices = NULL; mesh->indices = NULL;
    ui->geometry_upload_bytes += vertices + indices;
    ui->mesh_upload_bytes += vertices + indices;
    return true;
}
