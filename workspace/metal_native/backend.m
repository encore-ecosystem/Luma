#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>
#import <dispatch/dispatch.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { size_t ref_count; size_t len; char data[]; } encore_str_object;
typedef struct { encore_str_object *object; } encore_str;
extern void *encore_str_from_cstr(const char *value);

typedef struct {
    id<MTLDevice> device;
    bool available;
    bool window_support;
    bool borrowed;
    char error[256];
} GrapheneMetalContext;

static encore_str metal_string(const char *value) {
    encore_str result = {
        (encore_str_object *)encore_str_from_cstr(value == NULL ? "" : value)
    };
    return result;
}

static void metal_error(char *destination, size_t capacity, const char *value) {
    if (destination == NULL || capacity == 0) return;
    snprintf(destination, capacity, "%s", value == NULL ? "Unknown Metal error" : value);
}

static bool metal_trace_enabled(void) {
    return getenv("GRAPHENE_METAL_TRACE") != NULL;
}

static uint32_t metal_api_version(void) {
    return (3u << 22) | (1u << 12);
}

size_t graphene_metal_create(bool debug, bool window_support, bool external_window) {
    (void)debug;
    (void)external_window;
    GrapheneMetalContext *context = calloc(1, sizeof(GrapheneMetalContext));
    if (context == NULL) return 0;
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
    context->window_support = window_support;
    context->device = [MTLCreateSystemDefaultDevice() retain];
    if (context->device == nil) {
        metal_error(context->error, sizeof(context->error),
            "Metal is unavailable: MTLCreateSystemDefaultDevice returned nil");
        [pool drain];
        return (size_t)context;
    }
    context->available = true;
    if (metal_trace_enabled()) {
        fprintf(stderr, "Graphene Metal: adapter=%s\n",
            context->device.name.UTF8String);
    }
    [pool drain];
    return (size_t)context;
}

size_t graphene_metal_borrow(size_t instance, size_t physical_device) {
    id<MTLDevice> device = (id<MTLDevice>)(void *)physical_device;
    GrapheneMetalContext *source = (GrapheneMetalContext *)instance;
    if (device == nil && source != NULL) device = source->device;
    GrapheneMetalContext *context = calloc(1, sizeof(GrapheneMetalContext));
    if (context == NULL) return 0;
    context->device = [device retain];
    context->borrowed = true;
    context->available = device != nil;
    if (!context->available) {
        metal_error(context->error, sizeof(context->error),
            "Cannot borrow an empty Metal device");
    }
    return (size_t)context;
}

bool graphene_metal_destroy(size_t handle) {
    GrapheneMetalContext *context = (GrapheneMetalContext *)handle;
    if (context == NULL) return false;
    [context->device release];
    free(context);
    return true;
}

bool graphene_metal_available(size_t handle) {
    GrapheneMetalContext *context = (GrapheneMetalContext *)handle;
    return context != NULL && context->available && context->device != nil;
}

encore_str graphene_metal_error(size_t handle) {
    GrapheneMetalContext *context = (GrapheneMetalContext *)handle;
    return metal_string(context == NULL ? "Invalid Metal context" : context->error);
}

uint32_t graphene_metal_api_version(size_t handle) {
    return graphene_metal_available(handle) ? metal_api_version() : 0u;
}

size_t graphene_metal_instance(size_t handle) {
    return graphene_metal_available(handle) ? handle : 0;
}

size_t graphene_metal_physical_device(size_t handle, size_t index) {
    GrapheneMetalContext *context = (GrapheneMetalContext *)handle;
    return context != NULL && context->available && index == 0
        ? (size_t)(void *)context->device : 0;
}

size_t graphene_metal_adapter_count(size_t handle) {
    return graphene_metal_available(handle) ? 1 : 0;
}

encore_str graphene_metal_adapter_name(size_t handle, size_t index) {
    GrapheneMetalContext *context = (GrapheneMetalContext *)handle;
    if (context == NULL || !context->available || index != 0)
        return metal_string("");
    return metal_string(context->device.name.UTF8String);
}

uint32_t graphene_metal_adapter_vendor(size_t handle, size_t index) {
    return graphene_metal_available(handle) && index == 0 ? 0x106bu : 0u;
}

uint32_t graphene_metal_adapter_device(size_t handle, size_t index) {
    return graphene_metal_available(handle) && index == 0 ? 1u : 0u;
}

uint32_t graphene_metal_adapter_kind(size_t handle, size_t index) {
    return graphene_metal_available(handle) && index == 0 ? 1u : 0u;
}

uint32_t graphene_metal_adapter_api_version(size_t handle, size_t index) {
    return graphene_metal_available(handle) && index == 0 ? metal_api_version() : 0u;
}

uint32_t graphene_metal_adapter_tier(size_t handle, size_t index) {
    return graphene_metal_available(handle) && index == 0 ? 1u : 0u;
}

bool graphene_metal_adapter_ray_query(size_t handle, size_t index) {
    (void)handle; (void)index; return false;
}

bool graphene_metal_adapter_ray_pipeline(size_t handle, size_t index) {
    (void)handle; (void)index; return false;
}

bool graphene_metal_adapter_mesh_shader(size_t handle, size_t index) {
    (void)handle; (void)index; return false;
}

#include "resources.inc"
#include "presentation.inc"
#include "ui_compositor.inc"

#include "unsupported.inc"
