enum {
    GRAPHENE_METAL_MAX_BINDINGS = 32,
    GRAPHENE_METAL_STAGE_VERTEX = 1,
    GRAPHENE_METAL_STAGE_FRAGMENT = 16,
    GRAPHENE_METAL_STAGE_COMPUTE = 32,
};

#include "graphene_metallib.inc"

typedef struct {
    GrapheneMetalContext *context;
    id<MTLDevice> device;
    id<MTLCommandQueue> queue;
    uint32_t requested_features;
    uint32_t enabled_features;
    bool available;
    char error[256];
} GrapheneMetalDevice;

typedef struct {
    GrapheneMetalDevice *owner;
    id<MTLBuffer> buffer;
    id<MTLCommandBuffer> last_use;
    uint64_t size;
    uint32_t usage;
    bool host_visible;
    bool available;
    char error[256];
} GrapheneMetalBuffer;

typedef struct {
    GrapheneMetalDevice *owner;
    id<MTLTexture> texture;
    id<MTLCommandBuffer> last_use;
    uint32_t width;
    uint32_t height;
    uint32_t mip_levels;
    uint32_t format;
    uint32_t usage;
    uint32_t layout;
    bool available;
    char error[256];
} GrapheneMetalTexture;

typedef struct {
    GrapheneMetalDevice *owner;
    id<MTLLibrary> library;
    id<MTLFunction> function;
    uint32_t stage;
    uint32_t threadgroup_x;
    bool available;
    char error[512];
} GrapheneMetalShader;

typedef struct {
    GrapheneMetalDevice *owner;
    uint32_t bindings[GRAPHENE_METAL_MAX_BINDINGS];
    uint32_t kinds[GRAPHENE_METAL_MAX_BINDINGS];
    uint32_t stages[GRAPHENE_METAL_MAX_BINDINGS];
    size_t count;
    bool available;
    char error[256];
} GrapheneMetalBindGroupLayout;

typedef struct {
    GrapheneMetalDevice *owner;
    GrapheneMetalBindGroupLayout *layout;
    GrapheneMetalBuffer *buffers[GRAPHENE_METAL_MAX_BINDINGS];
    uint64_t offsets[GRAPHENE_METAL_MAX_BINDINGS];
    uint64_t ranges[GRAPHENE_METAL_MAX_BINDINGS];
    GrapheneMetalTexture *texture;
    id<MTLSamplerState> sampler;
    uint32_t texture_binding;
    bool available;
    char error[256];
} GrapheneMetalBindGroup;

typedef struct {
    GrapheneMetalDevice *owner;
    id<MTLSamplerState> sampler;
    bool available;
    char error[256];
} GrapheneMetalSampler;

typedef struct {
    GrapheneMetalDevice *owner;
    id<MTLComputePipelineState> state;
    uint32_t threadgroup_x;
    bool available;
    char error[512];
} GrapheneMetalComputePipeline;

typedef struct {
    GrapheneMetalBuffer **buffers;
    size_t buffer_count;
    size_t buffer_capacity;
    GrapheneMetalTexture **textures;
    size_t texture_count;
    size_t texture_capacity;
} GrapheneMetalResourceUsage;

typedef struct {
    GrapheneMetalDevice *owner;
    NSAutoreleasePool *pool;
    id<MTLCommandBuffer> command_buffer;
    id<MTLComputeCommandEncoder> compute;
    id<MTLBlitCommandEncoder> blit;
    GrapheneMetalComputePipeline *pipeline;
    GrapheneMetalBindGroup *group;
    GrapheneMetalResourceUsage resources;
    bool submitted;
    bool available;
    char error[512];
} GrapheneMetalEncoder;

typedef struct {
    id<MTLCommandBuffer> command_buffer;
    bool available;
    char error[512];
} GrapheneMetalSubmission;

static GrapheneMetalDevice *metal_device_new(GrapheneMetalContext *context,
    id<MTLDevice> borrowed_device, uint32_t required, uint32_t optional) {
    GrapheneMetalDevice *device = calloc(1, sizeof(GrapheneMetalDevice));
    if (device == NULL) return NULL;
    device->context = context;
    device->requested_features = required | optional;
    if (required != 0) {
        metal_error(device->error, sizeof(device->error),
            "Required advanced GPU features are unavailable on the Metal baseline backend");
        return device;
    }
    id<MTLDevice> native = borrowed_device != nil ? borrowed_device
        : (context == NULL ? nil : context->device);
    if (native == nil) {
        metal_error(device->error, sizeof(device->error), "Invalid Metal device");
        return device;
    }
    device->device = [native retain];
    device->queue = [device->device newCommandQueue];
    if (device->queue == nil) {
        metal_error(device->error, sizeof(device->error),
            "Metal device did not create a command queue");
        return device;
    }
    device->available = true;
    return device;
}

size_t graphene_metal_device_create_features(size_t handle, size_t adapter_index,
    uint32_t required, uint32_t optional);

size_t graphene_metal_device_create(size_t handle, size_t adapter_index) {
    return graphene_metal_device_create_features(handle, adapter_index, 0, 0);
}

size_t graphene_metal_device_create_features(size_t handle, size_t adapter_index,
    uint32_t required, uint32_t optional) {
    GrapheneMetalContext *context = (GrapheneMetalContext *)handle;
    if (adapter_index != 0) return (size_t)metal_device_new(NULL, nil, required, optional);
    return (size_t)metal_device_new(context, nil, required, optional);
}

size_t graphene_metal_device_borrow(size_t handle, size_t native_device,
    uint32_t queue_family) {
    (void)queue_family;
    return (size_t)metal_device_new((GrapheneMetalContext *)handle,
        (id<MTLDevice>)(void *)native_device, 0, 0);
}

size_t graphene_metal_present_device_create(size_t handle, size_t adapter_index,
    size_t window) {
    (void)window;
    return graphene_metal_device_create(handle, adapter_index);
}

size_t graphene_metal_present_device_create_features(size_t handle,
    size_t adapter_index, size_t window, uint32_t required, uint32_t optional) {
    (void)window;
    return graphene_metal_device_create_features(handle, adapter_index, required, optional);
}

