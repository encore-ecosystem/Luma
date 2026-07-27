#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/* windows.h maps CreateWindow to CreateWindowA/CreateWindowW.  The SDL
 * dispatch table deliberately uses the backend-neutral SDL symbol name. */
#ifdef CreateWindow
#undef CreateWindow
#endif
#else
#include <dlfcn.h>
#include <signal.h>
#include <sys/stat.h>
#endif

typedef struct { size_t ref_count; size_t len; char data[]; } encore_str_object;
typedef struct { encore_str_object *object; } encore_str;
extern void *encore_str_from_cstr(const char *value);
extern bool luma_vk_ui_begin(size_t handle, uint32_t clear_color);
extern bool luma_vk_ui_cache_begin(size_t handle, uint32_t clear_color);
extern bool luma_vk_ui_cache_end(size_t handle);
extern bool luma_vk_ui_cached_begin(size_t handle, uint32_t clear_color);
extern bool luma_vk_ui_cache_ready(size_t handle);
extern bool luma_vk_ui_set_logical_extent(size_t handle, uint32_t width, uint32_t height);
extern bool luma_vk_ui_set_present_mode(size_t handle, uint32_t mode);
extern bool luma_vk_ui_geometry(size_t handle, const float *vertices, size_t vertex_count,
    size_t vertex_stride_floats, size_t color_offset_floats,
    const int *indices, size_t index_count);
extern bool luma_vk_ui_clip(size_t handle, bool enabled, int32_t x, int32_t y,
    uint32_t width, uint32_t height);
extern bool luma_vk_ui_end(size_t handle);
extern size_t luma_vk_ui_frame_acquire(size_t handle);
extern bool luma_vk_ui_frame_finish(size_t handle, size_t frame);
extern bool luma_vk_ui_composition_split(size_t handle);
extern size_t luma_vk_ui_texture_create(size_t handle, const void *pixels,
    uint32_t width, uint32_t height, uint32_t pitch, uint32_t format);
extern size_t luma_vk_ui_texture_create_empty(size_t handle, uint32_t width,
    uint32_t height, uint32_t format);
extern bool luma_vk_ui_texture_update(size_t handle, const void *pixels,
    uint32_t width, uint32_t height, uint32_t pitch, uint32_t x, uint32_t y);
extern bool luma_vk_ui_texture_destroy(size_t handle);
extern bool luma_vk_ui_texture_draw(size_t handle, size_t texture,
    float x, float y, float width, float height);
extern bool luma_vk_ui_texture_draw_region(size_t handle, size_t texture,
    float x, float y, float width, float height, uint32_t source_x,
    uint32_t source_y, uint32_t source_width, uint32_t source_height);
extern size_t luma_vk_ui_external_texture_create(size_t handle, uint64_t image,
    uint32_t width, uint32_t height, uint32_t layout);

typedef struct UiSdlWindow UiSdlWindow;
typedef struct UiSdlRenderer UiSdlRenderer;
typedef struct UiSdlTexture UiSdlTexture;
typedef uint32_t UiSdlPropertiesID;
typedef struct UiSdlSurface {
    uint32_t flags;
    uint32_t format;
    int width;
    int height;
    int pitch;
    void *pixels;
    int refcount;
    void *reserved;
} UiSdlSurface;
typedef struct UiTtfFont UiTtfFont;
typedef struct { float x, y, w, h; } UiSdlFRect;
typedef struct { float x, y; } UiSdlFPoint;
typedef struct { int x, y, w, h; } UiSdlRect;
typedef struct { uint8_t r, g, b, a; } UiSdlColor;
typedef struct { float r, g, b, a; } UiSdlFColor;
typedef struct { UiSdlFPoint position; UiSdlFColor color; UiSdlFPoint tex_coord; } UiSdlVertex;

bool luma_vector_line(size_t handle, float x0, float y0, float x1, float y1,
    float width, uint32_t color);

typedef union {
    uint32_t type;
    uint8_t padding[128];
} UiSdlEvent;

typedef struct {
    UiTtfFont *font;
    float size;
    uint32_t weight;
    size_t face;
    uint64_t used_at;
} UiFontVariant;

typedef struct {
    char *family;
    char *path;
} UiFontFace;

typedef struct {
    UiSdlTexture *texture;
    char *text;
    size_t text_length;
    size_t face;
    float font_size;
    uint32_t font_weight;
    uint32_t color;
    int wrap_width;
    int width;
    int height;
    size_t bytes;
    uint64_t used_at;
    int atlas_x;
    int atlas_y;
    uint32_t atlas_generation;
    bool owns_texture;
} UiTextCacheEntry;

typedef struct {
    UiSdlTexture *texture;
    uint32_t format;
    int cursor_x;
    int cursor_y;
    int row_height;
    uint32_t generation;
} UiTextAtlasPage;

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t timestamp;
    uint32_t window_id;
    uint32_t which;
    uint32_t state;
    float x;
    float y;
    float xrel;
    float yrel;
} UiSdlMouseMotionEvent;

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t timestamp;
    uint32_t window_id;
    uint32_t which;
    uint8_t button;
    bool down;
    uint8_t clicks;
    uint8_t padding;
    float x;
    float y;
} UiSdlMouseButtonEvent;

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t timestamp;
    uint32_t window_id;
    uint32_t which;
    float x;
    float y;
    uint32_t direction;
    float mouse_x;
    float mouse_y;
} UiSdlMouseWheelEvent;

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t timestamp;
    uint32_t window_id;
    uint32_t which;
    uint32_t scancode;
    uint32_t key;
    uint16_t mod;
    uint16_t raw;
    bool down;
    bool repeat;
} UiSdlKeyboardEvent;

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t timestamp;
    uint32_t window_id;
    const char *text;
} UiSdlTextInputEvent;

typedef struct {
    uint32_t type;
    uint32_t reserved;
    uint64_t timestamp;
    uint32_t window_id;
    int32_t data1;
    int32_t data2;
} UiSdlWindowEvent;

enum {
    UI_SDL_INIT_VIDEO = 0x00000020u,
    UI_SDL_EVENT_QUIT = 0x100,
    UI_SDL_EVENT_WINDOW_CLOSE = 0x210,
    UI_SDL_EVENT_WINDOW_EXPOSED = 0x204,
    UI_SDL_EVENT_WINDOW_RESIZED = 0x206,
    UI_SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED = 0x207,
    UI_SDL_EVENT_KEY_DOWN = 0x300,
    UI_SDL_EVENT_KEY_UP = 0x301,
    UI_SDL_EVENT_TEXT_INPUT = 0x303,
    UI_SDL_EVENT_MOUSE_MOTION = 0x400,
    UI_SDL_EVENT_MOUSE_BUTTON_DOWN = 0x401,
    UI_SDL_EVENT_MOUSE_BUTTON_UP = 0x402,
    UI_SDL_EVENT_MOUSE_WHEEL = 0x403
};

enum {
    UI_EVENT_NONE = 0,
    UI_EVENT_CLOSE = 1,
    UI_EVENT_KEY_DOWN = 2,
    UI_EVENT_KEY_UP = 3,
    UI_EVENT_POINTER_DOWN = 4,
    UI_EVENT_POINTER_UP = 5,
    UI_EVENT_POINTER_MOVE = 6,
    UI_EVENT_SCROLL = 7,
    UI_EVENT_TEXT_INPUT = 8,
    UI_EVENT_RESIZE = 9
};

typedef struct {
    void *library;
    bool attempted;
    bool ready;
    bool (*Init)(uint32_t);
    void (*Quit)(void);
    UiSdlWindow *(*CreateWindow)(const char *, int, int, uint64_t);
    bool (*CreateWindowAndRenderer)(const char *, int, int, uint64_t, UiSdlWindow **, UiSdlRenderer **);
    UiSdlRenderer *(*CreateRenderer)(UiSdlWindow *, const char *);
    UiSdlPropertiesID (*CreateProperties)(void);
    void (*DestroyProperties)(UiSdlPropertiesID);
    bool (*SetNumberProperty)(UiSdlPropertiesID, const char *, int64_t);
    void *(*GetPointerProperty)(UiSdlPropertiesID, const char *, void *);
    int64_t (*GetNumberProperty)(UiSdlPropertiesID, const char *, int64_t);
    UiSdlPropertiesID (*GetRendererProperties)(UiSdlRenderer *);
    UiSdlSurface *(*CreateSurface)(int, int, uint32_t);
    UiSdlRenderer *(*CreateSoftwareRenderer)(UiSdlSurface *);
    const char *(*GetRendererName)(UiSdlRenderer *);
    bool (*ShowWindow)(UiSdlWindow *);
    bool (*SetWindowOpacity)(UiSdlWindow *, float);
    bool (*SyncWindow)(UiSdlWindow *);
    bool (*GetWindowSize)(UiSdlWindow *, int *, int *);
    float (*GetWindowPixelDensity)(UiSdlWindow *);
    uint32_t (*GetDisplayForWindow)(UiSdlWindow *);
    float (*GetDisplayContentScale)(uint32_t);
    bool (*GetWindowSizeInPixels)(UiSdlWindow *, int *, int *);
    void (*DestroyRenderer)(UiSdlRenderer *);
    void (*DestroyWindow)(UiSdlWindow *);
    bool (*PollEvent)(UiSdlEvent *);
    bool (*WaitEventTimeout)(UiSdlEvent *, int);
    bool (*StartTextInput)(UiSdlWindow *);
    bool (*StopTextInput)(UiSdlWindow *);
    bool (*SetClipboardText)(const char *);
    char *(*GetClipboardText)(void);
    void (*Free)(void *);
    bool (*SetRenderDrawColor)(UiSdlRenderer *, uint8_t, uint8_t, uint8_t, uint8_t);
    bool (*SetRenderDrawBlendMode)(UiSdlRenderer *, uint32_t);
    bool (*SetRenderClipRect)(UiSdlRenderer *, const UiSdlRect *);
    bool (*SetRenderScale)(UiSdlRenderer *, float, float);
    bool (*RenderClear)(UiSdlRenderer *);
    bool (*RenderFillRect)(UiSdlRenderer *, const UiSdlFRect *);
    bool (*RenderRect)(UiSdlRenderer *, const UiSdlFRect *);
    bool (*RenderLine)(UiSdlRenderer *, float, float, float, float);
    bool (*RenderGeometry)(UiSdlRenderer *, UiSdlTexture *, const UiSdlVertex *, int, const int *, int);
    bool (*RenderDebugText)(UiSdlRenderer *, float, float, const char *);
    UiSdlTexture *(*CreateTextureFromSurface)(UiSdlRenderer *, UiSdlSurface *);
    UiSdlTexture *(*CreateTexture)(UiSdlRenderer *, uint32_t, int, int, int);
    UiSdlTexture *(*CreateTextureWithProperties)(UiSdlRenderer *, UiSdlPropertiesID);
    bool (*UpdateTexture)(UiSdlTexture *, const UiSdlRect *, const void *, int);
    bool (*SetTextureBlendMode)(UiSdlTexture *, uint32_t);
    bool (*SetRenderTarget)(UiSdlRenderer *, UiSdlTexture *);
    bool (*RenderTexture)(UiSdlRenderer *, UiSdlTexture *, const UiSdlFRect *, const UiSdlFRect *);
    UiSdlSurface *(*RenderReadPixels)(UiSdlRenderer *, const UiSdlRect *);
    bool (*ReadSurfacePixel)(UiSdlSurface *, int, int, uint8_t *, uint8_t *, uint8_t *, uint8_t *);
    void (*DestroyTexture)(UiSdlTexture *);
    void (*DestroySurface)(UiSdlSurface *);
    bool (*RenderPresent)(UiSdlRenderer *);
    bool (*SetRenderVSync)(UiSdlRenderer *, int);
    const char *(*GetError)(void);
} UiSdlApi;

typedef struct {
    void *library;
    bool attempted;
    bool ready;
    bool (*Init)(void);
    void (*Quit)(void);
    UiTtfFont *(*OpenFont)(const char *, float);
    void (*CloseFont)(UiTtfFont *);
    void (*SetFontStyle)(UiTtfFont *, uint32_t);
    bool (*SetFontSize)(UiTtfFont *, float);
    bool (*GetStringSize)(UiTtfFont *, const char *, size_t, int *, int *);
    bool (*GetStringSizeWrapped)(UiTtfFont *, const char *, size_t, int, int *, int *);
    bool (*MeasureString)(UiTtfFont *, const char *, size_t, int, int *, size_t *);
    UiSdlSurface *(*RenderTextBlended)(UiTtfFont *, const char *, size_t, UiSdlColor);
    UiSdlSurface *(*RenderTextBlendedWrapped)(UiTtfFont *, const char *, size_t, UiSdlColor, int);
} UiTtfApi;

typedef struct {
    UiSdlWindow *window;
    UiSdlRenderer *renderer;
    uint32_t renderer_kind;
    size_t direct_compositor;
    size_t borrowed_frame;
    bool open;
    bool reveal_pending;
    bool transparent_mapped;
    bool first_frame_pending;
    uint64_t present_count;
    bool text_input_started;
    uint32_t event_kind;
    float event_x;
    float event_y;
    float wheel_x;
    float wheel_y;
    uint32_t event_key;
    uint32_t event_modifiers;
    char event_text[32];
    uint32_t width;
    uint32_t height;
    float pixel_density;
    UiTtfFont *font;
    float font_size;
    uint32_t font_weight;
    UiFontFace font_faces[32];
    size_t font_face_count;
    size_t current_face;
    UiFontVariant font_variants[32];
    size_t font_variant_count;
    uint64_t font_tick;
    UiTextCacheEntry text_cache[512];
    size_t text_cache_count;
    size_t text_cache_bytes;
    uint64_t text_cache_tick;
    UiTextAtlasPage text_atlas[4];
    size_t text_atlas_count;
    uint32_t text_atlas_generation;
    UiSdlFPoint vector_path[4096];
    size_t vector_path_count;
    UiSdlVertex *vector_vertices;
    size_t vector_vertex_capacity;
    int *vector_indices;
    size_t vector_index_capacity;
    UiSdlSurface *readback_surface;
    UiSdlTexture *layout_texture;
    UiSdlTexture *overlay_texture;
    UiSdlTexture *external_texture;
    UiSdlTexture *external_textures[4];
    uint64_t external_images[4];
    uint32_t external_widths[4];
    uint32_t external_heights[4];
    uint32_t external_layouts[4];
    uint32_t external_texture_next;
    uint32_t layout_width;
    uint32_t layout_height;
    uint32_t overlay_width;
    uint32_t overlay_height;
    bool overlay_valid;
    UiSdlEvent pending_event;
    bool has_pending_event;
} UiWindow;

