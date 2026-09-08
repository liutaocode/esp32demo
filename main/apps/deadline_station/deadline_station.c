#include "deadline_station.h"
#include "deadline_station_state.h"
#include "deadline_station_storage.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>
#include <stdio.h>

LV_FONT_DECLARE(deadline_station_zh_12);
LV_FONT_DECLARE(deadline_station_zh_16);
LV_FONT_DECLARE(deadline_station_zh_24);
typedef enum { HOME, DETAIL, MENU, FILTER, YEAR, CLOCK, CHECKLIST, FOCUS, ABOUT } page_t;
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_bytes[4*sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static bool s_prepared, s_buttons, s_clock_valid, s_dimmed;
static ds_progress_t s_progress;
static ds_date_t s_edit;
static int64_t s_anchor_utc, s_anchor_ms, s_activity, s_last_input, s_second;
static int64_t s_focus_end, s_focus_left;
static bool s_focus_running, s_focus_done;
static unsigned s_order[DS_MAX_ENTRIES], s_count, s_cursor, s_field, s_year=2027;
static unsigned s_menu, s_node, s_check, s_edit_field, s_choice;
static page_t s_page;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_hint, *s_battery;
static lv_obj_t *s_big, *s_small, *s_clock_label, *s_status;
static lv_timer_t *s_timer, *s_battery_timer;
static const char *const MENU_ITEMS[] = {"筛选会议", "切换年份", "投稿清单", "专注一刻", "校准时间", "资料说明"};
static const char *const CHECKS[] = {"账号与作者", "实验与图表", "正文与匿名", "上传并核验"};
static int64_t now_ms(void) { return esp_timer_get_time()/1000; }
static int64_t utc_now(void) { return s_clock_valid ? s_anchor_utc+(now_ms()-s_anchor_ms)/1000 : 0; }
static const ds_entry_t *entry(void) { return s_count ? &ds_catalog[s_order[s_cursor]] : NULL; }
static void save(void) { ds_storage_save(s_progress); }
static void refresh_list(void)
{
    s_count=ds_list(s_order,s_field,s_year,&s_progress,utc_now()); s_cursor=0;
}
static lv_obj_t *label(lv_obj_t *p, const char *value, int x, int y, int w, unsigned size, uint32_t color)
{
    const lv_font_t *font=size==24 ? &deadline_station_zh_24 : size==12 ? &deadline_station_zh_12 : &deadline_station_zh_16;
    lv_obj_t *o=ui_pixel_label(p,value,font,color);
    lv_obj_set_pos(o,x,y);lv_obj_set_width(o,w);
    lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);
    return o;
}
static lv_obj_t *text(const char *value,int y,unsigned size,uint32_t color)
{
    return label(s_content,value,0,y,198,size,color);
}
static lv_obj_t *box(int x,int y,int w,int h,uint32_t color)
{
    lv_obj_t *o=lv_obj_create(s_content);
    lv_obj_remove_flag(o,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o,x,y);lv_obj_set_size(o,w,h);
    lv_obj_set_style_pad_all(o,0,0);lv_obj_set_style_border_width(o,0,0);
    lv_obj_set_style_radius(o,3,0);lv_obj_set_style_bg_color(o,lv_color_hex(color),0);
    return o;
}
static void footer(const char *first,const char *second)
{
    lv_label_set_text(s_footer,s_buttons ? first : "按键不可用");
    lv_label_set_text(s_hint,second);
}
static void battery(lv_timer_t *timer)
{
    (void)timer;int soc=bsp_battery_soc();
    if(soc<0) lv_label_set_text(s_battery,"--%");
    else lv_label_set_text_fmt(s_battery,"%d%%",soc>100?100:soc);
}
static void date_text(lv_obj_t *o,int64_t utc)
{
    ds_date_t d=ds_beijing(utc);
    lv_label_set_text_fmt(o,"%04d-%02d-%02d  %02d:%02d",d.year,d.month,d.day,d.hour,d.minute);
}
static void countdown(void)
{
    const ds_entry_t *e=entry();
    if(!e || !s_big) return;
    int n=ds_next_node(e,utc_now());
    if(!s_clock_valid) {
        lv_label_set_text(s_big,"先校准时间");lv_label_set_text(s_small,"确定后按北京时间设置");
    } else if(!e->count) {
        lv_label_set_text(s_big,"等待官宣");lv_label_set_text(s_small,"日期未定，不显示倒计时");
    } else if(n<0) {
        lv_label_set_text(s_big,"本轮已截止");lv_label_set_text(s_small,"不再接受本轮新稿");
    } else {
        int64_t left=e->nodes[n].utc-utc_now();
        lv_label_set_text_fmt(s_big,"%lld天",(long long)(left/86400));
        lv_label_set_text_fmt(s_small,"%02u小时 %02u分 %02u秒",(unsigned)(left%86400/3600),(unsigned)(left%3600/60),(unsigned)(left%60));
    }
    if(s_clock_label && s_clock_valid) {
        ds_date_t d=ds_beijing(utc_now());
        lv_label_set_text_fmt(s_clock_label,"北京 %02d月%02d日 %02d:%02d",d.month,d.day,d.hour,d.minute);
    }
}
static void sprout(unsigned stage)
{
    /* A small paper plant celebrates preparation without interrupting work. */
    box(85,172,28,15,0xBA7860);box(80,170,38,6,0xD99B70);
    box(97,153,4,19,0x427660);
    if(stage) {box(82,161,17,6,0x66A878);box(101,158,15,6,0x8FBE73);}
    if(stage>=3) {box(89,147,21,15,0xFFF3CD);box(93,150,13,2,0x7A9C8F);box(93,155,10,2,0x7A9C8F);}
    if(stage==4) {box(119,150,5,5,UI_ORANGE);box(72,149,4,4,UI_ORANGE);}
}
static void render(void)
{
    s_big=s_small=s_clock_label=s_status=NULL;lv_obj_clean(s_content);
    const ds_entry_t *e=entry();
    lv_obj_t *o;
    int n=e ? ds_next_node(e,utc_now()) : -1;
    if(s_page==HOME) {
        o=text("",0,12,0x53776C);lv_label_set_text_fmt(o,"%u年 · %s  %u/%u",s_year,ds_fields[s_field],s_count?s_cursor+1:0,s_count);
        if(!e) {
            text("还没有收藏",43,24,UI_INK);text("在会议详情按确定收藏",90,16,UI_INK);
            text("长按确定可调整筛选",122,16,0x53776C);sprout(1);
            footer("长按确定打开菜单","北京时区 · 离线资料");return;
        }
        text(e->name,24,24,UI_INK);
        o=text("",55,12,0x53776C);
        lv_label_set_text_fmt(o,"%s · %s",e->track,n>=0 ? e->nodes[n].name : e->count ? "已截止" : "待公布");
        s_big=text("",75,24,0x285E53);s_small=text("",109,12,UI_INK);
        o=text("",130,12,0x53776C);
        if(e->count) date_text(o,e->nodes[n<0 ? e->count-1 : (unsigned)n].utc);
        else lv_label_set_text(o,e->note);
        sprout(ds_checks(s_progress.flags[s_order[s_cursor]]));
        o=label(s_content,(s_progress.flags[s_order[s_cursor]]&1)?"已收藏":"可收藏",4,167,70,12,0x53776C);(void)o;
        label(s_content,n>=0 && ds_urgency(e->nodes[n].utc,utc_now())<=2?"临近截稿":"慢慢推进",127,167,68,12,0x9B593D);
        s_clock_label=text("尚未校时",195,12,0x53776C);
        countdown();footer("上下换会议  确定看详情","长按确定：菜单 · 时间均为北京");
    } else if(s_page==DETAIL && e) {
        text(e->name,0,24,UI_INK);o=text("",32,12,0x53776C);
        lv_label_set_text_fmt(o,"%u年 · %s",e->year,e->track);
        if(e->count) {
            if(s_node>=e->count) s_node=0;
            text(e->nodes[s_node].name, 60,16,UI_INK);
            o=text("",92,16,0x285E53);date_text(o,e->nodes[s_node].utc);
            text("北京时间 · 原时区西十二区",120,12,0x53776C);
            text(e->nodes[s_node].utc<=utc_now()?"此节点已截止":"请提前完成，勿卡最后一刻",144,12,0x9B593D);
        } else {text("日期待公布",70,24,UI_INK);text(e->note,111,16,0x53776C);}
        text(e->note,168,12,0x9B593D);
        text((s_progress.flags[s_order[s_cursor]]&1)?"确定取消收藏":"确定加入收藏",191,16,0x285E53);
        footer("上下切换节点  确定收藏","长按确定返回首页");
    } else if(s_page==MENU) {
        text("小站菜单",0,24,UI_INK);
        unsigned start=s_menu/4*4;
        for(unsigned i=start;i<start+4 && i<6;i++) {
            int y=39+(int)(i-start)*39;
            if(i==s_menu)box(0,y-3,198,33,0xD8E9CF);
            text(MENU_ITEMS[i],y,16,UI_INK);
        }
        footer("上下选择  确定进入","长按确定返回首页");
    } else if(s_page==FILTER || s_page==YEAR) {
        text(s_page==FILTER?"选择方向":"选择会议年份",8,24,UI_INK);
        if(s_page==FILTER)text(ds_fields[s_choice],70,24,0x285E53);
        else {o=text("",70,24,0x285E53);lv_label_set_text_fmt(o,"%u年",2027+s_choice);}
        text(s_page==YEAR && s_choice>0?"远期关注，未定日期不倒数":"按下一次，选择更近一步",125,12,0x53776C);
        sprout(3);footer("上下切换  确定应用","长按确定取消");
    } else if(s_page==CLOCK) {
        static const char *const fields[]={"年","月","日","时","分","确认时间"};
        text("校准北京时间",0,24,UI_INK);
        text("断电或重启后需要重新校时",32,12,0x9B593D);
        o=text("", 60,24,UI_INK);lv_label_set_text_fmt(o,"%04d-%02d-%02d",s_edit.year,s_edit.month,s_edit.day);
        o=text("",95,24,UI_INK);lv_label_set_text_fmt(o,"%02d:%02d",s_edit.hour,s_edit.minute);
        box(0,137,198,29,0xD8E9CF);o=text("",142,16,0x285E53);lv_label_set_text_fmt(o,"正在设置：%s",fields[s_edit_field]);
        text("请对照手机的北京时间",176,12,0x53776C);
        text("确认时秒数从零开始",195,12,0x53776C);
        footer(s_edit_field==5?"上键重调  确定保存":"上下调整  确定下一项",s_clock_valid?"长按确定取消校时":"未校时不会显示倒计时");
    } else if(s_page==CHECKLIST) {
        text("投稿准备清单",0,24,UI_INK);
        text(e?e->name:"请先选择会议",32,16,0x53776C);
        for(unsigned i=0;i<4;i++) {
            int y=64+(int)i*30;
            if(i==s_check)box(0,y-3,198,27,0xD8E9CF);
            o=text("",y,16,UI_INK);
            lv_label_set_text_fmt(o,"%s  %s",e && (s_progress.flags[s_order[s_cursor]]&(2<<i))?"已做":"待做",CHECKS[i]);
        }
        s_status=text("",195,12,0x53776C);
        footer("上下选择  确定勾选","长按确定返回 · 清单不会替你投稿");
    } else if(s_page==FOCUS) {
        text(s_focus_done?"又推进了一点":"专注一刻",0,24,UI_INK);
        text("只做眼前的一小步",34,16,0x53776C);
        int64_t left=s_focus_running?(s_focus_end-now_ms()+999)/1000:s_focus_left;
        if(left<0)left=0;
        s_big=text("",75,24,0x285E53);lv_label_set_text_fmt(s_big,"%02u:%02u",(unsigned)(left/60),(unsigned)(left%60));
        text(s_focus_done?"收下一片论文叶":s_focus_running?"计时中，离开此页也继续":"十五分钟，随时暂停",113,12,0x53776C);
        sprout(s_focus_done?4:2);
        o=text("",195,12,0x53776C);lv_label_set_text_fmt(o,"累计专注 %u 次",s_progress.sessions);
        footer(s_focus_done?"确定再来一刻":s_focus_running?"确定暂停":"确定开始或继续","长按确定返回首页");
    } else if(s_page==ABOUT) {
        text("资料说明",0,24,UI_INK);
        text("核对日期：2026-09-05",39,16,UI_INK);
        text("所有截止均已换算北京时间", 70,12,0x53776C);
        text("摘要、正文、承诺分开显示",94,12,0x53776C);
        text("远期年份只是关注位置",118,12,0x53776C);
        text("离线版本，不会自动更新",142,12,0x9B593D);
        text("投稿前请核对会议官网",166,16,0x9B593D);
        text("超过三十天会提示复核资料",195,12,0x53776C);
        footer("确定返回首页","不联网 · 不需要账号");
    }
}
static void begin_clock(void)
{
    s_edit=s_clock_valid?ds_beijing(utc_now()):s_progress.seed;
    if(!ds_date_valid(s_edit)) s_edit=(ds_date_t){2026,9,5,12,0};
    s_edit_field=0;s_page=CLOCK;
}
static void dispatch(input_t in)
{
    if(in.event==BSP_BTN_LONG) {
        if(s_page==HOME){s_menu=0;s_page=MENU;}
        else if(s_page==CLOCK && !s_clock_valid) return;
        else {
            unsigned selected=s_count?s_order[s_cursor]:DS_MAX_ENTRIES;
            refresh_list();
            for(unsigned i=0;i<s_count;i++)if(s_order[i]==selected)s_cursor=i;
            s_page=HOME;
        }
        return;
    }
    bool ok=in.button==BSP_BTN_OK;int delta=in.button==BSP_BTN_UP?-1:1;
    const ds_entry_t *e=entry();
    switch(s_page) {
    case HOME:
        if(ok && !s_clock_valid)begin_clock();
        else if(ok && e){s_page=DETAIL;int n=ds_next_node(e,utc_now());s_node=n<0?0:(unsigned)n;}
        else if(!ok && s_count)s_cursor=(s_cursor+s_count+delta)%s_count;
        break;
    case DETAIL:
        if(ok && e){s_progress.flags[s_order[s_cursor]]^=1;save();}
        else if(e && e->count)s_node=(s_node+e->count+delta)%e->count;
        break;
    case MENU:
        if(!ok)s_menu=(s_menu+6+delta)%6;
        else if(s_menu==0){s_choice=s_field;s_page=FILTER;}
        else if(s_menu==1){s_choice=s_year-2027;s_page=YEAR;}
        else if(s_menu==2){s_check=0;s_page=CHECKLIST;}
        else if(s_menu==3)s_page=FOCUS;
        else if(s_menu==4)begin_clock();
        else s_page=ABOUT;
        break;
    case FILTER:case YEAR:
        if(!ok){unsigned count=s_page==FILTER?6:3;s_choice=(s_choice+count+delta)%count;}
        else {if(s_page==FILTER)s_field=s_choice;else s_year=2027+s_choice;refresh_list();s_page=HOME;}
        break;
    case CLOCK:
        if(ok && s_edit_field==5){s_anchor_utc=ds_date_utc(s_edit);s_anchor_ms=now_ms();s_clock_valid=true;s_progress.seed=s_edit;save();refresh_list();s_page=HOME;}
        else if(ok)s_edit_field++;
        else if(s_edit_field==5)s_edit_field=0;
        else ds_date_move(&s_edit,s_edit_field,delta);
        break;
    case CHECKLIST:
        if(!ok)s_check=(s_check+4+delta)%4;
        else if(e){s_progress.flags[s_order[s_cursor]]^=(2<<s_check);save();}
        break;
    case FOCUS:
        if(!ok)break;
        if(s_focus_running){s_focus_left=(s_focus_end-now_ms()+999)/1000;s_focus_running=false;}
        else {if(s_focus_done || s_focus_left<=0)s_focus_left=900;s_focus_done=false;s_focus_end=now_ms()+s_focus_left*1000;s_focus_running=true;}
        break;
    case ABOUT: if(ok)s_page=HOME;break;
    }
}
static void tick(lv_timer_t *timer)
{
    (void)timer;bool changed=false;int64_t now=now_ms();input_t in;
    if(s_focus_running && now>=s_focus_end) {
        s_focus_running=false;s_focus_done=true;s_focus_left=0;
        if(s_progress.sessions<UINT16_MAX)s_progress.sessions++;
        save();changed=true;bsp_display_backlight(100);s_dimmed=false;s_activity=now;
    }
    while(xQueueReceive(s_queue,&in,0)) {
        if(!s_buttons || now-in.at>500 || in.at<s_last_input+180)continue;
        s_activity=now;s_last_input=in.at;
        if(s_dimmed){bsp_display_backlight(100);s_dimmed=false;continue;}
        dispatch(in);changed=true;
    }
    if(!s_dimmed && now-s_activity>120000){bsp_display_backlight(25);s_dimmed=true;}
    if(now/1000!=s_second) {
        s_second=now/1000;
        if(s_page==HOME && entry() && s_clock_valid) {
            /* Rebuild only across deadline boundaries; otherwise update labels. */
            const ds_entry_t *e=entry();
            int prev=ds_next_node(e,utc_now()-1),next=ds_next_node(e,utc_now());
            if(prev!=next)changed=true;
            else countdown();
        }
        if(s_page==FOCUS)changed=true;
    }
    if(changed)render();
    if(s_status)lv_label_set_text(s_status,ds_storage_status()==0?"清单已保存":ds_storage_status()==1?"正在保存":"保存失败，仅本次有效");
    if(s_page==HOME && s_clock_valid && utc_now()-ds_checked_utc>30*86400)
        lv_label_set_text(s_hint,"资料已久，请核对官网 · 长按菜单");
}
void deadline_station_prepare(void)
{
    if(s_prepared)return;
    s_queue=xQueueCreateStatic(4,sizeof(input_t),s_queue_bytes,&s_queue_control);
    s_progress=ds_storage_init();s_prepared=true;
}
void deadline_station_enter(bool buttons_available)
{
    /* prepare is called before main takes the LVGL lock. */
    if(!s_prepared)return;
    xQueueReset(s_queue);s_buttons=buttons_available;s_dimmed=false;s_activity=now_ms();s_last_input=-1000;
    s_field=0;s_year=2027;s_focus_left=900;s_focus_running=s_focus_done=false;
    refresh_list();s_page=HOME;
    s_screen=ui_pixel_screen_create("");
    label(s_screen,"截稿小站",12,15,136,16,0xFFFFFF);
    s_battery=label(s_screen,"--%",163,30,70,12,UI_INK);
    s_content=ui_pixel_panel_create(s_screen,10,53,220,234,UI_PAPER);
    lv_obj_t *footer_bg=lv_obj_create(s_screen);
    lv_obj_remove_flag(footer_bg,LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(footer_bg,0,288);lv_obj_set_size(footer_bg,240,32);
    lv_obj_set_style_border_width(footer_bg,0,0);lv_obj_set_style_radius(footer_bg,0,0);
    lv_obj_set_style_bg_color(footer_bg,lv_color_hex(0xDAEBC6),0);
    s_footer=label(s_screen,"",4,289,232,12,UI_INK);
    s_hint=label(s_screen,"",4,304,232,12,UI_INK);
    if(!s_clock_valid)begin_clock();
    render();battery(NULL);s_second=now_ms()/1000;
    s_timer=lv_timer_create(tick,40,NULL);s_battery_timer=lv_timer_create(battery,10000,NULL);
    lv_screen_load(s_screen);atomic_store(&s_accept,true);
}
void deadline_station_exit(void)
{
    atomic_store(&s_accept,false);s_focus_running=false;
    if(s_timer)lv_timer_delete(s_timer);
    if(s_battery_timer)lv_timer_delete(s_battery_timer);
    s_timer=s_battery_timer=NULL;
    if(s_screen)lv_obj_delete(s_screen);
    s_screen=s_content=s_footer=s_hint=s_battery=s_big=s_small=s_clock_label=s_status=NULL;
}
void deadline_station_key(bsp_btn_t button,bsp_btn_ev_t event)
{
    if(!atomic_load(&s_accept) || !s_queue || button<BSP_BTN_UP || button>BSP_BTN_OK)return;
    /* CLICK avoids the initial press of a long OK also opening a detail page. */
    if(event!=BSP_BTN_CLICK && !(event==BSP_BTN_LONG && button==BSP_BTN_OK))return;
    input_t input={button,event,now_ms()};(void)xQueueSend(s_queue,&input,0);
}
