#include <assert.h>
#include "sdl_mock.h"
#include "../workspace/ui_native/backend.c"

bool luma_vk_ui_texture_destroy(size_t handle) { (void)handle; return true; }
void luma_vk_ui_wake(size_t handle) { (void)handle; }

static UiSdlEvent events[16];
static size_t next_event, event_count;
static bool poll_event(UiSdlEvent *out) {
    if (next_event == event_count) return false;
    *out = events[next_event++];
    return true;
}
static void button(uint32_t kind, uint8_t number) {
    UiSdlMouseButtonEvent *event = (void *)&events[event_count++];
    event->type = kind;
    event->button = number;
    event->x = 120;
    event->y = 80;
}
int main(void) {
    UiWindow *state = calloc(1, sizeof(*state));
    assert(state);
    state->open = true;
    g_windows[0] = state;
    test_sdl.PollEvent = poll_event;
    size_t handle = (size_t)(uintptr_t)state;
    UiSdlKeyboardEvent *key = (void *)&events[event_count++];
    key->type = UI_SDL_EVENT_KEY_DOWN;
    key->mod = 192;
    button(UI_SDL_EVENT_MOUSE_BUTTON_DOWN, 1);
    button(UI_SDL_EVENT_MOUSE_BUTTON_DOWN, 3);
    button(UI_SDL_EVENT_MOUSE_BUTTON_UP, 1);
    button(UI_SDL_EVENT_MOUSE_BUTTON_UP, 3);
    button(UI_SDL_EVENT_MOUSE_BUTTON_DOWN, 2);
    events[event_count++].type = UI_SDL_EVENT_WINDOW_FOCUS_LOST;
    assert(ui_window_poll(handle) == UI_EVENT_KEY_DOWN);
    assert(ui_window_poll(handle) == UI_EVENT_POINTER_DOWN);
    assert(state->event_key == 1 && state->event_buttons == 1 && state->event_modifiers == 192);
    assert(ui_window_poll(handle) == UI_EVENT_POINTER_DOWN && state->event_buttons == 5);
    assert(ui_window_poll(handle) == UI_EVENT_POINTER_UP && state->event_buttons == 4);
    assert(ui_window_poll(handle) == UI_EVENT_POINTER_UP && state->event_buttons == 0);
    assert(ui_window_poll(handle) == UI_EVENT_POINTER_DOWN && state->event_buttons == 2);
    assert(ui_window_poll(handle) == UI_EVENT_FOCUS_LOST && state->event_buttons == 0);
    assert(ui_window_poll(handle) == UI_EVENT_NONE && state->event_modifiers == 0);
    // Coalescing held motion must preserve its state across a queued release.
    UiSdlMouseMotionEvent *motion = (void *)&events[event_count++];
    motion->type = UI_SDL_EVENT_MOUSE_MOTION;
    motion->state = 1;
    motion->x = 130;
    button(UI_SDL_EVENT_MOUSE_BUTTON_UP, 1);
    assert(ui_window_poll(handle) == UI_EVENT_POINTER_MOVE && state->event_buttons == 1);
    assert(ui_window_poll(handle) == UI_EVENT_POINTER_UP && state->event_buttons == 0);
    g_windows[0] = NULL;
    free(state);
    return 0;
}
