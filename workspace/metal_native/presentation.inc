#include <dlfcn.h>

enum { GRAPHENE_METAL_VERTEX_BUFFER_INDEX = 30 };

typedef void *(*MetalSdlCreateWindowFn)(const char *, int, int, uint64_t);
typedef void (*MetalSdlDestroyWindowFn)(void *);
typedef void *(*MetalSdlCreateViewFn)(void *);
typedef void (*MetalSdlDestroyViewFn)(void *);
typedef void *(*MetalSdlGetLayerFn)(void *);
typedef bool (*MetalSdlGetWindowSizeInPixelsFn)(void *, int *, int *);

typedef struct {
    void *library;
    MetalSdlCreateWindowFn create_window;
    MetalSdlDestroyWindowFn destroy_window;
    MetalSdlCreateViewFn create_view;
    MetalSdlDestroyViewFn destroy_view;
    MetalSdlGetLayerFn get_layer;
    MetalSdlGetWindowSizeInPixelsFn get_size_pixels;
} GrapheneMetalSdl;

typedef struct {
    GrapheneMetalContext *context;
    GrapheneMetalSdl sdl;
    void *window;
    void *view;
    CAMetalLayer *layer;
    uint32_t width;
    uint32_t height;
    bool owns_window;
    bool available;
    char error[256];
} GrapheneMetalWindow;

typedef struct GrapheneMetalSwapchain {
    GrapheneMetalDevice *device;
    GrapheneMetalWindow *window;
    id<MTLTexture> color;
    id<MTLTexture> depth;
    id<MTLCommandBuffer> in_flight[2];
    NSMutableArray *in_flight_resources[2];
    uint8_t *pixels;
    uint32_t width;
    uint32_t height;
    uint32_t next_slot;
    bool offscreen;
    bool immediate;
    bool available;
    char error[512];
} GrapheneMetalSwapchain;

typedef struct {
    GrapheneMetalDevice *owner;
    id<MTLRenderPipelineState> state;
    id<MTLDepthStencilState> depth_state;
    MTLCullMode cull_mode;
    bool depth_attachment;
    bool load_color;
    bool available;
    char error[512];
} GrapheneMetalPipeline;

typedef struct {
    GrapheneMetalSwapchain *swapchain;
    NSAutoreleasePool *pool;
    id<CAMetalDrawable> drawable;
    id<MTLTexture> color;
    id<MTLCommandBuffer> command_buffer;
    id<MTLRenderCommandEncoder> render;
    id<MTLComputeCommandEncoder> compute;
    id<MTLBlitCommandEncoder> blit;
    NSMutableArray *transient_resources;
    GrapheneMetalPipeline *pipeline;
    GrapheneMetalComputePipeline *compute_pipeline;
    GrapheneMetalBindGroup *group;
    GrapheneMetalResourceUsage resources;
    GrapheneMetalBuffer *vertex;
    GrapheneMetalBuffer *index;
    uint32_t slot;
    bool presented;
    bool available;
    char error[512];
} GrapheneMetalFrame;

static bool metal_sdl_load(GrapheneMetalSdl *sdl) {
    if (sdl == NULL) return false;
    const char *names[] = {
        "/opt/homebrew/lib/libSDL3.dylib",
        "/usr/local/lib/libSDL3.dylib",
        "libSDL3.dylib",
    };
    for (size_t index = 0; index < sizeof(names) / sizeof(names[0]); ++index) {
        sdl->library = dlopen(names[index], RTLD_NOW | RTLD_LOCAL);
        if (sdl->library != NULL) break;
    }
    if (sdl->library == NULL) return false;
    *(void **)&sdl->create_window = dlsym(sdl->library, "SDL_CreateWindow");
    *(void **)&sdl->destroy_window = dlsym(sdl->library, "SDL_DestroyWindow");
    *(void **)&sdl->create_view = dlsym(sdl->library, "SDL_Metal_CreateView");
    *(void **)&sdl->destroy_view = dlsym(sdl->library, "SDL_Metal_DestroyView");
    *(void **)&sdl->get_layer = dlsym(sdl->library, "SDL_Metal_GetLayer");
    *(void **)&sdl->get_size_pixels = dlsym(sdl->library, "SDL_GetWindowSizeInPixels");
    return sdl->create_view != NULL && sdl->destroy_view != NULL &&
        sdl->get_layer != NULL && sdl->get_size_pixels != NULL;
}

static char *metal_c_string(encore_str value) {
    size_t length = value.object == NULL ? 0 : value.object->len;
    char *result = malloc(length + 1);
    if (result == NULL) return NULL;
    if (length > 0) memcpy(result, value.object->data, length);
    result[length] = '\0';
    return result;
}

