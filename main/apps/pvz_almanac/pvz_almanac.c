#include "pvz_almanac.h"
#include "pvz_state.h"
#include "pvz_audio.h"
#include "pvz_view.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdatomic.h>
typedef struct { bsp_btn_t btn; bsp_btn_ev_t event; } key_t;
static StaticQueue_t key_control;
static uint8_t key_storage[12*sizeof(key_t)];
static QueueHandle_t keys;
static atomic_bool active;
static pvz_state_t state;
static lv_timer_t *timer;
static uint32_t last_input, last_battery;
static bool buttons, dimmed;
static int last_status;
static void render(void) {
    pvz_view_render(&state,buttons);
    last_status=-99;
}
static void speak(void) {
    if(state.page==PVZ_QUIZ) pvz_audio_request(PVZ_COUNT+state.deck[state.round]);
    else if(state.page==PVZ_BROWSE || state.page==PVZ_REVEAL) pvz_audio_request(state.index);
}
static void handle(key_t k) {
    uint32_t now=lv_tick_get();last_input=now;
    if(dimmed) {dimmed=false;bsp_display_backlight(100);return;}
    if(k.event==BSP_BTN_LONG) {
        if(k.btn==BSP_BTN_OK) {pvz_audio_request(-1);pvz_home(&state);render();}
        else if(k.btn==BSP_BTN_UP) speak();
        return;
    }
    if(k.btn!=BSP_BTN_OK) {
        pvz_page_t before=state.page;
        pvz_move(&state,k.btn==BSP_BTN_UP?-1:1);
        if(before==PVZ_BROWSE) pvz_audio_request(-1);
        render();return;
    }
    if(state.page==PVZ_BROWSE) {speak();return;}
    pvz_audio_request(-1);
    if(state.page==PVZ_HOME) pvz_open(&state,state.menu,esp_random()%10000U);
    else pvz_confirm(&state);
    render();
    if(state.page==PVZ_QUIZ || state.page==PVZ_REVEAL) speak();
}
static void frame(lv_timer_t *t) {
    (void)t;
    key_t key;
    for(unsigned n=0;n<12 && xQueueReceive(keys,&key,0)==pdTRUE;n++) handle(key);
    int status=pvz_audio_status();
    if(status!=last_status) {pvz_view_status(status);last_status=status;}
    uint32_t now=lv_tick_get();
    if(now-last_battery>=30000U) {pvz_view_battery(bsp_battery_soc());last_battery=now;}
    if(!dimmed && now-last_input>=60000U && status!=1) {
        dimmed=true;bsp_display_backlight(20);
    }
}
void pvz_almanac_prepare(void) {
    if(!keys) keys=xQueueCreateStatic(12,sizeof(key_t),key_storage,&key_control);
}
void pvz_almanac_enter(bool audio_ok,bool buttons_ok) {
    if(atomic_load(&active)) return;
    pvz_almanac_prepare();buttons=buttons_ok && keys;
    xQueueReset(keys);pvz_init(&state);
    pvz_audio_start(audio_ok);
    lv_screen_load(pvz_view_create());render();
    pvz_view_battery(bsp_battery_soc());
    last_input=last_battery=lv_tick_get();dimmed=false;
    bsp_display_backlight(100);
    timer=lv_timer_create(frame,25,NULL);
    atomic_store(&active,true);
}
void pvz_almanac_key(bsp_btn_t btn,bsp_btn_ev_t event) {
    if(!atomic_load(&active) || !buttons || !keys ||
        (event!=BSP_BTN_CLICK && event!=BSP_BTN_LONG)) return;
    key_t key={btn,event};xQueueSend(keys,&key,0);
}
void pvz_almanac_exit(void) {
    atomic_store(&active,false);
    if(timer) {lv_timer_delete(timer);timer=NULL;}
    pvz_audio_stop(); /* worker never accesses LVGL, so joining under its lock is safe */
    if(keys) xQueueReset(keys);
    pvz_view_destroy();
}