bool graphene_metal_device_destroy(size_t handle) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)handle;
    if (device == NULL) return false;
    [device->queue release];
    [device->device release];
    free(device);
    return true;
}

bool graphene_metal_device_available(size_t handle) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)handle;
    return device != NULL && device->available && device->device != nil && device->queue != nil;
}

encore_str graphene_metal_device_error(size_t handle) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)handle;
    return metal_string(device == NULL ? "Invalid Metal device" : device->error);
}

uint32_t graphene_metal_device_queue_family(size_t handle) {
    return graphene_metal_device_available(handle) ? 0u : UINT32_MAX;
}

size_t graphene_metal_device_handle(size_t handle) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)handle;
    return graphene_metal_device_available(handle) ? (size_t)(void *)device->device : 0;
}

bool graphene_metal_device_graphics(size_t handle) {
    return graphene_metal_device_available(handle);
}

bool graphene_metal_device_compute(size_t handle) {
    return graphene_metal_device_available(handle);
}

uint32_t graphene_metal_device_requested_features(size_t handle) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)handle;
    return device == NULL ? 0u : device->requested_features;
}

uint32_t graphene_metal_device_enabled_features(size_t handle) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)handle;
    return device == NULL ? 0u : device->enabled_features;
}

static void metal_buffer_mark_use(GrapheneMetalBuffer *buffer,
    id<MTLCommandBuffer> command_buffer) {
    if (buffer == NULL || command_buffer == nil || buffer->last_use == command_buffer) return;
    [command_buffer retain];
    [buffer->last_use release];
    buffer->last_use = command_buffer;
}

static bool metal_buffer_prepare_host_access(GrapheneMetalBuffer *buffer) {
    if (buffer == NULL || !buffer->available || buffer->buffer == nil) return false;
    if (buffer->last_use == nil) return true;
    if (buffer->last_use.status == MTLCommandBufferStatusNotEnqueued) return false;
    [buffer->last_use waitUntilCompleted];
    bool succeeded = buffer->last_use.status != MTLCommandBufferStatusError;
    [buffer->last_use release];
    buffer->last_use = nil;
    return succeeded;
}

static void metal_texture_mark_use(GrapheneMetalTexture *texture,
    id<MTLCommandBuffer> command_buffer) {
    if (texture == NULL || command_buffer == nil || texture->last_use == command_buffer) return;
    [command_buffer retain];
    [texture->last_use release];
    texture->last_use = command_buffer;
}

static bool metal_texture_prepare_host_access(GrapheneMetalTexture *texture) {
    if (texture == NULL || !texture->available || texture->texture == nil) return false;
    if (texture->last_use == nil) return true;
    if (texture->last_use.status == MTLCommandBufferStatusNotEnqueued) return false;
    [texture->last_use waitUntilCompleted];
    bool succeeded = texture->last_use.status != MTLCommandBufferStatusError;
    [texture->last_use release];
    texture->last_use = nil;
    return succeeded;
}

static bool metal_resource_usage_track_buffer(GrapheneMetalResourceUsage *usage,
    GrapheneMetalBuffer *buffer) {
    if (usage == NULL || buffer == NULL) return false;
    for (size_t index = 0; index < usage->buffer_count; ++index)
        if (usage->buffers[index] == buffer) return true;
    if (usage->buffer_count == usage->buffer_capacity) {
        size_t capacity = usage->buffer_capacity == 0 ? 16 : usage->buffer_capacity * 2;
        GrapheneMetalBuffer **fresh = realloc(usage->buffers,
            capacity * sizeof(GrapheneMetalBuffer *));
        if (fresh == NULL) return false;
        usage->buffers = fresh;
        usage->buffer_capacity = capacity;
    }
    usage->buffers[usage->buffer_count++] = buffer;
    return true;
}

static bool metal_resource_usage_track_texture(GrapheneMetalResourceUsage *usage,
    GrapheneMetalTexture *texture) {
    if (usage == NULL || texture == NULL) return false;
    for (size_t index = 0; index < usage->texture_count; ++index)
        if (usage->textures[index] == texture) return true;
    if (usage->texture_count == usage->texture_capacity) {
        size_t capacity = usage->texture_capacity == 0 ? 8 : usage->texture_capacity * 2;
        GrapheneMetalTexture **fresh = realloc(usage->textures,
            capacity * sizeof(GrapheneMetalTexture *));
        if (fresh == NULL) return false;
        usage->textures = fresh;
        usage->texture_capacity = capacity;
    }
    usage->textures[usage->texture_count++] = texture;
    return true;
}

static void metal_resource_usage_submit(GrapheneMetalResourceUsage *usage,
    id<MTLCommandBuffer> command_buffer) {
    if (usage == NULL || command_buffer == nil) return;
    for (size_t index = 0; index < usage->buffer_count; ++index)
        metal_buffer_mark_use(usage->buffers[index], command_buffer);
    for (size_t index = 0; index < usage->texture_count; ++index)
        metal_texture_mark_use(usage->textures[index], command_buffer);
}

static void metal_resource_usage_destroy(GrapheneMetalResourceUsage *usage) {
    if (usage == NULL) return;
    free(usage->buffers);
    free(usage->textures);
    memset(usage, 0, sizeof(*usage));
}

size_t graphene_metal_buffer_create(size_t device_handle, uint64_t size,
    uint32_t usage, bool host_visible) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)device_handle;
    GrapheneMetalBuffer *buffer = calloc(1, sizeof(GrapheneMetalBuffer));
    if (buffer == NULL) return 0;
    buffer->owner = device;
    buffer->size = size;
    buffer->usage = usage;
    buffer->host_visible = host_visible;
    if (!graphene_metal_device_available(device_handle) || size == 0) {
        metal_error(buffer->error, sizeof(buffer->error), "Invalid Metal buffer request");
        return (size_t)buffer;
    }
    buffer->buffer = [device->device newBufferWithLength:(NSUInteger)size
        options:MTLResourceStorageModeShared];
    if (buffer->buffer == nil) {
        metal_error(buffer->error, sizeof(buffer->error), "Metal buffer allocation failed");
        return (size_t)buffer;
    }
    buffer->available = true;
    return (size_t)buffer;
}