static size_t metal_window_attach_internal(size_t instance_handle, void *platform_window,
    uint32_t width, uint32_t height, bool owns_window, GrapheneMetalSdl sdl) {
    GrapheneMetalContext *context = (GrapheneMetalContext *)instance_handle;
    GrapheneMetalWindow *window = calloc(1, sizeof(GrapheneMetalWindow));
    if (window == NULL) return 0;
    window->context = context;
    window->sdl = sdl;
    window->window = platform_window;
    window->owns_window = owns_window;
    window->width = width;
    window->height = height;
    if (!graphene_metal_available(instance_handle) || !context->window_support ||
        platform_window == NULL || sdl.create_view == NULL) {
        metal_error(window->error, sizeof(window->error),
            "Metal context was not created with valid SDL window support");
        return (size_t)window;
    }
    window->view = sdl.create_view(platform_window);
    if (window->view == NULL) {
        metal_error(window->error, sizeof(window->error), "SDL_Metal_CreateView failed");
        return (size_t)window;
    }
    window->layer = (CAMetalLayer *)sdl.get_layer(window->view);
    if (window->layer == nil) {
        metal_error(window->error, sizeof(window->error), "SDL_Metal_GetLayer returned nil");
        return (size_t)window;
    }
    window->layer.device = context->device;
    window->layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
    window->layer.framebufferOnly = NO;
    window->layer.maximumDrawableCount = 2;
    window->layer.displaySyncEnabled = YES;
    int pixel_width = 0, pixel_height = 0;
    if (sdl.get_size_pixels(platform_window, &pixel_width, &pixel_height) &&
        pixel_width > 0 && pixel_height > 0) {
        window->width = (uint32_t)pixel_width;
        window->height = (uint32_t)pixel_height;
    }
    window->layer.drawableSize = CGSizeMake(window->width, window->height);
    window->available = true;
    return (size_t)window;
}

size_t graphene_metal_window_attach(size_t instance_handle, size_t platform_handle,
    uint32_t width, uint32_t height) {
    GrapheneMetalSdl sdl = {0};
    if (!metal_sdl_load(&sdl)) return metal_window_attach_internal(instance_handle,
        (void *)platform_handle, width, height, false, sdl);
    return metal_window_attach_internal(instance_handle, (void *)platform_handle,
        width, height, false, sdl);
}

size_t graphene_metal_window_create(size_t instance_handle, encore_str title,
    uint32_t width, uint32_t height) {
    GrapheneMetalSdl sdl = {0};
    if (!metal_sdl_load(&sdl) || sdl.create_window == NULL) return 0;
    char *name = metal_c_string(title);
    if (name == NULL) return 0;
    void *window = sdl.create_window(name, (int)width, (int)height,
        0x20000000ull | 0x2000ull | 0x20ull);
    free(name);
    if (window == NULL) return 0;
    return metal_window_attach_internal(instance_handle, window, width, height, true, sdl);
}

bool graphene_metal_window_available(size_t handle) { GrapheneMetalWindow *w = (void *)handle; return w != NULL && w->available; }
encore_str graphene_metal_window_error(size_t handle) { GrapheneMetalWindow *w = (void *)handle; return metal_string(w == NULL ? "Invalid Metal window" : w->error); }
bool graphene_metal_window_open(size_t handle) { return graphene_metal_window_available(handle); }
bool graphene_metal_window_poll(size_t handle) { return graphene_metal_window_available(handle); }
uint32_t graphene_metal_window_width(size_t handle) { GrapheneMetalWindow *w = (void *)handle; return w == NULL ? 0 : w->width; }
uint32_t graphene_metal_window_height(size_t handle) { GrapheneMetalWindow *w = (void *)handle; return w == NULL ? 0 : w->height; }
bool graphene_metal_window_key_down(size_t handle, uint32_t scancode) { (void)handle; (void)scancode; return false; }
float graphene_metal_window_mouse_x(size_t handle) { (void)handle; return 0.0f; }
float graphene_metal_window_mouse_y(size_t handle) { (void)handle; return 0.0f; }
float graphene_metal_window_mouse_dx(size_t handle) { (void)handle; return 0.0f; }
float graphene_metal_window_mouse_dy(size_t handle) { (void)handle; return 0.0f; }
bool graphene_metal_window_mouse_down(size_t handle, uint32_t mask) { (void)handle; (void)mask; return false; }

bool graphene_metal_window_destroy(size_t handle) {
    GrapheneMetalWindow *window = (GrapheneMetalWindow *)handle;
    if (window == NULL) return false;
    if (window->view != NULL && window->sdl.destroy_view != NULL)
        window->sdl.destroy_view(window->view);
    if (window->owns_window && window->window != NULL && window->sdl.destroy_window != NULL)
        window->sdl.destroy_window(window->window);
    if (window->sdl.library != NULL) dlclose(window->sdl.library);
    free(window);
    return true;
}

static bool metal_swapchain_resize(GrapheneMetalSwapchain *swapchain,
    uint32_t width, uint32_t height) {
    if (swapchain == NULL || width == 0 || height == 0) return false;
    if (swapchain->width == width && swapchain->height == height &&
        swapchain->depth != nil && (swapchain->offscreen ? swapchain->color != nil : true))
        return true;
    [swapchain->depth release];
    swapchain->depth = nil;
    if (swapchain->offscreen) { [swapchain->color release]; swapchain->color = nil; }
    MTLTextureDescriptor *depth = [[MTLTextureDescriptor alloc] init];
    depth.textureType = MTLTextureType2D;
    depth.pixelFormat = MTLPixelFormatDepth32Float;
    depth.width = width;
    depth.height = height;
    depth.mipmapLevelCount = 1;
    depth.usage = MTLTextureUsageRenderTarget;
    depth.storageMode = MTLStorageModePrivate;
    swapchain->depth = [swapchain->device->device newTextureWithDescriptor:depth];
    [depth release];
    if (swapchain->offscreen) {
        MTLTextureDescriptor *color = [[MTLTextureDescriptor alloc] init];
        color.textureType = MTLTextureType2D;
        color.pixelFormat = MTLPixelFormatBGRA8Unorm;
        color.width = width;
        color.height = height;
        color.mipmapLevelCount = 1;
        color.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
        color.storageMode = MTLStorageModeShared;
        swapchain->color = [swapchain->device->device newTextureWithDescriptor:color];
        [color release];
    }
    swapchain->width = width;
    swapchain->height = height;
    if (swapchain->window != NULL)
        swapchain->window->layer.drawableSize = CGSizeMake(width, height);
    free(swapchain->pixels);
    swapchain->pixels = NULL;
    return swapchain->depth != nil && (!swapchain->offscreen || swapchain->color != nil);
}

