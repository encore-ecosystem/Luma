#include <assert.h>
#include "sdl_mock.h"
#include "../workspace/ui_native/backend.c"

bool luma_vk_ui_texture_destroy(size_t handle) { (void)handle; return true; }
void luma_vk_ui_wake(size_t handle) { (void)handle; }

static UiSdlEvent events[8];
static size_t next_event, event_count;
static bool poll_event(UiSdlEvent *out) {
    if (next_event == event_count) return false;
    *out = events[next_event++];
    return true;
}
static void wheel(float x, float amount) {
    UiSdlMouseWheelEvent *event = (UiSdlMouseWheelEvent *)&events[event_count++];
    event->type = UI_SDL_EVENT_MOUSE_WHEEL;
    event->mouse_x = x;
    event->mouse_y = 20;
    event->y = amount;
}
int main(void) {
    UiWindow *state = calloc(1, sizeof(*state));
    assert(state);
    state->open = true;
    g_windows[0] = state;
    test_sdl.PollEvent = poll_event;
    size_t handle = (size_t)(uintptr_t)state;
    wheel(10, -1);
    wheel(10, -2);
    wheel(10, 1); // Reversal must not cancel a preceding clamped scroll.
    wheel(30, 1); // Different pointer target must remain separate.
    events[event_count++].type = UI_SDL_EVENT_MOUSE_BUTTON_UP;
    wheel(30, 2);
    assert(ui_window_poll(handle) == UI_EVENT_SCROLL && state->wheel_y == -3);
    assert(ui_window_poll(handle) == UI_EVENT_SCROLL && state->wheel_y == 1 && state->event_x == 10);
    assert(ui_window_poll(handle) == UI_EVENT_SCROLL && state->wheel_y == 1 && state->event_x == 30);
    assert(ui_window_poll(handle) == UI_EVENT_POINTER_UP);
    assert(ui_window_poll(handle) == UI_EVENT_SCROLL && state->wheel_y == 2);
    assert(ui_window_poll(handle) == UI_EVENT_NONE);
    g_windows[0] = NULL;
    free(state);
    return 0;
}