static UiSdlApi g_sdl;
static UiTtfApi g_ttf;
static char g_error[256];
static size_t g_window_count;
static UiWindow *g_windows[64];
#if !defined(_WIN32)
static volatile sig_atomic_t g_terminate_requested;
static void ui_termination_signal(int signal_number) {
    (void)signal_number;
    g_terminate_requested = 1;
}
#endif

static encore_str ui_string(const char *value) {
    encore_str result = {(encore_str_object *)encore_str_from_cstr(value == NULL ? "" : value)};
    return result;
}

static char *ui_to_cstr(encore_str value) {
    size_t len = value.object == NULL ? 0 : value.object->len;
    char *result = (char *)malloc(len + 1);
    if (result == NULL) return NULL;
    if (len > 0) memcpy(result, value.object->data, len);
    result[len] = '\0';
    return result;
}

static char *ui_copy_cstr(const char *value) {
    size_t length = strlen(value);
    char *result = (char *)malloc(length + 1);
    if (result != NULL) memcpy(result, value, length + 1);
    return result;
}

bool luma_path_is_directory(encore_str path) {
    char *value = ui_to_cstr(path);
    if (value == NULL) return false;
#if defined(_WIN32)
    DWORD attributes = GetFileAttributesA(value);
    bool result = attributes != INVALID_FILE_ATTRIBUTES &&
        (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
#else
    struct stat info;
    bool result = stat(value, &info) == 0 && S_ISDIR(info.st_mode);
#endif
    free(value);
    return result;
}

static bool ui_str_equals_cstr(encore_str value, const char *text) {
    size_t length = value.object == NULL ? 0 : value.object->len;
    size_t text_length = text == NULL ? 0 : strlen(text);
    return length == text_length && (length == 0 || memcmp(value.object->data, text, length) == 0);
}

static char *ui_copy_text(const char *value, size_t length) {
    char *result = (char *)malloc(length + 1);
    if (result == NULL) return NULL;
    if (length > 0) memcpy(result, value, length);
    result[length] = '\0';
    return result;
}

static void ui_set_error(const char *message) {
    const char *value = message == NULL ? "unknown SDL3 error" : message;
    size_t len = strlen(value);
    if (len >= sizeof(g_error)) len = sizeof(g_error) - 1;
    memcpy(g_error, value, len);
    g_error[len] = '\0';
}

void luma_backend_set_error(encore_str message) {
    char *value = ui_to_cstr(message);
    ui_set_error(value == NULL ? "Direct Vulkan initialization failed" : value);
    free(value);
}

#if defined(_WIN32)
static void *ui_open_library(void) {
    const char *names[] = {"SDL3.dll", NULL};
    for (size_t i = 0; names[i] != NULL; ++i) {
        HMODULE lib = LoadLibraryA(names[i]);
        if (lib != NULL) return (void *)lib;
    }
    return NULL;
}
static void *ui_open_ttf_library(void) {
    const char *names[] = {"SDL3_ttf.dll", NULL};
    for (size_t i = 0; names[i] != NULL; ++i) {
        HMODULE lib = LoadLibraryA(names[i]);
        if (lib != NULL) return (void *)lib;
    }
    return NULL;
}
static void *ui_symbol(void *library, const char *name) {
    return (void *)GetProcAddress((HMODULE)library, name);
}
#else
static void *ui_open_library(void) {
#if defined(__APPLE__)
    const char *names[] = {"libSDL3.0.dylib", "libSDL3.dylib", "/opt/homebrew/lib/libSDL3.dylib", NULL};
#else
    const char *names[] = {"libSDL3.so.0", "libSDL3.so", NULL};
#endif
    for (size_t i = 0; names[i] != NULL; ++i) {
        void *lib = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if (lib != NULL) return lib;
    }
    return NULL;
}
static void *ui_open_ttf_library(void) {
#if defined(__APPLE__)
    const char *names[] = {"libSDL3_ttf.0.dylib", "libSDL3_ttf.dylib", "/opt/homebrew/lib/libSDL3_ttf.dylib", NULL};
#else
    const char *names[] = {"libSDL3_ttf.so.0", "libSDL3_ttf.so", NULL};
#endif
    for (size_t i = 0; names[i] != NULL; ++i) {
        void *lib = dlopen(names[i], RTLD_NOW | RTLD_LOCAL);
        if (lib != NULL) return lib;
    }
    return NULL;
}
static void *ui_symbol(void *library, const char *name) { return dlsym(library, name); }
#endif

#define UI_LOAD(field, symbol) do { \
    *(void **)(&g_sdl.field) = ui_symbol(g_sdl.library, symbol); \
    if (g_sdl.field == NULL) { ui_set_error("SDL3 is missing required rendering symbols"); return false; } \
} while (0)

static bool ui_load_sdl(void) {
    if (g_sdl.attempted) return g_sdl.ready;
    g_sdl.attempted = true;
#if !defined(_WIN32) && !defined(__APPLE__)
    if (getenv("SDL_VIDEODRIVER") == NULL && getenv("WAYLAND_DISPLAY") != NULL) {
        setenv("SDL_VIDEODRIVER", "wayland", 0);
    }
#endif
    g_sdl.library = ui_open_library();
    if (g_sdl.library == NULL) {
        ui_set_error("SDL3 runtime was not found");
        return false;
    }
    UI_LOAD(Init, "SDL_Init");
    UI_LOAD(Quit, "SDL_Quit");
    UI_LOAD(CreateWindow, "SDL_CreateWindow");
    UI_LOAD(CreateWindowAndRenderer, "SDL_CreateWindowAndRenderer");
    UI_LOAD(CreateRenderer, "SDL_CreateRenderer");
    UI_LOAD(CreateProperties, "SDL_CreateProperties");
    UI_LOAD(DestroyProperties, "SDL_DestroyProperties");
    UI_LOAD(SetNumberProperty, "SDL_SetNumberProperty");
    UI_LOAD(GetPointerProperty, "SDL_GetPointerProperty");
    UI_LOAD(GetNumberProperty, "SDL_GetNumberProperty");
    UI_LOAD(GetRendererProperties, "SDL_GetRendererProperties");
    UI_LOAD(CreateSurface, "SDL_CreateSurface");
    UI_LOAD(CreateSoftwareRenderer, "SDL_CreateSoftwareRenderer");
    UI_LOAD(GetRendererName, "SDL_GetRendererName");
    UI_LOAD(ShowWindow, "SDL_ShowWindow");
    UI_LOAD(SetWindowOpacity, "SDL_SetWindowOpacity");
    UI_LOAD(SyncWindow, "SDL_SyncWindow");
    UI_LOAD(GetWindowSize, "SDL_GetWindowSize");
    UI_LOAD(GetWindowPixelDensity, "SDL_GetWindowPixelDensity");
    UI_LOAD(GetDisplayForWindow, "SDL_GetDisplayForWindow");
    UI_LOAD(GetDisplayContentScale, "SDL_GetDisplayContentScale");
    UI_LOAD(GetWindowSizeInPixels, "SDL_GetWindowSizeInPixels");
    UI_LOAD(DestroyRenderer, "SDL_DestroyRenderer");
    UI_LOAD(DestroyWindow, "SDL_DestroyWindow");
    UI_LOAD(PollEvent, "SDL_PollEvent");
    UI_LOAD(WaitEventTimeout, "SDL_WaitEventTimeout");
    UI_LOAD(StartTextInput, "SDL_StartTextInput");
    UI_LOAD(StopTextInput, "SDL_StopTextInput");
    UI_LOAD(SetClipboardText, "SDL_SetClipboardText");
    UI_LOAD(GetClipboardText, "SDL_GetClipboardText");
    UI_LOAD(Free, "SDL_free");
    UI_LOAD(SetRenderDrawColor, "SDL_SetRenderDrawColor");
    UI_LOAD(SetRenderDrawBlendMode, "SDL_SetRenderDrawBlendMode");
    UI_LOAD(SetRenderClipRect, "SDL_SetRenderClipRect");
    UI_LOAD(SetRenderScale, "SDL_SetRenderScale");
    UI_LOAD(RenderClear, "SDL_RenderClear");
    UI_LOAD(RenderFillRect, "SDL_RenderFillRect");
    UI_LOAD(RenderRect, "SDL_RenderRect");
    UI_LOAD(RenderLine, "SDL_RenderLine");
    UI_LOAD(RenderGeometry, "SDL_RenderGeometry");
    UI_LOAD(RenderDebugText, "SDL_RenderDebugText");
    UI_LOAD(CreateTextureFromSurface, "SDL_CreateTextureFromSurface");
    UI_LOAD(CreateTexture, "SDL_CreateTexture");
    UI_LOAD(CreateTextureWithProperties, "SDL_CreateTextureWithProperties");
    UI_LOAD(UpdateTexture, "SDL_UpdateTexture");
    UI_LOAD(SetTextureBlendMode, "SDL_SetTextureBlendMode");
    UI_LOAD(SetRenderTarget, "SDL_SetRenderTarget");
    UI_LOAD(RenderTexture, "SDL_RenderTexture");
    UI_LOAD(RenderReadPixels, "SDL_RenderReadPixels");
    UI_LOAD(ReadSurfacePixel, "SDL_ReadSurfacePixel");
    UI_LOAD(DestroyTexture, "SDL_DestroyTexture");
    UI_LOAD(DestroySurface, "SDL_DestroySurface");
    UI_LOAD(RenderPresent, "SDL_RenderPresent");
    UI_LOAD(SetRenderVSync, "SDL_SetRenderVSync");
    UI_LOAD(GetError, "SDL_GetError");
    if (!g_sdl.Init(UI_SDL_INIT_VIDEO)) {
        ui_set_error(g_sdl.GetError());
        return false;
    }
    g_sdl.ready = true;
    return true;
}

#undef UI_LOAD

#define UI_TTF_LOAD(field, symbol) do { \
    *(void **)(&g_ttf.field) = ui_symbol(g_ttf.library, symbol); \
    if (g_ttf.field == NULL) { ui_set_error("SDL3_ttf is missing required font symbols"); return false; } \
} while (0)

static bool ui_load_ttf(void) {
    if (g_ttf.attempted) return g_ttf.ready;
    g_ttf.attempted = true;
    g_ttf.library = ui_open_ttf_library();
    if (g_ttf.library == NULL) {
        ui_set_error("SDL3_ttf runtime was not found");
        return false;
    }
    UI_TTF_LOAD(Init, "TTF_Init");
    UI_TTF_LOAD(Quit, "TTF_Quit");
    UI_TTF_LOAD(OpenFont, "TTF_OpenFont");
    UI_TTF_LOAD(CloseFont, "TTF_CloseFont");
    UI_TTF_LOAD(SetFontStyle, "TTF_SetFontStyle");
    UI_TTF_LOAD(SetFontSize, "TTF_SetFontSize");
    UI_TTF_LOAD(GetStringSize, "TTF_GetStringSize");
    UI_TTF_LOAD(GetStringSizeWrapped, "TTF_GetStringSizeWrapped");
    UI_TTF_LOAD(MeasureString, "TTF_MeasureString");
    UI_TTF_LOAD(RenderTextBlended, "TTF_RenderText_Blended");
    UI_TTF_LOAD(RenderTextBlendedWrapped, "TTF_RenderText_Blended_Wrapped");
    if (!g_ttf.Init()) {
        ui_set_error(g_sdl.GetError());
        return false;
    }
    g_ttf.ready = true;
    return true;
}

#undef UI_TTF_LOAD

static UiWindow *ui_window(size_t handle) {
    if (handle == 0) return NULL;
    UiWindow *candidate = (UiWindow *)(uintptr_t)handle;
    for (size_t i = 0; i < 64; ++i) {
        if (g_windows[i] == candidate) return candidate;
    }
    return NULL;
}

static bool ui_register_window(UiWindow *state) {
    if (state == NULL) return false;
    for (size_t i = 0; i < 64; ++i) {
        if (g_windows[i] == NULL) {
            g_windows[i] = state;
            ++g_window_count;
            return true;
        }
    }
    ui_set_error("Luma supports at most 64 simultaneous windows");
    return false;
}

static void ui_unregister_window(UiWindow *state) {
    for (size_t i = 0; i < 64; ++i) {
        if (g_windows[i] == state) {
            g_windows[i] = NULL;
            if (g_window_count > 0) --g_window_count;
            return;
        }
    }
}

static bool ui_select_font(UiWindow *state, size_t face, float size, uint32_t weight);

static void ui_destroy_text_texture(UiWindow *state, UiSdlTexture *texture) {
    if (texture == NULL || state == NULL) return;
    if (state->renderer_kind == 3u)
        luma_vk_ui_texture_destroy((size_t)(uintptr_t)texture);
    else
        g_sdl.DestroyTexture(texture);
}

static void ui_clear_text_cache(UiWindow *state) {
    if (state == NULL) return;
    for (size_t i = 0; i < state->text_cache_count; ++i) {
        if (state->text_cache[i].owns_texture)
            ui_destroy_text_texture(state, state->text_cache[i].texture);
        free(state->text_cache[i].text);
    }
    for (size_t i = 0; i < state->text_atlas_count; ++i) {
        if (state->text_atlas[i].texture != NULL)
            ui_destroy_text_texture(state, state->text_atlas[i].texture);
        state->text_atlas[i] = (UiTextAtlasPage){0};
    }
    state->text_cache_count = 0;
    state->text_cache_bytes = 0;
    state->text_cache_tick = 0;
    state->text_atlas_count = 0;
    ++state->text_atlas_generation;
    if (state->text_atlas_generation == 0) state->text_atlas_generation = 1;
}

