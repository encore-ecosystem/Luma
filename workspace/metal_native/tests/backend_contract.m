#include "../backend.m"

void *encore_str_from_cstr(const char *value) {
    const char *source = value == NULL ? "" : value;
    size_t length = strlen(source);
    encore_str_object *object = malloc(sizeof(encore_str_object) + length + 1);
    if (object == NULL) return NULL;
    object->ref_count = 1;
    object->len = length;
    memcpy(object->data, source, length + 1);
    return object;
}

#define REQUIRE(condition, code) do { \
    if (!(condition)) { \
        fprintf(stderr, "Metal backend contract failed at line %d (code %d)\n", \
            __LINE__, (code)); \
        return (code); \
    } \
} while (0)

static GrapheneMetalShader *contract_shader(GrapheneMetalDevice *device,
    const char *source, const char *name, uint32_t stage, uint32_t threadgroup_x) {
    NSError *error = nil;
    id<MTLLibrary> library = [device->device
        newLibraryWithSource:[NSString stringWithUTF8String:source]
        options:nil error:&error];
    if (library == nil) {
        fprintf(stderr, "Metal contract shader failed: %s\n",
            error == nil ? "unknown error" : error.localizedDescription.UTF8String);
        return NULL;
    }
    GrapheneMetalShader *shader = calloc(1, sizeof(GrapheneMetalShader));
    if (shader == NULL) {
        [library release];
        return NULL;
    }
    shader->owner = device;
    shader->library = library;
    shader->function = [library newFunctionWithName:[NSString stringWithUTF8String:name]];
    shader->stage = stage;
    shader->threadgroup_x = threadgroup_x;
    shader->available = shader->function != nil;
    return shader;
}

static bool contract_ui_rect(LumaMetalUiCompositor *ui, float x, float y,
    float width, float height, float red, float green, float blue) {
    float vertices[4][8] = {
        {x,y,red,green,blue,1,0,0},
        {x+width,y,red,green,blue,1,1,0},
        {x+width,y+height,red,green,blue,1,1,1},
        {x,y+height,red,green,blue,1,0,1}
    };
    int indices[6] = {0,1,2,0,2,3};
    return luma_metal_ui_geometry((size_t)ui, &vertices[0][0], 4, 8, 2,
        indices, 6);
}

