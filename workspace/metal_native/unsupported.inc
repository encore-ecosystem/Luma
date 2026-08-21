size_t graphene_metal_ray_query_bind_group_create(size_t device, size_t layout,
    uint32_t acceleration_binding, size_t acceleration,
    const uint32_t *bindings, size_t binding_count,
    const size_t *buffers, size_t buffer_count,
    const uint64_t *offsets, size_t offset_count,
    const uint64_t *ranges, size_t range_count) {
    (void)device; (void)layout; (void)acceleration_binding; (void)acceleration;
    (void)bindings; (void)binding_count; (void)buffers; (void)buffer_count;
    (void)offsets; (void)offset_count; (void)ranges; (void)range_count;
    return 0;
}

size_t graphene_metal_blas_create_triangles(size_t device, size_t vertices,
    uint64_t vertex_stride, uint32_t vertex_count, size_t indices,
    uint32_t index_count) {
    (void)device; (void)vertices; (void)vertex_stride; (void)vertex_count;
    (void)indices; (void)index_count;
    return 0;
}

size_t graphene_metal_tlas_create_instances(size_t device, const size_t *blas,
    size_t blas_count, const float *transforms, size_t transform_count) {
    (void)device; (void)blas; (void)blas_count; (void)transforms;
    (void)transform_count;
    return 0;
}

bool graphene_metal_acceleration_structure_available(size_t handle) {
    (void)handle;
    return false;
}

encore_str graphene_metal_acceleration_structure_error(size_t handle) {
    (void)handle;
    return metal_string("Metal acceleration structures are not implemented");
}

bool graphene_metal_acceleration_structure_destroy(size_t handle) {
    (void)handle;
    return false;
}

bool graphene_metal_frame_draw_indexed_indirect_count(size_t handle,
    size_t commands, uint64_t commands_offset, size_t count,
    uint64_t count_offset, uint32_t maximum_draw_count, uint32_t stride) {
    (void)handle; (void)commands; (void)commands_offset; (void)count;
    (void)count_offset; (void)maximum_draw_count; (void)stride;
    return false;
}

bool graphene_metal_frame_draw_mesh_tasks(size_t handle, uint32_t group_count_x,
    uint32_t group_count_y, uint32_t group_count_z) {
    (void)handle; (void)group_count_x; (void)group_count_y; (void)group_count_z;
    return false;
}

bool graphene_metal_frame_draw_mesh_tasks_indirect(size_t handle,
    size_t commands, uint64_t offset) {
    (void)handle; (void)commands; (void)offset;
    return false;
}

bool graphene_metal_frame_upload_ui(size_t handle, size_t pixels,
    uint32_t width, uint32_t height, uint32_t pitch, uint32_t hole_x,
    uint32_t hole_y, uint32_t hole_width, uint32_t hole_height) {
    (void)handle; (void)pixels; (void)width; (void)height; (void)pitch;
    (void)hole_x; (void)hole_y; (void)hole_width; (void)hole_height;
    return false;
}