static size_t metal_swapchain_create(size_t device_handle, GrapheneMetalWindow *window,
    uint32_t width, uint32_t height, bool offscreen) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)device_handle;
    GrapheneMetalSwapchain *swapchain = calloc(1, sizeof(GrapheneMetalSwapchain));
    if (swapchain == NULL) return 0;
    swapchain->device = device;
    swapchain->window = window;
    swapchain->offscreen = offscreen;
    if (!graphene_metal_device_available(device_handle) ||
        (!offscreen && (window == NULL || !window->available))) {
        metal_error(swapchain->error, sizeof(swapchain->error), "Invalid Metal presentation target");
        return (size_t)swapchain;
    }
    if (!metal_swapchain_resize(swapchain, width, height)) {
        metal_error(swapchain->error, sizeof(swapchain->error), "Metal target allocation failed");
        return (size_t)swapchain;
    }
    swapchain->available = true;
    return (size_t)swapchain;
}

size_t graphene_metal_swapchain_create(size_t device, size_t window) {
    GrapheneMetalWindow *native = (GrapheneMetalWindow *)window;
    return metal_swapchain_create(device, native,
        native == NULL ? 0 : native->width, native == NULL ? 0 : native->height, false);
}

size_t graphene_metal_offscreen_create(size_t device, uint32_t width, uint32_t height) {
    return metal_swapchain_create(device, NULL, width, height, true);
}

size_t graphene_metal_shared_offscreen_create(size_t device, uint32_t width, uint32_t height) {
    return graphene_metal_offscreen_create(device, width, height);
}

bool graphene_metal_swapchain_available(size_t handle) { GrapheneMetalSwapchain *s = (void *)handle; return s != NULL && s->available; }
encore_str graphene_metal_swapchain_error(size_t handle) { GrapheneMetalSwapchain *s = (void *)handle; return metal_string(s == NULL ? "Invalid Metal swapchain" : s->error); }
uint32_t graphene_metal_swapchain_width(size_t handle) { GrapheneMetalSwapchain *s = (void *)handle; return s == NULL ? 0 : s->width; }
uint32_t graphene_metal_swapchain_height(size_t handle) { GrapheneMetalSwapchain *s = (void *)handle; return s == NULL ? 0 : s->height; }
uint32_t graphene_metal_swapchain_frames_in_flight(size_t handle) { return graphene_metal_swapchain_available(handle) ? 2u : 0u; }
uint32_t graphene_metal_swapchain_pitch(size_t handle) { GrapheneMetalSwapchain *s = (void *)handle; return s == NULL ? 0 : s->width * 4u; }
size_t graphene_metal_swapchain_external_image(size_t handle) { GrapheneMetalSwapchain *s = (void *)handle; return s == NULL ? 0 : (size_t)(void *)s->color; }
uint32_t graphene_metal_swapchain_external_layout(size_t handle) { return graphene_metal_swapchain_available(handle) ? 5u : 0u; }

bool graphene_metal_swapchain_set_present_mode(size_t handle, bool immediate) {
    GrapheneMetalSwapchain *swapchain = (GrapheneMetalSwapchain *)handle;
    if (!graphene_metal_swapchain_available(handle)) return false;
    swapchain->immediate = immediate;
    if (swapchain->window != NULL)
        swapchain->window->layer.displaySyncEnabled = immediate ? NO : YES;
    return true;
}

static void metal_frame_end_encoders(GrapheneMetalFrame *frame) {
    if (frame == NULL) return;
    if (frame->render != nil) { [frame->render endEncoding]; frame->render = nil; }
    if (frame->compute != nil) { [frame->compute endEncoding]; frame->compute = nil; }
    if (frame->blit != nil) { [frame->blit endEncoding]; frame->blit = nil; }
}

size_t graphene_metal_frame_acquire(size_t swapchain_handle) {
    GrapheneMetalSwapchain *swapchain = (GrapheneMetalSwapchain *)swapchain_handle;
    GrapheneMetalFrame *frame = calloc(1, sizeof(GrapheneMetalFrame));
    if (frame == NULL) return 0;
    frame->pool = [[NSAutoreleasePool alloc] init];
    frame->swapchain = swapchain;
    if (!graphene_metal_swapchain_available(swapchain_handle)) {
        metal_error(frame->error, sizeof(frame->error), "Invalid Metal swapchain");
        return (size_t)frame;
    }
    if (swapchain->window != NULL) {
        int width = 0, height = 0;
        if (!swapchain->window->sdl.get_size_pixels(swapchain->window->window, &width, &height) ||
            width <= 0 || height <= 0) {
            metal_error(frame->error, sizeof(frame->error), "Metal drawable is temporarily unavailable");
            return (size_t)frame;
        }
        if (!metal_swapchain_resize(swapchain, (uint32_t)width, (uint32_t)height)) {
            metal_error(frame->error, sizeof(frame->error), "Metal drawable resize failed");
            return (size_t)frame;
        }
    }
    frame->slot = swapchain->next_slot % 2u;
    if (swapchain->in_flight[frame->slot] != nil) {
        [swapchain->in_flight[frame->slot] waitUntilCompleted];
        [swapchain->in_flight[frame->slot] release];
        swapchain->in_flight[frame->slot] = nil;
    }
    [swapchain->in_flight_resources[frame->slot] release];
    swapchain->in_flight_resources[frame->slot] = nil;
    if (swapchain->offscreen) {
        frame->color = [swapchain->color retain];
    } else {
        frame->drawable = [swapchain->window->layer.nextDrawable retain];
        frame->color = [frame->drawable.texture retain];
        if (frame->drawable == nil || frame->color == nil) {
            metal_error(frame->error, sizeof(frame->error), "CAMetalLayer returned no drawable");
            return (size_t)frame;
        }
    }
    frame->command_buffer = [swapchain->device->queue commandBuffer];
    [frame->command_buffer retain];
    if (frame->command_buffer == nil) {
        metal_error(frame->error, sizeof(frame->error), "Metal command buffer allocation failed");
        return (size_t)frame;
    }
    frame->available = true;
    return (size_t)frame;
}

