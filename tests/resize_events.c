#include <assert.h>
#include "sdl_mock.h"
#include "../workspace/ui_native/backend.c"

bool luma_vk_ui_texture_destroy(size_t handle) { (void)handle; return true; }
static unsigned wakes;
void luma_vk_ui_wake(size_t handle) { (void)handle; ++wakes; }
uint32_t luma_vk_ui_acquire_status(size_t handle) { (void)handle; return 2; }
static UiSdlEvent events[32];
static size_t next_event, event_count;
static bool poll_event(UiSdlEvent *out) {
    if (next_event == event_count) return false;
    *out = events[next_event++];
    return true;
}
static void resize(int width, int height) {
    UiSdlWindowEvent *event = (UiSdlWindowEvent *)&events[event_count++];
    event->type = UI_SDL_EVENT_WINDOW_RESIZED;
    event->data1 = width;
    event->data2 = height;
}
int main(void) {
    UiWindow *state = calloc(1, sizeof(*state));
    assert(state);
    state->open = true;
    g_windows[0] = state;
    test_sdl.PollEvent = poll_event;
    size_t handle = (size_t)(uintptr_t)state;
    for (int width = 1000; width <= 1008; ++width) resize(width, 600);
    events[event_count++].type = UI_SDL_EVENT_MOUSE_BUTTON_UP;
    resize(1010, 610);
    assert(ui_window_poll(handle) == UI_EVENT_RESIZE);
    assert(state->width == 1008 && state->height == 600);
    assert(ui_window_poll(handle) == UI_EVENT_POINTER_UP);
    assert(ui_window_poll(handle) == UI_EVENT_RESIZE);
    assert(state->width == 1010 && state->height == 610);
    assert(ui_window_poll(handle) == UI_EVENT_NONE);
    // Further movement produces a resize immediately, without any release.
    resize(1012, 612);
    assert(ui_window_poll(handle) == UI_EVENT_RESIZE);
    assert(state->width == 1012 && state->height == 612);
    assert(ui_window_poll(handle) == UI_EVENT_NONE);
    state->direct_compositor = 1;
    events[event_count++].type = UI_SDL_EVENT_WINDOW_MINIMIZED;
    assert(ui_window_poll(handle) == UI_EVENT_NONE);
    assert(luma_frame_status(handle) == 3);
    unsigned before = wakes;
    events[event_count++].type = UI_SDL_EVENT_WINDOW_RESTORED;
    assert(ui_window_poll(handle) == UI_EVENT_NONE);
    assert(luma_frame_status(handle) == 2 && wakes == before + 1);
    state->open = false;
    assert(luma_frame_status(handle) == 0);
    g_windows[0] = NULL;
    free(state);
    return 0;
}