bool graphene_metal_buffer_available(size_t handle) {
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)handle;
    return buffer != NULL && buffer->available && buffer->buffer != nil;
}

encore_str graphene_metal_buffer_error(size_t handle) {
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)handle;
    return metal_string(buffer == NULL ? "Invalid GPU buffer" : buffer->error);
}

uint64_t graphene_metal_buffer_size(size_t handle) {
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)handle;
    return buffer == NULL ? 0 : buffer->size;
}

bool graphene_metal_buffer_host_visible(size_t handle) {
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)handle;
    return buffer != NULL && buffer->host_visible;
}

static bool metal_buffer_write(GrapheneMetalBuffer *buffer, uint64_t offset,
    const void *values, size_t bytes) {
    if (!metal_buffer_prepare_host_access(buffer) || values == NULL ||
        offset > buffer->size || bytes > buffer->size - offset) return false;
    memcpy((uint8_t *)buffer->buffer.contents + offset, values, bytes);
    return true;
}

bool graphene_metal_buffer_write_u32(size_t handle, uint64_t offset, uint32_t value) {
    return metal_buffer_write((GrapheneMetalBuffer *)handle, offset, &value, sizeof(value));
}

bool graphene_metal_buffer_write_f32(size_t handle, uint64_t offset, float value) {
    return metal_buffer_write((GrapheneMetalBuffer *)handle, offset, &value, sizeof(value));
}

bool graphene_metal_buffer_write_u32s(size_t handle, uint64_t offset,
    const uint32_t *values, size_t count) {
    return count <= SIZE_MAX / sizeof(uint32_t) && metal_buffer_write(
        (GrapheneMetalBuffer *)handle, offset, values, count * sizeof(uint32_t));
}

bool graphene_metal_buffer_write_f32s(size_t handle, uint64_t offset,
    const float *values, size_t count) {
    return count <= SIZE_MAX / sizeof(float) && metal_buffer_write(
        (GrapheneMetalBuffer *)handle, offset, values, count * sizeof(float));
}

uint32_t graphene_metal_buffer_read_u32(size_t handle, uint64_t offset) {
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)handle;
    uint32_t result = 0;
    if (metal_buffer_prepare_host_access(buffer) && offset <= buffer->size &&
        sizeof(result) <= buffer->size - offset)
        memcpy(&result, (uint8_t *)buffer->buffer.contents + offset, sizeof(result));
    return result;
}

float graphene_metal_buffer_read_f32(size_t handle, uint64_t offset) {
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)handle;
    float result = 0.0f;
    if (metal_buffer_prepare_host_access(buffer) && offset <= buffer->size &&
        sizeof(result) <= buffer->size - offset)
        memcpy(&result, (uint8_t *)buffer->buffer.contents + offset, sizeof(result));
    return result;
}

bool graphene_metal_buffer_destroy(size_t handle) {
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)handle;
    if (buffer == NULL) return false;
    [buffer->last_use release];
    [buffer->buffer release];
    free(buffer);
    return true;
}

static MTLPixelFormat metal_pixel_format(uint32_t format) {
    if (format == 37u) return MTLPixelFormatRGBA8Unorm;
    if (format == 44u) return MTLPixelFormatBGRA8Unorm;
    if (format == 126u) return MTLPixelFormatDepth32Float;
    return MTLPixelFormatInvalid;
}

size_t graphene_metal_texture_create(size_t device_handle, uint32_t width,
    uint32_t height, uint32_t mip_levels, uint32_t format, uint32_t usage) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)device_handle;
    GrapheneMetalTexture *texture = calloc(1, sizeof(GrapheneMetalTexture));
    if (texture == NULL) return 0;
    texture->owner = device;
    texture->width = width;
    texture->height = height;
    texture->mip_levels = mip_levels;
    texture->format = format;
    texture->usage = usage;
    MTLPixelFormat pixel_format = metal_pixel_format(format);
    if (!graphene_metal_device_available(device_handle) || width == 0 || height == 0 ||
        mip_levels == 0 || pixel_format == MTLPixelFormatInvalid) {
        metal_error(texture->error, sizeof(texture->error), "Invalid Metal texture request");
        return (size_t)texture;
    }
    MTLTextureDescriptor *descriptor = [[MTLTextureDescriptor alloc] init];
    descriptor.textureType = MTLTextureType2D;
    descriptor.pixelFormat = pixel_format;
    descriptor.width = width;
    descriptor.height = height;
    descriptor.mipmapLevelCount = mip_levels;
    descriptor.sampleCount = 1;
    descriptor.storageMode = MTLStorageModeShared;
    MTLTextureUsage metal_usage = MTLTextureUsageUnknown;
    if ((usage & 4u) != 0) metal_usage |= MTLTextureUsageShaderRead;
    if ((usage & 8u) != 0) metal_usage |= MTLTextureUsageShaderWrite;
    if ((usage & (16u | 32u)) != 0) metal_usage |= MTLTextureUsageRenderTarget;
    descriptor.usage = metal_usage == MTLTextureUsageUnknown
        ? MTLTextureUsageShaderRead : metal_usage;
    texture->texture = [device->device newTextureWithDescriptor:descriptor];
    [descriptor release];
    if (texture->texture == nil) {
        metal_error(texture->error, sizeof(texture->error), "Metal texture allocation failed");
        return (size_t)texture;
    }
    texture->available = true;
    return (size_t)texture;
}

bool graphene_metal_texture_available(size_t handle) {
    GrapheneMetalTexture *texture = (GrapheneMetalTexture *)handle;
    return texture != NULL && texture->available && texture->texture != nil;
}

encore_str graphene_metal_texture_error(size_t handle) {
    GrapheneMetalTexture *texture = (GrapheneMetalTexture *)handle;
    return metal_string(texture == NULL ? "Invalid GPU texture" : texture->error);
}