bool graphene_metal_frame_available(size_t handle) { GrapheneMetalFrame *f = (void *)handle; return f != NULL && f->available && !f->presented; }
uint32_t graphene_metal_frame_slot(size_t handle) { GrapheneMetalFrame *f = (void *)handle; return f == NULL ? 0 : f->slot; }
encore_str graphene_metal_frame_error(size_t handle) { GrapheneMetalFrame *f = (void *)handle; return metal_string(f == NULL ? "Invalid Metal frame" : f->error); }

static bool metal_bind_render_resources(id<MTLRenderCommandEncoder> encoder,
    GrapheneMetalResourceUsage *resources, GrapheneMetalBindGroup *group) {
    if (encoder == nil) return false;
    if (group == NULL) return true;
    if (!group->available) return false;
    for (uint32_t binding = 0; binding < GRAPHENE_METAL_MAX_BINDINGS; ++binding) {
        GrapheneMetalBuffer *buffer = group->buffers[binding];
        if (buffer == NULL || !buffer->available) continue;
        if (!metal_resource_usage_track_buffer(resources, buffer)) return false;
        [encoder setVertexBuffer:buffer->buffer offset:(NSUInteger)group->offsets[binding] atIndex:binding];
        [encoder setFragmentBuffer:buffer->buffer offset:(NSUInteger)group->offsets[binding] atIndex:binding];
    }
    if (group->texture != NULL) {
        if (!metal_resource_usage_track_texture(resources, group->texture)) return false;
        [encoder setVertexTexture:group->texture->texture atIndex:group->texture_binding];
        [encoder setFragmentTexture:group->texture->texture atIndex:group->texture_binding];
    }
    if (group->sampler != nil) {
        [encoder setVertexSamplerState:group->sampler atIndex:group->texture_binding];
        [encoder setFragmentSamplerState:group->sampler atIndex:group->texture_binding];
    }
    return true;
}

size_t graphene_metal_pipeline_create(size_t device_handle, size_t swapchain_handle,
    size_t vertex_handle, size_t fragment_handle, size_t bind_group_layout,
    bool depth_attachment, bool depth_test, bool depth_write, uint32_t cull_mode,
    bool alpha_blend, bool additive_blend, bool load_color, uint32_t vertex_stride,
    const uint32_t *locations, size_t location_count, const uint32_t *formats,
    size_t format_count, const uint32_t *offsets, size_t offset_count) {
    GrapheneMetalDevice *device = (GrapheneMetalDevice *)device_handle;
    GrapheneMetalShader *vertex = (GrapheneMetalShader *)vertex_handle;
    GrapheneMetalShader *fragment = (GrapheneMetalShader *)fragment_handle;
    GrapheneMetalBindGroupLayout *layout =
        (GrapheneMetalBindGroupLayout *)bind_group_layout;
    GrapheneMetalPipeline *pipeline = calloc(1, sizeof(GrapheneMetalPipeline));
    if (pipeline == NULL) return 0;
    pipeline->owner = device;
    pipeline->depth_attachment = depth_attachment;
    pipeline->load_color = load_color;
    pipeline->cull_mode = cull_mode == 1u ? MTLCullModeFront :
        (cull_mode == 2u ? MTLCullModeBack : MTLCullModeNone);
    if (!graphene_metal_device_available(device_handle) ||
        !graphene_metal_swapchain_available(swapchain_handle) ||
        !graphene_metal_shader_available(vertex_handle) ||
        !graphene_metal_shader_available(fragment_handle) ||
        vertex->owner != device || fragment->owner != device ||
        ((GrapheneMetalSwapchain *)swapchain_handle)->device != device ||
        (layout != NULL && (!layout->available || layout->owner != device)) ||
        vertex->stage != GRAPHENE_METAL_STAGE_VERTEX ||
        fragment->stage != GRAPHENE_METAL_STAGE_FRAGMENT ||
        location_count != format_count || location_count != offset_count) {
        metal_error(pipeline->error, sizeof(pipeline->error), "Invalid Metal render pipeline request");
        return (size_t)pipeline;
    }
    MTLRenderPipelineDescriptor *descriptor = [[MTLRenderPipelineDescriptor alloc] init];
    descriptor.vertexFunction = vertex->function;
    descriptor.fragmentFunction = fragment->function;
    descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    if (alpha_blend || additive_blend) {
        descriptor.colorAttachments[0].blendingEnabled = YES;
        descriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
        descriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
        descriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
        descriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorOne;
        descriptor.colorAttachments[0].destinationRGBBlendFactor = additive_blend
            ? MTLBlendFactorOne : MTLBlendFactorOneMinusSourceAlpha;
        descriptor.colorAttachments[0].destinationAlphaBlendFactor = additive_blend
            ? MTLBlendFactorOne : MTLBlendFactorOneMinusSourceAlpha;
    }
    if (depth_attachment) descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;
    if (location_count > 0) {
        MTLVertexDescriptor *vertex_descriptor = [[MTLVertexDescriptor alloc] init];
        vertex_descriptor.layouts[GRAPHENE_METAL_VERTEX_BUFFER_INDEX].stride = vertex_stride;
        vertex_descriptor.layouts[GRAPHENE_METAL_VERTEX_BUFFER_INDEX].stepFunction = MTLVertexStepFunctionPerVertex;
        for (size_t index = 0; index < location_count; ++index) {
            if (locations[index] >= 31u) continue;
            MTLVertexFormat format = formats[index] == 103u ? MTLVertexFormatFloat2 :
                (formats[index] == 106u ? MTLVertexFormatFloat3 :
                    (formats[index] == 109u ? MTLVertexFormatFloat4 : MTLVertexFormatInvalid));
            vertex_descriptor.attributes[locations[index]].format = format;
            vertex_descriptor.attributes[locations[index]].offset = offsets[index];
            vertex_descriptor.attributes[locations[index]].bufferIndex = GRAPHENE_METAL_VERTEX_BUFFER_INDEX;
        }
        descriptor.vertexDescriptor = vertex_descriptor;
        [vertex_descriptor release];
    }
    NSError *error = nil;
    pipeline->state = [device->device newRenderPipelineStateWithDescriptor:descriptor error:&error];
    [descriptor release];
    if (pipeline->state == nil) {
        snprintf(pipeline->error, sizeof(pipeline->error), "Metal render pipeline failed: %s",
            error == nil ? "unknown error" : error.localizedDescription.UTF8String);
        return (size_t)pipeline;
    }
    if (depth_attachment) {
        MTLDepthStencilDescriptor *depth = [[MTLDepthStencilDescriptor alloc] init];
        depth.depthCompareFunction = depth_test ? MTLCompareFunctionLess : MTLCompareFunctionAlways;
        depth.depthWriteEnabled = depth_write;
        pipeline->depth_state = [device->device newDepthStencilStateWithDescriptor:depth];
        [depth release];
    }
    pipeline->available = true;
    return (size_t)pipeline;
}

