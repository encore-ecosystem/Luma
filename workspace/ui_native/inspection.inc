/* On-demand diagnostics. No readback or encoding occurs in ordinary frames. */
extern encore_str encore_str_from_buffer(const void *, size_t);
typedef struct { bool (*push)(UiSdlEvent *); uint32_t window; } LumaWindowWaker;
size_t luma_window_waker(size_t handle) {
    UiWindow *window = ui_window(handle);
    if (!window || !window->window) return 0;
    LumaWindowWaker *waker = calloc(1,sizeof *waker);
    if (!waker) return 0;
    *(void **)(&waker->push) = ui_symbol(g_sdl.library,"SDL_PushEvent");
    waker->window = g_sdl.GetWindowID(window->window);
    if (!waker->push) { free(waker); return 0; }
    return (size_t)waker;
}
bool luma_waker_wake(size_t handle) {
    LumaWindowWaker *waker = (void *)handle;
    if (!waker || !waker->push) return false;
    UiSdlEvent event = {0};
    UiSdlWindowEvent *exposed = (void *)&event;
    exposed->type = UI_SDL_EVENT_WINDOW_EXPOSED; exposed->window_id = waker->window;
    return waker->push(&event);
}
void luma_waker_destroy(size_t handle) { free((void *)handle); }
bool luma_window_capture_request(size_t handle) {
    UiWindow *window = ui_window(handle);
    if (!window || !window->open) return false;
    window->capture_present_count=window->present_count+1;
    if (window->direct_compositor) {
        LumaVkUiCompositor *ui = (void *)window->direct_compositor;
        ui->capture_ready = false; ui->capture_requested = true;
        return true;
    }
    return window->renderer != NULL;
}
uint64_t luma_window_capture_frame(size_t handle) {
    UiWindow *window=ui_window(handle);return window?window->capture_present_count:0;
}
static uint32_t luma_png_crc(const unsigned char *data, size_t size) {
    uint32_t crc = ~0u;
    for (size_t i=0;i<size;++i) {
        crc ^= data[i];
        for (unsigned bit=0;bit<8;++bit) crc = (crc>>1)^((0u-(crc&1u))&0xedb88320u);
    }
    return ~crc;
}
static void luma_png_word(unsigned char *p,uint32_t value) { p[0]=value>>24;p[1]=value>>16;p[2]=value>>8;p[3]=value; }
static size_t luma_png_chunk(unsigned char *out,const char *type,const void *data,size_t bytes) {
    luma_png_word(out,(uint32_t)bytes);memcpy(out+4,type,4);
    if (bytes) memcpy(out+8,data,bytes);
    luma_png_word(out+8+bytes,luma_png_crc(out+4,bytes+4));
    return bytes+12;
}
/* Returns a compact PNG of the last completed requested frame. Cropping uses
 * physical pixels. zlib is loaded on demand, and never becomes an idle cost. */
encore_str luma_window_capture_png(size_t handle,uint32_t x,uint32_t y,uint32_t width,uint32_t height,uint32_t maximum) {
    encore_str empty = {0};
    UiWindow *window = ui_window(handle);
    if (!window || !maximum || maximum > 3840) return empty.object ? empty : (encore_str){encore_str_from_cstr("")};
    unsigned char *source = NULL;
    uint32_t source_width=0,source_height=0,pitch=0,format=0;
    LumaVkUiCompositor *ui = (void *)window->direct_compositor;
    UiSdlSurface *surface = NULL;
    if (ui) {
        if (!ui->capture_ready || !ui->capture_buffer || !encore_vk_swapchain_wait((size_t)ui->swapchain)) return empty.object ? empty : (encore_str){encore_str_from_cstr("")};
        source_width=ui->capture_width;source_height=ui->capture_height;pitch=source_width*4;format=ui->capture_format;
        source=encore_vk_buffer_map((size_t)ui->capture_buffer,(uint64_t)pitch*source_height);
        if (!source) return empty.object ? empty : (encore_str){encore_str_from_cstr("")};
    } else {
        surface=g_sdl.RenderReadPixels(window->renderer,NULL);
        if (!surface) return empty.object ? empty : (encore_str){encore_str_from_cstr("")};
        source_width=surface->width;source_height=surface->height;
    }
    if (x>=source_width || y>=source_height) goto done;
    if (!width || width>source_width-x) width=source_width-x;
    if (!height || height>source_height-y) height=source_height-y;
    uint32_t output_width=width>maximum?maximum:width;
    uint32_t output_height=(uint64_t)height*output_width/width;
    if (!output_height) output_height=1;
    uint64_t row=(uint64_t)output_width*4+1,raw_size=row*output_height;
    if (raw_size>64u*1024u*1024u) goto done;
    unsigned char *raw=malloc((size_t)raw_size);
    if (!raw) goto done;
    for (uint32_t j=0;j<output_height;++j) {
        raw[j*row]=0;
        uint32_t sy=y+(uint64_t)j*height/output_height;
        for (uint32_t i=0;i<output_width;++i) {
            uint32_t sx=x+(uint64_t)i*width/output_width;
            unsigned char *pixel=raw+j*row+1+i*4;
            if (source) {
                const unsigned char *p=source+(size_t)sy*pitch+sx*4;
                bool bgra=format==44u || format==50u;
                pixel[0]=p[bgra?2:0];pixel[1]=p[1];pixel[2]=p[bgra?0:2];pixel[3]=255;
            } else {
                uint8_t alpha;
                g_sdl.ReadSurfacePixel(surface,sx,sy,&pixel[0],&pixel[1],&pixel[2],&alpha);pixel[3]=255;
            }
        }
    }
    unsigned char *compressed=NULL,*png=NULL;
    unsigned long compressed_size=(unsigned long)raw_size+(unsigned long)raw_size/1000+64;
    void *library=NULL;
#if !defined(_WIN32)
    library=dlopen("libz.so.1",RTLD_NOW|RTLD_LOCAL);
    int (*compress_png)(unsigned char *,unsigned long *,const unsigned char *,unsigned long,int)=NULL;
    if (library) *(void **)(&compress_png)=dlsym(library,"compress2");
    if (compress_png) {
        compressed=malloc(compressed_size);
        if (compressed && compress_png(compressed,&compressed_size,raw,(unsigned long)raw_size,3)==0) {
            png=malloc(compressed_size+57);
            if (png) {
                memcpy(png,"\x89PNG\r\n\x1a\n",8);
                unsigned char ihdr[13]={0};luma_png_word(ihdr,output_width);luma_png_word(ihdr+4,output_height);ihdr[8]=8;ihdr[9]=6;
                size_t count=8;count+=luma_png_chunk(png+count,"IHDR",ihdr,13);
                count+=luma_png_chunk(png+count,"IDAT",compressed,compressed_size);
                count+=luma_png_chunk(png+count,"IEND",NULL,0);
                empty=encore_str_from_buffer(png,count);
            }
        }
    }
#endif
    free(raw);free(compressed);free(png);
#if !defined(_WIN32)
    if (library) dlclose(library);
#endif
 done:
    if (source) encore_vk_buffer_unmap((size_t)ui->capture_buffer);
    if (surface) g_sdl.DestroySurface(surface);
    return empty.object ? empty : (encore_str){encore_str_from_cstr("")};
}