uint32_t graphene_metal_texture_width(size_t handle) { GrapheneMetalTexture *t = (void *)handle; return t == NULL ? 0 : t->width; }
uint32_t graphene_metal_texture_height(size_t handle) { GrapheneMetalTexture *t = (void *)handle; return t == NULL ? 0 : t->height; }
uint32_t graphene_metal_texture_mip_levels(size_t handle) { GrapheneMetalTexture *t = (void *)handle; return t == NULL ? 0 : t->mip_levels; }
uint32_t graphene_metal_texture_format(size_t handle) { GrapheneMetalTexture *t = (void *)handle; return t == NULL ? 0 : t->format; }
uint32_t graphene_metal_texture_layout(size_t handle) { GrapheneMetalTexture *t = (void *)handle; return t == NULL ? 0 : t->layout; }

bool graphene_metal_texture_destroy(size_t handle) {
    GrapheneMetalTexture *texture = (GrapheneMetalTexture *)handle;
    if (texture == NULL) return false;
    [texture->last_use release];
    [texture->texture release];
    free(texture);
    return true;
}

size_t graphene_metal_shader_create(size_t device_handle, const uint32_t *words,
    size_t word_count, uint32_t stage) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)device_handle;
    GrapheneMetalShader *shader = calloc(1, sizeof(GrapheneMetalShader));
    if (shader == NULL) return 0;
    shader->owner = device;
    shader->stage = stage;
    if (!graphene_metal_device_available(device_handle) || words == NULL || word_count == 0 ||
        (stage != GRAPHENE_METAL_STAGE_VERTEX && stage != GRAPHENE_METAL_STAGE_FRAGMENT &&
            stage != GRAPHENE_METAL_STAGE_COMPUTE)) {
        metal_error(shader->error, sizeof(shader->error), "Invalid Metal shader library request");
        return (size_t)shader;
    }
    const uint32_t *library_words = words;
    size_t library_bytes = word_count * sizeof(uint32_t);
    uint32_t threadgroup_x = stage == GRAPHENE_METAL_STAGE_COMPUTE ? 64u : 1u;
    if (words[0] == 0x07230203u) {
        uint64_t hash = 1469598103934665603ull;
        const uint8_t *bytes = (const uint8_t *)words;
        for (size_t index = 0; index < word_count * sizeof(uint32_t); ++index) {
            hash ^= bytes[index];
            hash *= 1099511628211ull;
        }
        if (!graphene_metal_shader_artifact(hash, stage, &library_words,
                &library_bytes, &threadgroup_x)) {
            snprintf(shader->error, sizeof(shader->error),
                "No precompiled Metal artifact for SPIR-V hash %016llx",
                (unsigned long long)hash);
            return (size_t)shader;
        }
    }
    void *library_copy = malloc(library_bytes);
    if (library_copy == NULL) {
        metal_error(shader->error, sizeof(shader->error),
            "Unable to copy the precompiled Metal library");
        return (size_t)shader;
    }
    memcpy(library_copy, library_words, library_bytes);
    dispatch_data_t data = dispatch_data_create(library_copy, library_bytes,
        dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0),
        DISPATCH_DATA_DESTRUCTOR_FREE);
    NSError *error = nil;
    shader->library = [device->device newLibraryWithData:data error:&error];
    [(id)data release];
    if (shader->library == nil) {
        snprintf(shader->error, sizeof(shader->error),
            "Unable to load precompiled Metal library: %s",
            error == nil ? "invalid metallib bytes" : error.localizedDescription.UTF8String);
        return (size_t)shader;
    }
    NSArray<NSString *> *names = shader->library.functionNames;
    if (names.count != 1) {
        metal_error(shader->error, sizeof(shader->error),
            "A generated metallib must contain exactly one entry point");
        return (size_t)shader;
    }
    NSString *name = names.firstObject;
    shader->function = [shader->library newFunctionWithName:name];
    if (shader->function == nil) {
        metal_error(shader->error, sizeof(shader->error),
            "The generated Metal entry point could not be loaded");
        return (size_t)shader;
    }
    shader->threadgroup_x = threadgroup_x;
    shader->available = true;
    return (size_t)shader;
}

bool graphene_metal_shader_available(size_t handle) {
    GrapheneMetalShader *shader = (GrapheneMetalShader *)handle;
    return shader != NULL && shader->available && shader->function != nil;
}

encore_str graphene_metal_shader_error(size_t handle) {
    GrapheneMetalShader *shader = (GrapheneMetalShader *)handle;
    return metal_string(shader == NULL ? "Invalid Metal shader module" : shader->error);
}

uint32_t graphene_metal_shader_stage(size_t handle) {
    GrapheneMetalShader *shader = (GrapheneMetalShader *)handle;
    return shader == NULL ? 0u : shader->stage;
}

bool graphene_metal_shader_destroy(size_t handle) {
    GrapheneMetalShader *shader = (GrapheneMetalShader *)handle;
    if (shader == NULL) return false;
    [shader->function release];
    [shader->library release];
    free(shader);
    return true;
}

size_t graphene_metal_bind_group_layout_create_entries(size_t device_handle,
    const uint32_t *bindings, size_t binding_count,
    const uint32_t *types, size_t type_count,
    const uint32_t *stages, size_t stage_count) {
    GrapheneMetalBindGroupLayout *layout = calloc(1, sizeof(GrapheneMetalBindGroupLayout));
    if (layout == NULL) return 0;
    layout->owner = (GrapheneMetalDevice *)device_handle;
    if (!graphene_metal_device_available(device_handle) || bindings == NULL || types == NULL ||
        stages == NULL || binding_count == 0 || binding_count != type_count ||
        binding_count != stage_count || binding_count > GRAPHENE_METAL_MAX_BINDINGS) {
        metal_error(layout->error, sizeof(layout->error), "Invalid Metal bind group layout");
        return (size_t)layout;
    }
    for (size_t index = 0; index < binding_count; ++index) {
        if (bindings[index] >= GRAPHENE_METAL_MAX_BINDINGS) {
            metal_error(layout->error, sizeof(layout->error),
                "Metal baseline bindings must be in descriptor set 0 with binding below 32");
            return (size_t)layout;
        }
        layout->bindings[index] = bindings[index];
        layout->kinds[index] = types[index];
        layout->stages[index] = stages[index];
    }
    layout->count = binding_count;
    layout->available = true;
    return (size_t)layout;
}

