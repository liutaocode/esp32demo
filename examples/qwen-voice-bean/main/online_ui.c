#include "online_ui.h"
#include "online_runtime.h"
#include "bsp_battery.h"
#include "esp_timer.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdatomic.h>
LV_FONT_DECLARE(mouthy_bean_zh_12);
LV_FONT_DECLARE(mouthy_bean_zh_16);
#define YELLOW 0xFFD43B
#define INK 0x553B23
#define PAPER 0xFFF6D6
static lv_obj_t *face_layer,*pupils[2],*lids[2],*closed_eyes[2],*ears[2],*mouth,*teeth,*tongue,*smile;
static lv_obj_t *title,*caption,*footer,*battery,*setup_panel,*setup_text;
static lv_obj_t *settings_layer,*confirm_layer,*settings_help,*wifi_value,*password_value,*host_value,*token_value;
static lv_obj_t *settings_choices[2],*confirm_choices[2];
static online_menu_t menu;
static atomic_uint pending_key;
static bool buttons_available;
static lv_obj_t *shape(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius) {
    lv_obj_t *o=lv_obj_create(p); lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0); lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0);
    lv_obj_set_style_radius(o,radius,0); return o;
}
static lv_obj_t *label(lv_obj_t *p,const char *value,int x,int y,int w,int h,bool small,uint32_t color) {
    lv_obj_t *o=lv_label_create(p); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_text_font(o,small?&mouthy_bean_zh_12:&mouthy_bean_zh_16,0);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_line_space(o,4,0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP); lv_label_set_text(o,value); return o;
}
static void visibility(lv_obj_t *o,bool show) { if(show) lv_obj_remove_flag(o,LV_OBJ_FLAG_HIDDEN); else lv_obj_add_flag(o,LV_OBJ_FLAG_HIDDEN); }
static void eye(int i,int x) {
    shape(face_layer,x,89,96,101,0xCE9D25,48);
    lv_obj_t *rim=shape(face_layer,x,82,96,100,0x766D64,47);
    lv_obj_set_style_border_width(rim,2,0); lv_obj_set_style_border_color(rim,lv_color_hex(0x625448),0);
    shape(rim,4,3,88,92,0xE7E4CF,43);
    shape(rim,8,7,80,84,0x9D9688,39);
    lv_obj_t *white=shape(rim,10,10,76,78,0xFFFEF5,38);
    lv_obj_set_style_clip_corner(white,true,0);
    pupils[i]=shape(white,22,25,37,43,0x9C6842,20);
    shape(pupils[i],5,5,27,33,0x593C2D,15);
    shape(pupils[i],10,8,19,26,0x231D1B,12);
    shape(pupils[i],6,6,11,13,0xFFFFFF,7);
    shape(pupils[i],25,27,5,6,0xEED4AC,3);
    lids[i]=shape(white,0,0,76,0,YELLOW,0);
    closed_eyes[i]=shape(white,10,53,56,4,INK,2);
    shape(rim,20,5,27,2,0xFFFFFF,1);
}
static void refresh(lv_timer_t *timer) {
    (void)timer;
    unsigned key=atomic_exchange(&pending_key,0);
    if(key) {
        switch(online_menu_key(&menu,(online_key_t)key)) {
            case ONLINE_ACTION_MIC: online_toggle_mic();break;
            case ONLINE_ACTION_VOLUME: online_volume();break;
            case ONLINE_ACTION_CANCEL: online_cancel();break;
            case ONLINE_ACTION_SETUP: online_setup();break;
            default: break;
        }
    }
    online_status_t s;online_snapshot(&s);uint64_t now=esp_timer_get_time()/1000;
    bool home=menu.page==ONLINE_HOME,setup=s.phase==ONLINE_SETUP;
    bool listen=s.mic && (s.phase==ONLINE_LISTENING || s.phase==ONLINE_READY);
    bool speaking=s.phase==ONLINE_SPEAKING;
    lv_label_set_text(title,home?"Qwen 语音豆":menu.page==ONLINE_SETTINGS?"设置":"重新配置？");
    visibility(face_layer,home && !setup);visibility(setup_panel,home && setup);
    visibility(footer,home);visibility(settings_layer,menu.page==ONLINE_SETTINGS);
    visibility(confirm_layer,menu.page==ONLINE_CONFIRM);
    if(menu.page==ONLINE_SETTINGS) {
        lv_label_set_text_fmt(settings_help,"首页按键 · 音量 %u%%\n确定：开关麦克风\n上：调音量  下：打断回答",s.volume);
        lv_label_set_text_fmt(wifi_value,"Wi-Fi：%s",s.configured?s.ssid:"未配置");
        lv_label_set_text_fmt(password_value,"Wi-Fi 密码：%s",!s.configured?"未配置":s.password_set?"已保存":"开放网络");
        lv_label_set_text_fmt(host_value,"后端：%s",s.configured?s.host:"未配置");
        lv_label_set_text_fmt(token_value,"访问令牌：%s",!s.configured?"未配置":s.token_set?"已保存":"无需令牌");
    }
    for(unsigned i=0;i<2;i++) {
        lv_obj_set_style_bg_color(settings_choices[i],lv_color_hex(menu.selected==i?INK:PAPER),0);
        lv_obj_set_style_bg_color(confirm_choices[i],lv_color_hex(menu.selected==i?INK:PAPER),0);
        lv_obj_set_style_text_color(lv_obj_get_child(settings_choices[i],0),lv_color_hex(menu.selected==i?PAPER:INK),0);
        lv_obj_set_style_text_color(lv_obj_get_child(confirm_choices[i],0),lv_color_hex(menu.selected==i?PAPER:INK),0);
    }
    if(setup)lv_label_set_text(setup_text,s.message);
    for(int i=0;i<2;i++) {
        visibility(ears[i],listen);lv_obj_set_y(ears[i],listen?49:82);
        bool blink=!listen && !speaking && now%4300<120;
        lv_obj_set_height(lids[i],blink?74:0);visibility(closed_eyes[i],blink);
        /* Keep eye contact while speaking, without the idle horizontal drift. */
        lv_obj_set_pos(pupils[i],22,25);
    }
    unsigned mh=speaking?22+s.level/1100:24;if(mh>46)mh=46;
    unsigned mw=speaking?64:60;
    lv_obj_set_size(mouth,mw,mh);
    lv_obj_set_pos(mouth,(240-mw)/2,218-mh/2);
    lv_obj_set_pos(teeth,(mw-42)/2,1);
    lv_obj_set_pos(tongue,(mw-30)/2,mh-12);
    visibility(mouth,speaking);
    visibility(smile,!speaking);
    visibility(tongue,speaking);
    lv_obj_set_y(footer,setup?281:300);lv_obj_set_height(footer,setup?34:18);
    lv_label_set_text(caption,(s.phase==ONLINE_ERROR || s.phase==ONLINE_CONNECTING)&&s.message[0]?s.message:
        s.phase==ONLINE_READY&&!s.mic?"按确定开麦":online_phase_text(s.phase));
    lv_label_set_text(footer,!buttons_available?"按键不可用，请检查设备":setup?
        "热点无网络时，请保持连接\n长按确定 · 设置":"长按确定 · 设置");
}
static void update_battery(lv_timer_t *timer) {
    (void)timer;int soc=bsp_battery_soc();
    if(soc<0)lv_label_set_text(battery,"--%");else lv_label_set_text_fmt(battery,"%d%%",soc>100?100:soc);
}
void online_ui_enter(bool buttons_ok) {
    buttons_available=buttons_ok;
    lv_obj_t *screen=shape(NULL,0,0,240,320,YELLOW,0);
    title=label(screen,"Qwen 语音豆",8,8,170,20,false,INK);
    battery=label(screen,"--%",191,8,44,20,true,INK);
    face_layer=shape(screen,0,0,240,320,YELLOW,0);lv_obj_move_background(face_layer);
    for(int i=0;i<2;i++) {
        ears[i]=shape(face_layer,i?178:37,82,25,68,0xB67D29,12);
        shape(ears[i],3,3,19,62,0xFFE37B,9);shape(ears[i],7,9,11,45,0xEDAB78,6);
    }
    eye(0,19);eye(1,125);
    shape(face_layer,27,191,37,12,0xE68048,6);shape(face_layer,177,191,37,12,0xE68048,6);
    mouth=shape(face_layer,90,206,60,24,0x49291F,18);
    lv_obj_set_style_clip_corner(mouth,true,0);
    teeth=shape(mouth,9,1,42,7,0xFFFEF5,3);
    tongue=shape(mouth,15,12,30,10,0xF28D89,7);
    smile=lv_arc_create(face_layer);lv_obj_remove_style_all(smile);
    lv_obj_remove_flag(smile,LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(smile,88,162);lv_obj_set_size(smile,64,64);
    lv_arc_set_bg_angles(smile,40,140);lv_arc_set_angles(smile,40,140);
    lv_obj_set_style_arc_color(smile,lv_color_hex(0x49291F),LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(smile,LV_OPA_COVER,LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(smile,4,LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(smile,true,LV_PART_INDICATOR);
    caption=label(face_layer,"",18,256,204,38,true,INK);
    setup_panel=shape(screen,8,40,224,235,PAPER,14);
    label(setup_panel,"1. 先在电脑搭建后端",8,8,208,20,true,INK);
    label(setup_panel,"https://github.com/QwenAudio/\nqwen-audio-agent",4,31,216,36,true,INK);
    label(setup_panel,"2. 手机连接热点配网",8,76,208,20,true,INK);
    setup_text=label(setup_panel,"",4,100,216,38,true,INK);
    label(setup_panel,"3. 手机浏览器打开",8,146,208,20,true,INK);
    label(setup_panel,"http://192.168.4.1",4,170,216,22,false,INK);
    label(setup_panel,"选择 Wi-Fi，填写密码\n再填后端 IP，保存配置",8,196,208,36,true,INK);
    footer=label(screen,"",6,281,228,34,true,INK);
    settings_layer=shape(screen,0,35,240,285,YELLOW,0);
    settings_help=label(settings_layer,"",8,0,224,58,true,INK);
    lv_obj_t *configuration=shape(settings_layer,8,64,224,104,PAPER,12);
    wifi_value=label(configuration,"",8,5,208,18,true,INK);
    password_value=label(configuration,"",8,29,208,18,true,INK);
    host_value=label(configuration,"",8,53,208,18,true,INK);
    token_value=label(configuration,"",8,77,208,18,true,INK);
    lv_label_set_long_mode(wifi_value,LV_LABEL_LONG_DOT);
    lv_label_set_long_mode(host_value,LV_LABEL_LONG_DOT);
    const char *choices[]={"返回首页","重新配置"};
    for(unsigned i=0;i<2;i++) {
        settings_choices[i]=shape(settings_layer,16,176+i*34,208,30,PAPER,9);
        label(settings_choices[i],choices[i],4,4,200,22,false,INK);
    }
    label(settings_layer,"上/下选择 · 确定进入",8,244,224,18,true,INK);
    label(settings_layer,"基于 qwen audio agent",8,266,224,18,true,INK);
    confirm_layer=shape(screen,0,35,240,285,YELLOW,0);
    label(confirm_layer,"是否重新配置？",12,26,216,24,false,INK);
    label(confirm_layer,"手机连接热点后，用浏览器\n打开 http://192.168.4.1 配置\n取消不会更改现有配置",12,70,216,66,true,INK);
    const char *confirm[]={"取消，保留当前配置","确认重新配置"};
    for(unsigned i=0;i<2;i++) {
        confirm_choices[i]=shape(confirm_layer,12,164+i*38,216,32,PAPER,9);
        label(confirm_choices[i],confirm[i],4,6,208,22,false,INK);
    }
    label(confirm_layer,"上/下选择 · 确定执行\n长按确定返回首页",8,247,224,36,true,INK);
    refresh(NULL);update_battery(NULL);lv_screen_load(screen);
    /* One application-lifetime screen. Workers never retain LVGL objects. */
    lv_timer_create(refresh,50,NULL);lv_timer_create(update_battery,15000,NULL);
}
void online_ui_key(bsp_btn_t key,bsp_btn_ev_t ev) {
    unsigned command=0;
    if(ev==BSP_BTN_LONG && key==BSP_BTN_OK)command=ONLINE_KEY_LONG;
    else if(ev==BSP_BTN_CLICK)command=key==BSP_BTN_OK?ONLINE_KEY_OK:key==BSP_BTN_UP?ONLINE_KEY_UP:ONLINE_KEY_DOWN;
    /* Button callbacks never touch LVGL. Consume one action on its task. */
    if(command){unsigned empty=0;atomic_compare_exchange_strong(&pending_key,&empty,command);}
}