bool graphene_metal_pipeline_available(size_t handle) { GrapheneMetalPipeline *p = (void *)handle; return p != NULL && p->available; }
encore_str graphene_metal_pipeline_error(size_t handle) { GrapheneMetalPipeline *p = (void *)handle; return metal_string(p == NULL ? "Invalid Metal render pipeline" : p->error); }
bool graphene_metal_pipeline_destroy(size_t handle) { GrapheneMetalPipeline *p = (void *)handle; if (p == NULL) return false; [p->depth_state release]; [p->state release]; free(p); return true; }

bool graphene_metal_frame_begin_render_pass(size_t handle, size_t pipeline_handle,
    float red, float green, float blue, float alpha) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    GrapheneMetalPipeline *pipeline = (GrapheneMetalPipeline *)pipeline_handle;
    if (!graphene_metal_frame_available(handle) ||
        !graphene_metal_pipeline_available(pipeline_handle) ||
        pipeline->owner != frame->swapchain->device || frame->render != nil) return false;
    if (frame->compute != nil) { [frame->compute endEncoding]; frame->compute = nil; }
    if (frame->blit != nil) { [frame->blit endEncoding]; frame->blit = nil; }
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = frame->color;
    pass.colorAttachments[0].loadAction = pipeline->load_color ? MTLLoadActionLoad : MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(red, green, blue, alpha);
    if (pipeline->depth_attachment) {
        pass.depthAttachment.texture = frame->swapchain->depth;
        pass.depthAttachment.loadAction = MTLLoadActionClear;
        pass.depthAttachment.storeAction = MTLStoreActionDontCare;
        pass.depthAttachment.clearDepth = 1.0;
    }
    frame->render = [frame->command_buffer renderCommandEncoderWithDescriptor:pass];
    if (frame->render == nil) return false;
    frame->pipeline = pipeline;
    [frame->render setRenderPipelineState:pipeline->state];
    if (pipeline->depth_state != nil)
        [frame->render setDepthStencilState:pipeline->depth_state];
    [frame->render setCullMode:pipeline->cull_mode];
    [frame->render setFrontFacingWinding:MTLWindingCounterClockwise];
    [frame->render setViewport:(MTLViewport){0, 0, frame->swapchain->width,
        frame->swapchain->height, 0.0, 1.0}];
    if (!metal_bind_render_resources(frame->render, &frame->resources,
            frame->group)) {
        [frame->render endEncoding];
        frame->render = nil;
        return false;
    }
    return true;
}

bool graphene_metal_frame_bind_group(size_t handle, size_t group_handle) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    GrapheneMetalBindGroup *group = (GrapheneMetalBindGroup *)group_handle;
    if (!graphene_metal_frame_available(handle) ||
        !graphene_metal_bind_group_available(group_handle) ||
        group->owner != frame->swapchain->device) return false;
    frame->group = group;
    if (frame->render != nil &&
        !metal_bind_render_resources(frame->render, &frame->resources, group)) return false;
    if (frame->compute != nil &&
        !metal_bind_compute_resources(frame->compute, &frame->resources, group)) return false;
    return true;
}

