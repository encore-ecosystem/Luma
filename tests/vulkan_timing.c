/* Real-device UI timestamp integration and cumulative host counters. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../runtime.c"
static UiSdlApi mock_sdl;
const UiSdlApi *encore_sdl_api_v1(void){return &mock_sdl;}
void *encore_str_from_cstr(const char *s){size_t n=strlen(s);encore_str_object*o=malloc(sizeof(*o)+n+1);o->ref_count=1;o->len=n;memcpy(o->data,s,n+1);return o;}
extern uint32_t encore_vk_timing_support(size_t);
extern bool encore_vk_timing_begin(size_t,uint64_t);
extern void encore_vk_timing_finish(size_t,bool);
extern uint64_t encore_vk_timing_poll(size_t);
extern uint64_t encore_vk_timing_value(size_t,uint32_t,uint32_t);
extern void encore_vk_timing_release(size_t);
static uint64_t clock_ns(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return(uint64_t)t.tv_sec*1000000000ull+t.tv_nsec;}
int main(void){
 if(getenv("LUMA_TEST_PARALLEL"))assert(encore_vk_parallel_configure(4));
 mock_sdl.GetTicksNS=clock_ns;
 size_t context=encore_vk_create(false);assert(encore_vk_available(context));
 size_t device=encore_vk_device_create(context,0);assert(encore_vk_device_available(device));assert(encore_vk_timing_support(device));
 size_t target=encore_vk_offscreen_create(device,64,64);size_t ui=luma_vk_ui_create(device,target);assert(luma_vk_ui_available(ui));
 for(unsigned n=1;n<=8;++n){
  assert(encore_vk_timing_begin(device,n));assert(luma_vk_ui_begin(ui,0xff151619));assert(luma_vk_ui_end(ui));encore_vk_timing_finish(device,true);
  assert(encore_vk_swapchain_wait(target));assert(encore_vk_timing_poll(device)==n);
  assert(encore_vk_timing_value(device,0,0)==1);assert(encore_vk_timing_value(device,0,1)==1);assert(encore_vk_timing_value(device,0,2)==8);assert(encore_vk_timing_value(device,0,4)>0);
  encore_vk_timing_release(device);
 }
 assert(luma_vk_ui_timing_value(ui,0)>0&&luma_vk_ui_timing_value(ui,1)>0&&luma_vk_ui_timing_value(ui,2)>0);
 assert(luma_vk_ui_timing_value(ui,0)+luma_vk_ui_timing_value(ui,1)==luma_vk_ui_presentation_ns(ui));
 assert(luma_vk_ui_destroy(ui));assert(encore_vk_swapchain_destroy(target));assert(encore_vk_device_destroy(device));assert(encore_vk_destroy(context));puts("UI GPU timestamps and host timings: pass");
}
