#include "night_call.h"
#include "night_call_audio.h"
#include "night_call_storage.h"
#include "ui_pixel.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(night_call_zh_12);
LV_FONT_DECLARE(night_call_zh_16);
LV_FONT_DECLARE(night_call_zh_24);
#define INK 0x0B1526
#define PANEL 0x13253B
#define CYAN 0x7EE5D3
#define AMBER 0xFBCB86
#define WHITE 0xF0F1E8
#define MUTED 0xA0B4C7
typedef enum { HOME, CHAPTERS, STORY, ENDING, ARCHIVE, SETTINGS, HELP, PAUSE } page_t;
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; } input_t;
static StaticQueue_t s_control;
static uint8_t s_buffer[8*sizeof(input_t)];
static QueueHandle_t s_keys;
static lv_obj_t *s_screen, *s_view, *s_battery, *s_footer, *s_signal;
static lv_timer_t *s_timer, *s_battery_timer;
static nc_state_t s_state;
static page_t s_page;
static unsigned s_selected, s_archive, s_frame;
static int s_ending, s_soc=-1, s_last_status=99;
static bool s_buttons, s_asleep, s_last_ready, s_last_busy;
static int64_t s_last_input;
static const char *const menu_items[]={"继续通话","接通新来电","回声档案","声音设置","操作说明"};
static void render(void);
static lv_obj_t *label(lv_obj_t *p,int x,int y,int w,int h,const char *text,int size,uint32_t color) {
    lv_obj_t *o=ui_pixel_label(p,text,size==24?&night_call_zh_24:size==12?&night_call_zh_12:&night_call_zh_16,color);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_text_line_space(o,1,0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_WRAP); return o;
}
static lv_obj_t *box(lv_obj_t *p,int x,int y,int w,int h,uint32_t color) {
    lv_obj_t *o=ui_pixel_panel_create(p,x,y,w,h,color);
    lv_obj_set_style_border_width(o,1,0); lv_obj_set_style_border_color(o,lv_color_hex(0x29445A),0);
    lv_obj_set_style_pad_all(o,0,0); lv_obj_set_style_radius(o,6,0); return o;
}
static lv_obj_t *rect(lv_obj_t *p,int x,int y,int w,int h,uint32_t color) {
    lv_obj_t *o=lv_obj_create(p); lv_obj_remove_style_all(o); lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h); lv_obj_set_style_bg_opa(o,255,0);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0); return o;
}
static void paragraph(int y,int h,const char *body) {
    label(s_view,18,y,204,h,body,16,WHITE);
}
static void choice(unsigned i,const char *text,int y) {
    bool selected=s_selected==i;
    lv_obj_t *p=box(s_view,12,y,212,34,selected?CYAN:PANEL);
    label(p,9,5,192,23,text,16,selected?INK:WHITE);
}
static void telephone(int x,int y,bool dawn) {
    uint32_t color=dawn?AMBER:CYAN;
    lv_obj_t *p=box(s_view,x,y,92,59,0x203B51);
    rect(p,13,10,66,7,color); rect(p,10,10,12,21,color); rect(p,69,10,12,21,color);
    for(int row=0;row<2;row++) for(int col=0;col<3;col++) rect(p,29+col*13,29+row*10,7,5,AMBER);
    rect(s_view,x+98,y+10,3,12,color); rect(s_view,x+105,y+5,3,22,color);
    rect(s_view,x+112,y,3,32,color);
}
static const char *footer(void) {
    if(!s_buttons) return "按键不可用，请检查设备";
    if(s_asleep) return "按任意键唤醒";
    int status=nc_storage_status();
    if(status<0) return "存档异常，本次进度可能丢失";
    if(status>0) return "正在保存，请稍等再关机";
    if(!nc_audio_ready()) return "语音不可用，字幕可继续";
    switch(s_page) {
        case STORY: return "上下选择 · 确定回应 · 长按暂停";
        case ARCHIVE: return "上下翻页 · 确定返回";
        case HELP: return "确定返回值班室";
        case ENDING: return "上下选择 · 确定继续";
        default: return "上下选择 · 确定进入";
    }
}
static void header(const char *title) { label(s_view,14,1,212,32,title,24,AMBER); }
static void render(void) {
    if(!s_view) return;
    lv_obj_clean(s_view); s_signal=NULL;
    char text[120];
    if(s_page==HOME) {
        header("今夜，请别挂断");
        telephone(57,46,false);
        label(s_view,27,120,198,24,"有人在等你的回答",16,WHITE);
        snprintf(text,sizeof(text),"线索 %u/6    结局 %u/7",nc_count(s_state.clues),nc_count(s_state.endings));
        label(s_view,40,153,186,20,text,12,MUTED);
        choice(0,menu_items[s_selected],183);
        /* The menu is a carousel, so its only tile remains visibly selected. */
        lv_obj_t *tile=lv_obj_get_child(s_view,lv_obj_get_child_count(s_view)-1);
        lv_obj_set_style_bg_color(tile,lv_color_hex(CYAN),0);
        lv_obj_set_style_text_color(lv_obj_get_child(tile,0),lv_color_hex(INK),0);
    } else if(s_page==CHAPTERS) {
        header("接哪一通来电");
        label(s_view,16,42,208,36,"三通电话，一个雨夜。\n先从车站开始也很好。",12,MUTED);
        for(unsigned i=0;i<3;i++) choice(i,nc_chapters[i],91+(int)i*43);
    } else if(s_page==STORY) {
        const nc_node_t *n=&nc_nodes[s_state.node]; header(n->title);
        snprintf(text,sizeof(text),"%s · %s",nc_chapters[s_state.chapter],s_state.volume?"字幕与语音":"静音字幕");
        label(s_view,16,38,214,18,text,12,MUTED);
        box(s_view,10,62,218,92,PANEL); paragraph(66,84,n->body);
        s_signal=rect(s_view,17,160,12,3,CYAN);
        label(s_view,36,154,190,17,"长按上重听 · 长按下静音",12,MUTED);
        choice(0,n->choices[0].text,172); choice(1,n->choices[1].text,211);
    } else if(s_page==ENDING) {
        header(nc_endings[s_ending].title);
        label(s_view,18,42,200,19,s_ending==6?"隐藏终章 · 来电已全部接通":"来电结束 · 已收入结局档案",12,CYAN);
        paragraph(77,87,nc_endings[s_ending].body);
        choice(0,"再接一次，换个回答",172); choice(1,"返回值班室",211);
    } else if(s_page==ARCHIVE) {
        bool is_clue=s_archive<NC_CLUES;
        unsigned index=is_clue?s_archive:s_archive-NC_CLUES;
        bool found=(is_clue?s_state.clues:s_state.endings)&(1u<<index);
        header("回声档案");
        snprintf(text,sizeof(text),"%s %u/%u · %s",is_clue?"线索":"结局",index+1,is_clue?NC_CLUES:NC_ENDINGS,found?"已解锁":"未解锁");
        label(s_view,18,43,204,20,text,12,CYAN);
        label(s_view,18,83,204,28,found?(is_clue?nc_clues[index].title:nc_endings[index].title):"尚未接通的回声",16,AMBER);
        paragraph(119,100,found?(is_clue?nc_clues[index].body:nc_endings[index].body):"继续接听来电，尝试不同回应。线索会保存，未发现的故事暂时为你保密。");
    } else if(s_page==HELP) {
        header("接线员手册");
        label(s_view,18,48,204,84,"上、下键：选择回应\n确定键：推进故事\n长按上键：重听对白\n长按下键：切换静音",16,WHITE);
        label(s_view,18,144,204,40,"长按确定：暂停与返回",16,CYAN);
        label(s_view,18,178,204,60,"没有倒计时，可以慢慢想。\n回应会自动保存。\n保存提示消失后再关机。",12,MUTED);
    } else if(s_page==SETTINGS) {
        header("声音设置");
        static const char *const volumes[]={"静音","轻声","标准","响亮"};
        snprintf(text,sizeof(text),"音量：%s",volumes[s_state.volume]); choice(0,text,74); choice(1,"返回值班室",120);
        paragraph(172,64,"全部语音都有字幕。长按下键，也能随时切换静音。");
    } else if(s_page==PAUSE) {
        header("我还在这里");
        paragraph(57,75,"通话已暂停，你可以慢慢想。回值班室后，选择继续通话就能接着听。");
        choice(0,"继续通话",156); choice(1,"返回值班室",201);
    }
    lv_label_set_text(s_footer,footer());
}
static void play(void) {
    if(s_page==STORY) nc_audio_play(s_state.node,s_state.volume);
    else if(s_page==ENDING) nc_audio_play(nc_node_count+(unsigned)s_ending,s_state.volume);
}
static void home(void) { s_page=HOME; s_selected=s_state.active?0:1; nc_audio_stop(); render(); }
static void save(void) { nc_storage_save(&s_state); }
static void handle(input_t e) {
    if(!s_buttons) return;
    if(e.event!=BSP_BTN_CLICK && e.event!=BSP_BTN_LONG) return;
    s_last_input=esp_timer_get_time();
    if(s_asleep) { s_asleep=false; bsp_display_backlight(80); render(); return; }
    if(e.event==BSP_BTN_LONG) {
        if(e.button==BSP_BTN_UP) { play(); return; }
        if(e.button==BSP_BTN_DOWN) { s_state.volume=s_state.volume?0:2; nc_audio_stop(); save(); render(); return; }
        if(e.button==BSP_BTN_OK) {
            nc_audio_stop();
            if(s_page==STORY) { s_page=PAUSE; s_selected=0; render(); }
            else if(s_page==PAUSE) { s_page=STORY; s_selected=0; render(); play(); }
            else home();
            return;
        }
    }
    if(e.button!=BSP_BTN_OK) {
        int delta=e.button==BSP_BTN_UP?-1:1;
        if(s_page==ARCHIVE) { s_archive=(s_archive+NC_CLUES+NC_ENDINGS+delta)%(NC_CLUES+NC_ENDINGS); }
        else if(s_page==HOME) {
            s_selected=(s_selected+5+delta)%5;
            if(!s_state.active && s_selected==0) s_selected=delta<0?4:1;
        } else if(s_page==CHAPTERS) s_selected=(s_selected+3+delta)%3;
        else if(s_page!=HELP) s_selected=1-s_selected;
        render(); return;
    }
    nc_audio_stop();
    switch(s_page) {
        case HOME:
            if(s_selected==0 && s_state.active) { s_page=STORY; s_selected=0; }
            else if(s_selected==1) { s_page=CHAPTERS; s_selected=0; }
            else if(s_selected==2) { s_page=ARCHIVE; s_archive=0; }
            else if(s_selected==3) { s_page=SETTINGS; s_selected=0; }
            else s_page=HELP;
            break;
        case CHAPTERS: nc_begin(&s_state,s_selected); s_page=STORY; s_selected=0; save(); break;
        case STORY: {
            nc_result_t r=nc_choose(&s_state,s_selected,&s_ending); s_selected=0; save();
            if(r==NC_END) s_page=ENDING;
            else if(r==NC_HOME) { home(); return; }
            break;
        }
        case ENDING:
            if(s_selected==0) { nc_begin(&s_state,s_state.chapter); s_page=STORY; save(); }
            else { home(); return; }
            s_selected=0; break;
        case SETTINGS:
            if(s_selected==0) { s_state.volume=(s_state.volume+1)%4; save(); }
            else { home(); return; }
            break;
        case PAUSE:
            if(s_selected==0) { s_page=STORY; s_selected=0; }
            else { home(); return; }
            break;
        default: home(); return;
    }
    render(); play();
}
static void battery(lv_timer_t *timer) {
    (void)timer; s_soc=bsp_battery_soc();
    if(s_battery) {
        char b[32]; if(s_soc<0) snprintf(b,sizeof(b),"电量 --"); else snprintf(b,sizeof(b),"电量 %d%%",s_soc);
        lv_label_set_text(s_battery,b);
    }
}
static void frame(lv_timer_t *timer) {
    (void)timer; input_t e;
    for(unsigned i=0;i<4 && xQueueReceive(s_keys,&e,0)==pdTRUE;i++) handle(e);
    if(!s_screen) return;
    bool busy=nc_audio_busy(), ready=nc_audio_ready(); int status=nc_storage_status();
    if(status!=s_last_status || ready!=s_last_ready || busy!=s_last_busy) {
        lv_label_set_text(s_footer,footer()); s_last_status=status; s_last_ready=ready; s_last_busy=busy;
    }
    if(s_signal && !s_asleep) {
        s_frame++; lv_obj_set_width(s_signal,busy?8+(s_frame%5)*3:12);
        lv_obj_set_style_bg_color(s_signal,lv_color_hex(busy?CYAN:MUTED),0);
    }
    if(!s_asleep && esp_timer_get_time()-s_last_input>180000000 && !busy) {
        s_asleep=true; bsp_display_backlight(0); lv_label_set_text(s_footer,footer());
    }
}
void night_call_prepare(void) {
    if(!s_keys) s_keys=xQueueCreateStatic(8,sizeof(input_t),s_buffer,&s_control);
}
void night_call_key(bsp_btn_t b,bsp_btn_ev_t e) {
    /* Called by the ADC button task: no LVGL, flash, codec or blocking work. */
    if(s_keys) { input_t input={b,e}; (void)xQueueSend(s_keys,&input,0); }
}
void night_call_enter(const nc_state_t *state,bool buttons) {
    night_call_prepare(); xQueueReset(s_keys); s_state=*state; s_buttons=buttons;
    s_screen=ui_pixel_screen_create("");
    /* Retain the shared pixel skyline, title plaque and ground in a night palette. */
    lv_obj_set_style_bg_color(s_screen,lv_color_hex(INK),0);
    for(unsigned i=0;i<lv_obj_get_child_count(s_screen);i++) {
        lv_obj_t *o=lv_obj_get_child(s_screen,i);
        lv_obj_set_style_bg_color(o,lv_color_hex(lv_obj_get_y(o)>280?0x183748:0x233C52),0);
    }
    for(unsigned i=0;i<lv_obj_get_child_count(s_screen);i++) {
        lv_obj_t *o=lv_obj_get_child(s_screen,i);
        for(unsigned j=0;j<lv_obj_get_child_count(o);j++) {
            lv_obj_t *child=lv_obj_get_child(o,j);
            if(!lv_obj_check_type(child,&lv_label_class))
                lv_obj_set_style_bg_color(child,lv_color_hex(lv_obj_get_y(child)<8?0x417D78:0x1B3045),0);
        }
    }
    label(s_screen,16,14,138,25,"深夜来电",16,WHITE);
    s_battery=label(s_screen,161,31,77,19,"电量 --",12,MUTED);
    s_view=lv_obj_create(s_screen); lv_obj_remove_style_all(s_view); lv_obj_remove_flag(s_view,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_view,0,51); lv_obj_set_size(s_view,240,248);
    s_footer=label(s_screen,8,301,224,17,"",12,WHITE);
    lv_obj_set_style_text_align(s_footer,LV_TEXT_ALIGN_CENTER,0);
    s_last_input=esp_timer_get_time(); s_asleep=false; s_last_status=99;
    s_page=HOME; s_selected=s_state.active?0:1; render(); battery(NULL);
    lv_screen_load(s_screen); s_timer=lv_timer_create(frame,40,NULL);
    s_battery_timer=lv_timer_create(battery,30000,NULL);
}
void night_call_exit(void) {
    nc_audio_stop();
    if(s_timer) { lv_timer_delete(s_timer); s_timer=NULL; }
    if(s_battery_timer) { lv_timer_delete(s_battery_timer); s_battery_timer=NULL; }
    if(s_keys) xQueueReset(s_keys);
    if(s_screen) { lv_obj_delete(s_screen); s_screen=NULL; }
    s_view=NULL; s_footer=NULL; s_battery=NULL; s_signal=NULL;
}
