#include "excuse_call.h"
#include "excuse_call_state.h"
#include "excuse_call_audio.h"
#include "bsp_battery.h"
#include "esp_timer.h"
#include "lvgl.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(excuse_call_zh_16);
LV_FONT_DECLARE(excuse_call_zh_32);
LV_FONT_DECLARE(excuse_call_zh_24);
LV_IMAGE_DECLARE(ec_call_icon);
LV_IMAGE_DECLARE(ec_person_icon);
static ec_state_t s_state;
static atomic_bool s_accept;
static atomic_uint s_confirms;
static atomic_int s_selection;
static bool s_buttons;
static lv_obj_t *s_screen,*s_battery,*s_status,*s_caller,*s_action;
static lv_obj_t *s_battery_fill,*s_button,*s_halos[2];
static lv_timer_t *s_timer,*s_battery_timer;
static uint64_t s_ended_until;
static unsigned s_last_caller;
static uint64_t now_ms(void) { return (uint64_t)(esp_timer_get_time()/1000); }
static lv_obj_t *shape(lv_obj_t *parent,int x,int y,int w,int h,int radius,uint32_t color)
{
    lv_obj_t *o=lv_obj_create(parent); lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_radius(o,radius,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); return o;
}
static lv_obj_t *label(int x,int y,int w,bool big,uint32_t color,const char *text)
{
    lv_obj_t *o=lv_label_create(s_screen);
    lv_obj_set_style_text_font(o,big ? &excuse_call_zh_32 : &excuse_call_zh_16,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,big ? 42 : 22);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP); lv_label_set_text(o,text); return o;
}
static void set_text(lv_obj_t *label,const char *text)
{ if (strcmp(lv_label_get_text(label),text)) lv_label_set_text(label,text); }
static void battery(lv_timer_t *timer)
{
    (void)timer; int n=bsp_battery_soc(); char text[12];
    if(n<0) snprintf(text,sizeof text,"--");
    else snprintf(text,sizeof text,"%d%%",n>100 ? 100 : n);
    set_text(s_battery,text);
    lv_obj_set_width(s_battery_fill,n<0 ? 1 : 1+(n>100 ? 100 : n)*17/100);
    lv_obj_set_style_bg_color(s_battery_fill,lv_color_hex(n>=0 && n<20 ? 0xF58383 : 0x475866),0);
}
static void render(void)
{
    uint64_t now=now_ms(); bool ended=!s_state.ringing && now<s_ended_until;
    int audio=ec_audio_status();
    set_text(s_caller,ec_caller_name(ended ? s_last_caller : s_state.caller));
    set_text(s_status,!s_buttons ? "按键不可用" : audio<0 ? "声音不可用" :
        !audio ? "正在连接" : ended ? (s_state.timed_out ? "未接来电" : "通话已结束") : "手机来电");
    uint64_t elapsed=s_state.ringing ? now-(s_state.deadline-EC_DURATION_MS) : 0;
    for(unsigned i=0;i<2;i++) {
        unsigned phase=(unsigned)(elapsed/240)%4;
        lv_opa_t opacity=s_state.ringing ? (phase==i ? 90 : phase==i+1 ? 45 : 0) : 0;
        if(lv_obj_get_style_border_opa(s_halos[i],0)!=opacity)
            lv_obj_set_style_border_opa(s_halos[i],opacity,0);
    }
}
static void frame(lv_timer_t *timer)
{
    (void)timer; uint64_t now=now_ms();
    bool old=s_state.ringing; unsigned ring=s_state.ring, caller=s_state.caller;
    unsigned confirmations=atomic_exchange(&s_confirms,0);
    int selection=atomic_exchange(&s_selection,0);
    /* Process stop before timeout: a stop at the deadline must not restart. */
    if (s_buttons && ec_audio_status()==1) {
        for (unsigned i=0;i<confirmations;i++) ec_confirm(&s_state,now);
    }
    while (selection>0) { ec_select(&s_state,1); selection--; }
    while (selection<0) { ec_select(&s_state,-1); selection++; }
    ec_tick(&s_state,now);
    if (ec_audio_status()<0 && s_state.ringing) s_state.ringing=false;
    if (s_state.ringing && (!old || ring!=s_state.ring || confirmations)) ec_audio_play(s_state.ring,(int64_t)s_state.deadline);
    else if (!s_state.ringing && old) ec_audio_stop();
    if(old && !s_state.ringing) { s_last_caller=caller; s_ended_until=now+1800; }
    render();
}
void excuse_call_prepare(void) { ec_audio_prepare(); }
void excuse_call_enter(bool buttons_available)
{
    if(s_screen) return;
    s_buttons=buttons_available; ec_init(&s_state); s_ended_until=0; s_last_caller=0;
    atomic_store(&s_confirms,0); atomic_store(&s_selection,0);
    s_screen=lv_obj_create(NULL); lv_obj_remove_style_all(s_screen);
    lv_obj_remove_flag(s_screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(s_screen,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(s_screen,lv_color_hex(0xF1F4F7),0);
    s_battery=label(156,10,43,false,0x475866,"");
    lv_obj_t *battery_outline=shape(s_screen,204,14,24,12,3,0x475866);
    lv_obj_set_style_bg_opa(battery_outline,0,0);
    lv_obj_set_style_border_width(battery_outline,1,0);
    lv_obj_set_style_border_color(battery_outline,lv_color_hex(0x667582),0);
    shape(s_screen,229,18,2,4,1,0x667582);
    s_battery_fill=shape(s_screen,207,17,18,6,1,0x475866);
    s_caller=label(16,50,208,true,0x172B3A,"");
    s_status=label(20,98,200,false,0x70818E,"");
    lv_obj_t *avatar=shape(s_screen,74,132,92,92,46,0xDEE8EE);
    lv_obj_t *person=lv_image_create(avatar); lv_image_set_src(person,&ec_person_icon);
    lv_obj_set_pos(person,18,16);
    for(unsigned i=0;i<2;i++) {
        s_halos[i]=shape(s_screen,20-(int)i*4,244-(int)i*4,200+i*8,64+i*8,32+i*4,0x22B66E);
        lv_obj_set_style_bg_opa(s_halos[i],0,0);
        lv_obj_set_style_border_width(s_halos[i],1,0);
        lv_obj_set_style_border_color(s_halos[i],lv_color_hex(0x22B66E),0);
        lv_obj_set_style_border_opa(s_halos[i],0,0);
    }
    s_button=shape(s_screen,24,248,192,56,28,0x20B56A);
    lv_obj_t *phone=lv_image_create(s_button); lv_image_set_src(phone,&ec_call_icon);
    lv_obj_set_pos(phone,40,12);
    s_action=label(100,259,76,false,0xFFFFFF,"接听");
    lv_obj_set_style_text_font(s_action,&excuse_call_zh_24,0);
    lv_obj_set_height(s_action,32);
    render(); battery(NULL); lv_screen_load(s_screen);
    s_timer=lv_timer_create(frame,20,NULL);
    s_battery_timer=lv_timer_create(battery,10000,NULL);
    atomic_store(&s_accept,true);
}
void excuse_call_exit(void)
{
    atomic_store(&s_accept,false); ec_audio_stop();
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer=s_battery_timer=NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen=NULL;
}
void excuse_call_key(bsp_btn_t button,bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept) || !s_buttons || event!=BSP_BTN_PRESS) return;
    if (button==BSP_BTN_OK) atomic_fetch_add(&s_confirms,1);
    else if (button==BSP_BTN_UP) atomic_fetch_sub(&s_selection,1);
    else if (button==BSP_BTN_DOWN) atomic_fetch_add(&s_selection,1);
}