bool graphene_metal_frame_bind_pipeline(size_t handle, size_t pipeline_handle) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    GrapheneMetalPipeline *pipeline = (GrapheneMetalPipeline *)pipeline_handle;
    if (!graphene_metal_frame_available(handle) || frame->render == nil ||
        !graphene_metal_pipeline_available(pipeline_handle) ||
        pipeline->owner != frame->swapchain->device) return false;
    frame->pipeline = pipeline;
    [frame->render setRenderPipelineState:pipeline->state];
    if (pipeline->depth_state != nil)
        [frame->render setDepthStencilState:pipeline->depth_state];
    [frame->render setCullMode:pipeline->cull_mode];
    return true;
}

bool graphene_metal_frame_bind_vertex_buffer(size_t handle, size_t buffer_handle) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)buffer_handle;
    if (!graphene_metal_frame_available(handle) || frame->render == nil ||
        !graphene_metal_buffer_available(buffer_handle) ||
        buffer->owner != frame->swapchain->device) return false;
    if (!metal_resource_usage_track_buffer(&frame->resources, buffer)) return false;
    frame->vertex = buffer;
    [frame->render setVertexBuffer:buffer->buffer offset:0 atIndex:GRAPHENE_METAL_VERTEX_BUFFER_INDEX];
    return true;
}

bool graphene_metal_frame_bind_index_buffer(size_t handle, size_t buffer_handle) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)buffer_handle;
    if (!graphene_metal_frame_available(handle) ||
        !graphene_metal_buffer_available(buffer_handle) ||
        buffer->owner != frame->swapchain->device) return false;
    if (!metal_resource_usage_track_buffer(&frame->resources, buffer)) return false;
    frame->index = buffer;
    return true;
}

bool graphene_metal_frame_draw_indexed(size_t handle, uint32_t index_count,
    uint32_t instance_count, uint32_t first_index, int32_t vertex_offset,
    uint32_t first_instance) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    if (!graphene_metal_frame_available(handle) || frame->render == nil ||
        frame->index == NULL || index_count == 0 || instance_count == 0) return false;
    [frame->render drawIndexedPrimitives:MTLPrimitiveTypeTriangle
        indexCount:index_count indexType:MTLIndexTypeUInt32
        indexBuffer:frame->index->buffer indexBufferOffset:(NSUInteger)first_index * 4u
        instanceCount:instance_count baseVertex:vertex_offset baseInstance:first_instance];
    return true;
}

bool graphene_metal_frame_end_render_pass(size_t handle) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    if (!graphene_metal_frame_available(handle) || frame->render == nil) return false;
    [frame->render endEncoding];
    frame->render = nil;
    return true;
}

bool graphene_metal_frame_set_viewport(size_t handle, uint32_t x, uint32_t y,
    uint32_t width, uint32_t height) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    if (!graphene_metal_frame_available(handle) || frame->render == nil || width == 0 || height == 0)
        return false;
    [frame->render setViewport:(MTLViewport){x, y, width, height, 0.0, 1.0}];
    [frame->render setScissorRect:(MTLScissorRect){x, y, width, height}];
    return true;
}

bool graphene_metal_frame_dispatch_compute(size_t handle, size_t pipeline_handle,
    size_t group_handle, uint32_t x, uint32_t y, uint32_t z) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    GrapheneMetalComputePipeline *pipeline = (GrapheneMetalComputePipeline *)pipeline_handle;
    GrapheneMetalBindGroup *group = (GrapheneMetalBindGroup *)group_handle;
    if (!graphene_metal_frame_available(handle) ||
        !graphene_metal_compute_pipeline_available(pipeline_handle) ||
        !graphene_metal_bind_group_available(group_handle) ||
        pipeline->owner != frame->swapchain->device ||
        group->owner != frame->swapchain->device || x == 0 || y == 0 || z == 0)
        return false;
    if (frame->render != nil) { [frame->render endEncoding]; frame->render = nil; }
    if (frame->blit != nil) { [frame->blit endEncoding]; frame->blit = nil; }
    if (frame->compute == nil) frame->compute = [frame->command_buffer computeCommandEncoder];
    [frame->compute setComputePipelineState:pipeline->state];
    if (!metal_bind_compute_resources(frame->compute, &frame->resources, group)) return false;
    [frame->compute dispatchThreadgroups:MTLSizeMake(x, y, z)
        threadsPerThreadgroup:MTLSizeMake(pipeline->threadgroup_x, 1, 1)];
    frame->compute_pipeline = pipeline;
    frame->group = group;
    return true;
}

bool graphene_metal_frame_compute_barrier(size_t handle) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    if (!graphene_metal_frame_available(handle)) return false;
    if (frame->compute != nil) { [frame->compute endEncoding]; frame->compute = nil; }
    return true;
}
bool graphene_metal_frame_transfer_storage_barrier(size_t handle) { return graphene_metal_frame_compute_barrier(handle); }
bool graphene_metal_frame_fragment_storage_barrier(size_t handle) { return graphene_metal_frame_compute_barrier(handle); }

