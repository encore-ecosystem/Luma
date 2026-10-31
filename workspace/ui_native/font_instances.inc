/* Public HarfBuzz 6+ instancing ABI. No SDL_ttf private structures or disk
 * intermediates. Each window owns a bounded, size-independent instance LRU;
 * open SDL fonts retain their own blob references across LRU eviction. */
typedef struct {
    unsigned axis_index, tag, name_id, flags;
    float min_value, default_value, max_value;
    unsigned reserved;
} UiHbAxis;

static struct {
    bool attempted, ready;
    void *base, *subset;
    void *(*blob_file)(const char *);
    void *(*blob_ref)(void *);
    void (*blob_destroy)(void *);
    const char *(*blob_data)(void *, unsigned *);
    unsigned (*blob_length)(void *);
    void *(*face_create)(void *, unsigned);
    void (*face_destroy)(void *);
    void *(*face_blob)(void *);
    unsigned (*glyph_count)(void *);
    unsigned (*axis_count)(void *);
    unsigned (*axes)(void *, unsigned, unsigned *, UiHbAxis *);
    void *(*input_create)(void);
    void (*input_destroy)(void *);
    void *(*input_set)(void *, unsigned);
    void (*set_invert)(void *);
    void (*set_clear)(void *);
    void (*input_flags)(void *, unsigned);
    int (*pin)(void *, void *, unsigned, float);
    void *(*instantiate)(void *, const void *);
} g_hb;

typedef struct {
    char *path;
    uint32_t weight;
    void *blob;
    bool variable_weight;
    uint64_t used_at;
    size_t bytes;
} UiFontInstance;

struct UiFontInstanceCache {
    UiFontInstance entries[16];
    size_t count, bytes;
    uint64_t tick;
};

static void *ui_font_library(const char *name) {
#if defined(_WIN32)
    return (void *)LoadLibraryA(name);
#else
    return dlopen(name, RTLD_NOW | RTLD_LOCAL);
#endif
}

static bool ui_load_harfbuzz(void) {
    if (g_hb.attempted) {
        if (!g_hb.ready) ui_set_error("Font weights require HarfBuzz subset 6.0 or newer");
        return g_hb.ready;
    }
    g_hb.attempted = true;
#if defined(_WIN32)
    g_hb.base = ui_font_library("libharfbuzz-0.dll");
    if (!g_hb.base) g_hb.base = ui_font_library("harfbuzz.dll");
    g_hb.subset = ui_font_library("libharfbuzz-subset-0.dll");
    if (!g_hb.subset) g_hb.subset = ui_font_library("harfbuzz-subset.dll");
#elif defined(__APPLE__)
    g_hb.base = ui_font_library("libharfbuzz.0.dylib");
    if (!g_hb.base) g_hb.base = ui_font_library("/opt/homebrew/lib/libharfbuzz.0.dylib");
    g_hb.subset = ui_font_library("libharfbuzz-subset.0.dylib");
    if (!g_hb.subset) g_hb.subset = ui_font_library("/opt/homebrew/lib/libharfbuzz-subset.0.dylib");
#else
    g_hb.base = ui_font_library("libharfbuzz.so.0");
    g_hb.subset = ui_font_library("libharfbuzz-subset.so.0");
#endif
    if (!g_hb.base || !g_hb.subset) {
        ui_set_error("Font weights require HarfBuzz subset 6.0 or newer");
        return false;
    }
#define UI_HB(field, symbol) do { \
    *(void **)(&g_hb.field) = ui_symbol(g_hb.base, symbol); \
    if (!g_hb.field) *(void **)(&g_hb.field) = ui_symbol(g_hb.subset, symbol); \
    if (!g_hb.field) { ui_set_error("HarfBuzz subset is missing " symbol); return false; } \
} while (0)
    UI_HB(blob_file, "hb_blob_create_from_file_or_fail");
    UI_HB(blob_ref, "hb_blob_reference");
    UI_HB(blob_destroy, "hb_blob_destroy");
    UI_HB(blob_data, "hb_blob_get_data");
    UI_HB(blob_length, "hb_blob_get_length");
    UI_HB(face_create, "hb_face_create");
    UI_HB(face_destroy, "hb_face_destroy");
    UI_HB(face_blob, "hb_face_reference_blob");
    UI_HB(glyph_count, "hb_face_get_glyph_count");
    UI_HB(axis_count, "hb_ot_var_get_axis_count");
    UI_HB(axes, "hb_ot_var_get_axis_infos");
    UI_HB(input_create, "hb_subset_input_create_or_fail");
    UI_HB(input_destroy, "hb_subset_input_destroy");
    UI_HB(input_set, "hb_subset_input_set");
    UI_HB(set_invert, "hb_set_invert");
    UI_HB(set_clear, "hb_set_clear");
    UI_HB(input_flags, "hb_subset_input_set_flags");
    UI_HB(pin, "hb_subset_input_pin_axis_location");
    UI_HB(instantiate, "hb_subset_or_fail");
#undef UI_HB
    g_hb.ready = true;
    return true;
}

static void ui_font_blob_release(void *blob) {
    if (blob) g_hb.blob_destroy(blob);
}

static void ui_font_instances_clear(UiFontInstanceCache **cache) {
    if (!*cache) return;
    for (size_t i = 0; i < (*cache)->count; ++i) {
        free((*cache)->entries[i].path);
        ui_font_blob_release((*cache)->entries[i].blob);
    }
    free(*cache);
    *cache = NULL;
}

