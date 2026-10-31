#include <assert.h>
#include "sdl_mock.h"
#include "../workspace/ui_native/backend.c"
static unsigned draws;
static UiSdlVertex captured[4];
bool luma_vk_ui_geometry(size_t h,const float*v,size_t n,size_t s,size_t c,const int*i,size_t z) {
    (void)h;(void)v;(void)n;(void)s;(void)c;(void)i;(void)z;return false;
}
static bool draw(UiSdlRenderer*r,UiSdlTexture*t,const UiSdlVertex*v,int n,const int*i,int count) {
    (void)r;(void)t;assert(n==4 && count==6);assert(i[0]==0 && i[5]==3);
    memcpy(captured,v,sizeof(captured));++draws;return true;
}
int main(void) {
    UiWindow state={0};state.open=true;state.renderer=(UiSdlRenderer*)1;
    assert(ui_register_window(&state));test_sdl.RenderGeometry=draw;
    assert(luma_gradient_rect((size_t)&state,5.25f,8.5f,208,208,0x00000000,0x000000ff,true));
    assert(draws==1);assert(captured[0].position.x==5.25f && captured[2].position.y==216.5f);
    assert(captured[0].color.a==0 && captured[1].color.a==0);
    assert(captured[2].color.a==1 && captured[3].color.a==1);
    assert(luma_gradient_rect((size_t)&state,0,0,20,50,0xff0000ff,0x0000ffff,false));
    assert(draws==2);assert(captured[0].color.r==1 && captured[3].color.r==1);
    assert(captured[1].color.b==1 && captured[2].color.b==1);
    assert(!luma_gradient_rect((size_t)&state,0,0,0,50,0,0,true));assert(draws==2);
    return 0;
}
