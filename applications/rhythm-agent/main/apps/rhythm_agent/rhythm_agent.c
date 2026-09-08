#include "rhythm_agent.h"
#include "rhythm_state.h"
#include "rhythm_audio.h"
#include "rhythm_service.h"
#include "ui_pixel.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(rhythm_zh_12);
LV_FONT_DECLARE(rhythm_zh_16);
LV_FONT_DECLARE(rhythm_zh_24);
typedef enum { HOME, GAME, SETTINGS, HELP, PAUSE, RESULT } page_t;
typedef struct { bsp_btn_t key; bsp_btn_ev_t event; int64_t ms; } ra_key_event_t;
static QueueHandle_t queue;
static StaticQueue_t queue_control;
static uint8_t queue_buffer[8*sizeof(ra_key_event_t)];
static atomic_bool enabled, overflow;
static lv_obj_t *screen,*body,*battery_label,*footer;
static lv_timer_t *timer;
static ra_game_t game, before_ok;
static page_t page;
static unsigned selected, volume=2, help_page, demo_seen;
static bool voice=true, buttons, sleeping, game_ok_press, undo_valid, feedback_spoken, visual_demo;
static int flash_key=-1,last_demo=-2,last_soc=-2;
static int64_t flash_until,demo_started,last_activity,feedback_started;
static unsigned last_best;
static bool last_saved;
static uint32_t next_seed=137;
static const char *const key_names[]={"上","下","确定"};
static const uint32_t colors[]={0x268AB8,0xBF7947,0x8761B4};
static const char *const mark_names[]={"精准","不错","早了一点","晚了一点","按错键了","漏拍了"};
static int64_t now_ms(void) { return esp_timer_get_time()/1000; }
static lv_obj_t *rect(lv_obj_t *p,int x,int y,int w,int h,uint32_t color) {
    lv_obj_t *o=lv_obj_create(p); lv_obj_remove_style_all(o); lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h); lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); return o;
}
static lv_obj_t *label(lv_obj_t *p,const char *text,int x,int y,int w,int h,unsigned size,uint32_t color) {
    const lv_font_t *f=size==24?&rhythm_zh_24:size==16?&rhythm_zh_16:&rhythm_zh_12;
    lv_obj_t *o=ui_pixel_label(p,text,f,color); lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_text_line_space(o,2,0); lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP); return o;
}
static void line(const char *text,int y,unsigned size,uint32_t color) { label(body,text,10,y,193,size==24?30:size==16?23:18,size,color); }
static void option(unsigned i,const char *text,int y) {
    lv_obj_t *o=rect(body,9,y,195,28,selected==i?UI_YELLOW:0xE5E9DF);
    lv_obj_set_style_border_width(o,2,0); lv_obj_set_style_border_color(o,lv_color_hex(selected==i?UI_INK:0xC5CDC5),0);
    label(o,text,7,2,175,22,16,UI_INK);
}
static void lock_art(bool open,int y) {
    rect(body,24,y,27,24,UI_INK); rect(body,29,y+5,17,18,UI_PAPER);
    if(open) rect(body,42,y+11,12,14,UI_PAPER);
    rect(body,17,y+19,43,32,UI_INK); rect(body,21,y+23,35,24,open?UI_GRASS:UI_YELLOW);
    rect(body,35,y+28,7,7,UI_INK); rect(body,37,y+33,3,7,UI_INK);
}
static void pads(int active) {
    /* Match the device from top to bottom: UP, DOWN, OK. */
    const unsigned order[]={BSP_BTN_UP,BSP_BTN_DOWN,BSP_BTN_OK};
    for(unsigned row=0;row<3;row++) {
        unsigned k=order[row];
        lv_obj_t *o=rect(body,135,69+row*38,65,32,(int)k==active?UI_YELLOW:colors[k]);
        lv_obj_set_style_border_width(o,(int)k==active?3:1,0); lv_obj_set_style_border_color(o,lv_color_hex(UI_INK),0);
        lv_obj_t *t=label(o,key_names[k],2,3,57,23,16,(int)k==active?UI_INK:0xFFFFFF);
        lv_obj_set_style_text_align(t,LV_TEXT_ALIGN_CENTER,0);
    }
}
static void render(void) {
    if(!screen) return;
    lv_obj_clean(body);
    char text[100];
    const char *hint="上下选择 · 确定进入";
    if(page==HOME) {
        line("暗号到手，就差你这一拍",5,16,UI_INK); lock_art(false,38); ui_pixel_mascot_create(body,159,37);
        label(body,"听一遍\n按回来",70,42,82,48,16,UI_INK);
        option(0,"新手训练 · 放心试",99); option(1,"特工挑战 · 三次机会",131); option(2,"声音设置与玩法",163);
        snprintf(text,sizeof(text),"挑战纪录 %u / 800",ra_service_best()); line(text,196,12,0x526354);
    } else if(page==SETTINGS) {
        line("行动装备",7,24,UI_INK);
        static const char *levels[]={"静音","轻声","适中","响亮"};
        snprintf(text,sizeof(text),"音量：%s",levels[volume]); option(0,text,49);
        option(1,voice?"语音提示：开启":"语音提示：关闭",83);
        option(2,"查看玩法说明",117); option(3,"返回首页",151);
        line(ra_audio_ready()?"静音也能看灯光练习":"声音不可用，可看灯光玩",190,12,0x526354);
    } else if(page==HELP) {
        line(help_page?"第二课 · 留意停顿":"第一课 · 认清三键",7,16,UI_INK);
        if(!help_page) {
            label(body,"先看灯、听音\n灯光停下后\n按同样的顺序\n复现刚才节奏",10,51,118,104,16,UI_INK); pads(-1);
            line("首拍由你开始，不用抢拍",179,12,0x526354); line("静音与故障时保留灯光",198,12,0x526354);
        } else {
            label(body,"短停顿是一拍\n长停顿是两拍\n每次按下后要松开\n不用同时按两个键",10,47,193,104,16,UI_INK);
            line("训练按对就过；挑战三次机会",157,12,0x526354);
            line("按键全对，再争取节奏准确",177,12,0x526354);
            line("长按确定暂停，继续会重听",197,12,0x526354);
        }
        hint=help_page?"确定返回设置":"确定看下一页";
    } else if(page==PAUSE) {
        line("任务已暂停",8,24,UI_INK); lock_art(false,38);
        label(body,"继续会重听\n本关不扣机会",71,43,124,46,16,UI_INK);
        option(0,"继续这道锁",106); option(1,"返回首页",142);
        line("休息一下，再找回节奏",189,12,0x526354);
    } else if(page==RESULT) {
        line(game.completed?"任务完成！":"差一点就破解了",7,24,UI_INK);
        snprintf(text,sizeof(text),"%u",game.score); label(body,text,12,45,100,35,24,UI_INK);
        label(body,"任务得分 / 800",12,81,130,18,12,0x526354); ui_pixel_mascot_create(body,162,43);
        snprintf(text,sizeof(text),"%s · 任务 %04u",game.mode==RA_TRAIN?"训练":"挑战",(unsigned)game.seed); line(text,105,12,UI_INK);
        option(0,"同题再来 · 递给朋友",128); option(1,"换个任务",161);
        line(game.mode==RA_TRAIN?"训练完成，随时再练":ra_service_saved()?(ra_service_best()>=game.score?"挑战纪录已保存":"正在保存挑战纪录"):"纪录暂存本次开机",196,12,0x526354);
        hint="上下选择 · 确定重玩";
    } else {
        snprintf(text,sizeof(text),"第 %u 道锁 / 8    任务 %04u",game.door+1,(unsigned)game.seed); line(text,5,12,UI_INK);
        if(game.phase==RA_FEEDBACK) {
            line(game.passed?"破解成功！":"再试一次",30,24,game.passed?0x367442:0x965333);
            snprintf(text,sizeof(text),"节奏准确度 %u%%",game.accuracy); line(text,69,16,UI_INK);
            line(game.all_keys?"按键顺序正确":"有按错或漏掉的键",98,16,UI_INK);
            for(unsigned i=0;i<game.pattern.count;i++) {
                uint32_t c=game.marks[i]<=RA_GOOD?0x79AF67:0xD68C61;
                rect(body,10+i*24,132,20,18,c);
            }
            if(game.mode==RA_TRAIN) snprintf(text,sizeof(text),"训练可以一直重试");
            else snprintf(text,sizeof(text),"剩余机会 %u · 累计 %u 分",game.lives,game.score);
            line(text,166,12,0x526354);
            line(game.passed?(game.accuracy<60?"顺序正确，继续练习节奏":"记住这个手感，下一道锁见"):
                (game.all_keys?"顺序正确，再听清长短停顿":"看准按键顺序，再试一次"),193,12,0x526354);
            hint=game.passed?"确定继续 · 长按确定暂停":"确定重听 · 长按确定暂停";
        } else {
            const char *phase=game.phase==RA_DEMO?"先听暗号":game.phase==RA_READY?"轮到你了":"正在破解";
            line(phase,28,24,UI_INK);
            int active=flash_key;
            if(game.phase==RA_DEMO && last_demo>=0 && last_demo<game.pattern.count) active=game.pattern.keys[last_demo];
            pads(active); lock_art(false,69);
            if(game.phase==RA_DEMO) label(body,"看灯记停顿",10,126,120,23,16,UI_INK);
            else if(game.phase==RA_READY) label(body,"首拍随时开始\n照刚才的节奏",10,124,124,44,16,UI_INK);
            else label(body,mark_names[game.last_mark],10,126,119,24,16,game.last_mark<=RA_GOOD?0x367442:0x965333);
            snprintf(text,sizeof(text),"%u / %u 拍",game.phase==RA_DEMO?demo_seen:game.index,game.pattern.count);
            label(body,text,12,172,118,18,12,UI_INK);
            for(unsigned i=0;i<game.pattern.count;i++) rect(body,10+i*24,190,19,8,i<game.index?UI_GRASS:0xCAD2C6);
            line(game.mode==RA_TRAIN?"训练按对就过，节奏计分":"三次机会，闯过八道锁",202,12,0x526354);
            hint="按上 / 下 / 确定 · 长按暂停";
        }
    }
    if(!buttons) hint="按键不可用，请检查设备";
    lv_label_set_text(footer,hint);
}
static void begin_demo(void) {
    feedback_spoken=false; demo_seen=0; flash_key=-1; last_demo=-2; demo_started=now_ms();
    visual_demo=!ra_audio_ready() || !volume;
    ra_audio_demo(&game.pattern,volume,voice);
}
static void start_game(uint32_t seed) {
    ra_start(&game,seed,selected==0?RA_TRAIN:RA_CHALLENGE);
    page=GAME; begin_demo(); render();
}
static void to_result(void) {
    page=RESULT; selected=0;
    if(game.mode==RA_CHALLENGE) ra_service_save(game.score);
}
static void act(ra_key_event_t e) {
    if(!buttons) return;
    if(sleeping) {
        if(e.event==BSP_BTN_PRESS) { sleeping=false; bsp_display_backlight(85); last_activity=e.ms; game_ok_press=e.key==BSP_BTN_OK; }
        return;
    }
    if(e.event==BSP_BTN_PRESS) last_activity=e.ms;
    if(e.key==BSP_BTN_OK && e.event==BSP_BTN_LONG) {
        if(page==GAME || (page==RESULT && undo_valid)) {
            /* Undo the initial down-event of the hold, including its score/life change. */
            if(undo_valid) game=before_ok;
            ra_audio_stop(); page=PAUSE; selected=0;
        } else if(page!=PAUSE) { ra_audio_stop(); page=HOME; selected=0; }
        game_ok_press=false; undo_valid=false; render(); return;
    }
    if(e.key==BSP_BTN_OK && (e.event==BSP_BTN_CLICK || e.event==BSP_BTN_DOUBLE) && game_ok_press) {
        game_ok_press=false; undo_valid=false; return;
    }
    if(page==GAME && (game.phase==RA_READY || game.phase==RA_INPUT)) {
        if(e.event!=BSP_BTN_PRESS) return;
        if(e.key==BSP_BTN_OK) { before_ok=game; undo_valid=true; game_ok_press=true; }
        if(ra_press(&game,(unsigned)e.key,e.ms)) {
            ra_audio_note(e.key,volume); flash_key=e.key; flash_until=e.ms+170;
            if(game.phase==RA_FEEDBACK) feedback_started=e.ms;
            render();
        } else if(game.phase==RA_FEEDBACK) { feedback_started=e.ms; render(); }
        return;
    }
    bool confirm=e.key==BSP_BTN_OK && e.event==BSP_BTN_CLICK;
    if(e.event==BSP_BTN_PRESS && e.key!=BSP_BTN_OK && page!=GAME && page!=HELP) {
        unsigned count=page==HOME?3:page==SETTINGS?4:2;
        selected=(selected+count+(e.key==BSP_BTN_UP?-1:1))%count; render(); return;
    }
    if(!confirm) return;
    if(page==HOME) {
        if(selected<2) { next_seed=(next_seed+379+(uint32_t)e.ms)%10000; start_game(next_seed); }
        else {page=SETTINGS; selected=0; render();}
    } else if(page==SETTINGS) {
        if(selected==0) { volume=(volume+1)%4; ra_audio_note(0,volume); }
        else if(selected==1) voice=!voice;
        else if(selected==2) {page=HELP;help_page=0;}
        else {page=HOME;selected=0;}
        render();
    } else if(page==HELP) { if(help_page) {page=SETTINGS;selected=2;} else help_page=1; render(); }
    else if(page==PAUSE) {
        if(selected==0) {
            if(game.phase==RA_FEEDBACK) {page=GAME;render();}
            else { ra_replay(&game); page=GAME; begin_demo(); render(); }
        } else {page=HOME;selected=0;render();}
    } else if(page==RESULT) {
        uint32_t seed=selected==0?game.seed:(game.seed+379+(uint32_t)e.ms)%10000;
        selected=game.mode; start_game(seed);
    } else if(page==GAME && game.phase==RA_FEEDBACK && e.ms-feedback_started>=400) {
        ra_audio_stop(); ra_next(&game);
        if(game.phase==RA_FINISHED) to_result(); else begin_demo();
        render();
    }
}
static void frame(lv_timer_t *unused) {
    (void)unused;
    int64_t now=now_ms(); ra_key_event_t e;
    while(xQueueReceive(queue,&e,0)==pdTRUE) act(e);
    if(atomic_exchange(&overflow,false) && page==GAME) {ra_audio_stop(); page=PAUSE;selected=0;render();}
    if(page==GAME) {
        if(game.phase==RA_DEMO) {
            if(!ra_audio_ready() && !visual_demo) {visual_demo=true;demo_started=now;}
            int note=-1;
            bool done=false;
            if(visual_demo) {
                int64_t t=now-demo_started-350;
                for(unsigned i=0;i<game.pattern.count;i++) if(t>=game.pattern.at[i] && t<game.pattern.at[i]+170) note=i;
                done=t>game.pattern.at[game.pattern.count-1]+600;
            } else {note=ra_audio_note_index(); done=!ra_audio_busy();}
            if(done) {ra_ready(&game); last_demo=-1; render();}
            else if(note!=last_demo) {last_demo=note;if(note>=0)demo_seen=(unsigned)note+1;render();}
        } else if(game.phase==RA_INPUT) {
            ra_tick(&game,now);
            if(game.phase==RA_FEEDBACK) {feedback_started=now;render();}
        }
        if(game.phase==RA_FEEDBACK && !feedback_spoken && now-feedback_started>=250) {
            feedback_spoken=true; ra_audio_feedback(game.passed,volume,voice);
        }
    }
    if(flash_key>=0 && now>=flash_until) {flash_key=-1;render();}
    unsigned latest_best=ra_service_best(); bool latest_saved=ra_service_saved();
    if(latest_best!=last_best || latest_saved!=last_saved) {last_best=latest_best;last_saved=latest_saved;if(page==HOME || page==RESULT)render();}
    int soc=ra_service_soc();
    if(soc!=last_soc) {last_soc=soc;char s[24];if(soc<0) snprintf(s,sizeof(s),"电量 --");else snprintf(s,sizeof(s),"电量 %d%%",soc);lv_label_set_text(battery_label,s);}
    /* Never dim during a timed performance or demonstration. */
    if(!sleeping && page!=GAME && now-last_activity>120000) {sleeping=true;bsp_display_backlight(0);}
}
void rhythm_agent_prepare(void) { if(!queue) queue=xQueueCreateStatic(8,sizeof(ra_key_event_t),queue_buffer,&queue_control); }
void rhythm_agent_enter(bool buttons_ok) {
    if(screen) return;
    rhythm_agent_prepare();xQueueReset(queue);buttons=buttons_ok;page=HOME;selected=0;sleeping=false;
    game_ok_press=undo_valid=false;last_soc=-2;flash_key=-1;last_activity=now_ms();
    screen=ui_pixel_screen_create("");
    label(screen,"节奏特工",22,10,125,30,24,0xFFFFFF);
    battery_label=label(screen,"电量 --",163,31,76,18,12,UI_INK);
    body=ui_pixel_panel_create(screen,8,54,219,227,UI_PAPER);
    lv_obj_set_style_pad_all(body,0,0);lv_obj_set_style_border_width(body,3,0);
    footer=label(screen,"",8,294,226,19,12,UI_INK); lv_obj_set_style_text_align(footer,LV_TEXT_ALIGN_CENTER,0);
    lv_screen_load(screen); render(); timer=lv_timer_create(frame,10,NULL); atomic_store(&enabled,true);
}
void rhythm_agent_exit(void) {
    atomic_store(&enabled,false);ra_audio_stop();
    if(timer) {lv_timer_delete(timer);timer=NULL;} if(queue) xQueueReset(queue);
    if(screen) {lv_obj_delete(screen);screen=NULL;body=NULL;battery_label=NULL;footer=NULL;}
}
void rhythm_agent_key(bsp_btn_t key,bsp_btn_ev_t event) {
    if(!atomic_load(&enabled) || !queue || (unsigned)key>2) return;
    ra_key_event_t e={key,event,now_ms()}; if(xQueueSend(queue,&e,0)!=pdTRUE) atomic_store(&overflow,true);
}