static void ui_clear_external_textures(UiWindow *state) {
    if (state == NULL) return;
    for (size_t i = 0; i < 4; ++i) {
        if (state->external_textures[i] != NULL) {
            if (state->renderer_kind == 3u)
                luma_vk_ui_texture_destroy(
                    (size_t)(uintptr_t)state->external_textures[i]);
            else
                g_sdl.DestroyTexture(state->external_textures[i]);
        }
        state->external_textures[i] = NULL;
        state->external_images[i] = 0;
        state->external_widths[i] = 0;
        state->external_heights[i] = 0;
        state->external_layouts[i] = 0;
    }
    state->external_texture = NULL;
    state->external_texture_next = 0;
}

static void ui_remove_text_cache_entry(UiWindow *state, size_t index) {
    if (state == NULL || index >= state->text_cache_count) return;
    UiTextCacheEntry *entry = &state->text_cache[index];
    if (entry->owns_texture)
        ui_destroy_text_texture(state, entry->texture);
    free(entry->text);
    if (state->text_cache_bytes >= entry->bytes) state->text_cache_bytes -= entry->bytes;
    size_t last = state->text_cache_count - 1;
    if (index != last) state->text_cache[index] = state->text_cache[last];
    --state->text_cache_count;
}

static size_t ui_oldest_text_cache_entry(UiWindow *state) {
    size_t oldest = 0;
    for (size_t i = 1; i < state->text_cache_count; ++i) {
        if (state->text_cache[i].used_at < state->text_cache[oldest].used_at) oldest = i;
    }
    return oldest;
}

static void ui_clear_fonts(UiWindow *state) {
    if (state == NULL) return;
    ui_clear_text_cache(state);
    if (g_ttf.ready) {
        for (size_t i = 0; i < state->font_variant_count; ++i) {
            if (state->font_variants[i].font != NULL) g_ttf.CloseFont(state->font_variants[i].font);
        }
    }
    for (size_t i = 0; i < state->font_face_count; ++i) {
        free(state->font_faces[i].family);
        free(state->font_faces[i].path);
    }
    state->font_face_count = 0;
    state->current_face = 0;
    state->font = NULL;
    state->font_size = 0.0f;
    state->font_weight = 0;
    state->font_variant_count = 0;
    state->font_tick = 0;
}

static void ui_clear_font_variants(UiWindow *state) {
    if (state == NULL) return;
    ui_clear_text_cache(state);
    if (g_ttf.ready) {
        for (size_t i = 0; i < state->font_variant_count; ++i) {
            if (state->font_variants[i].font != NULL) g_ttf.CloseFont(state->font_variants[i].font);
        }
    }
    state->font = NULL;
    state->font_size = 0.0f;
    state->font_weight = 0;
    state->font_variant_count = 0;
    state->font_tick = 0;
}

static void ui_refresh_pixel_density(UiWindow *state) {
    if (state == NULL || state->window == NULL) return;
    float density = g_sdl.GetWindowPixelDensity(state->window);
    if (density <= 1.001f) {
        uint32_t display = g_sdl.GetDisplayForWindow(state->window);
        float display_scale = display == 0 ? 0.0f :
            g_sdl.GetDisplayContentScale(display);
        if (display_scale > density) density = display_scale;
    }
    if (density < 1.0f) density = 1.0f;
    if (state->pixel_density != density && state->pixel_density > 0.0f) {
        size_t face = state->current_face;
        float size = state->font_size;
        uint32_t weight = state->font_weight;
        ui_clear_font_variants(state);
        state->pixel_density = density;
        if (size > 0.0f && face < state->font_face_count) ui_select_font(state, face, size, weight);
    }
    state->pixel_density = density;
    if (state->renderer != NULL) g_sdl.SetRenderScale(state->renderer, density, density);
}

/* Window systems may emit EXPOSED before RESIZED during an interactive
   resize. Consumers must never receive a resize event paired with stale
   logical dimensions or pixel density: that produces one frame at the old
   layout/DPI and leaves embedded viewports stale until another input event.
   Query SDL's authoritative state once after coalescing the event batch. */
static void ui_refresh_window_metrics(UiWindow *state) {
    if (state == NULL || state->window == NULL) return;
    int width = 0, height = 0;
    if (g_sdl.GetWindowSize(state->window, &width, &height) &&
        width > 0 && height > 0) {
        state->width = (uint32_t)width;
        state->height = (uint32_t)height;
    }
    ui_refresh_pixel_density(state);
}

static void ui_window_pixel_extent(UiWindow *state, uint32_t *width,
    uint32_t *height) {
    int pixel_width = 0, pixel_height = 0;
    if (state != NULL && state->window != NULL &&
        g_sdl.GetWindowSizeInPixels(state->window, &pixel_width, &pixel_height) &&
        pixel_width > 0 && pixel_height > 0) {
        *width = (uint32_t)pixel_width;
        *height = (uint32_t)pixel_height;
        return;
    }
    float density = state != NULL && state->pixel_density > 0.0f
        ? state->pixel_density : 1.0f;
    *width = state == NULL ? 0u : (uint32_t)((float)state->width * density);
    *height = state == NULL ? 0u : (uint32_t)((float)state->height * density);
}

static bool ui_color(UiWindow *window, uint32_t color) {
    return g_sdl.SetRenderDrawColor(window->renderer,
        (uint8_t)(color >> 24), (uint8_t)(color >> 16),
        (uint8_t)(color >> 8), (uint8_t)color);
}

size_t luma_window_create_with_renderer(encore_str title, uint32_t width, uint32_t height,
    uint32_t renderer_kind) {
    if (width == 0 || height == 0 || !ui_load_sdl()) return 0;
    UiWindow *state = (UiWindow *)calloc(1, sizeof(UiWindow));
    char *name = ui_to_cstr(title);
    if (state == NULL || name == NULL) {
        free(state); free(name); ui_set_error("Unable to allocate UI window"); return 0;
    }
    bool created = false;
    state->renderer_kind = renderer_kind;
    uint64_t window_flags = 0x8u | 0x20u | 0x2000u;
    if (renderer_kind == 3u) window_flags |= 0x10000000ull;
    state->window = g_sdl.CreateWindow(name, (int)width, (int)height, window_flags);
    if (state->window != NULL) {
        if (renderer_kind == 3u) {
            created = true;
        } else {
            const char *requested = renderer_kind == 2 ? "software" : "vulkan";
            state->renderer = g_sdl.CreateRenderer(state->window, requested);
            created = state->renderer != NULL;
        }
    }
    free(name);
    if (!created) {
        ui_set_error(g_sdl.GetError());
        if (state->renderer != NULL) g_sdl.DestroyRenderer(state->renderer);
        if (state->window != NULL) g_sdl.DestroyWindow(state->window);
        free(state);
        return 0;
    }
    if ((state->renderer != NULL && !g_sdl.SetRenderDrawBlendMode(state->renderer, 1u)) ||
        !g_sdl.StartTextInput(state->window)) {
        ui_set_error(g_sdl.GetError());
        if (state->renderer != NULL) g_sdl.DestroyRenderer(state->renderer);
        if (state->window != NULL) g_sdl.DestroyWindow(state->window);
        free(state);
        return 0;
    }
    /* Build the first complete frame while the window is still hidden. A
       transparent pre-map required an extra compositor sync and technical
       black present, which was observable as multiple startup states on some
       Wayland compositors. */
    state->reveal_pending = false;
    state->transparent_mapped = false;
    state->first_frame_pending = true;
    int actual_width = (int)width;
    int actual_height = (int)height;
    if (g_sdl.GetWindowSize(state->window, &actual_width, &actual_height) &&
        actual_width > 0 && actual_height > 0) {
        width = (uint32_t)actual_width;
        height = (uint32_t)actual_height;
    }
    state->text_input_started = true;
    state->open = true;
    state->width = width;
    state->height = height;
    ui_refresh_pixel_density(state);
    if (!ui_register_window(state)) {
        g_sdl.StopTextInput(state->window);
        if (state->renderer != NULL) g_sdl.DestroyRenderer(state->renderer);
        g_sdl.DestroyWindow(state->window);
        free(state);
        return 0;
    }
#if !defined(_WIN32)
    if (g_window_count == 1) {
        g_terminate_requested = 0;
        signal(SIGTERM, ui_termination_signal);
        signal(SIGINT, ui_termination_signal);
    }
#endif
    return (size_t)(uintptr_t)state;
}

bool luma_window_reveal(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || state->window == NULL) return false;
    if (!g_sdl.ShowWindow(state->window) || !g_sdl.SyncWindow(state->window)) {
        ui_set_error(g_sdl.GetError());
        return false;
    }
    if (state->reveal_pending) {
        g_sdl.SetWindowOpacity(state->window, 1.0f);
        state->reveal_pending = false;
    }
    return true;
}

size_t luma_window_create(encore_str title, uint32_t width, uint32_t height) {
    return luma_window_create_with_renderer(title, width, height, 0);
}

bool luma_window_destroy(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL) return false;
    ui_unregister_window(state);
    if (state->window != NULL && state->text_input_started) g_sdl.StopTextInput(state->window);
    ui_clear_fonts(state);
    if (state->readback_surface != NULL) g_sdl.DestroySurface(state->readback_surface);
    if (state->layout_texture != NULL) g_sdl.DestroyTexture(state->layout_texture);
    if (state->overlay_texture != NULL) g_sdl.DestroyTexture(state->overlay_texture);
    ui_clear_external_textures(state);
    if (state->renderer != NULL) g_sdl.DestroyRenderer(state->renderer);
    if (state->window != NULL) g_sdl.DestroyWindow(state->window);
    free(state->vector_vertices);
    free(state->vector_indices);
    free(state);
    if (g_window_count == 0) {
        if (g_ttf.ready) { g_ttf.Quit(); g_ttf.ready = false; g_ttf.attempted = false; }
        if (g_sdl.ready) { g_sdl.Quit(); g_sdl.ready = false; g_sdl.attempted = false; }
    }
    return true;
}

bool luma_window_open(size_t handle) {
    UiWindow *state = ui_window(handle);
    return state != NULL && state->open;
}

uint32_t luma_window_poll(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open) return UI_EVENT_CLOSE;
#if !defined(_WIN32)
    if (g_terminate_requested) {
        state->open = false;
        state->event_kind = UI_EVENT_CLOSE;
        return state->event_kind;
    }
#endif
    state->event_kind = UI_EVENT_NONE;
    state->event_key = 0;
    state->event_modifiers = 0;
    state->event_text[0] = '\0';
    state->wheel_x = 0.0f;
    state->wheel_y = 0.0f;
    UiSdlEvent event;
    bool had_motion = false;
    bool had_resize = false;
    while (state->has_pending_event || g_sdl.PollEvent(&event)) {
        if (state->has_pending_event) {
            event = state->pending_event;
            state->has_pending_event = false;
        }
        if (event.type == UI_SDL_EVENT_QUIT || event.type == UI_SDL_EVENT_WINDOW_CLOSE) {
            state->event_kind = UI_EVENT_CLOSE;
            return state->event_kind;
        } else if (event.type == UI_SDL_EVENT_WINDOW_EXPOSED) {
            state->event_kind = UI_EVENT_RESIZE;
            had_resize = true;
        } else if (event.type == UI_SDL_EVENT_WINDOW_RESIZED) {
            UiSdlWindowEvent *window = (UiSdlWindowEvent *)&event;
            if (window->data1 > 0 && window->data2 > 0) {
                state->width = (uint32_t)window->data1;
                state->height = (uint32_t)window->data2;
                state->event_kind = UI_EVENT_RESIZE;
                had_resize = true;
            }
        } else if (event.type == UI_SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
            ui_refresh_pixel_density(state);
            state->event_kind = UI_EVENT_RESIZE;
            had_resize = true;
        } else if (event.type == UI_SDL_EVENT_KEY_DOWN || event.type == UI_SDL_EVENT_KEY_UP) {
            if (had_resize) {
                state->pending_event = event;
                state->has_pending_event = true;
                break;
            }
            UiSdlKeyboardEvent *key = (UiSdlKeyboardEvent *)&event;
            state->event_kind = event.type == UI_SDL_EVENT_KEY_DOWN ? UI_EVENT_KEY_DOWN : UI_EVENT_KEY_UP;
            state->event_key = key->key;
            state->event_modifiers = key->mod;
            return state->event_kind;
        } else if (event.type == UI_SDL_EVENT_TEXT_INPUT) {
            if (had_resize) {
                state->pending_event = event;
                state->has_pending_event = true;
                break;
            }
            UiSdlTextInputEvent *input = (UiSdlTextInputEvent *)&event;
            if (input->text == NULL) { state->event_text[0] = '\0'; }
            else { strncpy(state->event_text, input->text, sizeof(state->event_text) - 1); }
            state->event_text[sizeof(state->event_text) - 1] = '\0';
            state->event_kind = UI_EVENT_TEXT_INPUT;
            return state->event_kind;
        } else if (event.type == UI_SDL_EVENT_MOUSE_MOTION) {
            if (had_resize) {
                state->pending_event = event;
                state->has_pending_event = true;
                break;
            }
            UiSdlMouseMotionEvent *motion = (UiSdlMouseMotionEvent *)&event;
            state->event_kind = UI_EVENT_POINTER_MOVE;
            state->event_x = motion->x;
            state->event_y = motion->y;
            had_motion = true;
        } else if (event.type == UI_SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == UI_SDL_EVENT_MOUSE_BUTTON_UP) {
            if (had_resize) {
                state->pending_event = event;
                state->has_pending_event = true;
                break;
            }
            UiSdlMouseButtonEvent *button = (UiSdlMouseButtonEvent *)&event;
            state->event_kind = event.type == UI_SDL_EVENT_MOUSE_BUTTON_DOWN ? UI_EVENT_POINTER_DOWN : UI_EVENT_POINTER_UP;
            state->event_x = button->x;
            state->event_y = button->y;
            state->event_key = button->button;
            return state->event_kind;
        } else if (event.type == UI_SDL_EVENT_MOUSE_WHEEL) {
            if (had_resize) {
                state->pending_event = event;
                state->has_pending_event = true;
                break;
            }
            UiSdlMouseWheelEvent *wheel = (UiSdlMouseWheelEvent *)&event;
            state->event_kind = UI_EVENT_SCROLL;
            state->event_x = wheel->mouse_x;
            state->event_y = wheel->mouse_y;
            state->wheel_x = wheel->x;
            state->wheel_y = wheel->y;
            return state->event_kind;
        }
    }
    if (had_resize) {
        ui_refresh_window_metrics(state);
        return UI_EVENT_RESIZE;
    }
    if (had_motion) { return UI_EVENT_POINTER_MOVE; }
    return state->event_kind;
}