bool graphene_metal_frame_fill_buffer(size_t handle, size_t buffer_handle,
    uint64_t offset, uint64_t size, uint32_t value) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    GrapheneMetalBuffer *buffer = (GrapheneMetalBuffer *)buffer_handle;
    if (!graphene_metal_frame_available(handle) ||
        !graphene_metal_buffer_available(buffer_handle) ||
        buffer->owner != frame->swapchain->device || (buffer->usage & 2u) == 0 ||
        size == 0 || (offset & 3u) != 0 || (size & 3u) != 0 ||
        offset > buffer->size || size > buffer->size - offset) return false;
    if (!metal_resource_usage_track_buffer(&frame->resources, buffer)) return false;
    if (frame->render != nil) { [frame->render endEncoding]; frame->render = nil; }
    if (frame->compute != nil) { [frame->compute endEncoding]; frame->compute = nil; }
    if (frame->blit == nil) frame->blit = [frame->command_buffer blitCommandEncoder];
    uint8_t byte = (uint8_t)value;
    uint32_t repeated = (uint32_t)byte * 0x01010101u;
    if (value == repeated) {
        [frame->blit fillBuffer:buffer->buffer
            range:NSMakeRange((NSUInteger)offset, (NSUInteger)size) value:byte];
    } else {
        id<MTLBuffer> staging = [frame->swapchain->device->device
            newBufferWithLength:(NSUInteger)size options:MTLResourceStorageModeShared];
        if (staging == nil) return false;
        uint32_t *words = staging.contents;
        for (uint64_t index = 0; index < size / 4u; ++index) words[index] = value;
        if (frame->transient_resources == nil)
            frame->transient_resources = [[NSMutableArray alloc] init];
        [frame->transient_resources addObject:staging];
        [staging release];
        [frame->blit copyFromBuffer:staging sourceOffset:0 toBuffer:buffer->buffer
            destinationOffset:(NSUInteger)offset size:(NSUInteger)size];
    }
    return true;
}

bool graphene_metal_frame_copy_buffer(size_t handle, size_t source_handle,
    size_t destination_handle, uint64_t source_offset, uint64_t destination_offset,
    uint64_t size) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    GrapheneMetalBuffer *source = (GrapheneMetalBuffer *)source_handle;
    GrapheneMetalBuffer *destination = (GrapheneMetalBuffer *)destination_handle;
    if (!graphene_metal_frame_available(handle) || !graphene_metal_buffer_available(source_handle) ||
        !graphene_metal_buffer_available(destination_handle) ||
        source->owner != frame->swapchain->device || destination->owner != frame->swapchain->device ||
        (source->usage & 1u) == 0 || (destination->usage & 2u) == 0 || size == 0 ||
        source_offset > source->size ||
        size > source->size - source_offset || destination_offset > destination->size ||
        size > destination->size - destination_offset) return false;
    if (!metal_resource_usage_track_buffer(&frame->resources, source) ||
        !metal_resource_usage_track_buffer(&frame->resources, destination)) return false;
    if (frame->render != nil) { [frame->render endEncoding]; frame->render = nil; }
    if (frame->compute != nil) { [frame->compute endEncoding]; frame->compute = nil; }
    if (frame->blit == nil) frame->blit = [frame->command_buffer blitCommandEncoder];
    [frame->blit copyFromBuffer:source->buffer sourceOffset:(NSUInteger)source_offset
        toBuffer:destination->buffer destinationOffset:(NSUInteger)destination_offset
        size:(NSUInteger)size];
    return true;
}

bool graphene_metal_frame_present(size_t handle) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    if (!graphene_metal_frame_available(handle)) return false;
    metal_frame_end_encoders(frame);
    metal_resource_usage_submit(&frame->resources, frame->command_buffer);
    if (frame->drawable != nil) [frame->command_buffer presentDrawable:frame->drawable];
    [frame->command_buffer commit];
    frame->swapchain->in_flight[frame->slot] = [frame->command_buffer retain];
    frame->swapchain->in_flight_resources[frame->slot] = frame->transient_resources;
    frame->transient_resources = nil;
    frame->swapchain->next_slot = (frame->slot + 1u) % 2u;
    frame->presented = true;
    if (metal_trace_enabled())
        fprintf(stderr, "Graphene Metal: commit slot=%u present=%s\n",
            frame->slot, frame->drawable == nil ? "offscreen" : "drawable");
    return true;
}

bool graphene_metal_frame_destroy(size_t handle) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)handle;
    if (frame == NULL) return false;
    if (!frame->presented) metal_frame_end_encoders(frame);
    [frame->command_buffer release];
    [frame->transient_resources release];
    [frame->color release];
    [frame->drawable release];
    [frame->pool drain];
    metal_resource_usage_destroy(&frame->resources);
    free(frame);
    return true;
}

bool graphene_metal_swapchain_last_frame_complete(size_t handle) {
    GrapheneMetalSwapchain *swapchain = (GrapheneMetalSwapchain *)handle;
    if (!graphene_metal_swapchain_available(handle)) return false;
    for (size_t index = 0; index < 2; ++index) {
        id<MTLCommandBuffer> buffer = swapchain->in_flight[index];
        if (buffer != nil && buffer.status != MTLCommandBufferStatusCompleted &&
            buffer.status != MTLCommandBufferStatusError) return false;
    }
    return true;
}

bool graphene_metal_swapchain_destroy_retired(size_t handle) {
    GrapheneMetalSwapchain *swapchain = (GrapheneMetalSwapchain *)handle;
    if (!graphene_metal_swapchain_available(handle)) return false;
    for (size_t index = 0; index < 2; ++index) {
        id<MTLCommandBuffer> buffer = swapchain->in_flight[index];
        if (buffer != nil && (buffer.status == MTLCommandBufferStatusCompleted ||
            buffer.status == MTLCommandBufferStatusError)) {
            [buffer release];
            swapchain->in_flight[index] = nil;
            [swapchain->in_flight_resources[index] release];
            swapchain->in_flight_resources[index] = nil;
        }
    }
    return true;
}

