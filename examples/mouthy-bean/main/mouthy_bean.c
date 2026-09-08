#include "mouthy_bean.h"
#include "bean_state.h"
#include "bean_runtime.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(mouthy_bean_zh_12);
LV_FONT_DECLARE(mouthy_bean_zh_16);
#define YELLOW 0xFFD43B
#define INK 0x553B23
#define PAPER 0xFFF6D6

typedef struct { bsp_btn_t key; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t control;
static uint8_t input_buffer[6*sizeof(input_t)];
static QueueHandle_t queue;
static atomic_bool accepting;
static bean_t state;
static lv_obj_t *screen, *placeholder, *face_layer, *menu_layer;
static lv_obj_t *pupils[2], *lids[2], *closed_eyes[2], *mouth, *cheeks[2], *ears[2], *status_badge;
static lv_obj_t *title, *status, *caption, *footer, *battery_label, *collection, *rows[8], *hint;
static lv_timer_t *timer, *battery_timer;
static unsigned page, selected, album_selected;
static bool buttons, dimmed;
static uint64_t guard;
static uint32_t last_saved;
static const char *const hints[8]={"陪它发会呆", "上键摸摸它", "下键逗逗它", "连续摸它三次", "安静陪它两分钟", "说话让它听一听", "摸摸和逗逗交替", "连续逗它五次"};
static uint64_t now_ms(void) { return esp_timer_get_time()/1000; }
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
static void update_battery(lv_timer_t *t) {
    (void)t; int soc=bsp_battery_soc();
    if(soc<0) lv_label_set_text(battery_label,"电量未知");
    else lv_label_set_text_fmt(battery_label,"%d%%",soc>100?100:soc);
}
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
static void set_text(lv_obj_t *o,const char *value) {
    if(strcmp(lv_label_get_text(o),value)) lv_label_set_text(o,value);
}
static void draw_face(void) {
    uint64_t now=now_ms();
    bool resting=state.phase==BEAN_REST;
    bool listening=state.phase==BEAN_LISTEN;
    bool blink=!listening && now%4300<120;
    visibility(status_badge,!listening);visibility(status,!listening);visibility(collection,!listening);
    for(int i=0;i<2;i++) {
        visibility(ears[i],listening);
        if(listening) lv_obj_set_y(ears[i],82-(int)bean_ear_raise(now-state.since));
    }
    int gaze=(int)(now/2100%3)-1;
    for(int i=0;i<2;i++) {
        int lid=resting?73:blink?74:state.face==2?29:state.face==3?32:state.face==7?42:0;
        if(state.phase==BEAN_LISTEN) lid=0;
        lv_obj_set_height(lids[i],lid);
        visibility(closed_eyes[i],resting || blink);
        int x=state.phase==BEAN_LISTEN?20:22+gaze*7;
        if(state.face==6) x+=i?6:-6;
        lv_obj_set_pos(pupils[i],x,state.phase==BEAN_THINK?15:state.face==1?18:25);
        lv_obj_set_style_bg_opa(cheeks[i],state.face==1 || state.face==3 || state.face==7?180:55,0);
    }
    int mw=20,mh=16;
    if(resting) {mw=18;mh=5;}
    else if(state.phase==BEAN_TALK && bean_runtime_busy()) {mw=25;mh=8+(int)(bean_runtime_level()/400); if(mh>33)mh=33;}
    else if(state.face==7) {mw=44;mh=28;}
    else if(state.face==3 || state.face==1) {mw=34;mh=11;}
    else if(state.face==2) {mw=29;mh=6;}
    lv_obj_set_size(mouth,mw,mh); lv_obj_set_pos(mouth,(240-mw)/2,196-mh/2);
    const char *s=resting?"眯一小会":state.phase==BEAN_LISTEN?(state.noise?"有点吵，等安静":"你说，我在听"):state.phase==BEAN_THINK?"思考一下":bean_faces[state.face];
    set_text(status,s);
    if(state.phase==BEAN_TALK && state.line>=0) set_text(caption,bean_lines[state.line]);
    else if(state.phase==BEAN_LISTEN) set_text(caption,state.noise?"有点吵，我等你说完":"我在认真听，你继续说");
    else if(state.phase==BEAN_THINK) {
        static const char *const thoughts[]={"思考一下","思考一下·","思考一下··","思考一下···"};
        set_text(caption,thoughts[(now-state.since)/400%4]);
    }
    else set_text(caption,resting?bean_lines[state.line>=0?state.line:24]:"你说吧，我听着呢");
    char count[32];snprintf(count,sizeof(count),"表情 %u / 8",bean_count(state.discovered));
    set_text(collection,count);
    if(!buttons) set_text(footer,"按键不可用，可试着说话");
    else if(!bean_runtime_ready()) set_text(footer,"上摸摸  下逗逗  确定接话\n语音不可用，长按确定设置");
    else set_text(footer,"上摸摸  下逗逗  确定接话\n长按确定：设置与表情册");
}
static void render_menu(void) {
    visibility(face_layer,page==0); visibility(menu_layer,page!=0);
    if(!page) { lv_label_set_text(title,"嘴硬小豆"); return; }
    lv_label_set_text(title,page==1?"小豆设置":"表情收藏册");
    static const char *const volumes[]={"静音","轻声","正常","响亮"};
    for(unsigned i=0;i<8;i++) visibility(rows[i],page==2 || i<4);
    if(page==1) {
        lv_label_set_text_fmt(rows[0],"说话音量    %s",volumes[state.volume]);
        lv_label_set_text_fmt(rows[1],"听声接话    %s",state.auto_listen?"开启":"关闭");
        lv_label_set_text_fmt(rows[2],"表情收藏    %u / 8",bean_count(state.discovered));
        lv_label_set_text(rows[3],"返回小豆");
        for(unsigned i=0;i<4;i++) {
            lv_obj_set_pos(rows[i],14,69+(int)i*39); lv_obj_set_size(rows[i],212,30);
            lv_obj_set_style_bg_opa(rows[i],i==selected?LV_OPA_COVER:LV_OPA_TRANSP,0);
        }
        lv_label_set_text(hint,!bean_runtime_storage_ok()?"暂不能保存，设置本次有效":!bean_runtime_ready()?"语音不可用，文字照常玩":"听声只辨音量，不理解内容");
    } else {
        for(unsigned i=0;i<8;i++) {
            lv_label_set_text_fmt(rows[i],"%s    %s",bean_faces[i],state.discovered&(1u<<i)?"已发现":"未发现");
            lv_obj_set_pos(rows[i],24,49+(int)i*24); lv_obj_set_size(rows[i],192,22);
            lv_obj_set_style_bg_opa(rows[i],i==album_selected?LV_OPA_COVER:LV_OPA_TRANSP,0);
        }
        lv_label_set_text(hint,hints[album_selected]);
    }
}
static void save_preferences(void) {
    uint32_t p=0xB2000000u|(state.discovered&255)|(state.volume<<8)|(state.auto_listen?1u<<10:0);
    if(p!=last_saved) {bean_runtime_save(p);last_saved=p;}
}
static void handle(input_t input,uint64_t now) {
    if(now<guard) return;
    guard=now+160;
    if(input.event==BSP_BTN_LONG) {
        if(input.key!=BSP_BTN_OK) return;
        bean_runtime_stop(); bean_cancel(&state,now);
        page=page?0:1; selected=0; render_menu(); return;
    }
    if(page) {
        if(input.key==BSP_BTN_UP) { if(page==1) selected=(selected+3)%4; else album_selected=(album_selected+7)%8; }
        else if(input.key==BSP_BTN_DOWN) { if(page==1) selected=(selected+1)%4; else album_selected=(album_selected+1)%8; }
        else if(page==2) page=1;
        else if(selected==0) state.volume=(state.volume+1)%4;
        else if(selected==1) state.auto_listen=!state.auto_listen;
        else if(selected==2) page=2;
        else {page=0;bean_cancel(&state,now);}
        render_menu(); return;
    }
    bean_runtime_stop(); bean_key(&state,input.key,now);
}
static void tick(lv_timer_t *t) {
    (void)t; uint64_t now=now_ms(); input_t input;
    while(xQueueReceive(queue,&input,0)==pdTRUE) if(now-(uint64_t)input.at<600) handle(input,now);
    bean_runtime_listen(!page && state.auto_listen && state.phase!=BEAN_TALK);
    if(!page) {
        bean_phase_t before=state.phase;
        bean_tick(&state,now,bean_runtime_sound(),bean_runtime_busy());
        if((before!=BEAN_TALK && state.phase==BEAN_TALK) ||
           (before!=BEAN_REST && state.phase==BEAN_REST)) bean_runtime_play(state.line,state.volume);
        bool dim=state.phase==BEAN_REST;
        if(dim!=dimmed) {dimmed=dim;bsp_display_backlight(dim?25:100);}
        draw_face();
    } else if(dimmed) {dimmed=false;bsp_display_backlight(100);}
    save_preferences();
}
void mouthy_bean_prepare(void) {
    if(!queue) queue=xQueueCreateStatic(6,sizeof(input_t),input_buffer,&control);
}
void mouthy_bean_key(bsp_btn_t key,bsp_btn_ev_t event) {
    if(!atomic_load(&accepting)) return;
    if(event!=BSP_BTN_CLICK && event!=BSP_BTN_DOUBLE && !(event==BSP_BTN_LONG && key==BSP_BTN_OK)) return;
    input_t i={key,event,(int64_t)now_ms()}; (void)xQueueSend(queue,&i,0);
}
void mouthy_bean_enter(bool available) {
    if(screen) return;
    mouthy_bean_prepare(); buttons=available; page=selected=album_selected=0;guard=0;dimmed=false;
    bean_init(&state,esp_random(),now_ms());
    uint32_t p=bean_runtime_saved();
    uint32_t version=p&0xFF000000u;
    if(version==0xB1000000u || version==0xB2000000u) {
        state.discovered=(p&255)|1;state.volume=(p>>8)&3;state.auto_listen=(p&(1u<<10))!=0;
        /* One-time loudness upgrade preserves mute and previously louder choices. */
        if(version==0xB1000000u && state.volume==1) state.volume=2;
    }
    last_saved=p;
    screen=shape(NULL,0,0,240,320,YELLOW,0);
    title=label(screen,"嘴硬小豆",12,14,134,22,false,INK);
    battery_label=label(screen,"电量未知",163,17,65,18,true,INK);
    face_layer=shape(screen,0,40,240,280,YELLOW,0);
    lv_obj_set_y(face_layer,0); lv_obj_set_height(face_layer,320);
    lv_obj_move_background(face_layer);
    status_badge=shape(face_layer,18,44,106,25,0xFFE88B,12);
    status=label(face_layer,"发呆",23,48,96,19,true,INK);
    collection=label(face_layer,"",139,48,89,19,true,INK);
    for(int i=0;i<2;i++) {
        ears[i]=shape(face_layer,i?178:37,82,25,68,0xB67D29,12);
        shape(ears[i],3,3,19,62,0xFFE37B,9);
        shape(ears[i],7,9,11,45,0xEDAB78,6);
    }
    eye(0,19);eye(1,125);
    cheeks[0]=shape(face_layer,27,191,37,12,0xE68048,6);
    cheeks[1]=shape(face_layer,177,191,37,12,0xE68048,6);
    mouth=shape(face_layer,110,189,20,16,0x59402B,12);
    lv_obj_t *bubble=shape(face_layer,14,222,212,49,PAPER,14);
    caption=label(bubble,"",8,7,196,38,false,INK);
    footer=label(face_layer,"",8,280,224,34,true,INK);
    menu_layer=shape(screen,0,42,240,278,PAPER,0);
    /* Menu child positions use full-screen coordinates via a zero-origin layer. */
    lv_obj_set_y(menu_layer,0);lv_obj_set_height(menu_layer,320);lv_obj_move_background(menu_layer);
    for(unsigned i=0;i<8;i++) {rows[i]=label(menu_layer,"",24,49+i*24,192,22,false,INK);lv_obj_set_style_bg_color(rows[i],lv_color_hex(0xFFDE65),0);lv_obj_set_style_radius(rows[i],6,0);}
    hint=label(menu_layer,"",10,251,220,22,true,INK);
    label(menu_layer,"上下选择  确定切换或返回\n长按确定：回到小豆",8,282,224,34,true,INK);
    render_menu();draw_face();update_battery(NULL);
    timer=lv_timer_create(tick,40,NULL);battery_timer=lv_timer_create(update_battery,15000,NULL);
    lv_screen_load(screen);if(placeholder) {lv_obj_delete(placeholder);placeholder=NULL;}
    xQueueReset(queue);atomic_store(&accepting,buttons);
}
void mouthy_bean_exit(void) {
    atomic_store(&accepting,false);bean_runtime_listen(false);bean_runtime_stop();
    if(timer)lv_timer_delete(timer);
    if(battery_timer)lv_timer_delete(battery_timer);
    timer=battery_timer=NULL;
    if(screen) {if(lv_screen_active()==screen) {placeholder=lv_obj_create(NULL);lv_screen_load(placeholder);}lv_obj_delete(screen);}
    screen=NULL;xQueueReset(queue);
    /* The application-lifetime worker owns no UI objects; it only drains audio. */
}
