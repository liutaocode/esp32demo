#include "code_theater.h"
#include "code_theater_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(code_theater_zh_14);
LV_FONT_DECLARE(code_theater_zh_16);
#define BACKGROUND 0x262624
#define ORANGE 0xD97757
#define WHITE 0xE8E6E0
#define MUTED 0xABA79F
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_control;
static uint8_t s_storage[8 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static ct_state_t s_state;
static lv_obj_t *s_screen, *s_title, *s_battery, *s_task, *s_rows[CT_ROWS];
static lv_obj_t *s_status, *s_hint, *s_footer, *s_cursor, *s_spinner[9], *s_live;
static lv_obj_t *s_path, *s_speech, *s_prompt_bg, *s_row_bgs[CT_ROWS];
static lv_obj_t *s_pet, *s_body, *s_arms[2], *s_eyes[2], *s_legs[4], *s_mouth, *s_tears[2];
static lv_timer_t *s_timer, *s_battery_timer;
static unsigned s_page, s_card_page, s_reveal, s_motion, s_scroll;
static unsigned s_rendered_page = 99;
static bool s_buttons, s_appeal_armed;
static int64_t s_last, s_guard, s_started;
static char s_last_line[CT_LINE_BYTES];
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static lv_obj_t *box_on(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent); lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(o,LV_OPA_COVER,0); return o;
}
static lv_obj_t *box(int x,int y,int w,int h,uint32_t color)
{ return box_on(s_screen,x,y,w,h,color); }
static lv_obj_t *label(int x,int y,int width,bool title)
{
    lv_obj_t *o=lv_label_create(s_screen);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,width,title ? 21 : 17);
    lv_obj_set_style_text_font(o,title ? &code_theater_zh_16 : &code_theater_zh_14,0);
    lv_obj_set_style_text_color(o,lv_color_hex(WHITE),0);
    lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP); lv_label_set_text(o,""); return o;
}
static void text(lv_obj_t *o,const char *v,uint32_t color)
{
    if (strcmp(lv_label_get_text(o),v)) lv_label_set_text(o,v);
    lv_obj_set_style_text_color(o,lv_color_hex(color),0);
}
static void mascot(void)
{
    s_pet=box(6,8,72,67,BACKGROUND);
    lv_obj_set_style_bg_opa(s_pet,0,0);
    s_body=box_on(s_pet,17,17,38,29,ORANGE);
    for (unsigned i=0;i<2;i++) {
        s_arms[i]=box_on(s_pet,i ? 55 : 7,30,10,10,ORANGE);
        s_eyes[i]=box_on(s_pet,25+i*16,24,4,7,BACKGROUND);
        s_tears[i]=box_on(s_pet,26+i*16,32,3,5,0x95BBC9);
    }
    for (unsigned i=0;i<4;i++) s_legs[i]=box_on(s_pet,20+i*9,46,4,8,ORANGE);
    s_mouth=box_on(s_pet,32,38,8,2,BACKGROUND);
    box_on(s_pet,10,60,54,2,0x74716A);
    for (unsigned i=0;i<6;i++) box_on(s_pet,14+i*8,57,5,2,0x74716A);
}
static void animate_pet(void)
{
    ct_pose_t pose=ct_pose(&s_state);
    unsigned frame=s_motion/180;
    bool blink=s_motion%3600<180;
    int bounce=pose==CT_PET_HAPPY ? (int)(frame%3)*-2 : (frame%4==0 ? -1 : 0);
    lv_obj_set_y(s_pet,8+bounce);
    lv_obj_set_style_bg_color(s_body,lv_color_hex(pose==CT_PET_SAD ? 0xB46C58 : ORANGE),0);
    for (unsigned i=0;i<2;i++) {
        int hand_y=30;
        if (pose==CT_PET_TYPE) hand_y=32+((frame+i)%2 ? 3 : -2);
        if (pose==CT_PET_HAPPY) hand_y=20+((frame+i)%2)*3;
        if (pose==CT_PET_WAVE) hand_y=i ? 14+(frame%2)*5 : 28;
        if (pose==CT_PET_WORRY) hand_y=i ? 20 : 35;
        if (pose==CT_PET_WAIT) hand_y=i ? 24+(frame%4==0 ? -5 : 0) : 34;
        if (pose==CT_PET_SAD) hand_y=38;
        lv_obj_set_y(s_arms[i],hand_y);
        int look=pose==CT_PET_READ ? (int)(frame%3)-1 : pose==CT_PET_THINK ? 1 : 0;
        lv_obj_set_x(s_eyes[i],25+i*16+look);
        lv_obj_set_y(s_eyes[i],pose==CT_PET_THINK ? 22 : 24);
        lv_obj_set_height(s_eyes[i],blink || pose==CT_PET_SLEEP || pose==CT_PET_SAD ? 2 : 7);
        lv_obj_set_style_bg_opa(s_tears[i],pose==CT_PET_SAD ? 255 : 0,0);
        lv_obj_set_y(s_tears[i],31+(frame%3));
    }
    for (unsigned i=0;i<4;i++)
        lv_obj_set_y(s_legs[i],46+(pose==CT_PET_TYPE || pose==CT_PET_HAPPY ? (frame+i)%2*2 : 0));
    lv_obj_set_height(s_mouth,pose==CT_PET_HAPPY ? 5 : pose==CT_PET_WORRY ? 4 : 2);
    lv_obj_set_width(s_mouth,pose==CT_PET_SAD ? 5 : 8);
}
static void animate(void)
{
    static const int8_t xy[8][2]={{0,-1},{1,-1},{1,0},{1,1},{0,1},{-1,1},{-1,0},{-1,-1}};
    unsigned frame=s_motion/120%8;
    bool moving=!s_page && !s_state.paused && s_state.access==CT_ACCESS_OK &&
        s_state.phase!=CT_DONE && s_state.phase!=CT_REST && s_state.phase!=CT_INTERRUPTED;
    int radius=moving ? (frame%4==0 ? 7 : frame%4==2 ? 4 : 6) : 5;
    for (unsigned n=0;n<8;n++) {
        lv_obj_set_pos(s_spinner[n],19+xy[n][0]*radius,232+xy[n][1]*radius);
        lv_obj_set_size(s_spinner[n],n%2 ? 2 : (xy[n][0] ? 5 : 2),n%2 ? 2 : (xy[n][1] ? 5 : 2));
        lv_obj_set_style_bg_opa(s_spinner[n],moving ? ((n+8-frame)%8<4 ? 255 : 105) : 105,0);
    }
    bool cursor_on=!s_page && s_state.access==CT_ACCESS_OK && s_state.phase!=CT_WAIT && s_motion/480%2==0;
    const char *prompt=lv_label_get_text(s_hint);
    uint32_t chars=0;
    for (const unsigned char *p=(const unsigned char *)prompt;*p;p++)
        if ((*p&0xC0)!=0x80) chars++;
    lv_point_t end; lv_label_get_letter_pos(s_hint,chars,&end);
    int cursor_x=12+end.x+3;
    lv_obj_set_x(s_cursor,cursor_x>222 ? 222 : cursor_x);
    lv_obj_set_style_bg_opa(s_cursor,cursor_on ? 210 : 0,0);
    animate_pet();
}
static void render(void)
{
    static const uint32_t colors[]={WHITE,ORANGE,0xEEABA4,0xB2D2A4,MUTED};
    unsigned view=s_state.access!=CT_ACCESS_OK ? 2+s_state.access : s_page;
    if (s_rendered_page!=view) {
        for (unsigned i=0;i<CT_ROWS;i++) text(s_rows[i],"",WHITE);
        s_rendered_page=view;
    }
    char b[96];
    snprintf(b,sizeof b,"~/%s",ct_project(s_state.project)); text(s_path,b,MUTED);
    text(s_speech,ct_speech(&s_state),ORANGE);
    lv_obj_set_style_bg_color(s_prompt_bg,lv_color_hex(s_state.access==CT_ACCESS_OK ? 0x363633 : 0x482F2D),0);
    if (s_state.access!=CT_ACCESS_OK) {
        text(s_task,"> 账户访问受限",0xEEABA4);
        text(s_rows[0],"我们发现您的账户异常，",WHITE);
        text(s_rows[1],"已经停止了您的访问权限。",WHITE);
        if (s_state.access==CT_APPEAL_FAILED) {
            text(s_rows[3],"申诉失败，封禁维持。",0xEEABA4);
            text(s_hint,"> 申诉已处理，请稍候",MUTED);
        } else {
            text(s_rows[3],"可以尝试提交一次申诉。",MUTED);
            text(s_hint,"> 点击申诉",WHITE);
        }
        snprintf(b,sizeof b,"%u 秒后自动恢复",(unsigned)((s_state.ban_left+999)/1000));
        text(s_status,b,MUTED);
        text(s_footer,s_state.access==CT_BANNED ? "确定键申诉 · 稍等自动恢复" : "稍等一会，小人还在这里",MUTED);
    } else if (s_page) {
        snprintf(b,sizeof b,"> 一起完成 %u 项",(unsigned)s_state.completed); text(s_task,b,WHITE);
        for (unsigned i=0;i<CT_ROWS;i++) {
            unsigned card=s_card_page*CT_ROWS+i;
            snprintf(b,sizeof b,"%02u %s",card+1,s_state.cards&(1u<<card) ? ct_card(card) : "等我们一起遇见");
            text(s_rows[i],b,s_state.cards&(1u<<card) ? colors[3] : MUTED);
        }
        snprintf(b,sizeof b,"陪伴记录 %u/12 · 第 %u/2 页",ct_card_count(&s_state),s_card_page+1);
        text(s_status,b,MUTED); text(s_hint,"> 确定回到工作台",WHITE);
        text(s_footer,"上下翻页 · 记录关机清零",MUTED);
    } else {
        snprintf(b,sizeof b,"> %s",ct_task(s_state.task)); text(s_task,b,WHITE);
        for (unsigned i=0;i<s_state.count;i++) {
            snprintf(b,sizeof b,"%s",s_state.lines[i]);
            if (i+1==s_state.count) {
                unsigned bytes=0,chars=0;
                while (b[bytes] && chars<s_reveal) { bytes++; while ((b[bytes]&0xC0)==0x80) bytes++; chars++; }
                b[bytes]='\0';
            }
            text(s_rows[i],b,colors[s_state.colors[i]]);
        }
        for (unsigned i=s_state.count;i<CT_ROWS;i++) text(s_rows[i],"",WHITE);
        unsigned minutes=(unsigned)((now_ms()-s_started)/60000);
        if (minutes>999) minutes=999;
        snprintf(b,sizeof b,"陪你 %u 分钟 · 完成 %u 项",minutes,(unsigned)s_state.completed);
        text(s_status,b,MUTED);
        if (s_state.phase==CT_WAIT) {
            text(s_hint,"> 稳妥修复 / 大胆重写",WHITE);
            text(s_footer,"上稳妥 下大胆 确定默认",MUTED);
        } else {
            text(s_hint,s_state.phase==CT_INTERRUPTED ? "> 按确定，告诉我新想法" : "> 确定键，和我说句话",WHITE);
            text(s_footer,s_motion/3600%2 ? "长按确定看陪伴记录" :
                 s_state.phase==CT_INTERRUPTED ? "上继续 下换项目 确定互动" : "上打断 下换项目 确定互动",MUTED);
        }
    }
    if (!s_buttons) text(s_footer,"按键未就绪 · 我陪你等会",colors[2]);
    for (unsigned i=0;i<CT_ROWS;i++) {
        uint8_t c=view==0 && i<s_state.count ? s_state.colors[i] : 0;
        lv_obj_set_style_bg_color(s_row_bgs[i],lv_color_hex(c==2 ? 0x442D2B : 0x2C3A29),0);
        lv_obj_set_style_bg_opa(s_row_bgs[i],c==2 || c==3 ? 255 : 0,0);
        int y=112+i*18+(view ? 0 : (int)s_scroll);
        lv_obj_set_y(s_rows[i],y); lv_obj_set_y(s_row_bgs[i],y);
    }
    if (s_state.access==CT_ACCESS_OK && s_state.phase==CT_WAIT && !s_page)
        snprintf(b,sizeof b,"%s · %u秒",ct_verb(&s_state),(unsigned)((6000-s_state.elapsed+999)/1000));
    else snprintf(b,sizeof b,"%s%s",ct_verb(&s_state),
                  s_state.access!=CT_ACCESS_OK || s_state.phase==CT_DONE || s_state.phase==CT_INTERRUPTED || s_state.paused ? "" : "…");
    text(s_live,b,ORANGE); animate();
}
static void battery(lv_timer_t *t)
{
    (void)t; int n=bsp_battery_soc();
    if (n<0) lv_label_set_text(s_battery,"--%");
    else lv_label_set_text_fmt(s_battery,"%d%%",n>100 ? 100 : n);
}
static void dispatch(input_t i)
{
    if (s_state.access!=CT_ACCESS_OK) {
        if (s_appeal_armed && i.button==BSP_BTN_OK && i.event!=BSP_BTN_LONG) {
            (void)ct_appeal(&s_state,esp_random()); s_appeal_armed=false;
        }
        return;
    }
    if (i.event==BSP_BTN_LONG) { s_page=!s_page; s_state.paused=s_page!=0; return; }
    if (s_page) {
        if (i.button==BSP_BTN_OK) { s_page=0; s_state.paused=false; }
        else s_card_page^=1;
    } else if (s_state.phase==CT_WAIT) ct_choose(&s_state,i.button==BSP_BTN_DOWN);
    else if (i.button==BSP_BTN_UP) ct_interrupt(&s_state);
    else if (i.button==BSP_BTN_DOWN) ct_next(&s_state);
    else ct_boost(&s_state);
}
static void tick(lv_timer_t *t)
{
    (void)t; int64_t now=now_ms();
    uint32_t delta=now>s_last ? (uint32_t)(now-s_last) : 0; s_last=now;
    unsigned old_access=s_state.access; ct_tick(&s_state,delta);
    if (old_access && !s_state.access) { s_page=0; s_appeal_armed=false; }
    input_t i;
    while (xQueueReceive(s_queue,&i,0)==pdTRUE) {
        if (!s_buttons || now-i.at>250) continue;
        /* Count raw physical presses before the visual input guard. CLICK/DOUBLE
           must never inflate the streak; the triggering release cannot appeal. */
        if (i.event==BSP_BTN_PRESS) {
            if (s_state.access!=CT_ACCESS_OK) s_appeal_armed=i.button==BSP_BTN_OK;
            else if (ct_press(&s_state,(uint32_t)i.at)) {
                s_page=0; s_appeal_armed=false; s_guard=now+180;
            }
            continue;
        }
        if (now<s_guard) continue;
        dispatch(i); s_guard=now+180;
    }
    s_motion=(s_motion+(delta>1000 ? 1000 : delta))%14400;
    const char *last=s_state.count ? s_state.lines[s_state.count-1] : "";
    if (strcmp(last,s_last_line)) {
        snprintf(s_last_line,sizeof s_last_line,"%s",last); s_reveal=0;
        s_scroll=s_state.count==CT_ROWS ? 2 : 0;
    } else if (!s_state.paused && s_state.access==CT_ACCESS_OK) {
        if (s_reveal<80) s_reveal++;
        if (s_scroll) s_scroll--;
    }
    render();
}
void code_theater_prepare(void)
{
    if (!s_queue) s_queue=xQueueCreateStatic(8,sizeof(input_t),s_storage,&s_control);
}
void code_theater_enter(bool buttons_available)
{
    code_theater_prepare(); xQueueReset(s_queue); ct_init(&s_state,esp_random());
    s_rendered_page=99; s_buttons=buttons_available; s_appeal_armed=false;
    s_page=s_card_page=s_reveal=s_motion=s_scroll=0;
    s_last_line[0]='\0'; s_started=s_last=now_ms(); s_guard=0;
    s_screen=lv_obj_create(NULL); lv_obj_remove_flag(s_screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(s_screen,0,0); lv_obj_set_style_border_width(s_screen,0,0);
    lv_obj_set_style_bg_color(s_screen,lv_color_hex(BACKGROUND),0);
    mascot(); s_title=label(84,8,108,true); text(s_title,"Claude Code",WHITE);
    s_battery=label(195,9,37,false); lv_obj_set_style_text_align(s_battery,LV_TEXT_ALIGN_RIGHT,0);
    s_speech=label(84,33,148,false); s_path=label(84,55,148,false);
    s_prompt_bg=box(8,82,224,22,0x363633); s_task=label(12,84,216,false);
    for (unsigned i=0;i<CT_ROWS;i++) {
        s_row_bgs[i]=box(8,112+i*18,224,17,BACKGROUND);
        s_rows[i]=label(12,112+i*18,216,false);
    }
    for (unsigned i=0;i<9;i++) s_spinner[i]=box(19,232,3,3,ORANGE);
    s_live=label(36,227,196,false); s_status=label(12,248,216,false);
    box(8,269,224,1,0x75756D); box(8,292,224,1,0x75756D);
    s_hint=label(12,273,216,false); s_cursor=box(205,274,7,13,MUTED);
    s_footer=label(12,301,216,false);
    render(); battery(NULL); lv_screen_load(s_screen);
    s_timer=lv_timer_create(tick,60,NULL); s_battery_timer=lv_timer_create(battery,10000,NULL);
    atomic_store(&s_accept,true);
}
void code_theater_exit(void)
{
    atomic_store(&s_accept,false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer=s_battery_timer=NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen=NULL;
}
void code_theater_key(bsp_btn_t b,bsp_btn_ev_t e)
{
    if (!atomic_load(&s_accept) || !s_queue || b<BSP_BTN_UP || b>BSP_BTN_OK) return;
    if (e!=BSP_BTN_PRESS && e!=BSP_BTN_CLICK && e!=BSP_BTN_DOUBLE && !(b==BSP_BTN_OK && e==BSP_BTN_LONG)) return;
    input_t i={b,e,now_ms()}; (void)xQueueSend(s_queue,&i,0);
}