bool luma_window_wait(size_t handle, uint32_t timeout_ms) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open) return false;
    if (state->has_pending_event) return true;
    int timeout = timeout_ms == UINT32_MAX ? -1 :
        (int)(timeout_ms > 60000u ? 60000u : timeout_ms);
    if (!g_sdl.WaitEventTimeout(&state->pending_event, timeout)) return false;
    state->has_pending_event = true;
    return true;
}

float luma_event_x(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL ? 0.0f : s->event_x; }
float luma_event_y(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL ? 0.0f : s->event_y; }
uint32_t luma_event_key(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL ? 0 : s->event_key; }
uint32_t luma_event_modifiers(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL ? 0 : s->event_modifiers; }
encore_str luma_event_text(size_t handle) { UiWindow *s = ui_window(handle); return ui_string(s == NULL ? "" : s->event_text); }
uint32_t luma_window_width(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL ? 0 : s->width; }
uint32_t luma_window_height(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL ? 0 : s->height; }
uint32_t luma_window_pixel_width(size_t handle) {
    UiWindow *s = ui_window(handle);
    int width = 0, height = 0;
    return s != NULL && s->window != NULL &&
        g_sdl.GetWindowSizeInPixels(s->window, &width, &height) && width > 0
        ? (uint32_t)width : 0;
}
uint32_t luma_window_pixel_height(size_t handle) {
    UiWindow *s = ui_window(handle);
    int width = 0, height = 0;
    return s != NULL && s->window != NULL &&
        g_sdl.GetWindowSizeInPixels(s->window, &width, &height) && height > 0
        ? (uint32_t)height : 0;
}
float luma_window_pixel_density(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL || s->pixel_density <= 0.0f ? 1.0f : s->pixel_density; }
uint64_t luma_window_present_count(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL ? 0 : s->present_count; }
uint32_t luma_window_read_pixel(size_t handle, uint32_t x, uint32_t y) {
    UiWindow *s = ui_window(handle);
    if (s == NULL || s->renderer == NULL || x >= s->width || y >= s->height)
        return 0;
    /* Read the target once and sample its physical backing coordinates.
       Repeated 1x1 SDL_RenderReadPixels calls are not stable on the software
       backend with a fractional render scale: flushing successive tiny
       regions can return different portions of queued geometry. */
    if (s->readback_surface == NULL) {
        s->readback_surface = g_sdl.RenderReadPixels(s->renderer, NULL);
        if (s->readback_surface == NULL) {
            ui_set_error(g_sdl.GetError());
            return 0;
        }
    }
    UiSdlSurface *surface = s->readback_surface;
    float density = s->pixel_density > 0.0f ? s->pixel_density : 1.0f;
    int x0 = (int)((float)x * density);
    int y0 = (int)((float)y * density);
    float x1_value = (float)(x + 1u) * density;
    float y1_value = (float)(y + 1u) * density;
    int x1 = (int)x1_value;
    int y1 = (int)y1_value;
    if ((float)x1 < x1_value) ++x1;
    if ((float)y1 < y1_value) ++y1;
    if (x1 > surface->width) x1 = surface->width;
    if (y1 > surface->height) y1 = surface->height;
    uint64_t red = 0, green = 0, blue = 0, alpha = 0, samples = 0;
    for (int py = y0; py < y1; ++py) {
        for (int px = x0; px < x1; ++px) {
            uint8_t r = 0, g = 0, b = 0, a = 0;
            if (g_sdl.ReadSurfacePixel(surface, px, py, &r, &g, &b, &a)) {
                red += r; green += g; blue += b; alpha += a; ++samples;
            }
        }
    }
    if (samples == 0) return 0;
    return ((uint32_t)(red / samples) << 24) |
        ((uint32_t)(green / samples) << 16) |
        ((uint32_t)(blue / samples) << 8) |
        (uint32_t)(alpha / samples);
}
size_t luma_window_platform_handle(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL ? 0 : (size_t)(uintptr_t)s->window; }
bool luma_direct_compositor_bind(size_t handle, size_t compositor) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || state->renderer_kind != 3u || compositor == 0) return false;
    state->direct_compositor = compositor;
    return luma_vk_ui_set_logical_extent(compositor, state->width, state->height);
}
bool luma_direct_resources_release(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || state->renderer_kind != 3u) return false;
    if (state->borrowed_frame != 0) {
        size_t frame = state->borrowed_frame;
        state->borrowed_frame = 0;
        if (!luma_vk_ui_frame_finish(state->direct_compositor, frame))
            return false;
    }
    ui_clear_text_cache(state);
    ui_clear_external_textures(state);
    return true;
}
float luma_event_wheel_x(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL ? 0.0f : s->wheel_x; }
float luma_event_wheel_y(size_t handle) { UiWindow *s = ui_window(handle); return s == NULL ? 0.0f : s->wheel_y; }

bool luma_clipboard_set(encore_str value) {
    if (!ui_load_sdl()) return false;
    char *text = ui_to_cstr(value);
    if (text == NULL) return false;
    bool result = g_sdl.SetClipboardText(text);
    free(text);
    return result;
}

encore_str luma_clipboard_text(void) {
    if (!ui_load_sdl()) return ui_string("");
    char *text = g_sdl.GetClipboardText();
    encore_str result = ui_string(text == NULL ? "" : text);
    if (text != NULL) g_sdl.Free(text);
    return result;
}

bool luma_frame_begin(size_t handle, uint32_t color) {
    UiWindow *state = ui_window(handle); if (state == NULL || !state->open) return false;
    if (state->readback_surface != NULL) {
        g_sdl.DestroySurface(state->readback_surface);
        state->readback_surface = NULL;
    }
    if (state->renderer_kind == 3u)
        return state->direct_compositor != 0 &&
            luma_vk_ui_set_logical_extent(state->direct_compositor,
                state->width, state->height) &&
            luma_vk_ui_begin(state->direct_compositor, color);
    g_sdl.SetRenderClipRect(state->renderer, NULL);
    return ui_color(state, color) && g_sdl.RenderClear(state->renderer);
}

bool luma_layout_cache_begin(size_t handle, uint32_t color) {
    UiWindow *state = ui_window(handle);
    if (state != NULL && state->renderer_kind == 3u)
        return state->direct_compositor != 0 &&
            luma_vk_ui_set_logical_extent(state->direct_compositor,
                state->width, state->height) &&
            luma_vk_ui_cache_begin(state->direct_compositor, color);
    if (state == NULL || !state->open || state->renderer == NULL) return false;
    uint32_t width = 0, height = 0;
    ui_window_pixel_extent(state, &width, &height);
    if (width == 0 || height == 0) return false;
    if (state->layout_texture == NULL || state->layout_width != width || state->layout_height != height) {
        if (state->layout_texture != NULL) g_sdl.DestroyTexture(state->layout_texture);
        state->layout_texture = g_sdl.CreateTexture(state->renderer, 0x16362004u, 2, (int)width, (int)height);
        state->layout_width = width;
        state->layout_height = height;
    }
    if (state->layout_texture == NULL || !g_sdl.SetRenderTarget(state->renderer, state->layout_texture)) {
        ui_set_error(g_sdl.GetError()); return false;
    }
    g_sdl.SetRenderScale(state->renderer, state->pixel_density, state->pixel_density);
    g_sdl.SetRenderClipRect(state->renderer, NULL);
    return ui_color(state, color) && g_sdl.RenderClear(state->renderer);
}

bool luma_layout_cache_end(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state != NULL && state->renderer_kind == 3u)
        return state->direct_compositor != 0 &&
            luma_vk_ui_cache_end(state->direct_compositor);
    if (state == NULL || state->renderer == NULL || !g_sdl.SetRenderTarget(state->renderer, NULL)) return false;
    return g_sdl.SetRenderScale(state->renderer, state->pixel_density, state->pixel_density);
}

bool luma_overlay_cache_begin(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open || state->renderer == NULL) return false;
    uint32_t width = 0, height = 0;
    ui_window_pixel_extent(state, &width, &height);
    if (width == 0 || height == 0) return false;
    if (state->overlay_texture == NULL || state->overlay_width != width ||
        state->overlay_height != height) {
        if (state->overlay_texture != NULL) g_sdl.DestroyTexture(state->overlay_texture);
        state->overlay_texture = g_sdl.CreateTexture(state->renderer, 0x16362004u,
            2, (int)width, (int)height);
        state->overlay_width = width;
        state->overlay_height = height;
        state->overlay_valid = false;
        if (state->overlay_texture != NULL &&
            !g_sdl.SetTextureBlendMode(state->overlay_texture, 1u)) {
            g_sdl.DestroyTexture(state->overlay_texture);
            state->overlay_texture = NULL;
        }
    }
    if (state->overlay_texture == NULL ||
        !g_sdl.SetRenderTarget(state->renderer, state->overlay_texture)) {
        ui_set_error(g_sdl.GetError());
        return false;
    }
    g_sdl.SetRenderScale(state->renderer, state->pixel_density, state->pixel_density);
    g_sdl.SetRenderClipRect(state->renderer, NULL);
    state->overlay_valid = false;
    return g_sdl.SetRenderDrawColor(state->renderer, 0, 0, 0, 0) &&
        g_sdl.RenderClear(state->renderer);
}

bool luma_overlay_cache_end(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || state->renderer == NULL ||
        !g_sdl.SetRenderTarget(state->renderer, NULL)) return false;
    state->overlay_valid = true;
    return g_sdl.SetRenderScale(state->renderer, state->pixel_density, state->pixel_density);
}

bool luma_overlay_cache_draw(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || state->renderer == NULL || state->overlay_texture == NULL ||
        !state->overlay_valid) return false;
    UiSdlFRect destination = {0.0f, 0.0f, (float)state->width, (float)state->height};
    return g_sdl.RenderTexture(state->renderer, state->overlay_texture, NULL, &destination);
}

bool luma_layout_cache_patch_begin(size_t handle, uint32_t color,
        float x, float y, float width, float height) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open || state->renderer == NULL ||
            state->layout_texture == NULL || width <= 0.0f || height <= 0.0f) return false;
    if (!g_sdl.SetRenderTarget(state->renderer, state->layout_texture)) {
        ui_set_error(g_sdl.GetError()); return false;
    }
    g_sdl.SetRenderScale(state->renderer, state->pixel_density, state->pixel_density);
    UiSdlRect clip = {(int)x, (int)y, (int)(width + 1.0f), (int)(height + 1.0f)};
    if (!g_sdl.SetRenderClipRect(state->renderer, &clip)) return false;
    UiSdlFRect bounds = {x, y, width, height};
    return ui_color(state, color) && g_sdl.RenderFillRect(state->renderer, &bounds);
}

bool luma_cached_frame_begin(size_t handle, uint32_t color) {
    UiWindow *state = ui_window(handle);
    if (state != NULL && state->renderer_kind == 3u)
        return state->direct_compositor != 0 &&
            luma_vk_ui_set_logical_extent(state->direct_compositor,
                state->width, state->height) &&
            luma_vk_ui_cached_begin(state->direct_compositor, color);
    if (state == NULL || state->renderer == NULL || state->layout_texture == NULL) return false;
    if (!g_sdl.SetRenderTarget(state->renderer, NULL)) return false;
    g_sdl.SetRenderScale(state->renderer, state->pixel_density, state->pixel_density);
    g_sdl.SetRenderClipRect(state->renderer, NULL);
    if (!ui_color(state, color) || !g_sdl.RenderClear(state->renderer)) return false;
    UiSdlFRect destination = {0.0f, 0.0f, (float)state->width, (float)state->height};
    return g_sdl.RenderTexture(state->renderer, state->layout_texture, NULL, &destination);
}
bool luma_layout_cache_ready(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state != NULL && state->renderer_kind == 3u)
        return state->direct_compositor != 0 &&
            luma_vk_ui_cache_ready(state->direct_compositor);
    return state != NULL && state->renderer != NULL && state->layout_texture != NULL;
}
bool luma_clip_rect(size_t handle, bool enabled, float x, float y, float width, float height) {
    UiWindow *state = ui_window(handle); if (state == NULL || !state->open) return false;
    if (state->renderer_kind == 3u)
        return state->direct_compositor != 0 &&
            luma_vk_ui_clip(state->direct_compositor, enabled, (int32_t)x, (int32_t)y,
                width > 0.0f ? (uint32_t)width : 0u,
                height > 0.0f ? (uint32_t)height : 0u);
    if (!enabled) return g_sdl.SetRenderClipRect(state->renderer, NULL);
    int left = (int)x;
    int top = (int)y;
    int right = (int)(x + width);
    int bottom = (int)(y + height);
    UiSdlRect rect = {left, top, right > left ? right - left : 0, bottom > top ? bottom - top : 0};
    return g_sdl.SetRenderClipRect(state->renderer, &rect);
}
bool luma_fill_rect(size_t handle, float x, float y, float width, float height, uint32_t color) {
    UiWindow *state = ui_window(handle); if (state == NULL || !state->open) return false;
    if ((color & 0xffu) == 0) return true;
    if (state->renderer_kind == 3u) {
        float r = (float)((color >> 24) & 255u) / 255.0f;
        float g = (float)((color >> 16) & 255u) / 255.0f;
        float b = (float)((color >> 8) & 255u) / 255.0f;
        float a = (float)(color & 255u) / 255.0f;
        UiSdlVertex vertices[4] = {
            {{x, y}, {r,g,b,a}, {0,0}},
            {{x + width, y}, {r,g,b,a}, {0,0}},
            {{x + width, y + height}, {r,g,b,a}, {0,0}},
            {{x, y + height}, {r,g,b,a}, {0,0}}
        };
        int indices[6] = {0,1,2,0,2,3};
        return state->direct_compositor != 0 &&
            luma_vk_ui_geometry(state->direct_compositor, (const float *)vertices,
                4, sizeof(UiSdlVertex) / sizeof(float), 2, indices, 6);
    }
    UiSdlFRect rect = {x, y, width, height}; return ui_color(state, color) && g_sdl.RenderFillRect(state->renderer, &rect);
}
bool luma_stroke_rect(size_t handle, float x, float y, float width, float height, uint32_t color) {
    UiWindow *state = ui_window(handle); if (state == NULL || !state->open) return false;
    if (state->renderer_kind == 3u) {
        return luma_vector_line(handle, x, y, x + width, y, 1.0f, color) &&
            luma_vector_line(handle, x + width, y, x + width, y + height, 1.0f, color) &&
            luma_vector_line(handle, x + width, y + height, x, y + height, 1.0f, color) &&
            luma_vector_line(handle, x, y + height, x, y, 1.0f, color);
    }
    UiSdlFRect rect = {x, y, width, height}; return ui_color(state, color) && g_sdl.RenderRect(state->renderer, &rect);
}
bool luma_line(size_t handle, float x0, float y0, float x1, float y1, uint32_t color) {
    UiWindow *state = ui_window(handle); if (state == NULL || !state->open) return false;
    if (state->renderer_kind == 3u)
        return luma_vector_line(handle, x0, y0, x1, y1, 1.0f, color);
    return ui_color(state, color) && g_sdl.RenderLine(state->renderer, x0, y0, x1, y1);
}