static int run_contract(void) {
    size_t instance = graphene_metal_create(true, true, false);
    REQUIRE(graphene_metal_available(instance), 1);
    REQUIRE(graphene_metal_adapter_count(instance) == 1, 2);
    REQUIRE(graphene_metal_adapter_kind(instance, 0) == 1, 3);

    size_t device = graphene_metal_device_create_features(instance, 0, 0, 0);
    REQUIRE(graphene_metal_device_available(device), 4);
    REQUIRE(graphene_metal_device_graphics(device), 5);
    REQUIRE(graphene_metal_device_compute(device), 6);
    REQUIRE(graphene_metal_device_enabled_features(device) == 0, 7);

    size_t rejected = graphene_metal_device_create_features(instance, 0, 1, 0);
    REQUIRE(!graphene_metal_device_available(rejected), 8);
    REQUIRE(graphene_metal_device_destroy(rejected), 9);

    size_t source = graphene_metal_buffer_create(device, 64, 1, true);
    size_t destination = graphene_metal_buffer_create(device, 64, 2, true);
    size_t private_buffer = graphene_metal_buffer_create(device, 64, 4, false);
    REQUIRE(graphene_metal_buffer_available(source), 10);
    REQUIRE(graphene_metal_buffer_available(destination), 11);
    REQUIRE(graphene_metal_buffer_available(private_buffer), 12);
    REQUIRE(graphene_metal_buffer_host_visible(source), 13);
    REQUIRE(!graphene_metal_buffer_host_visible(private_buffer), 14);
    REQUIRE(graphene_metal_buffer_write_u32(source, 0, 0x11223344u), 15);

    uint32_t missing_spirv[] = {0x07230203u};
    size_t missing_shader = graphene_metal_shader_create(device, missing_spirv, 1,
        GRAPHENE_METAL_STAGE_COMPUTE);
    REQUIRE(!graphene_metal_shader_available(missing_shader), 65);
    REQUIRE(graphene_metal_shader_error(missing_shader).object != NULL, 66);
    REQUIRE(graphene_metal_shader_destroy(missing_shader), 67);

    size_t copy = graphene_metal_encoder_create(device);
    REQUIRE(graphene_metal_encoder_available(copy), 16);
    REQUIRE(graphene_metal_encoder_copy_buffer(copy, source, destination, 0, 4, 4), 17);
    REQUIRE(!graphene_metal_encoder_copy_buffer(copy, destination, source, 0, 0, 4), 18);
    REQUIRE(graphene_metal_buffer_write_u32(source, 0, 0xaabbccddu), 19);
    REQUIRE(graphene_metal_encoder_submit_wait(copy), 20);
    REQUIRE(graphene_metal_encoder_destroy(copy), 21);
    REQUIRE(graphene_metal_buffer_read_u32(destination, 4) == 0xaabbccddu, 22);

    size_t abandoned = graphene_metal_encoder_create(device);
    REQUIRE(graphene_metal_encoder_copy_buffer(abandoned, source, destination, 0, 8, 4), 23);
    REQUIRE(graphene_metal_encoder_destroy(abandoned), 24);
    REQUIRE(graphene_metal_buffer_write_u32(source, 0, 0x55667788u), 25);

    size_t second_device = graphene_metal_device_create(instance, 0);
    size_t foreign = graphene_metal_buffer_create(second_device, 64, 2, true);
    size_t ownership = graphene_metal_encoder_create(device);
    REQUIRE(graphene_metal_device_available(second_device), 26);
    REQUIRE(graphene_metal_buffer_available(foreign), 27);
    REQUIRE(!graphene_metal_encoder_copy_buffer(ownership, source, foreign, 0, 0, 4), 28);
    REQUIRE(graphene_metal_encoder_destroy(ownership), 29);

    uint32_t binding = 0;
    uint32_t kind = 6;
    uint32_t stage = GRAPHENE_METAL_STAGE_COMPUTE;
    size_t layout = graphene_metal_bind_group_layout_create_entries(device,
        &binding, 1, &kind, 1, &stage, 1);
    size_t group = graphene_metal_bind_group_create(device, layout, source, 0, 64);
    size_t foreign_group = graphene_metal_bind_group_create(device, layout, foreign, 0, 64);
    REQUIRE(graphene_metal_bind_group_layout_available(layout), 68);
    REQUIRE(graphene_metal_bind_group_available(group), 69);
    REQUIRE(!graphene_metal_bind_group_available(foreign_group), 70);
    REQUIRE(!graphene_metal_bind_group_update_buffer(group, 0, foreign, 0, 64), 71);
    REQUIRE(graphene_metal_bind_group_destroy(foreign_group), 72);
    REQUIRE(graphene_metal_bind_group_destroy(group), 73);
    REQUIRE(graphene_metal_bind_group_layout_destroy(layout), 74);

    static const char compute_source[] =
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "kernel void contract_double(device uint *values [[buffer(0)]], "
        "uint index [[thread_position_in_grid]]) { values[index] *= 2; }\n";
    GrapheneMetalShader *compute_shader = contract_shader(
        (GrapheneMetalDevice *)device, compute_source, "contract_double",
        GRAPHENE_METAL_STAGE_COMPUTE, 1);
    uint32_t storage_kind = 7;
    size_t compute_layout = graphene_metal_bind_group_layout_create_entries(device,
        &binding, 1, &storage_kind, 1, &stage, 1);
    size_t compute_buffer = graphene_metal_buffer_create(device, 16, 32, true);
    size_t compute_group = graphene_metal_bind_group_create(
        device, compute_layout, compute_buffer, 0, 16);
    size_t compute_pipeline = graphene_metal_compute_pipeline_create(device,
        (size_t)compute_shader, compute_layout);
    REQUIRE(compute_shader != NULL && compute_shader->available, 75);
    REQUIRE(graphene_metal_bind_group_layout_available(compute_layout), 76);
    REQUIRE(graphene_metal_bind_group_available(compute_group), 77);
    REQUIRE(graphene_metal_compute_pipeline_available(compute_pipeline), 78);
    for (uint64_t offset = 0; offset < 16; offset += 4)
        REQUIRE(graphene_metal_buffer_write_u32(compute_buffer, offset,
            (uint32_t)(offset / 4 + 1)), 79);
    size_t compute_encoder = graphene_metal_encoder_create(device);
    REQUIRE(graphene_metal_encoder_bind_compute_pipeline(
        compute_encoder, compute_pipeline), 80);
    REQUIRE(graphene_metal_encoder_bind_compute_group(
        compute_encoder, compute_pipeline, compute_group), 81);
    REQUIRE(graphene_metal_encoder_dispatch(compute_encoder, 4, 1, 1), 82);
    REQUIRE(graphene_metal_encoder_storage_host_barrier(compute_encoder), 83);
    REQUIRE(graphene_metal_encoder_submit_wait(compute_encoder), 84);
    REQUIRE(graphene_metal_encoder_destroy(compute_encoder), 85);
    for (uint64_t offset = 0; offset < 16; offset += 4)
        REQUIRE(graphene_metal_buffer_read_u32(compute_buffer, offset) ==
            (uint32_t)(offset / 2 + 2), 86);
    REQUIRE(graphene_metal_compute_pipeline_destroy(compute_pipeline), 87);
    REQUIRE(graphene_metal_bind_group_destroy(compute_group), 88);
    REQUIRE(graphene_metal_buffer_destroy(compute_buffer), 89);
    REQUIRE(graphene_metal_bind_group_layout_destroy(compute_layout), 90);
    REQUIRE(graphene_metal_shader_destroy((size_t)compute_shader), 91);

    size_t target = graphene_metal_offscreen_create(device, 16, 8);
    REQUIRE(graphene_metal_swapchain_available(target), 30);
    REQUIRE(graphene_metal_swapchain_width(target) == 16, 31);
    REQUIRE(graphene_metal_swapchain_height(target) == 8, 32);
    REQUIRE(graphene_metal_swapchain_pitch(target) == 64, 33);
    REQUIRE(graphene_metal_swapchain_frames_in_flight(target) == 2, 34);

    LumaMetalUiCompositor *ui = (LumaMetalUiCompositor *)luma_metal_ui_create(device, target);
    REQUIRE(ui != NULL && luma_metal_ui_available((size_t)ui), 101);
    uint8_t atlas_rows[4][8] = {
        {255, 0, 0, 255, 255, 0, 0, 255},
        {0, 255, 0, 255, 0, 255, 0, 255},
        {0, 0, 255, 255, 0, 0, 255, 255},
        {255, 255, 255, 255, 255, 255, 255, 255},
    };
    size_t atlas = luma_metal_ui_texture_create_empty((size_t)ui, 8, 8, 44);
    REQUIRE(atlas != 0, 127);
    REQUIRE(luma_metal_ui_texture_update(atlas, atlas_rows, 2, 4, 8, 3, 2), 128);
    REQUIRE(luma_metal_ui_cache_begin((size_t)ui, 0x000000ffu), 129);
    REQUIRE(luma_metal_ui_texture_draw_region((size_t)ui, atlas,
        0, 0, 16, 8, 3, 2, 2, 4), 130);
    REQUIRE(luma_metal_ui_cache_end((size_t)ui), 131);
    size_t atlas_frame = luma_metal_ui_frame_acquire((size_t)ui);
    REQUIRE(atlas_frame != 0, 132);
    REQUIRE(luma_metal_ui_frame_finish((size_t)ui, atlas_frame), 133);
    uint8_t *atlas_output = (uint8_t *)graphene_metal_swapchain_pixels(target);
    REQUIRE(atlas_output != NULL, 134);
    size_t atlas_blue = (size_t)(1 * 16 + 8) * 4;
    size_t atlas_green = (size_t)(3 * 16 + 8) * 4;
    size_t atlas_red = (size_t)(5 * 16 + 8) * 4;
    size_t atlas_white = (size_t)(7 * 16 + 8) * 4;
    REQUIRE(atlas_output[atlas_blue] > atlas_output[atlas_blue + 1] &&
        atlas_output[atlas_blue] > atlas_output[atlas_blue + 2], 135);
    REQUIRE(atlas_output[atlas_green + 1] > atlas_output[atlas_green] &&
        atlas_output[atlas_green + 1] > atlas_output[atlas_green + 2], 136);
    REQUIRE(atlas_output[atlas_red + 2] > atlas_output[atlas_red] &&
        atlas_output[atlas_red + 2] > atlas_output[atlas_red + 1], 137);
    REQUIRE(atlas_output[atlas_white] > 100 && atlas_output[atlas_white + 1] > 100 &&
        atlas_output[atlas_white + 2] > 100, 138);
    REQUIRE(luma_metal_ui_texture_destroy(atlas), 139);
    REQUIRE(luma_metal_ui_cache_begin((size_t)ui, 0x000000ffu), 102);
    REQUIRE(contract_ui_rect(ui, 0, 0, 16, 8, 0, 0, 1), 103);
    REQUIRE(luma_metal_ui_cache_end((size_t)ui), 104);
    REQUIRE(luma_metal_ui_cache_patch_begin(
        (size_t)ui, 0x0000ffffu, 0, 0, 4, 4), 105);
    REQUIRE(contract_ui_rect(ui, 0, 0, 4, 4, 1, 1, 0), 106);
    REQUIRE(luma_metal_ui_cache_end((size_t)ui), 107);
    REQUIRE(ui->segmented_commit_count == 1 && ui->patch_count == 1, 108);
    REQUIRE(luma_metal_ui_cached_begin((size_t)ui, 0x000000ffu), 109);
    REQUIRE(luma_metal_ui_composition_split((size_t)ui), 110);
    REQUIRE(contract_ui_rect(ui, 12, 0, 4, 4, 0, 1, 0), 111);

    static const char scene_vertex_source[] =
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "struct ContractOut { float4 position [[position]]; };\n"
        "vertex ContractOut contract_scene_vertex(uint id [[vertex_id]]) { "
        "float2 p[3] = {float2(-1,-1), float2(3,-1), float2(-1,3)}; "
        "ContractOut out; out.position = float4(p[id], 0, 1); return out; }\n";
    static const char scene_fragment_source[] =
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "fragment float4 contract_scene_fragment() { return float4(1,0,0,1); }\n";
    GrapheneMetalShader *scene_vertex = contract_shader(
        (GrapheneMetalDevice *)device, scene_vertex_source,
        "contract_scene_vertex", GRAPHENE_METAL_STAGE_VERTEX, 1);
    GrapheneMetalShader *scene_fragment = contract_shader(
        (GrapheneMetalDevice *)device, scene_fragment_source,
        "contract_scene_fragment", GRAPHENE_METAL_STAGE_FRAGMENT, 1);
    size_t scene_pipeline = graphene_metal_pipeline_create(device, target,
        (size_t)scene_vertex, (size_t)scene_fragment, 0,
        true, true, true, 0, false, false, true, 0,
        NULL, 0, NULL, 0, NULL, 0);
    REQUIRE(graphene_metal_pipeline_available(scene_pipeline), 112);
    size_t composed_frame = luma_metal_ui_frame_prepare((size_t)ui);
    REQUIRE(composed_frame != 0, 113);
    REQUIRE(luma_metal_ui_frame_begin_composition((size_t)ui, composed_frame), 114);
    REQUIRE(graphene_metal_frame_bind_pipeline(composed_frame, scene_pipeline), 115);
    REQUIRE(graphene_metal_frame_set_viewport(composed_frame, 4, 2, 8, 4), 116);
    GrapheneMetalFrame *native_frame = (GrapheneMetalFrame *)composed_frame;
    [native_frame->render drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    REQUIRE(luma_metal_ui_frame_finish((size_t)ui, composed_frame), 117);
    uint8_t *composed = (uint8_t *)graphene_metal_swapchain_pixels(target);
    size_t patch_pixel = (size_t)(1 * 16 + 1) * 4;
    size_t overlay_pixel = (size_t)(1 * 16 + 14) * 4;
    size_t scene_pixel = (size_t)(4 * 16 + 8) * 4;
    size_t base_pixel = (size_t)(6 * 16 + 1) * 4;
    REQUIRE(composed != NULL, 118);
    REQUIRE(composed[patch_pixel] == 0 && composed[patch_pixel + 1] == 255 &&
        composed[patch_pixel + 2] == 255 && composed[patch_pixel + 3] == 255, 119);
    REQUIRE(composed[overlay_pixel] == 0 && composed[overlay_pixel + 1] == 255 &&
        composed[overlay_pixel + 2] == 0 && composed[overlay_pixel + 3] == 255, 120);
    REQUIRE(composed[scene_pixel] == 0 && composed[scene_pixel + 1] == 0 &&
        composed[scene_pixel + 2] == 255 && composed[scene_pixel + 3] == 255, 121);
    REQUIRE(composed[base_pixel] == 255 && composed[base_pixel + 1] == 0 &&
        composed[base_pixel + 2] == 0 && composed[base_pixel + 3] == 255, 122);
    REQUIRE(graphene_metal_pipeline_destroy(scene_pipeline), 123);
    REQUIRE(graphene_metal_shader_destroy((size_t)scene_fragment), 124);
    REQUIRE(graphene_metal_shader_destroy((size_t)scene_vertex), 125);
    REQUIRE(luma_metal_ui_destroy((size_t)ui), 126);

    static const char vertex_source[] =
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "struct ContractOut { float4 position [[position]]; };\n"
        "vertex ContractOut contract_vertex(uint id [[vertex_id]]) { "
        "float2 p[3] = {float2(-1,-1), float2(3,-1), float2(-1,3)}; "
        "ContractOut out; out.position = float4(p[id], 0, 1); return out; }\n";
    static const char fragment_source[] =
        "#include <metal_stdlib>\n"
        "using namespace metal;\n"
        "fragment float4 contract_fragment() { return float4(0,1,0,1); }\n";
    GrapheneMetalShader *vertex_shader = contract_shader(
        (GrapheneMetalDevice *)device, vertex_source, "contract_vertex",
        GRAPHENE_METAL_STAGE_VERTEX, 1);
    GrapheneMetalShader *fragment_shader = contract_shader(
        (GrapheneMetalDevice *)device, fragment_source, "contract_fragment",
        GRAPHENE_METAL_STAGE_FRAGMENT, 1);
    size_t render_pipeline = graphene_metal_pipeline_create(device, target,
        (size_t)vertex_shader, (size_t)fragment_shader, 0,
        false, false, false, 0, false, false, false, 0,
        NULL, 0, NULL, 0, NULL, 0);
    REQUIRE(vertex_shader != NULL && vertex_shader->available, 92);
    REQUIRE(fragment_shader != NULL && fragment_shader->available, 93);
    REQUIRE(graphene_metal_pipeline_available(render_pipeline), 94);
    REQUIRE(graphene_metal_swapchain_draw_pipeline(target, render_pipeline,
        0.0f, 0.0f, 0.0f, 1.0f), 95);
    uint8_t *rendered = (uint8_t *)graphene_metal_swapchain_pixels(target);
    size_t center = (size_t)(4 * 16 + 8) * 4;
    REQUIRE(rendered != NULL, 96);
    REQUIRE(rendered[center] == 0 && rendered[center + 1] == 255 &&
        rendered[center + 2] == 0 && rendered[center + 3] == 255, 97);
    encore_str title = {(encore_str_object *)encore_str_from_cstr(
        "Graphene Metal Contract")};
    size_t window = graphene_metal_window_create(instance, title, 320, 200);
    free(title.object);
    REQUIRE(graphene_metal_window_available(window), 127);
    size_t window_target = graphene_metal_swapchain_create(device, window);
    REQUIRE(graphene_metal_swapchain_available(window_target), 128);
    REQUIRE(graphene_metal_swapchain_draw_pipeline(window_target, render_pipeline,
        0.0f, 0.0f, 0.0f, 1.0f), 129);
    REQUIRE(graphene_metal_swapchain_destroy(window_target), 130);
    REQUIRE(graphene_metal_window_destroy(window), 131);
    REQUIRE(graphene_metal_pipeline_destroy(render_pipeline), 98);
    REQUIRE(graphene_metal_shader_destroy((size_t)fragment_shader), 99);
    REQUIRE(graphene_metal_shader_destroy((size_t)vertex_shader), 100);

    size_t frame = graphene_metal_frame_acquire(target);
    REQUIRE(graphene_metal_frame_available(frame), 35);
    REQUIRE(graphene_metal_frame_fill_buffer(frame, destination, 0, 16, 0x12345678u), 36);
    REQUIRE(!graphene_metal_frame_fill_buffer(frame, source, 0, 16, 0), 37);
    REQUIRE(graphene_metal_frame_present(frame), 38);
    REQUIRE(!graphene_metal_frame_present(frame), 39);
    REQUIRE(graphene_metal_frame_destroy(frame), 40);
    for (uint64_t offset = 0; offset < 16; offset += 4)
        REQUIRE(graphene_metal_buffer_read_u32(destination, offset) == 0x12345678u, 41);

    size_t staging = graphene_metal_buffer_create(device, 16, 1, true);
    size_t texture = graphene_metal_texture_create(device, 2, 2, 1, 37, 6);
    size_t sampled_only = graphene_metal_texture_create(device, 2, 2, 1, 37, 4);
    REQUIRE(graphene_metal_buffer_write_u32(staging, 0, 0xff0000ffu), 42);
    REQUIRE(graphene_metal_texture_available(texture), 43);
    size_t upload = graphene_metal_encoder_create(device);
    REQUIRE(graphene_metal_encoder_transition_texture(upload, texture, 7), 44);
    REQUIRE(graphene_metal_encoder_copy_buffer_to_texture(upload, staging, texture, 2, 2, 0), 45);
    REQUIRE(!graphene_metal_encoder_copy_buffer_to_texture(upload, staging, sampled_only, 2, 2, 0), 46);
    REQUIRE(graphene_metal_encoder_transition_texture(upload, texture, 5), 47);
    REQUIRE(graphene_metal_encoder_submit_wait(upload), 48);
    REQUIRE(graphene_metal_encoder_destroy(upload), 49);
    REQUIRE(graphene_metal_texture_layout(texture) == 5, 50);

    REQUIRE(graphene_metal_swapchain_clear(target, 1.0f, 0.0f, 0.0f, 1.0f), 51);
    uint8_t *pixels = (uint8_t *)graphene_metal_swapchain_pixels(target);
    REQUIRE(pixels != NULL, 52);
    REQUIRE(pixels[0] == 0 && pixels[1] == 0 && pixels[2] == 255 && pixels[3] == 255, 53);

    REQUIRE(graphene_metal_texture_destroy(sampled_only), 54);
    REQUIRE(graphene_metal_texture_destroy(texture), 55);
    REQUIRE(graphene_metal_buffer_destroy(staging), 56);
    REQUIRE(graphene_metal_swapchain_destroy(target), 57);
    REQUIRE(graphene_metal_buffer_destroy(foreign), 58);
    REQUIRE(graphene_metal_device_destroy(second_device), 59);
    REQUIRE(graphene_metal_buffer_destroy(private_buffer), 60);
    REQUIRE(graphene_metal_buffer_destroy(destination), 61);
    REQUIRE(graphene_metal_buffer_destroy(source), 62);
    REQUIRE(graphene_metal_device_destroy(device), 63);
    REQUIRE(graphene_metal_destroy(instance), 64);
    return 0;
}

int main(void) {
    @autoreleasepool {
        return run_contract();
    }
}
