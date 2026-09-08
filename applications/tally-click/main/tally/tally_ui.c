#include "tally_ui.h"
#include <stdio.h>
LV_FONT_DECLARE(tally_zh_12);
LV_FONT_DECLARE(tally_zh_14);
LV_FONT_DECLARE(tally_zh_20);
#define BG 0x101619
#define WHITE 0xF2F7ED
#define GREEN 0xB5F76B
#define BLUE 0x7DD9F6
#define AMBER 0xFFD280
#define MUTED 0x84958E
static lv_obj_t *screen,*previous_screen,*content,*digits,*popup,*ripple,*spark[8],*sweep;
static lv_timer_t *fx_timer;
static int digit_y;
static tc_feedback fx;
static uint32_t fx_at, digit_color, effect_color;
static lv_obj_t *rect(lv_obj_t *p,int x,int y,int w,int h,uint32_t color,int radius)
{
    lv_obj_t *o=lv_obj_create(p); lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y); lv_obj_set_size(o,w,h);
    lv_obj_set_style_pad_all(o,0,0); lv_obj_set_style_border_width(o,0,0);
    lv_obj_set_style_radius(o,radius,0); lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    return o;
}
static lv_obj_t *label(const char *text,int x,int y,int w,const lv_font_t *font,uint32_t color)
{
    lv_obj_t *o=lv_label_create(content); lv_label_set_text(o,text);
    lv_obj_set_style_text_font(o,font,0); lv_obj_set_style_text_color(o,lv_color_hex(color),0);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_pos(o,x,y); lv_obj_set_width(o,w); lv_label_set_long_mode(o,LV_LABEL_LONG_CLIP);
    return o;
}
static void line(const char *text,int y,const lv_font_t *font,uint32_t color)
{ label(text,8,y,224,font,color); }
/* Original scalable seven-segment digits, no missing glyphs or font stretching.
   Each count keeps the same tall baseline, including four-digit values. */