size_t graphene_metal_bind_group_layout_create(size_t device_handle,
    uint32_t binding, uint32_t stage) {
    const uint32_t kind = 6u;
    return graphene_metal_bind_group_layout_create_entries(device_handle,
        &binding, 1, &kind, 1, &stage, 1);
}

bool graphene_metal_bind_group_layout_available(size_t handle) {
    GrapheneMetalBindGroupLayout *layout = (GrapheneMetalBindGroupLayout *)handle;
    return layout != NULL && layout->available;
}

encore_str graphene_metal_bind_group_layout_error(size_t handle) {
    GrapheneMetalBindGroupLayout *layout = (GrapheneMetalBindGroupLayout *)handle;
    return metal_string(layout == NULL ? "Invalid Metal bind group layout" : layout->error);
}

bool graphene_metal_bind_group_layout_destroy(size_t handle) {
    if (handle == 0) return false;
    free((void *)handle);
    return true;
}

static GrapheneMetalBindGroup *metal_bind_group_new(size_t layout_handle) {
    GrapheneMetalBindGroup *group = calloc(1, sizeof(GrapheneMetalBindGroup));
    if (group == NULL) return NULL;
    group->layout = (GrapheneMetalBindGroupLayout *)layout_handle;
    if (!graphene_metal_bind_group_layout_available(layout_handle)) {
        metal_error(group->error, sizeof(group->error), "Invalid Metal bind group layout");
        return group;
    }
    group->owner = group->layout->owner;
    group->available = true;
    return group;
}

static bool metal_bind_group_buffers(GrapheneMetalBindGroup *group,
    const uint32_t *bindings, size_t binding_count, const size_t *buffers,
    size_t buffer_count, const uint64_t *offsets, size_t offset_count,
    const uint64_t *ranges, size_t range_count) {
    if (group == NULL || !group->available || bindings == NULL || buffers == NULL ||
        offsets == NULL || ranges == NULL || binding_count != buffer_count ||
        binding_count != offset_count || binding_count != range_count) return false;
    for (size_t index = 0; index < binding_count; ++index) {
        uint32_t binding = bindings[index];
        GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)buffers[index];
        if (binding >= GRAPHENE_METAL_MAX_BINDINGS ||
            !graphene_metal_buffer_available((size_t)buffer) || offsets[index] > buffer->size ||
            ranges[index] == 0 || ranges[index] > buffer->size - offsets[index] ||
            buffer->owner != group->owner) return false;
        group->buffers[binding] = buffer;
        group->offsets[binding] = offsets[index];
        group->ranges[binding] = ranges[index];
    }
    return true;
}

size_t graphene_metal_storage_bind_group_create_many(size_t device, size_t layout,
    const uint32_t *bindings, size_t binding_count, const size_t *buffers,
    size_t buffer_count, const uint64_t *offsets, size_t offset_count,
    const uint64_t *ranges, size_t range_count) {
    GrapheneMetalBindGroup *group = metal_bind_group_new(layout);
    if (group != NULL && ((GrapheneMetalDevice *)device != group->owner ||
        !metal_bind_group_buffers(group, bindings, binding_count,
            buffers, buffer_count, offsets, offset_count, ranges, range_count))) {
        group->available = false;
        metal_error(group->error, sizeof(group->error), "Invalid Metal buffer binding");
    }
    return (size_t)group;
}

size_t graphene_metal_buffer_bind_group_create_many(size_t device, size_t layout,
    const uint32_t *bindings, size_t binding_count, const size_t *buffers,
    size_t buffer_count, const uint64_t *offsets, size_t offset_count,
    const uint64_t *ranges, size_t range_count) {
    return graphene_metal_storage_bind_group_create_many(device, layout, bindings,
        binding_count, buffers, buffer_count, offsets, offset_count, ranges, range_count);
}

size_t graphene_metal_bind_group_create(size_t device, size_t layout, size_t buffer,
    uint64_t offset, uint64_t range) {
    GrapheneMetalBindGroupLayout *native_layout = (GrapheneMetalBindGroupLayout *)layout;
    uint32_t binding = native_layout != NULL && native_layout->count > 0
        ? native_layout->bindings[0] : 0u;
    return graphene_metal_storage_bind_group_create_many(device, layout, &binding, 1,
        &buffer, 1, &offset, 1, &range, 1);
}

size_t graphene_metal_storage_bind_group_create(size_t device, size_t layout,
    size_t buffer, uint64_t offset, uint64_t range) {
    return graphene_metal_bind_group_create(device, layout, buffer, offset, range);
}

size_t graphene_metal_buffer_texture_bind_group_create_many(size_t device,
    size_t layout, const uint32_t *bindings, size_t binding_count,
    const size_t *buffers, size_t buffer_count, const uint64_t *offsets,
    size_t offset_count, const uint64_t *ranges, size_t range_count,
    uint32_t texture_binding, size_t texture, size_t sampler) {
    GrapheneMetalBindGroup *group = (GrapheneMetalBindGroup *)
        graphene_metal_storage_bind_group_create_many(device, layout, bindings,
            binding_count, buffers, buffer_count, offsets, offset_count, ranges, range_count);
    GrapheneMetalTexture *native_texture = (GrapheneMetalTexture *)texture;
    GrapheneMetalSampler *native_sampler = (GrapheneMetalSampler *)sampler;
    if (group == NULL || texture_binding >= GRAPHENE_METAL_MAX_BINDINGS ||
        !graphene_metal_texture_available(texture) || native_sampler == NULL ||
        !native_sampler->available || native_texture->owner != group->owner ||
        native_sampler->owner != group->owner) {
        if (group != NULL) {
            group->available = false;
            metal_error(group->error, sizeof(group->error), "Invalid Metal texture binding");
        }
        return (size_t)group;
    }
    group->texture_binding = texture_binding;
    group->texture = native_texture;
    group->sampler = native_sampler->sampler;
    return (size_t)group;
}