static float ui_sqrt(float value) {
    if (value <= 0.0f) return 0.0f;
    float result = value > 1.0f ? value : 1.0f;
    for (int i = 0; i < 8; ++i) result = (result + value / result) * 0.5f;
    return result;
}

static UiSdlFColor ui_float_color(uint32_t color, float alpha_scale) {
    UiSdlFColor result = {
        (float)((color >> 24) & 0xffu) / 255.0f,
        (float)((color >> 16) & 0xffu) / 255.0f,
        (float)((color >> 8) & 0xffu) / 255.0f,
        ((float)(color & 0xffu) / 255.0f) * alpha_scale
    };
    return result;
}

static float ui_pixel_center(UiWindow *state, float value) {
    float density = state->pixel_density > 0.0f ? state->pixel_density : 1.0f;
    float physical = value * density;
    int pixel = (int)physical;
    if (physical < 0.0f && (float)pixel != physical) --pixel;
    return ((float)pixel + 0.5f) / density;
}

static uint32_t ui_scale_alpha(uint32_t color, float scale) {
    uint32_t alpha = color & 0xffu;
    uint32_t scaled = (uint32_t)((float)alpha * scale + 0.5f);
    if (scaled > 255u) scaled = 255u;
    return (color & 0xffffff00u) | scaled;
}

static bool ui_axis_stroke(UiWindow *state, UiSdlFPoint first,
    UiSdlFPoint second, float width, uint32_t color) {
    float density = state->pixel_density > 0.0f ? state->pixel_density : 1.0f;
    float fringe = 1.0f / density;
    float radius = width * 0.5f;
    float inner = radius - fringe * 0.5f;
    if (inner < 0.0f) inner = 0.0f;
    float outer = radius + fringe * 0.5f;
    bool horizontal = first.y == second.y;
    float start = horizontal ? first.x : first.y;
    float finish = horizontal ? second.x : second.y;
    if (finish < start) { float swap = start; start = finish; finish = swap; }
    bool result = true;
    if (inner > 0.0f) {
        UiSdlFRect core = horizontal
            ? (UiSdlFRect){start, first.y - inner, finish - start, inner * 2.0f}
            : (UiSdlFRect){first.x - inner, start, inner * 2.0f, finish - start};
        result = ui_color(state, color) && g_sdl.RenderFillRect(state->renderer, &core);
    }
    uint32_t fringe_color = ui_scale_alpha(color, 0.5f);
    UiSdlFRect low = horizontal
        ? (UiSdlFRect){start, first.y - outer, finish - start, outer - inner}
        : (UiSdlFRect){first.x - outer, start, outer - inner, finish - start};
    UiSdlFRect high = horizontal
        ? (UiSdlFRect){start, first.y + inner, finish - start, outer - inner}
        : (UiSdlFRect){first.x + inner, start, outer - inner, finish - start};
    return result && ui_color(state, fringe_color) &&
        g_sdl.RenderFillRect(state->renderer, &low) &&
        g_sdl.RenderFillRect(state->renderer, &high);
}

static bool ui_submit_geometry(UiWindow *state, const UiSdlVertex *vertices,
    int vertex_count, const int *indices, int index_count) {
    if (state == NULL || vertices == NULL || indices == NULL ||
        vertex_count <= 0 || index_count <= 0) return false;
    if (state->renderer_kind == 3u)
        return state->direct_compositor != 0 &&
            luma_vk_ui_geometry(state->direct_compositor, (const float *)vertices,
                (size_t)vertex_count, sizeof(UiSdlVertex) / sizeof(float), 2,
                indices, (size_t)index_count);
    return g_sdl.RenderGeometry(state->renderer, NULL, vertices,
        vertex_count, indices, index_count);
}

bool luma_circle(size_t handle, float cx, float cy, float radius, uint32_t color, bool filled);

bool luma_vector_line(size_t handle, float x0, float y0, float x1, float y1, float width, uint32_t color) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open || width <= 0.0f || (color & 0xffu) == 0) return false;
    float dx = x1 - x0;
    float dy = y1 - y0;
    if (dy > -0.0001f && dy < 0.0001f) {
        y0 = ui_pixel_center(state, y0);
        y1 = y0;
        dy = 0.0f;
    } else if (dx > -0.0001f && dx < 0.0001f) {
        x0 = ui_pixel_center(state, x0);
        x1 = x0;
        dx = 0.0f;
    }
    float length = ui_sqrt(dx * dx + dy * dy);
    if (length <= 0.0001f) return true;
    float px = -dy / length;
    float py = dx / length;
    /* Keep the one-pixel AA fringe centered on the requested stroke edge.
       Expanding the full fringe outwards makes small strokes a pixel too wide. */
    float radius = width * 0.5f;
    float fringe = state->pixel_density > 0.0f ? 1.0f / state->pixel_density : 1.0f;
    float inner = radius - fringe * 0.5f;
    /* A zero-width solid core degenerates the middle strip into coincident
       triangles. Some SDL backends then rasterize only one triangle half,
       producing a diagonal fade/dots along sub-pixel SVG strokes. Keep a
       numerically non-degenerate core; its area is far below one pixel and
       does not change the intended coverage ramp. */
    if (inner < fringe * 0.001f) inner = fringe * 0.001f;
    float outer = radius + fringe * 0.5f;
    UiSdlFColor solid = ui_float_color(color, 1.0f);
    UiSdlFColor clear = ui_float_color(color, 0.0f);
    UiSdlVertex vertices[8] = {
        {{x0 + px * outer, y0 + py * outer}, clear, {0, 0}},
        {{x0 + px * inner, y0 + py * inner}, solid, {0, 0}},
        {{x1 + px * outer, y1 + py * outer}, clear, {0, 0}},
        {{x1 + px * inner, y1 + py * inner}, solid, {0, 0}},
        {{x1 - px * inner, y1 - py * inner}, solid, {0, 0}},
        {{x1 - px * outer, y1 - py * outer}, clear, {0, 0}},
        {{x0 - px * inner, y0 - py * inner}, solid, {0, 0}},
        {{x0 - px * outer, y0 - py * outer}, clear, {0, 0}}
    };
    int indices[18] = {0, 2, 1, 1, 2, 3, 1, 3, 6, 6, 3, 4, 6, 4, 7, 7, 4, 5};
    return ui_submit_geometry(state, vertices, 8, indices, 18);
}

bool luma_vector_path_begin(size_t handle, float x, float y) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open) return false;
    state->vector_path_count = 1;
    state->vector_path[0] = (UiSdlFPoint){x, y};
    return true;
}

bool luma_vector_path_point(size_t handle, float x, float y) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open || state->vector_path_count == 0) return false;
    UiSdlFPoint last = state->vector_path[state->vector_path_count - 1];
    if ((x - last.x) * (x - last.x) + (y - last.y) * (y - last.y) < 0.000001f) return true;
    if (state->vector_path_count >= 4096) return false;
    state->vector_path[state->vector_path_count++] = (UiSdlFPoint){x, y};
    return true;
}

static bool ui_reserve_vector_geometry(UiWindow *state, size_t vertex_count,
    size_t index_count) {
    if (vertex_count > state->vector_vertex_capacity) {
        size_t capacity = state->vector_vertex_capacity == 0 ? 256 :
            state->vector_vertex_capacity;
        while (capacity < vertex_count) capacity *= 2;
        UiSdlVertex *vertices = (UiSdlVertex *)realloc(state->vector_vertices,
            sizeof(UiSdlVertex) * capacity);
        if (vertices == NULL) return false;
        state->vector_vertices = vertices;
        state->vector_vertex_capacity = capacity;
    }
    if (index_count > state->vector_index_capacity) {
        size_t capacity = state->vector_index_capacity == 0 ? 512 :
            state->vector_index_capacity;
        while (capacity < index_count) capacity *= 2;
        int *indices = (int *)realloc(state->vector_indices,
            sizeof(int) * capacity);
        if (indices == NULL) return false;
        state->vector_indices = indices;
        state->vector_index_capacity = capacity;
    }
    return true;
}

/* SVG/Lucide paths use round joins and caps. Segment quads stop at each
   centerline vertex and a solid round fan covers the join. Do not add a full
   translucent AA ring at every flattened curve point: those rings overlap
   their neighbouring segment fringes and show up as regularly spaced dark
   dots at small sizes. A ring is needed only at an exposed cap or a genuine
   corner; smooth joins get their AA coverage from the adjacent side fringes. */
