#include <assert.h>
#include <stdio.h>
#include "../../main/apps/pvz_almanac/pvz_almanac.c"
static int requested=-1, audio_state, backlight;
static bool audio_available;
int bsp_battery_soc(void) {return 87;}
uint32_t esp_random(void) {return 2026;}
void bsp_display_backlight(uint8_t level) {backlight=level;}
bool pvz_audio_start(bool ok) {audio_available=ok;audio_state=0;return ok;}
void pvz_audio_request(int clip) {requested=clip;audio_state=clip<0?0:1;}
int pvz_audio_status(void) {return audio_available?audio_state:-1;}
void pvz_audio_stop(void) {audio_available=false;requested=-1;}
static void key(bsp_btn_t b,bsp_btn_ev_t e) {pvz_almanac_key(b,e);frame(NULL);}
int main(void) {
    lv_init();assert(lv_display_create(240,320));
    pvz_almanac_prepare();pvz_almanac_enter(true,true);
    pvz_almanac_key(BSP_BTN_OK,BSP_BTN_CLICK);
    assert(state.page==PVZ_HOME); /* callback does not mutate UI/state */
    frame(NULL);assert(state.page==PVZ_BROWSE);
    key(BSP_BTN_OK,BSP_BTN_CLICK);assert(requested==0);
    key(BSP_BTN_DOWN,BSP_BTN_CLICK);assert(state.index==1 && requested==-1);
    key(BSP_BTN_UP,BSP_BTN_LONG);assert(state.index==1 && requested==1);
    key(BSP_BTN_OK,BSP_BTN_LONG);assert(state.page==PVZ_HOME && requested==-1);
    key(BSP_BTN_UP,BSP_BTN_CLICK);assert(state.menu==2);
    key(BSP_BTN_OK,BSP_BTN_CLICK);assert(state.page==PVZ_QUIZ && requested>=PVZ_COUNT);
    unsigned answer=state.deck[state.round];
    while(state.options[state.choice]!=answer) key(BSP_BTN_DOWN,BSP_BTN_CLICK);
    key(BSP_BTN_OK,BSP_BTN_CLICK);assert(state.page==PVZ_REVEAL && state.correct && requested==(int)answer);
    key(BSP_BTN_OK,BSP_BTN_LONG);
    audio_state=0;lv_tick_inc(60001);frame(NULL);assert(dimmed && backlight==20);
    unsigned menu=state.menu;
    key(BSP_BTN_DOWN,BSP_BTN_CLICK);assert(!dimmed && backlight==100 && state.menu==menu);
    key(BSP_BTN_DOWN,BSP_BTN_CLICK);assert(state.menu==(menu+1)%3);
    unsigned old=state.menu;key(BSP_BTN_DOWN,BSP_BTN_PRESS);key(BSP_BTN_DOWN,BSP_BTN_DOUBLE);
    assert(state.menu==old);
    for(unsigned i=0;i<1000;i++) pvz_almanac_key(BSP_BTN_DOWN,BSP_BTN_CLICK);
    assert(keys->count==12);frame(NULL);assert(keys->count==0);
    pvz_almanac_exit();pvz_almanac_key(BSP_BTN_OK,BSP_BTN_CLICK);assert(keys->count==0);
    for(unsigned i=0;i<100;i++) {pvz_almanac_enter(false,true);assert(pvz_audio_status()==-1);pvz_almanac_exit();}
    pvz_almanac_enter(false,false);key(BSP_BTN_OK,BSP_BTN_CLICK);assert(state.page==PVZ_HOME);
    pvz_almanac_exit();puts("PVZ input queue, playback dispatch, wake, overload, missing peripherals and 100 lifecycles PASS");
}