size_t graphene_metal_material_bind_group_create(size_t device, size_t layout,
    uint32_t uniform_binding, size_t buffer, uint64_t offset, uint64_t range,
    uint32_t texture_binding, size_t texture, size_t sampler) {
    return graphene_metal_buffer_texture_bind_group_create_many(device, layout,
        &uniform_binding, 1, &buffer, 1, &offset, 1, &range, 1,
        texture_binding, texture, sampler);
}

size_t graphene_metal_scene_bind_group_create(size_t device, size_t layout,
    uint32_t uniform_binding, size_t uniform_buffer, uint64_t uniform_range,
    uint32_t storage_binding, size_t storage_buffer, uint64_t storage_range) {
    uint32_t bindings[2] = {uniform_binding, storage_binding};
    size_t buffers[2] = {uniform_buffer, storage_buffer};
    uint64_t offsets[2] = {0, 0};
    uint64_t ranges[2] = {uniform_range, storage_range};
    return graphene_metal_storage_bind_group_create_many(device, layout,
        bindings, 2, buffers, 2, offsets, 2, ranges, 2);
}

bool graphene_metal_bind_group_available(size_t handle) {
    GrapheneMetalBindGroup *group = (GrapheneMetalBindGroup *)handle;
    return group != NULL && group->available;
}

encore_str graphene_metal_bind_group_error(size_t handle) {
    GrapheneMetalBindGroup *group = (GrapheneMetalBindGroup *)handle;
    return metal_string(group == NULL ? "Invalid Metal bind group" : group->error);
}

bool graphene_metal_bind_group_destroy(size_t handle) {
    if (handle == 0) return false;
    free((void *)handle);
    return true;
}

bool graphene_metal_bind_group_update_buffer(size_t handle, uint32_t binding,
    size_t buffer, uint64_t offset, uint64_t range) {
    GrapheneMetalBindGroup *group = (GrapheneMetalBindGroup *)handle;
    GrapheneMetalBuffer *native = (GrapheneMetalBuffer *)buffer;
    if (!graphene_metal_bind_group_available(handle) ||
        !graphene_metal_buffer_available(buffer) || binding >= GRAPHENE_METAL_MAX_BINDINGS ||
        offset > native->size || range == 0 || range > native->size - offset ||
        native->owner != group->owner) return false;
    group->buffers[binding] = native;
    group->offsets[binding] = offset;
    group->ranges[binding] = range;
    return true;
}

static size_t metal_sampler_create(size_t device_handle, bool repeat) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)device_handle;
    GrapheneMetalSampler *sampler = calloc(1, sizeof(GrapheneMetalSampler));
    if (sampler == NULL) return 0;
    sampler->owner = device;
    if (!graphene_metal_device_available(device_handle)) {
        metal_error(sampler->error, sizeof(sampler->error), "Invalid Metal sampler device");
        return (size_t)sampler;
    }
    MTLSamplerDescriptor *descriptor = [[MTLSamplerDescriptor alloc] init];
    descriptor.minFilter = MTLSamplerMinMagFilterLinear;
    descriptor.magFilter = MTLSamplerMinMagFilterLinear;
    descriptor.mipFilter = MTLSamplerMipFilterLinear;
    descriptor.sAddressMode = repeat ? MTLSamplerAddressModeRepeat : MTLSamplerAddressModeClampToEdge;
    descriptor.tAddressMode = descriptor.sAddressMode;
    sampler->sampler = [device->device newSamplerStateWithDescriptor:descriptor];
    [descriptor release];
    sampler->available = sampler->sampler != nil;
    if (!sampler->available)
        metal_error(sampler->error, sizeof(sampler->error), "Metal sampler creation failed");
    return (size_t)sampler;
}

size_t graphene_metal_sampler_create(size_t device) { return metal_sampler_create(device, false); }
size_t graphene_metal_sampler_create_repeat(size_t device) { return metal_sampler_create(device, true); }
bool graphene_metal_sampler_available(size_t handle) { GrapheneMetalSampler *s = (void *)handle; return s != NULL && s->available; }
encore_str graphene_metal_sampler_error(size_t handle) { GrapheneMetalSampler *s = (void *)handle; return metal_string(s == NULL ? "Invalid Metal sampler" : s->error); }
bool graphene_metal_sampler_destroy(size_t handle) { GrapheneMetalSampler *s = (void *)handle; if (s == NULL) return false; [s->sampler release]; free(s); return true; }

size_t graphene_metal_compute_pipeline_create(size_t device_handle,
    size_t shader_handle, size_t layout) {
    (void)layout;
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)device_handle;
    GrapheneMetalShader *shader = (GrapheneMetalShader *)shader_handle;
    GrapheneMetalComputePipeline *pipeline = calloc(1, sizeof(GrapheneMetalComputePipeline));
    if (pipeline == NULL) return 0;
    pipeline->owner = device;
    if (!graphene_metal_device_available(device_handle) ||
        !graphene_metal_shader_available(shader_handle) || shader->owner != device ||
        shader->stage != GRAPHENE_METAL_STAGE_COMPUTE) {
        metal_error(pipeline->error, sizeof(pipeline->error), "Invalid Metal compute pipeline request");
        return (size_t)pipeline;
    }
    NSError *error = nil;
    pipeline->state = [device->device newComputePipelineStateWithFunction:shader->function error:&error];
    if (pipeline->state == nil) {
        snprintf(pipeline->error, sizeof(pipeline->error), "Metal compute pipeline failed: %s",
            error == nil ? "unknown error" : error.localizedDescription.UTF8String);
        return (size_t)pipeline;
    }
    pipeline->threadgroup_x = shader->threadgroup_x;
    pipeline->available = true;
    return (size_t)pipeline;
}

bool graphene_metal_compute_pipeline_available(size_t handle) { GrapheneMetalComputePipeline *p = (void *)handle; return p != NULL && p->available; }
encore_str graphene_metal_compute_pipeline_error(size_t handle) { GrapheneMetalComputePipeline *p = (void *)handle; return metal_string(p == NULL ? "Invalid Metal compute pipeline" : p->error); }
bool graphene_metal_compute_pipeline_destroy(size_t handle) { GrapheneMetalComputePipeline *p = (void *)handle; if (p == NULL) return false; [p->state release]; free(p); return true; }