static void *ui_font_instance(UiFontInstanceCache **owner, const char *path,
    uint32_t weight, bool *variable_weight) {
    if (!ui_load_harfbuzz()) return NULL;
    if (!*owner) *owner = calloc(1, sizeof(**owner));
    if (!*owner) { ui_set_error("Unable to allocate font instance cache"); return NULL; }
    UiFontInstanceCache *cache = *owner;
    ++cache->tick;
    for (size_t i = 0; i < cache->count; ++i) {
        UiFontInstance *entry = &cache->entries[i];
        if (entry->weight == weight && strcmp(entry->path, path) == 0) {
            entry->used_at = cache->tick;
            *variable_weight = entry->variable_weight;
            return g_hb.blob_ref(entry->blob);
        }
    }
    void *blob = g_hb.blob_file(path);
    if (!blob || !g_hb.blob_length(blob) || g_hb.blob_length(blob) > 64u * 1024u * 1024u) {
        ui_font_blob_release(blob);
        ui_set_error("Unable to read font (maximum font size is 64 MiB)");
        return NULL;
    }
    void *face = g_hb.face_create(blob, 0);
    unsigned count = g_hb.axis_count(face);
    *variable_weight = false;
    if (count > 64) {
        g_hb.face_destroy(face); g_hb.blob_destroy(blob);
        ui_set_error("Font has too many variation axes"); return NULL;
    }
    UiHbAxis axes[64];
    if (count) g_hb.axes(face, 0, &count, axes);
    for (unsigned i = 0; i < count; ++i)
        if (axes[i].tag == 0x77676874u) *variable_weight = true; /* wght */
    if (*variable_weight) {
        void *input = g_hb.input_create();
        bool valid = input != NULL;
        if (valid) {
            /* Preserve all glyphs, names, languages and layout features,
             * including ligatures and codepoints not used by the current text. */
            const unsigned sets[] = {0, 1, 4, 5, 6, 7};
            for (size_t i = 0; i < sizeof(sets) / sizeof(sets[0]); ++i) {
                void *set = g_hb.input_set(input, sets[i]);
                g_hb.set_clear(set); g_hb.set_invert(set);
            }
            g_hb.input_flags(input, 0x02u | 0x20u | 0x40u | 0x80u);
            for (unsigned i = 0; i < count; ++i) {
                float value = axes[i].tag == 0x77676874u ? (float)weight : axes[i].default_value;
                if (value < axes[i].min_value) value = axes[i].min_value;
                if (value > axes[i].max_value) value = axes[i].max_value;
                if (!g_hb.pin(input, face, axes[i].tag, value)) valid = false;
            }
        }
        void *instance = valid ? g_hb.instantiate(face, input) : NULL;
        if (input) g_hb.input_destroy(input);
        if (!instance || g_hb.glyph_count(instance) != g_hb.glyph_count(face)) {
            if (instance) g_hb.face_destroy(instance);
            g_hb.face_destroy(face); g_hb.blob_destroy(blob);
            ui_set_error("Unable to instantiate variable font weight"); return NULL;
        }
        g_hb.blob_destroy(blob);
        blob = g_hb.face_blob(instance);
        g_hb.face_destroy(instance);
    }
    g_hb.face_destroy(face);
    size_t bytes = g_hb.blob_length(blob);
    if (!bytes || bytes > 64u * 1024u * 1024u) {
        g_hb.blob_destroy(blob); ui_set_error("Invalid instantiated font size"); return NULL;
    }
    const size_t budget = 16u * 1024u * 1024u;
    if (bytes <= budget) {
        while (cache->count && (cache->count == 16 || cache->bytes + bytes > budget)) {
            size_t oldest = 0;
            for (size_t i = 1; i < cache->count; ++i)
                if (cache->entries[i].used_at < cache->entries[oldest].used_at) oldest = i;
            UiFontInstance *entry = &cache->entries[oldest];
            cache->bytes -= entry->bytes;
            free(entry->path); g_hb.blob_destroy(entry->blob);
            *entry = cache->entries[--cache->count];
        }
        char *key = ui_copy_cstr(path);
        if (key) {
            cache->entries[cache->count++] = (UiFontInstance){
                key, weight, g_hb.blob_ref(blob), *variable_weight, cache->tick, bytes};
            cache->bytes += bytes;
        }
    }
    return blob;
}

typedef struct { void *blob, *io; } UiFontBacking;

/* Called only after TTF_CloseFont; the stream and bytes outlive the face. */
static void ui_font_backing_release(void *value) {
    UiFontBacking *backing = value;
    if (!backing) return;
    if (backing->io) g_sdl.CloseIO(backing->io);
    ui_font_blob_release(backing->blob);
    free(backing);
}

static UiTtfFont *ui_open_weighted_font(UiFontInstanceCache **cache,
    const char *path, float pixels, uint32_t weight, void **backing) {
    bool variable = false;
    *backing = NULL;
    UiFontBacking *owned = calloc(1, sizeof(*owned));
    if (!owned) { ui_set_error("Unable to allocate font backing"); return NULL; }
    owned->blob = ui_font_instance(cache, path, weight, &variable);
    if (!owned->blob) { free(owned); return NULL; }
    unsigned length = 0;
    const char *data = g_hb.blob_data(owned->blob, &length);
    owned->io = g_sdl.IOFromConstMem(data, length);
    UiTtfFont *font = owned->io ? g_ttf.OpenFontIO(owned->io, false, pixels) : NULL;
    if (!font) {
        ui_font_backing_release(owned);
        ui_set_error(g_sdl.GetError()); return NULL;
    }
    *backing = owned;
    g_ttf.SetFontStyle(font, !variable && weight >= 600 ? 0x01u : 0x00u);
    return font;
}