bool luma_vector_path_stroke(size_t handle, float width, uint32_t color, bool closed) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open || state->vector_path_count < 2 || width <= 0.0f) return false;
    size_t count = state->vector_path_count;
    if (closed && count > 2) {
        UiSdlFPoint first = state->vector_path[0], last = state->vector_path[count - 1];
        if ((first.x-last.x)*(first.x-last.x) + (first.y-last.y)*(first.y-last.y) < 0.000001f) count--;
    }
    if (count < 2) return true;
    int segments = closed ? (int)count : (int)count - 1;
    const int circle_steps = 16;
    size_t vertex_count = (size_t)segments * 8u + count * (size_t)(1 + circle_steps * 2);
    size_t index_count = (size_t)segments * 18u + count * (size_t)(circle_steps * 9);
    if (!ui_reserve_vector_geometry(state, vertex_count, index_count)) {
        state->vector_path_count = 0;
        return false;
    }
    UiSdlVertex *vertices = state->vector_vertices;
    int *indices = state->vector_indices;
    UiSdlFColor solid = ui_float_color(color, 1.0f);
    UiSdlFColor clear = ui_float_color(color, 0.0f);
    float radius = width * 0.5f;
    float fringe = state->pixel_density > 0.0f ? 1.0f / state->pixel_density : 1.0f;
    float inner = radius - fringe * 0.5f;
    if (inner < fringe * 0.001f) inner = fringe * 0.001f;
    float outer = radius + fringe * 0.5f;
    size_t vertex_at = 0, index_at = 0;
    bool result = true;
    for (int i = 0; i < segments; ++i) {
        int a = i, b = (i + 1) % (int)count;
        UiSdlFPoint first = state->vector_path[a], second = state->vector_path[b];
        float dx = second.x - first.x, dy = second.y - first.y;
        if (dy > -0.0001f && dy < 0.0001f) {
            first.y = ui_pixel_center(state, first.y);
            second.y = first.y;
            dy = 0.0f;
        } else if (dx > -0.0001f && dx < 0.0001f) {
            first.x = ui_pixel_center(state, first.x);
            second.x = first.x;
            dx = 0.0f;
        }
        if (state->renderer_kind != 3u && (dx == 0.0f || dy == 0.0f)) {
            result = ui_axis_stroke(state, first, second, width, color) && result;
            continue;
        }
        float length = ui_sqrt(dx * dx + dy * dy);
        if (length <= 0.0001f) continue;
        float px = -dy / length, py = dx / length;
        int base = (int)vertex_at;
        vertices[vertex_at++] = (UiSdlVertex){{first.x + px*outer,first.y + py*outer},clear,{0,0}};
        vertices[vertex_at++] = (UiSdlVertex){{first.x + px*inner,first.y + py*inner},solid,{0,0}};
        vertices[vertex_at++] = (UiSdlVertex){{second.x + px*outer,second.y + py*outer},clear,{0,0}};
        vertices[vertex_at++] = (UiSdlVertex){{second.x + px*inner,second.y + py*inner},solid,{0,0}};
        vertices[vertex_at++] = (UiSdlVertex){{second.x - px*inner,second.y - py*inner},solid,{0,0}};
        vertices[vertex_at++] = (UiSdlVertex){{second.x - px*outer,second.y - py*outer},clear,{0,0}};
        vertices[vertex_at++] = (UiSdlVertex){{first.x - px*inner,first.y - py*inner},solid,{0,0}};
        vertices[vertex_at++] = (UiSdlVertex){{first.x - px*outer,first.y - py*outer},clear,{0,0}};
        int strip[18] = {base,base+2,base+1, base+1,base+2,base+3,
            base+1,base+3,base+6, base+6,base+3,base+4,
            base+6,base+4,base+7, base+7,base+4,base+5};
        for (int j = 0; j < 18; ++j) indices[index_at++] = strip[j];
    }
    static const float circle_x[16] = {
        1.0f,0.9238795f,0.7071068f,0.3826834f,0.0f,-0.3826834f,-0.7071068f,-0.9238795f,
        -1.0f,-0.9238795f,-0.7071068f,-0.3826834f,0.0f,0.3826834f,0.7071068f,0.9238795f
    };
    static const float circle_y[16] = {
        0.0f,0.3826834f,0.7071068f,0.9238795f,1.0f,0.9238795f,0.7071068f,0.3826834f,
        0.0f,-0.3826834f,-0.7071068f,-0.9238795f,-1.0f,-0.9238795f,-0.7071068f,-0.3826834f
    };
    for (size_t i = 0; i < count; ++i) {
        UiSdlFPoint point = state->vector_path[i];
        bool aa_ring = !closed && (i == 0 || i + 1 == count);
        if (!aa_ring && count > 2) {
            size_t previous = i == 0 ? count - 1 : i - 1;
            size_t next = i + 1 == count ? 0 : i + 1;
            float in_x = point.x - state->vector_path[previous].x;
            float in_y = point.y - state->vector_path[previous].y;
            float out_x = state->vector_path[next].x - point.x;
            float out_y = state->vector_path[next].y - point.y;
            float in_length = ui_sqrt(in_x * in_x + in_y * in_y);
            float out_length = ui_sqrt(out_x * out_x + out_y * out_y);
            if (in_length > 0.0001f && out_length > 0.0001f) {
                float turn_cosine = (in_x * out_x + in_y * out_y) /
                    (in_length * out_length);
                /* A 48-segment circle turns 7.5 degrees at each point
                   (cos ~= .991). Reserve explicit round-join AA for visibly
                   sharper corners, not curve tessellation vertices. */
                aa_ring = turn_cosine < 0.98f;
            }
        }
        int center = (int)vertex_at++;
        int inner_base = (int)vertex_at;
        vertices[center] = (UiSdlVertex){point,solid,{0,0}};
        for (int step = 0; step < circle_steps; ++step) {
            vertices[vertex_at++] = (UiSdlVertex){{point.x + circle_x[step]*inner,
                point.y + circle_y[step]*inner},solid,{0,0}};
        }
        for (int step = 0; step < circle_steps; ++step) {
            int next = (step + 1) % circle_steps;
            indices[index_at++] = center;
            indices[index_at++] = inner_base + step;
            indices[index_at++] = inner_base + next;
        }
        if (aa_ring) {
            int outer_base = (int)vertex_at;
            for (int step = 0; step < circle_steps; ++step) {
                vertices[vertex_at++] = (UiSdlVertex){{point.x + circle_x[step]*outer,
                    point.y + circle_y[step]*outer},clear,{0,0}};
            }
            for (int step = 0; step < circle_steps; ++step) {
                int next = (step + 1) % circle_steps;
                indices[index_at++] = inner_base + step;
                indices[index_at++] = outer_base + step;
                indices[index_at++] = inner_base + next;
                indices[index_at++] = inner_base + next;
                indices[index_at++] = outer_base + step;
                indices[index_at++] = outer_base + next;
            }
        }
    }
    if (vertex_at > 0 && index_at > 0) {
        result = ui_submit_geometry(state, vertices, (int)vertex_at,
            indices, (int)index_at) && result;
    }
    state->vector_path_count = 0;
    return result;
}
static int ui_round_boundary(float x, float y, float width, float height, float radius, UiSdlFPoint *points) {
    static const float axis[9] = {0.0f, 0.1950903f, 0.3826834f, 0.5555702f, 0.7071068f, 0.8314696f, 0.9238795f, 0.9807853f, 1.0f};
    static const float inverse[9] = {1.0f, 0.9807853f, 0.9238795f, 0.8314696f, 0.7071068f, 0.5555702f, 0.3826834f, 0.1950903f, 0.0f};
    float r = radius;
    if (r < 0.0f) r = 0.0f;
    if (r > width * 0.5f) r = width * 0.5f;
    if (r > height * 0.5f) r = height * 0.5f;
    float left = x + r, right = x + width - r, top = y + r, bottom = y + height - r;
    int count = 0;
    for (int i = 0; i < 9; ++i) points[count++] = (UiSdlFPoint){left - r * inverse[i], top - r * axis[i]};
    for (int i = 0; i < 9; ++i) points[count++] = (UiSdlFPoint){right + r * axis[i], top - r * inverse[i]};
    for (int i = 0; i < 9; ++i) points[count++] = (UiSdlFPoint){right + r * inverse[i], bottom + r * axis[i]};
    for (int i = 0; i < 9; ++i) points[count++] = (UiSdlFPoint){left - r * axis[i], bottom + r * inverse[i]};
    return count;
}

static bool ui_fill_rounded_geometry(UiWindow *state, float x, float y, float width, float height, float radius, uint32_t color) {
    UiSdlFPoint inner[36], outer[36];
    float fringe = state->pixel_density > 0.0f ? 1.0f / state->pixel_density : 1.0f;
    float inset = fringe * 0.5f;
    if (width < fringe && width * 0.5f < inset) inset = width * 0.5f;
    if (height < fringe && height * 0.5f < inset) inset = height * 0.5f;
    float inner_radius = radius - inset;
    if (inner_radius < 0.0f) inner_radius = 0.0f;
    int count = ui_round_boundary(x + inset, y + inset,
        width - inset * 2.0f, height - inset * 2.0f, inner_radius, inner);
    ui_round_boundary(x - fringe * 0.5f, y - fringe * 0.5f,
        width + fringe, height + fringe, radius + fringe * 0.5f, outer);
    UiSdlFColor solid = ui_float_color(color, 1.0f);
    UiSdlFColor clear = ui_float_color(color, 0.0f);
    UiSdlVertex vertices[73];
    int indices[324];
    vertices[0] = (UiSdlVertex){{x + width * 0.5f, y + height * 0.5f}, solid, {0, 0}};
    for (int i = 0; i < count; ++i) {
        vertices[1 + i] = (UiSdlVertex){inner[i], solid, {0, 0}};
        vertices[1 + count + i] = (UiSdlVertex){outer[i], clear, {0, 0}};
    }
    int at = 0;
    for (int i = 0; i < count; ++i) {
        int next = (i + 1) % count;
        int inner_i = 1 + i, inner_next = 1 + next;
        int outer_i = 1 + count + i, outer_next = 1 + count + next;
        indices[at++] = 0; indices[at++] = inner_i; indices[at++] = inner_next;
        indices[at++] = inner_i; indices[at++] = outer_i; indices[at++] = inner_next;
        indices[at++] = inner_next; indices[at++] = outer_i; indices[at++] = outer_next;
    }
    return ui_submit_geometry(state, vertices, 1 + count * 2, indices, at);
}

static bool ui_fill_rounded_aa(UiWindow *state, float x, float y, float width, float height, float radius, uint32_t color) {
    float r = radius;
    if (r > width * 0.5f) r = width * 0.5f;
    if (r > height * 0.5f) r = height * 0.5f;
    if (r <= 0.0f ||
        (state->renderer_kind != 3u && !ui_color(state, color))) return false;
    /* The geometry helper already covers the complete rounded rectangle and
       its AA fringe. Rendering four corner-sized copies caused a circle to be
       blended four times, which showed up as dark dots on SVG caps. */
    return ui_fill_rounded_geometry(state, x, y, width, height, r, color);
}

static bool ui_stroke_rounded_aa(size_t handle, float x, float y, float width, float height, float radius, uint32_t color) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open || width <= 0.0f || height <= 0.0f) return false;
    float fringe = state->pixel_density > 0.0f ? 1.0f / state->pixel_density : 1.0f;
    float solid_offset = 0.5f - fringe * 0.5f;
    if (solid_offset < 0.0f) solid_offset = 0.0f;
    float clear_offset = 0.5f + fringe * 0.5f;
    float max_inset = width < height ? width * 0.5f : height * 0.5f;
    if (clear_offset > max_inset) clear_offset = max_inset;

    UiSdlFPoint boundaries[4][36];
    int count = ui_round_boundary(x + clear_offset, y + clear_offset,
        width - clear_offset * 2.0f, height - clear_offset * 2.0f,
        radius - clear_offset, boundaries[0]);
    ui_round_boundary(x + solid_offset, y + solid_offset,
        width - solid_offset * 2.0f, height - solid_offset * 2.0f,
        radius - solid_offset, boundaries[1]);
    ui_round_boundary(x - solid_offset, y - solid_offset,
        width + solid_offset * 2.0f, height + solid_offset * 2.0f,
        radius + solid_offset, boundaries[2]);
    ui_round_boundary(x - clear_offset, y - clear_offset,
        width + clear_offset * 2.0f, height + clear_offset * 2.0f,
        radius + clear_offset, boundaries[3]);

    UiSdlFColor solid = ui_float_color(color, 1.0f);
    UiSdlFColor clear = ui_float_color(color, 0.0f);
    UiSdlVertex vertices[144];
    int indices[648];
    for (int boundary = 0; boundary < 4; ++boundary) {
        UiSdlFColor vertex_color = boundary == 0 || boundary == 3 ? clear : solid;
        for (int i = 0; i < count; ++i) {
            vertices[boundary * count + i] = (UiSdlVertex){boundaries[boundary][i], vertex_color, {0, 0}};
        }
    }
    int at = 0;
    for (int band = 0; band < 3; ++band) {
        int first = band * count;
        int second = (band + 1) * count;
        for (int i = 0; i < count; ++i) {
            int next = (i + 1) % count;
            indices[at++] = first + i;
            indices[at++] = second + i;
            indices[at++] = first + next;
            indices[at++] = first + next;
            indices[at++] = second + i;
            indices[at++] = second + next;
        }
    }
    return ui_submit_geometry(state, vertices, count * 4, indices, at);
}

bool luma_circle(size_t handle, float cx, float cy, float radius, uint32_t color, bool filled) {
    UiWindow *state = ui_window(handle); if (state == NULL || !state->open || radius <= 0.0f) return false;
    return filled
        ? ui_fill_rounded_aa(state, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, radius, color)
        : ui_stroke_rounded_aa(handle, cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, radius, color);
}
bool luma_round_rect(size_t handle, float x, float y, float width, float height, float radius, uint32_t color, bool filled) {
    UiWindow *state = ui_window(handle); if (state == NULL || !state->open || width <= 0.0f || height <= 0.0f) return false;
    float r = radius;
    if (r < 0.0f) r = 0.0f;
    if (r > width / 2.0f) r = width / 2.0f;
    if (r > height / 2.0f) r = height / 2.0f;
    if (r < 1.0f) {
        if (state->renderer_kind == 3u)
            return filled ? luma_fill_rect(handle, x, y, width, height, color)
                : luma_stroke_rect(handle, x, y, width, height, color);
        UiSdlFRect rect = {x, y, width, height}; if (!ui_color(state, color)) return false;
        return filled ? g_sdl.RenderFillRect(state->renderer, &rect) : g_sdl.RenderRect(state->renderer, &rect);
    }
    return filled ? ui_fill_rounded_aa(state, x, y, width, height, r, color)
        : ui_stroke_rounded_aa(handle, x, y, width, height, r, color);
}

enum {
    UI_TEXT_ATLAS_SIZE = 2048,
    UI_TEXT_ATLAS_GUTTER = 1
};

/* Packs complete shaped text runs rather than individual codepoints. This
   preserves SDL_ttf shaping and kerning while replacing hundreds of
   per-string textures/descriptors with a small set of persistent pages. */
static UiSdlTexture *ui_text_atlas_upload(UiWindow *state,
        const UiSdlSurface *surface, int *atlas_x, int *atlas_y,
        uint32_t *generation) {
    if (state == NULL || surface == NULL || surface->pixels == NULL ||
            surface->width <= 0 || surface->height <= 0 ||
            surface->width + UI_TEXT_ATLAS_GUTTER > UI_TEXT_ATLAS_SIZE ||
            surface->height + UI_TEXT_ATLAS_GUTTER > UI_TEXT_ATLAS_SIZE)
        return NULL;

    size_t selected = state->text_atlas_count;
    for (size_t i = 0; i < state->text_atlas_count; ++i) {
        UiTextAtlasPage *page = &state->text_atlas[i];
        if (page->format != surface->format) continue;
        int x = page->cursor_x;
        int y = page->cursor_y;
        if (x + surface->width > UI_TEXT_ATLAS_SIZE) {
            x = 0;
            y += page->row_height;
        }
        if (y + surface->height <= UI_TEXT_ATLAS_SIZE) {
            selected = i;
            break;
        }
    }

    if (selected == state->text_atlas_count) {
        if (state->text_atlas_count >= 4) {
            /* Entries only borrow atlas pages. Clearing them together makes
               generation invalidation atomic and prevents stale descriptors. */
            ui_clear_text_cache(state);
            state->text_cache_tick = 1;
        }
        selected = state->text_atlas_count++;
        UiSdlTexture *texture = state->renderer_kind == 3u
            ? (UiSdlTexture *)(uintptr_t)luma_vk_ui_texture_create_empty(
                state->direct_compositor, UI_TEXT_ATLAS_SIZE,
                UI_TEXT_ATLAS_SIZE, 44u)
            : g_sdl.CreateTexture(state->renderer, surface->format, 0,
                UI_TEXT_ATLAS_SIZE, UI_TEXT_ATLAS_SIZE);
        if (texture == NULL) {
            --state->text_atlas_count;
            return NULL;
        }
        if (state->renderer_kind != 3u)
            g_sdl.SetTextureBlendMode(texture, 1u);
        state->text_atlas[selected] = (UiTextAtlasPage){
            texture, surface->format, 0, 0, 0, state->text_atlas_generation
        };
    }

    UiTextAtlasPage *page = &state->text_atlas[selected];
    if (page->cursor_x + surface->width > UI_TEXT_ATLAS_SIZE) {
        page->cursor_x = 0;
        page->cursor_y += page->row_height;
        page->row_height = 0;
    }
    UiSdlRect destination = {
        page->cursor_x, page->cursor_y, surface->width, surface->height
    };
    bool updated = state->renderer_kind == 3u
        ? luma_vk_ui_texture_update((size_t)(uintptr_t)page->texture,
            surface->pixels, (uint32_t)surface->width,
            (uint32_t)surface->height, (uint32_t)surface->pitch,
            (uint32_t)destination.x, (uint32_t)destination.y)
        : g_sdl.UpdateTexture(page->texture, &destination,
            surface->pixels, surface->pitch);
    if (!updated)
        return NULL;
    *atlas_x = destination.x;
    *atlas_y = destination.y;
    *generation = page->generation;
    page->cursor_x += surface->width + UI_TEXT_ATLAS_GUTTER;
    int occupied_height = surface->height + UI_TEXT_ATLAS_GUTTER;
    if (occupied_height > page->row_height)
        page->row_height = occupied_height;
    return page->texture;
}