size_t graphene_metal_swapchain_pixels(size_t handle) {
    GrapheneMetalSwapchain *swapchain = (GrapheneMetalSwapchain *)handle;
    if (!graphene_metal_swapchain_available(handle) || !swapchain->offscreen ||
        swapchain->color == nil) return 0;
    for (size_t index = 0; index < 2; ++index)
        if (swapchain->in_flight[index] != nil) [swapchain->in_flight[index] waitUntilCompleted];
    size_t bytes = (size_t)swapchain->width * swapchain->height * 4u;
    if (swapchain->pixels == NULL) swapchain->pixels = malloc(bytes);
    if (swapchain->pixels == NULL) return 0;
    [swapchain->color getBytes:swapchain->pixels bytesPerRow:swapchain->width * 4u
        fromRegion:MTLRegionMake2D(0, 0, swapchain->width, swapchain->height) mipmapLevel:0];
    return (size_t)swapchain->pixels;
}

bool graphene_metal_swapchain_clear(size_t handle, float red, float green,
    float blue, float alpha) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)graphene_metal_frame_acquire(handle);
    if (frame == NULL || !frame->available) { graphene_metal_frame_destroy((size_t)frame); return false; }
    MTLRenderPassDescriptor *pass = [MTLRenderPassDescriptor renderPassDescriptor];
    pass.colorAttachments[0].texture = frame->color;
    pass.colorAttachments[0].loadAction = MTLLoadActionClear;
    pass.colorAttachments[0].storeAction = MTLStoreActionStore;
    pass.colorAttachments[0].clearColor = MTLClearColorMake(red, green, blue, alpha);
    id<MTLRenderCommandEncoder> encoder = [frame->command_buffer renderCommandEncoderWithDescriptor:pass];
    [encoder endEncoding];
    bool result = graphene_metal_frame_present((size_t)frame);
    graphene_metal_frame_destroy((size_t)frame);
    return result;
}

bool graphene_metal_swapchain_draw_pipeline(size_t handle, size_t pipeline,
    float red, float green, float blue, float alpha) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)graphene_metal_frame_acquire(handle);
    if (frame == NULL || !frame->available) { graphene_metal_frame_destroy((size_t)frame); return false; }
    bool result = graphene_metal_frame_begin_render_pass((size_t)frame, pipeline, red, green, blue, alpha);
    if (result) [frame->render drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:3];
    if (result) result = graphene_metal_frame_end_render_pass((size_t)frame);
    if (result) result = graphene_metal_frame_present((size_t)frame);
    graphene_metal_frame_destroy((size_t)frame);
    return result;
}

bool graphene_metal_swapchain_draw_indexed_bound(size_t handle, size_t pipeline,
    size_t vertex, size_t index, size_t bind_group, uint32_t index_count,
    float red, float green, float blue, float alpha) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)graphene_metal_frame_acquire(handle);
    if (frame == NULL || !frame->available) { graphene_metal_frame_destroy((size_t)frame); return false; }
    bool result = graphene_metal_frame_bind_group((size_t)frame, bind_group) &&
        graphene_metal_frame_begin_render_pass((size_t)frame, pipeline, red, green, blue, alpha) &&
        graphene_metal_frame_bind_vertex_buffer((size_t)frame, vertex) &&
        graphene_metal_frame_bind_index_buffer((size_t)frame, index) &&
        graphene_metal_frame_draw_indexed((size_t)frame, index_count, 1, 0, 0, 0) &&
        graphene_metal_frame_end_render_pass((size_t)frame) &&
        graphene_metal_frame_present((size_t)frame);
    graphene_metal_frame_destroy((size_t)frame);
    return result;
}

bool graphene_metal_swapchain_draw_indexed(size_t handle, size_t pipeline,
    size_t vertex, size_t index, uint32_t index_count,
    float red, float green, float blue, float alpha) {
    GrapheneMetalFrame *frame = (GrapheneMetalFrame *)graphene_metal_frame_acquire(handle);
    if (frame == NULL || !frame->available) { graphene_metal_frame_destroy((size_t)frame); return false; }
    bool result = graphene_metal_frame_begin_render_pass((size_t)frame, pipeline, red, green, blue, alpha) &&
        graphene_metal_frame_bind_vertex_buffer((size_t)frame, vertex) &&
        graphene_metal_frame_bind_index_buffer((size_t)frame, index) &&
        graphene_metal_frame_draw_indexed((size_t)frame, index_count, 1, 0, 0, 0) &&
        graphene_metal_frame_end_render_pass((size_t)frame) &&
        graphene_metal_frame_present((size_t)frame);
    graphene_metal_frame_destroy((size_t)frame);
    return result;
}

bool graphene_metal_swapchain_destroy(size_t handle) {
    GrapheneMetalSwapchain *swapchain = (GrapheneMetalSwapchain *)handle;
    if (swapchain == NULL) return false;
    for (size_t index = 0; index < 2; ++index) {
        if (swapchain->in_flight[index] != nil) {
            [swapchain->in_flight[index] waitUntilCompleted];
            [swapchain->in_flight[index] release];
        }
        [swapchain->in_flight_resources[index] release];
    }
    [swapchain->depth release];
    [swapchain->color release];
    free(swapchain->pixels);
    free(swapchain);
    return true;
}