static void metal_encoder_end(GrapheneMetalEncoder *encoder) {
    if (encoder == NULL) return;
    if (encoder->compute != nil) { [encoder->compute endEncoding]; encoder->compute = nil; }
    if (encoder->blit != nil) { [encoder->blit endEncoding]; encoder->blit = nil; }
}

static bool metal_bind_compute_resources(id<MTLComputeCommandEncoder> encoder,
    GrapheneMetalResourceUsage *resources, GrapheneMetalBindGroup *group) {
    if (encoder == nil || group == NULL || !group->available) return false;
    for (uint32_t binding = 0; binding < GRAPHENE_METAL_MAX_BINDINGS; ++binding) {
        GrapheneMetalBuffer *buffer = group->buffers[binding];
        if (buffer != NULL && buffer->available) {
            if (!metal_resource_usage_track_buffer(resources, buffer)) return false;
            [encoder setBuffer:buffer->buffer offset:(NSUInteger)group->offsets[binding] atIndex:binding];
        }
    }
    if (group->texture != NULL) {
        if (!metal_resource_usage_track_texture(resources, group->texture)) return false;
        [encoder setTexture:group->texture->texture atIndex:group->texture_binding];
    }
    if (group->sampler != nil)
        [encoder setSamplerState:group->sampler atIndex:group->texture_binding];
    return true;
}

size_t graphene_metal_encoder_create(size_t device_handle) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)device_handle;
    GrapheneMetalEncoder *encoder = calloc(1, sizeof(GrapheneMetalEncoder));
    if (encoder == NULL) return 0;
    encoder->pool = [[NSAutoreleasePool alloc] init];
    encoder->owner = device;
    if (!graphene_metal_device_available(device_handle)) {
        metal_error(encoder->error, sizeof(encoder->error), "Invalid Metal command encoder device");
        return (size_t)encoder;
    }
    encoder->command_buffer = [device->queue commandBuffer];
    [encoder->command_buffer retain];
    encoder->available = encoder->command_buffer != nil;
    if (!encoder->available)
        metal_error(encoder->error, sizeof(encoder->error), "Metal command buffer allocation failed");
    return (size_t)encoder;
}

bool graphene_metal_encoder_available(size_t handle) { GrapheneMetalEncoder *e = (void *)handle; return e != NULL && e->available && !e->submitted; }
encore_str graphene_metal_encoder_error(size_t handle) { GrapheneMetalEncoder *e = (void *)handle; return metal_string(e == NULL ? "Invalid Metal command encoder" : e->error); }

bool graphene_metal_encoder_bind_compute_pipeline(size_t handle, size_t pipeline_handle) {
    GrapheneMetalEncoder *encoder = (GrapheneMetalEncoder *)handle;
    GrapheneMetalComputePipeline *pipeline = (GrapheneMetalComputePipeline *)pipeline_handle;
    if (!graphene_metal_encoder_available(handle) ||
        !graphene_metal_compute_pipeline_available(pipeline_handle) ||
        pipeline->owner != encoder->owner) return false;
    if (encoder->blit != nil) { [encoder->blit endEncoding]; encoder->blit = nil; }
    if (encoder->compute == nil) encoder->compute = [encoder->command_buffer computeCommandEncoder];
    [encoder->compute setComputePipelineState:pipeline->state];
    encoder->pipeline = pipeline;
    return true;
}

bool graphene_metal_encoder_bind_compute_group(size_t handle, size_t pipeline,
    size_t group_handle) {
    (void)pipeline;
    GrapheneMetalEncoder *encoder = (GrapheneMetalEncoder *)handle;
    GrapheneMetalBindGroup *group = (GrapheneMetalBindGroup *)group_handle;
    if (!graphene_metal_encoder_available(handle) ||
        !graphene_metal_bind_group_available(group_handle) ||
        group->owner != encoder->owner) return false;
    encoder->group = group;
    if (encoder->compute != nil)
        return metal_bind_compute_resources(encoder->compute,
            &encoder->resources, group);
    return true;
}

bool graphene_metal_encoder_dispatch(size_t handle, uint32_t x, uint32_t y, uint32_t z) {
    GrapheneMetalEncoder *encoder = (GrapheneMetalEncoder *)handle;
    if (!graphene_metal_encoder_available(handle) || encoder->pipeline == NULL ||
        encoder->compute == nil || x == 0 || y == 0 || z == 0) return false;
    if (!metal_bind_compute_resources(encoder->compute, &encoder->resources,
            encoder->group)) return false;
    MTLSize groups = MTLSizeMake(x, y, z);
    MTLSize threads = MTLSizeMake(encoder->pipeline->threadgroup_x, 1, 1);
    [encoder->compute dispatchThreadgroups:groups threadsPerThreadgroup:threads];
    return true;
}

bool graphene_metal_encoder_storage_host_barrier(size_t handle) {
    GrapheneMetalEncoder *encoder = (GrapheneMetalEncoder *)handle;
    if (!graphene_metal_encoder_available(handle)) return false;
    if (encoder->compute != nil) { [encoder->compute endEncoding]; encoder->compute = nil; }
    return true;
}
bool graphene_metal_encoder_transfer_host_barrier(size_t handle) { return graphene_metal_encoder_storage_host_barrier(handle); }
bool graphene_metal_encoder_transfer_geometry_barrier(size_t handle) { return graphene_metal_encoder_storage_host_barrier(handle); }