static UiSdlTexture *ui_text_texture(UiWindow *state, const char *text, size_t length, uint32_t color,
        int wrap_width, int *width, int *height, bool *transient,
        int *atlas_x, int *atlas_y, uint32_t *atlas_generation) {
    *transient = false;
    *atlas_x = 0;
    *atlas_y = 0;
    *atlas_generation = 0;
    ++state->text_cache_tick;
    for (size_t i = 0; i < state->text_cache_count; ++i) {
        UiTextCacheEntry *entry = &state->text_cache[i];
        if (entry->face == state->current_face && entry->font_size == state->font_size &&
                entry->font_weight == state->font_weight && entry->color == color &&
                entry->wrap_width == wrap_width && entry->text_length == length &&
                (length == 0 || memcmp(entry->text, text, length) == 0)) {
            entry->used_at = state->text_cache_tick;
            *width = entry->width;
            *height = entry->height;
            *atlas_x = entry->atlas_x;
            *atlas_y = entry->atlas_y;
            *atlas_generation = entry->atlas_generation;
            return entry->texture;
        }
    }
    UiSdlColor foreground = {
        (uint8_t)(color >> 24), (uint8_t)(color >> 16),
        (uint8_t)(color >> 8), (uint8_t)color
    };
    UiSdlSurface *surface = wrap_width > 0
        ? g_ttf.RenderTextBlendedWrapped(state->font, text, length, foreground, wrap_width)
        : g_ttf.RenderTextBlended(state->font, text, length, foreground);
    bool measured = wrap_width > 0
        ? g_ttf.GetStringSizeWrapped(state->font, text, length, wrap_width, width, height)
        : g_ttf.GetStringSize(state->font, text, length, width, height);
    if (surface == NULL || !measured) {
        if (surface != NULL) g_sdl.DestroySurface(surface);
        ui_set_error(g_sdl.GetError());
        return NULL;
    }
    bool owns_texture = false;
    UiSdlTexture *texture = ui_text_atlas_upload(state, surface, atlas_x,
        atlas_y, atlas_generation);
    if (texture == NULL) {
        /* Oversized runs remain supported without forcing an oversized page. */
        texture = state->renderer_kind == 3u
            ? (UiSdlTexture *)(uintptr_t)luma_vk_ui_texture_create(
                state->direct_compositor, surface->pixels,
                (uint32_t)surface->width, (uint32_t)surface->height,
                (uint32_t)surface->pitch, 44u)
            : g_sdl.CreateTextureFromSurface(state->renderer, surface);
        owns_texture = texture != NULL;
        *atlas_x = 0;
        *atlas_y = 0;
        *atlas_generation = 0;
    }
    g_sdl.DestroySurface(surface);
    if (texture == NULL) { ui_set_error(g_sdl.GetError()); return NULL; }
    size_t pixel_count = *width > 0 && *height > 0 ? (size_t)*width * (size_t)*height : 0;
    size_t bytes = pixel_count <= SIZE_MAX / 4 ? pixel_count * 4 : SIZE_MAX;
    const size_t byte_limit = 64u * 1024u * 1024u;
    if (bytes > byte_limit) {
        *transient = true;
        return texture;
    }
    char *owned_text = ui_copy_text(text, length);
    if (owned_text == NULL) {
        ui_destroy_text_texture(state, texture);
        ui_set_error("Unable to cache rendered text");
        return NULL;
    }
    while (state->text_cache_count > 0 &&
            (state->text_cache_count >= 512 || state->text_cache_bytes + bytes > byte_limit)) {
        ui_remove_text_cache_entry(state, ui_oldest_text_cache_entry(state));
    }
    size_t slot = state->text_cache_count++;
    state->text_cache[slot] = (UiTextCacheEntry){texture, owned_text, length, state->current_face,
        state->font_size, state->font_weight, color, wrap_width, *width, *height,
        bytes, state->text_cache_tick, *atlas_x, *atlas_y,
        *atlas_generation, owns_texture};
    state->text_cache_bytes += bytes;
    return texture;
}

bool luma_text(size_t handle, float x, float y, encore_str value, uint32_t color) {
    UiWindow *state = ui_window(handle); if (state == NULL || !state->open) return false;
    const char *text = value.object == NULL ? "" : value.object->data;
    size_t length = value.object == NULL ? 0 : value.object->len;
    if (length == 0) return true;
    if (state->font == NULL) {
        if (state->renderer_kind == 3u) return false;
        char *terminated = ui_copy_text(text, length); if (terminated == NULL) return false;
        if (!ui_color(state, color)) { free(terminated); return false; }
        bool result = g_sdl.RenderDebugText(state->renderer, x, y, terminated);
        free(terminated);
        return result;
    }
    int width = 0;
    int height = 0;
    bool transient = false;
    int atlas_x = 0;
    int atlas_y = 0;
    uint32_t atlas_generation = 0;
    UiSdlTexture *texture = ui_text_texture(state, text, length, color, 0,
        &width, &height, &transient, &atlas_x, &atlas_y,
        &atlas_generation);
    if (texture == NULL) return false;
    float density = state->pixel_density > 0.0f ? state->pixel_density : 1.0f;
    UiSdlFRect destination = {x, y, (float)width / density, (float)height / density};
    UiSdlFRect source = {
        (float)atlas_x, (float)atlas_y, (float)width, (float)height
    };
    bool result = state->renderer_kind == 3u
        ? (atlas_generation == 0
            ? luma_vk_ui_texture_draw(state->direct_compositor,
                (size_t)(uintptr_t)texture, destination.x, destination.y,
                destination.w, destination.h)
            : luma_vk_ui_texture_draw_region(state->direct_compositor,
                (size_t)(uintptr_t)texture, destination.x, destination.y,
                destination.w, destination.h, (uint32_t)atlas_x,
                (uint32_t)atlas_y, (uint32_t)width, (uint32_t)height))
        : g_sdl.RenderTexture(state->renderer, texture,
            atlas_generation == 0 ? NULL : &source, &destination);
    if (transient) ui_destroy_text_texture(state, texture);
    return result;
}

bool luma_text_wrapped(size_t handle, float x, float y, encore_str value, uint32_t color, float wrap_width) {
    UiWindow *state = ui_window(handle); if (state == NULL || !state->open || wrap_width <= 0.0f) return false;
    const char *text = value.object == NULL ? "" : value.object->data;
    size_t length = value.object == NULL ? 0 : value.object->len;
    if (length == 0) return true;
    if (state->font == NULL) {
        if (state->renderer_kind == 3u) return false;
        char *terminated = ui_copy_text(text, length); if (terminated == NULL) return false;
        if (!ui_color(state, color)) { free(terminated); return false; }
        bool result = g_sdl.RenderDebugText(state->renderer, x, y, terminated);
        free(terminated);
        return result;
    }
    float density = state->pixel_density > 0.0f ? state->pixel_density : 1.0f;
    int limit = (int)(wrap_width * density);
    int width = 0;
    int height = 0;
    bool transient = false;
    int atlas_x = 0;
    int atlas_y = 0;
    uint32_t atlas_generation = 0;
    UiSdlTexture *texture = ui_text_texture(state, text, length, color, limit,
        &width, &height, &transient, &atlas_x, &atlas_y,
        &atlas_generation);
    if (texture == NULL) return false;
    UiSdlFRect destination = {x, y, (float)width / density, (float)height / density};
    UiSdlFRect source = {
        (float)atlas_x, (float)atlas_y, (float)width, (float)height
    };
    bool result = state->renderer_kind == 3u
        ? (atlas_generation == 0
            ? luma_vk_ui_texture_draw(state->direct_compositor,
                (size_t)(uintptr_t)texture, destination.x, destination.y,
                destination.w, destination.h)
            : luma_vk_ui_texture_draw_region(state->direct_compositor,
                (size_t)(uintptr_t)texture, destination.x, destination.y,
                destination.w, destination.h, (uint32_t)atlas_x,
                (uint32_t)atlas_y, (uint32_t)width, (uint32_t)height))
        : g_sdl.RenderTexture(state->renderer, texture,
            atlas_generation == 0 ? NULL : &source, &destination);
    if (transient) ui_destroy_text_texture(state, texture);
    return result;
}

bool luma_font_load(size_t handle, encore_str path, float size, uint32_t weight) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open || size <= 0.0f || !ui_load_ttf()) return false;
    char *file = ui_to_cstr(path);
    if (file == NULL) { ui_set_error("Unable to allocate font path"); return false; }
    float density = state->pixel_density > 0.0f ? state->pixel_density : 1.0f;
    UiTtfFont *font = g_ttf.OpenFont(file, size * density);
    if (font == NULL) { free(file); ui_set_error(g_sdl.GetError()); return false; }
    char *default_family = ui_copy_cstr("");
    if (default_family == NULL) { g_ttf.CloseFont(font); free(file); return false; }
    g_ttf.SetFontStyle(font, weight >= 600 ? 0x01u : 0x00u);
    ui_clear_fonts(state);
    state->font = font;
    state->font_size = size;
    state->font_weight = weight;
    state->font_faces[0] = (UiFontFace){default_family, file};
    state->font_face_count = 1;
    state->current_face = 0;
    state->font_tick = 1;
    state->font_variants[0] = (UiFontVariant){font, size, weight, 0, state->font_tick};
    state->font_variant_count = 1;
    return true;
}

bool luma_font_register(size_t handle, encore_str family, encore_str path) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open || !ui_load_ttf()) return false;
    char *name = ui_to_cstr(family);
    char *file = ui_to_cstr(path);
    if (name == NULL || file == NULL) { free(name); free(file); return false; }
    for (size_t i = 0; i < state->font_face_count; ++i) {
        if (strcmp(state->font_faces[i].family, name) == 0) { free(name); free(file); return true; }
    }
    if (state->font_face_count >= 32) { free(name); free(file); return false; }
    state->font_faces[state->font_face_count++] = (UiFontFace){name, file};
    return true;
}

static bool ui_select_font(UiWindow *state, size_t face, float size, uint32_t weight) {
    if (face >= state->font_face_count || size <= 0.0f) return false;
    ++state->font_tick;
    if (state->font != NULL && state->current_face == face && state->font_size == size && state->font_weight == weight) {
        for (size_t i = 0; i < state->font_variant_count; ++i) {
            if (state->font_variants[i].font == state->font) { state->font_variants[i].used_at = state->font_tick; break; }
        }
        return true;
    }
    for (size_t i = 0; i < state->font_variant_count; ++i) {
        UiFontVariant *variant = &state->font_variants[i];
        if (variant->face == face && variant->size == size && variant->weight == weight) {
            state->font = variant->font;
            state->current_face = face;
            state->font_size = size;
            state->font_weight = weight;
            variant->used_at = state->font_tick;
            return true;
        }
    }
    float density = state->pixel_density > 0.0f ? state->pixel_density : 1.0f;
    UiTtfFont *font = g_ttf.OpenFont(state->font_faces[face].path, size * density);
    if (font == NULL) { ui_set_error(g_sdl.GetError()); return false; }
    g_ttf.SetFontStyle(font, weight >= 600 ? 0x01u : 0x00u);
    size_t slot = state->font_variant_count;
    if (slot < 32) {
        ++state->font_variant_count;
    } else {
        slot = 0;
        for (size_t i = 1; i < state->font_variant_count; ++i) {
            if (state->font_variants[i].used_at < state->font_variants[slot].used_at) slot = i;
        }
        if (state->font_variants[slot].font != NULL) g_ttf.CloseFont(state->font_variants[slot].font);
    }
    state->font_variants[slot] = (UiFontVariant){font, size, weight, face, state->font_tick};
    state->font = font;
    state->current_face = face;
    state->font_size = size;
    state->font_weight = weight;
    return true;
}

bool luma_font_select(size_t handle, encore_str family, float size, uint32_t weight) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open) return false;
    if (state->current_face < state->font_face_count &&
            ui_str_equals_cstr(family, state->font_faces[state->current_face].family)) {
        return ui_select_font(state, state->current_face, size, weight);
    }
    char *name = ui_to_cstr(family);
    if (name == NULL) return false;
    size_t face = state->font_face_count;
    for (size_t i = 0; i < state->font_face_count; ++i) {
        if (strcmp(state->font_faces[i].family, name) == 0) { face = i; break; }
    }
    free(name);
    return face < state->font_face_count && ui_select_font(state, face, size, weight);
}

bool luma_font_clear(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL) return false;
    ui_clear_fonts(state);
    return true;
}

bool luma_font_style(size_t handle, float size, uint32_t weight) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || size <= 0.0f) return false;
    if (state->font == NULL) return true;
    return ui_select_font(state, state->current_face, size, weight);
}