static void digit(lv_obj_t *parent,int x,int w,int h,unsigned number,uint32_t color)
{
    static const unsigned masks[]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};
    int t=w/6; if(t<6)t=6; if(t>13)t=13;
    int half=(h-t)/2;
    const int shape[7][4]={{t,0,w-2*t,t},{w-t,t,t,half-t},{w-t,half+t,t,h-half-2*t},
        {t,h-t,w-2*t,t},{0,half+t,t,h-half-2*t},{0,t,t,half-t},{t,half,w-2*t,t}};
    for(unsigned j=0;j<7;j++) {
        lv_obj_t *o=rect(parent,x+shape[j][0],shape[j][1],shape[j][2],shape[j][3],
            masks[number]&(1u<<j) ? color : 0x1D2829,3);
        if(masks[number]&(1u<<j)) {
            lv_obj_set_user_data(o,(void *)(uintptr_t)1);
            lv_obj_set_style_shadow_color(o,lv_color_hex(color),0);
            lv_obj_set_style_shadow_width(o,5,0); lv_obj_set_style_shadow_opa(o,22,0);
        }
    }
}
static void value(uint32_t v,int y,int h,uint32_t color)
{
    char b[16]; snprintf(b,sizeof(b),"%u",(unsigned)v);
    unsigned n=0; while(b[n])++n;
    int widths[]={0,112,94,64,47}; int w=widths[n],gap=9,total=(int)n*w+((int)n-1)*gap;
    digits=rect(content,(240-total)/2,y,total,h,BG,0); digit_y=y; digit_color=color;
    lv_obj_set_style_bg_opa(digits,LV_OPA_TRANSP,0);
    for(unsigned i=0;i<n;i++)digit(digits,(int)i*(w+gap),w,h,(unsigned)(b[i]-'0'),color);
}
static void hint(const char *left,const char *right,uint32_t accent)
{
    rect(content,12,262,104,33,0x223034,7); rect(content,124,262,104,33,0x223034,7);
    label(left,12,265,104,&tally_zh_20,accent); label(right,124,265,104,&tally_zh_20,WHITE);
}
static void hide_fx(void)
{
    if(popup)lv_obj_add_flag(popup,LV_OBJ_FLAG_HIDDEN);
    if(ripple)lv_obj_add_flag(ripple,LV_OBJ_FLAG_HIDDEN);
    if(sweep)lv_obj_add_flag(sweep,LV_OBJ_FLAG_HIDDEN);
    for(unsigned i=0;i<8;i++)if(spark[i])lv_obj_add_flag(spark[i],LV_OBJ_FLAG_HIDDEN);
    if(digits) {
        lv_obj_set_y(digits,digit_y);
        for(unsigned i=0;i<lv_obj_get_child_count(digits);i++) {
            lv_obj_t *o=lv_obj_get_child(digits,i);
            if(lv_obj_get_user_data(o))lv_obj_set_style_bg_color(o,lv_color_hex(digit_color),0);
        }
    }
}
static void animate(lv_timer_t *timer)
{
    (void)timer;
    if(!screen || fx==TC_FX_NONE)return;
    uint32_t elapsed=lv_tick_elaps(fx_at);
    unsigned duration=fx==TC_FX_ARCHIVE ? 620:360;
    if(elapsed>=duration) { hide_fx(); fx=TC_FX_NONE; return; }
    int p=(int)(elapsed*1000/duration),fade=255-p*255/1000;
    int kick=(1000-p)*(1000-p)*9/1000000;
    lv_obj_set_y(digits,digit_y+(fx==TC_FX_SUBTRACT ? kick : -kick));
    for(unsigned i=0;i<lv_obj_get_child_count(digits);i++) {
        lv_obj_t *o=lv_obj_get_child(digits,i);
        if(lv_obj_get_user_data(o))lv_obj_set_style_bg_color(o,
            lv_color_mix(lv_color_hex(effect_color),lv_color_hex(digit_color),(uint8_t)fade),0);
    }
    lv_obj_set_style_opa(popup,(lv_opa_t)fade,0);
    lv_obj_set_style_border_opa(ripple,(lv_opa_t)(fade/2),0);
    int inset=18-p*16/1000;
    lv_obj_set_pos(ripple,inset,64+inset); lv_obj_set_size(ripple,240-2*inset,192-2*inset);
    if(fx==TC_FX_ARCHIVE) {
        lv_obj_set_y(sweep,70+p*177/1000); lv_obj_set_style_opa(sweep,(lv_opa_t)(fade/3),0);
        for(int i=0;i<8;i++) {
            int side=i%2 ? 1:-1;
            lv_obj_set_pos(spark[i],118+side*(22+p*(26+(i/2)*8)/1000),138+(i/2-2)*22+p*(12+i*6)/1000);
            lv_obj_set_style_opa(spark[i],(lv_opa_t)fade,0);
        }
    }
}
void tc_ui_feedback(tc_feedback event)
{
    if(!screen || !digits || event==TC_FX_NONE)return;
    fx=event; fx_at=lv_tick_get(); hide_fx();
    const char *t=event==TC_FX_ADD ? "+1" : event==TC_FX_SUBTRACT ? "-1" :
        event==TC_FX_PAUSE ? "已锁定" : event==TC_FX_RESUME ? "继续" :
        event==TC_FX_ARCHIVE ? "已归档" : event==TC_FX_LIMIT ? "到边界了" :
        event==TC_FX_ERROR ? "请重试" : "";
    uint32_t c=event==TC_FX_SUBTRACT || event==TC_FX_UP ? BLUE :
        event==TC_FX_PAUSE || event==TC_FX_LIMIT || event==TC_FX_ERROR ? AMBER : GREEN;
    effect_color=c;
    lv_label_set_text(popup,t); lv_obj_set_style_text_color(popup,lv_color_hex(c),0);
    lv_obj_remove_flag(popup,LV_OBJ_FLAG_HIDDEN); lv_obj_set_style_border_color(ripple,lv_color_hex(c),0);
    lv_obj_remove_flag(ripple,LV_OBJ_FLAG_HIDDEN);
    if(event==TC_FX_ARCHIVE) {
        lv_obj_remove_flag(sweep,LV_OBJ_FLAG_HIDDEN);
        for(int i=0;i<8;i++)lv_obj_remove_flag(spark[i],LV_OBJ_FLAG_HIDDEN);
    }
    animate(NULL);
}
void tc_ui_create(void)
{
    previous_screen=lv_screen_active(); screen=lv_obj_create(NULL);
    lv_obj_remove_flag(screen,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen,lv_color_hex(BG),0); lv_obj_set_style_pad_all(screen,0,0);
    lv_obj_set_style_border_width(screen,0,0);
    content=rect(screen,0,0,240,320,BG,0);
    lv_screen_load(screen); fx_timer=lv_timer_create(animate,30,NULL);
}
void tc_ui_render(const tc_state *s,tc_notice notice,int battery,int64_t now)
{
    if(!screen)return;
    /* Only rebuild for application changes, never from the animation timer. */
    fx=TC_FX_NONE; digits=popup=ripple=sweep=NULL;
    for(int i=0;i<8;i++)spark[i]=NULL;
    lv_obj_clean(content);
    char b[80]; uint32_t accent=s->page==TC_PAUSE ? AMBER:GREEN;
    const char *head=s->page==TC_HISTORY_PAGE ? "记录小本" :
        s->page==TC_PAUSE ? "暂停计数" : "点点有数";
    label(head,10,17,126,&tally_zh_20,accent);
    snprintf(b,sizeof(b),battery>=0 && battery<=100 ? "%d%%" : "--",battery);
    label(b,178,22,52,&tally_zh_14,MUTED);
    label(notice==TC_PENDING ? "待存" : notice==TC_STORED || notice==TC_ARCHIVED ? "已存":"",
        139,24,34,&tally_zh_12,notice==TC_PENDING ? AMBER:MUTED);
    rect(content,14,51,212,1,0x32403D,0);
    /* Decorative feedback lives behind the numeral group. */
    ripple=rect(content,12,76,216,168,BG,18);
    lv_obj_set_style_bg_opa(ripple,LV_OPA_TRANSP,0); lv_obj_set_style_border_width(ripple,2,0);
    sweep=rect(content,12,70,216,9,GREEN,4);
    for(int i=0;i<8;i++)spark[i]=rect(content,116,136,5,5,i%2 ? WHITE:GREEN,2);
    const char *foot="确定暂停 · 长按归档";
    if(s->page==TC_HISTORY_PAGE) {
        if(s->data.length) {
            const tc_record *r=&s->data.records[s->selected];
            snprintf(b,sizeof(b),"最近第 %u 条 / %u",s->selected+1,(unsigned)s->data.length);
            line(b,63,&tally_zh_14,MUTED); value(r->count,94,142,WHITE);
            line("本轮已归档",249,&tally_zh_20,WHITE);
            line("上键较新 · 下键较早",277,&tally_zh_14,GREEN);
        } else { value(0,82,150,WHITE); line("还没有归档记录",267,&tally_zh_20,MUTED); }
        foot="确定返回 · 保留十条";
    } else {
        value(s->data.count,83,163,s->page==TC_PAUSE ? AMBER:WHITE);
        if(s->page==TC_PAUSE) { line("上下查看记录",265,&tally_zh_20,AMBER); foot="确定继续 · 长按归档"; }
        else hint("下 +1","上 -1",GREEN);
    }
    switch(notice) {
    case TC_PENDING: foot="保存中 · 确定暂停";break;
    case TC_SAVING: foot="正在归档，请稍候";break;
    case TC_SAVE_ERROR: foot="保存失败，数目保留";break;
    case TC_INPUT_ERROR: foot="已暂停，请核对数目";break;
    case TC_RECOVER_ERROR: foot="读取失败，请重启";break;
    case TC_BUTTON_ERROR: foot="按键不可用，请重启";break;
    case TC_ARCHIVED: foot="新一轮，开始计数";break;
    default: break;
    }
    line(foot,302,&tally_zh_12,notice==TC_SAVE_ERROR || notice==TC_INPUT_ERROR ? AMBER:MUTED);
    popup=label("",20,54,200,&tally_zh_14,GREEN);
    hide_fx(); (void)now;
}
lv_obj_t *tc_ui_screen(void) { return screen; }
void tc_ui_destroy(void)
{
    if(!screen)return;
    if(fx_timer)lv_timer_delete(fx_timer);
    fx_timer=NULL; fx=TC_FX_NONE;
    lv_obj_t *old=screen;
    if(lv_screen_active()==old && previous_screen)lv_screen_load(previous_screen);
    screen=content=digits=popup=ripple=sweep=NULL;
    for(int i=0;i<8;i++)spark[i]=NULL;
    lv_obj_delete(old);
}