bool graphene_metal_encoder_copy_buffer(size_t handle, size_t source_handle,
    size_t destination_handle, uint64_t source_offset, uint64_t destination_offset,
    uint64_t size) {
    GrapheneMetalEncoder *encoder = (GrapheneMetalEncoder *)handle;
    GrapheneMetalBuffer *source = (GrapheneMetalBuffer *)source_handle;
    GrapheneMetalBuffer *destination = (GrapheneMetalBuffer *)destination_handle;
    if (!graphene_metal_encoder_available(handle) ||
        !graphene_metal_buffer_available(source_handle) ||
        !graphene_metal_buffer_available(destination_handle) ||
        source->owner != encoder->owner || destination->owner != encoder->owner ||
        (source->usage & 1u) == 0 || (destination->usage & 2u) == 0 || size == 0 ||
        source_offset > source->size || size > source->size - source_offset ||
        destination_offset > destination->size || size > destination->size - destination_offset)
        return false;
    if (!metal_resource_usage_track_buffer(&encoder->resources, source) ||
        !metal_resource_usage_track_buffer(&encoder->resources, destination)) return false;
    if (encoder->compute != nil) { [encoder->compute endEncoding]; encoder->compute = nil; }
    if (encoder->blit == nil) encoder->blit = [encoder->command_buffer blitCommandEncoder];
    [encoder->blit copyFromBuffer:source->buffer sourceOffset:(NSUInteger)source_offset
        toBuffer:destination->buffer destinationOffset:(NSUInteger)destination_offset
        size:(NSUInteger)size];
    return true;
}

bool graphene_metal_encoder_transition_texture(size_t handle, size_t texture_handle,
    uint32_t new_layout) {
    GrapheneMetalEncoder *encoder = (GrapheneMetalEncoder *)handle;
    GrapheneMetalTexture *texture = (GrapheneMetalTexture *)texture_handle;
    if (!graphene_metal_encoder_available(handle) ||
        !graphene_metal_texture_available(texture_handle) ||
        texture->owner != encoder->owner) return false;
    texture->layout = new_layout;
    return true;
}

bool graphene_metal_encoder_copy_buffer_to_texture(size_t handle, size_t buffer_handle,
    size_t texture_handle, uint32_t width, uint32_t height, uint32_t mip_level) {
    GrapheneMetalEncoder *encoder = (GrapheneMetalEncoder *)handle;
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)buffer_handle;
    GrapheneMetalTexture *texture = (GrapheneMetalTexture *)texture_handle;
    uint64_t bytes = (uint64_t)width * (uint64_t)height * 4u;
    if (!graphene_metal_encoder_available(handle) || !graphene_metal_buffer_available(buffer_handle) ||
        !graphene_metal_texture_available(texture_handle) || width == 0 || height == 0 ||
        buffer->owner != encoder->owner || texture->owner != encoder->owner ||
        (buffer->usage & 1u) == 0 || (texture->usage & 2u) == 0 ||
        width > texture->width || height > texture->height || mip_level >= texture->mip_levels ||
        bytes > buffer->size) return false;
    if (!metal_resource_usage_track_buffer(&encoder->resources, buffer) ||
        !metal_resource_usage_track_texture(&encoder->resources, texture)) return false;
    if (encoder->compute != nil) { [encoder->compute endEncoding]; encoder->compute = nil; }
    if (encoder->blit == nil) encoder->blit = [encoder->command_buffer blitCommandEncoder];
    [encoder->blit copyFromBuffer:buffer->buffer sourceOffset:0
        sourceBytesPerRow:width * 4u sourceBytesPerImage:(NSUInteger)bytes
        sourceSize:MTLSizeMake(width, height, 1) toTexture:texture->texture
        destinationSlice:0 destinationLevel:mip_level
        destinationOrigin:MTLOriginMake(0, 0, 0)];
    return true;
}

static GrapheneMetalSubmission *metal_encoder_submit(GrapheneMetalEncoder *encoder) {
    if (encoder == NULL || !encoder->available || encoder->submitted) return NULL;
    metal_encoder_end(encoder);
    metal_resource_usage_submit(&encoder->resources, encoder->command_buffer);
    [encoder->command_buffer commit];
    encoder->submitted = true;
    GrapheneMetalSubmission *submission = calloc(1, sizeof(GrapheneMetalSubmission));
    if (submission == NULL) return NULL;
    submission->command_buffer = [encoder->command_buffer retain];
    submission->available = true;
    return submission;
}

size_t graphene_metal_encoder_submit(size_t handle) {
    return (size_t)metal_encoder_submit((GrapheneMetalEncoder *)handle);
}

bool graphene_metal_encoder_submit_wait(size_t handle) {
    GrapheneMetalSubmission *submission = metal_encoder_submit((GrapheneMetalEncoder *)handle);
    if (submission == NULL) return false;
    [submission->command_buffer waitUntilCompleted];
    bool success = submission->command_buffer.status == MTLCommandBufferStatusCompleted;
    [submission->command_buffer release];
    free(submission);
    return success;
}

bool graphene_metal_encoder_destroy(size_t handle) {
    GrapheneMetalEncoder *encoder = (GrapheneMetalEncoder *)handle;
    if (encoder == NULL) return false;
    metal_encoder_end(encoder);
    [encoder->command_buffer release];
    [encoder->pool drain];
    metal_resource_usage_destroy(&encoder->resources);
    free(encoder);
    return true;
}

bool graphene_metal_submission_available(size_t handle) { GrapheneMetalSubmission *s = (void *)handle; return s != NULL && s->available; }
bool graphene_metal_submission_complete(size_t handle) { GrapheneMetalSubmission *s = (void *)handle; return s != NULL && (s->command_buffer.status == MTLCommandBufferStatusCompleted || s->command_buffer.status == MTLCommandBufferStatusError); }
bool graphene_metal_submission_wait(size_t handle, uint64_t timeout_ns) { (void)timeout_ns; GrapheneMetalSubmission *s = (void *)handle; if (s == NULL) return false; [s->command_buffer waitUntilCompleted]; return s->command_buffer.status == MTLCommandBufferStatusCompleted; }
encore_str graphene_metal_submission_error(size_t handle) { GrapheneMetalSubmission *s = (void *)handle; return metal_string(s == NULL || s->command_buffer.error == nil ? "" : s->command_buffer.error.localizedDescription.UTF8String); }
bool graphene_metal_submission_destroy(size_t handle) { GrapheneMetalSubmission *s = (void *)handle; if (s == NULL) return false; [s->command_buffer release]; free(s); return true; }