static float ui_text_extent(size_t handle, encore_str value, bool width) {
    UiWindow *state = ui_window(handle);
    if (state == NULL) return 0.0f;
    if (state->font == NULL) return (float)(width ? (value.object == NULL ? 0 : value.object->len * 8) : 8);
    int measured_width = 0;
    int measured_height = 0;
    const char *text = value.object == NULL ? "" : value.object->data;
    size_t length = value.object == NULL ? 0 : value.object->len;
    for (size_t i = 0; i < state->text_cache_count; ++i) {
        UiTextCacheEntry *entry = &state->text_cache[i];
        if (entry->face == state->current_face && entry->font_size == state->font_size &&
                entry->font_weight == state->font_weight && entry->wrap_width == 0 &&
                entry->text_length == length &&
                (length == 0 || memcmp(entry->text, text, length) == 0)) {
            return (float)(width ? entry->width : entry->height) /
                (state->pixel_density > 0.0f ? state->pixel_density : 1.0f);
        }
    }
    if (!g_ttf.GetStringSize(state->font, text, length, &measured_width, &measured_height)) return 0.0f;
    float density = state->pixel_density > 0.0f ? state->pixel_density : 1.0f;
    return (float)(width ? measured_width : measured_height) / density;
}

float luma_text_width(size_t handle, encore_str value) { return ui_text_extent(handle, value, true); }
float luma_text_height(size_t handle, encore_str value) { return ui_text_extent(handle, value, false); }
size_t luma_text_index(size_t handle, encore_str value, float max_width) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || value.object == NULL || max_width <= 0.0f) return 0;
    size_t byte_count = 0;
    if (state->font == NULL) {
        byte_count = (size_t)(max_width / 8.0f);
        if (byte_count > value.object->len) byte_count = value.object->len;
    } else {
        int measured_width = 0;
        float density = state->pixel_density > 0.0f ? state->pixel_density : 1.0f;
        if (!g_ttf.MeasureString(state->font, value.object->data, value.object->len,
                (int)(max_width * density), &measured_width, &byte_count)) return 0;
    }
    size_t characters = 0;
    size_t index = 0;
    while (index < byte_count) {
        uint8_t first = (uint8_t)value.object->data[index];
        size_t width = first < 0x80u ? 1u : (first & 0xe0u) == 0xc0u ? 2u :
            (first & 0xf0u) == 0xe0u ? 3u : 4u;
        index += width;
        characters += 1;
    }
    return characters;
}
bool luma_frame_end(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open) return false;
    if (state->renderer_kind == 3u) {
        if (state->first_frame_pending && !state->transparent_mapped &&
            (!g_sdl.ShowWindow(state->window) || !g_sdl.SyncWindow(state->window))) {
            ui_set_error(g_sdl.GetError());
            return false;
        }
        bool presented = state->direct_compositor != 0 &&
            luma_vk_ui_end(state->direct_compositor);
        if (presented && state->first_frame_pending) {
            if (state->reveal_pending)
                presented = g_sdl.SetWindowOpacity(state->window, 1.0f);
            if (presented) {
                state->reveal_pending = false;
                state->first_frame_pending = false;
            }
        }
        if (presented) ++state->present_count;
        return presented;
    }
    bool presented = g_sdl.RenderPresent(state->renderer);
    if (presented && state->first_frame_pending) {
        if (state->reveal_pending) {
            presented = g_sdl.SetWindowOpacity(state->window, 1.0f);
        } else {
            presented = g_sdl.ShowWindow(state->window) &&
                g_sdl.SyncWindow(state->window);
        }
        if (presented) {
            state->reveal_pending = false;
            state->first_frame_pending = false;
        }
    }
    if (presented) ++state->present_count;
    return presented;
}

size_t luma_direct_frame_acquire(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open || state->renderer_kind != 3u ||
        state->direct_compositor == 0 || state->borrowed_frame != 0) return 0;
    size_t frame = luma_vk_ui_frame_acquire(state->direct_compositor);
    if (frame != 0) state->borrowed_frame = frame;
    return frame;
}

bool luma_direct_composition_split(size_t handle) {
    UiWindow *state = ui_window(handle);
    return state != NULL && state->open && state->renderer_kind == 3u &&
        state->direct_compositor != 0 &&
        luma_vk_ui_composition_split(state->direct_compositor);
}

bool luma_direct_frame_finish(size_t handle, size_t frame) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || !state->open || state->renderer_kind != 3u ||
        state->direct_compositor == 0 || frame == 0 ||
        state->borrowed_frame != frame) return false;
    if (state->first_frame_pending && !state->transparent_mapped &&
        (!g_sdl.ShowWindow(state->window) || !g_sdl.SyncWindow(state->window))) {
        ui_set_error(g_sdl.GetError());
        return false;
    }
    state->borrowed_frame = 0;
    bool presented = luma_vk_ui_frame_finish(state->direct_compositor, frame);
    if (presented && state->first_frame_pending) {
        if (state->reveal_pending)
            presented = g_sdl.SetWindowOpacity(state->window, 1.0f);
        if (presented) {
            state->reveal_pending = false;
            state->first_frame_pending = false;
        }
    }
    if (presented) ++state->present_count;
    return presented;
}

size_t luma_vulkan_instance(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || state->renderer == NULL ||
        strcmp(g_sdl.GetRendererName(state->renderer), "vulkan") != 0) return 0;
    UiSdlPropertiesID props = g_sdl.GetRendererProperties(state->renderer);
    return (size_t)(uintptr_t)g_sdl.GetPointerProperty(props,
        "SDL.renderer.vulkan.instance", NULL);
}

size_t luma_vulkan_physical_device(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || state->renderer == NULL ||
        strcmp(g_sdl.GetRendererName(state->renderer), "vulkan") != 0) return 0;
    UiSdlPropertiesID props = g_sdl.GetRendererProperties(state->renderer);
    return (size_t)(uintptr_t)g_sdl.GetPointerProperty(props,
        "SDL.renderer.vulkan.physical_device", NULL);
}

size_t luma_vulkan_device(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || state->renderer == NULL ||
        strcmp(g_sdl.GetRendererName(state->renderer), "vulkan") != 0) return 0;
    UiSdlPropertiesID props = g_sdl.GetRendererProperties(state->renderer);
    return (size_t)(uintptr_t)g_sdl.GetPointerProperty(props,
        "SDL.renderer.vulkan.device", NULL);
}

uint32_t luma_vulkan_graphics_queue_family(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL || state->renderer == NULL ||
        strcmp(g_sdl.GetRendererName(state->renderer), "vulkan") != 0) return UINT32_MAX;
    UiSdlPropertiesID props = g_sdl.GetRendererProperties(state->renderer);
    int64_t family = g_sdl.GetNumberProperty(props,
        "SDL.renderer.vulkan.graphics_queue_family_index", -1);
    return family < 0 || family > UINT32_MAX ? UINT32_MAX : (uint32_t)family;
}

bool luma_external_vulkan_image_bind(size_t handle, uint64_t image,
    uint32_t width, uint32_t height, uint32_t layout) {
    UiWindow *state = ui_window(handle);
    bool direct = state != NULL && state->renderer_kind == 3u;
    if (state == NULL || image == 0 || width == 0 || height == 0 ||
        (!direct && (state->renderer == NULL ||
            strcmp(g_sdl.GetRendererName(state->renderer), "vulkan") != 0))) {
        ui_set_error("External Vulkan images require a Vulkan Luma renderer");
        return false;
    }
    for (size_t i = 0; i < 4; ++i) {
        if (state->external_textures[i] != NULL &&
            state->external_images[i] == image &&
            state->external_widths[i] == width &&
            state->external_heights[i] == height &&
            state->external_layouts[i] == layout) {
            state->external_texture = state->external_textures[i];
            return true;
        }
    }
    uint32_t slot = state->external_texture_next % 4u;
    state->external_texture_next = (slot + 1u) % 4u;
    if (state->external_textures[slot] != NULL) {
        if (direct)
            luma_vk_ui_texture_destroy(
                (size_t)(uintptr_t)state->external_textures[slot]);
        else
            g_sdl.DestroyTexture(state->external_textures[slot]);
    }
    if (direct) {
        state->external_textures[slot] = (UiSdlTexture *)(uintptr_t)
            luma_vk_ui_external_texture_create(state->direct_compositor,
                image, width, height, layout);
        if (state->external_textures[slot] == NULL) {
            ui_set_error("Unable to wrap external direct Vulkan image");
            return false;
        }
        state->external_images[slot] = image;
        state->external_widths[slot] = width;
        state->external_heights[slot] = height;
        state->external_layouts[slot] = layout;
        state->external_texture = state->external_textures[slot];
        return true;
    }
    UiSdlPropertiesID props = g_sdl.CreateProperties();
    if (props == 0 ||
        !g_sdl.SetNumberProperty(props, "SDL.texture.create.format",
            0x16362004u) ||
        !g_sdl.SetNumberProperty(props, "SDL.texture.create.width", width) ||
        !g_sdl.SetNumberProperty(props, "SDL.texture.create.height", height) ||
        !g_sdl.SetNumberProperty(props, "SDL.texture.create.vulkan.texture",
            (int64_t)image) ||
        !g_sdl.SetNumberProperty(props, "SDL.texture.create.vulkan.layout",
            layout)) {
        if (props != 0) g_sdl.DestroyProperties(props);
        ui_set_error(g_sdl.GetError());
        return false;
    }
    state->external_textures[slot] =
        g_sdl.CreateTextureWithProperties(state->renderer, props);
    g_sdl.DestroyProperties(props);
    if (state->external_textures[slot] == NULL) {
        ui_set_error(g_sdl.GetError());
        return false;
    }
    state->external_images[slot] = image;
    state->external_widths[slot] = width;
    state->external_heights[slot] = height;
    state->external_layouts[slot] = layout;
    state->external_texture = state->external_textures[slot];
    return true;
}

bool luma_external_vulkan_images_clear(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state == NULL) return false;
    ui_clear_external_textures(state);
    return true;
}

bool luma_external_image_draw(size_t handle, float x, float y,
    float width, float height) {
    UiWindow *state = ui_window(handle);
    if (state == NULL ||
        state->external_texture == NULL || width <= 0.0f || height <= 0.0f) {
        return false;
    }
    if (state->renderer_kind == 3u)
        return luma_vk_ui_texture_draw(state->direct_compositor,
            (size_t)(uintptr_t)state->external_texture, x, y, width, height);
    if (state->renderer == NULL) return false;
    UiSdlFRect destination = {x, y, width, height};
    bool rendered = g_sdl.RenderTexture(state->renderer,
        state->external_texture, NULL, &destination);
    if (!rendered) ui_set_error(g_sdl.GetError());
    return rendered;
}

bool luma_window_set_present_mode(size_t handle, uint32_t mode) {
    UiWindow *state = ui_window(handle);
    if (state != NULL && state->renderer_kind == 3u && mode <= 1u) {
        bool configured = luma_vk_ui_set_present_mode(state->direct_compositor, mode);
        if (!configured)
            ui_set_error("Unable to set direct Vulkan presentation mode");
        return configured;
    }
    if (state == NULL || state->renderer == NULL || mode > 1u) {
        ui_set_error("Invalid window or presentation mode");
        return false;
    }
    if (!g_sdl.SetRenderVSync(state->renderer, mode == 0u ? 1 : 0)) {
        ui_set_error(g_sdl.GetError());
        return false;
    }
    return true;
}

encore_str luma_backend_name(void) { return ui_string(g_sdl.ready ? "SDL3" : "unavailable"); }
encore_str luma_renderer_name(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state != NULL && state->renderer_kind == 3u) return ui_string("direct-vulkan");
    if (state == NULL || state->renderer == NULL || g_sdl.GetRendererName == NULL) return ui_string("unavailable");
    return ui_string(g_sdl.GetRendererName(state->renderer));
}
encore_str luma_renderer_device_name(size_t handle) {
    UiWindow *state = ui_window(handle);
    if (state != NULL && state->renderer_kind == 3u) return ui_string("direct-vulkan");
    if (state == NULL || state->renderer == NULL || g_sdl.GetRendererName == NULL)
        return ui_string("unavailable");
    const char *renderer = g_sdl.GetRendererName(state->renderer);
    if (renderer == NULL || strcmp(renderer, "vulkan") != 0)
        return ui_string(renderer == NULL ? "unavailable" : renderer);
    UiSdlPropertiesID props = g_sdl.GetRendererProperties(state->renderer);
    void *physical = g_sdl.GetPointerProperty(props,
        "SDL.renderer.vulkan.physical_device", NULL);
    if (physical == NULL) return ui_string("vulkan");

    /* VkPhysicalDeviceProperties starts with five u32 values followed by the
       fixed 256-byte deviceName. Keeping the rest opaque avoids a build-time
       Vulkan SDK dependency while still reporting the real adapter (including
       llvmpipe/lavapipe) on every Vulkan 1.x implementation. */
    typedef void (*UiVkGetPhysicalDeviceProperties)(void *, void *);
    unsigned char properties[1024];
    memset(properties, 0, sizeof(properties));
#if defined(_WIN32)
    HMODULE vulkan = LoadLibraryA("vulkan-1.dll");
    if (vulkan == NULL) return ui_string("vulkan");
    UiVkGetPhysicalDeviceProperties query =
        (UiVkGetPhysicalDeviceProperties)(uintptr_t)GetProcAddress(
            vulkan, "vkGetPhysicalDeviceProperties");
    if (query != NULL) query(physical, properties);
    FreeLibrary(vulkan);
#else
    void *vulkan = dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
#if defined(__APPLE__)
    if (vulkan == NULL) vulkan = dlopen("libvulkan.dylib", RTLD_NOW | RTLD_LOCAL);
    if (vulkan == NULL) vulkan = dlopen("libMoltenVK.dylib", RTLD_NOW | RTLD_LOCAL);
#endif
    if (vulkan == NULL) return ui_string("vulkan");
    UiVkGetPhysicalDeviceProperties query =
        (UiVkGetPhysicalDeviceProperties)dlsym(vulkan,
            "vkGetPhysicalDeviceProperties");
    if (query != NULL) query(physical, properties);
    dlclose(vulkan);
#endif
    const char *name = (const char *)(properties + 20);
    return ui_string(name[0] == '\0' ? "vulkan" : name);
}
encore_str luma_backend_error(void) { return ui_string(g_error); }
