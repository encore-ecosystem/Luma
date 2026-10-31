/* Mock only the public SDL ABI; Luma's tests do not own an SDL loader. */
#include "../workspace/ui_native/sdl_api.h"
static UiSdlApi test_sdl;
static UiTtfApi test_ttf;
const UiSdlApi *encore_sdl_api_v1(void) { return &test_sdl; }
const UiTtfApi *encore_sdl_ttf_api_v1(void) { return &test_ttf; }
bool encore_sdl_poll_raw(UiSdlWindow *window, UiSdlEvent *event) {
    (void)window;
    return test_sdl.PollEvent && test_sdl.PollEvent(event);
}
