#include "pocket_breach.h"
#include "pb_game.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
LV_FONT_DECLARE(pocket_breach_zh_16);
static pb_game game;
static lv_obj_t *screen,*placeholder,*image,*panel,*overlay_root,*stats,*footer,*battery,*threat;
static lv_timer_t *timer;
static uint16_t pixels[PB_W*PB_H];
static lv_image_dsc_t image_desc={.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=PB_W,.h=PB_H,.stride=PB_W*2},.data_size=sizeof(pixels),.data=(const uint8_t*)pixels};
typedef struct {bsp_btn_t key;bsp_btn_ev_t event;int64_t at;} input_t;
static StaticQueue_t queue_control;
static uint8_t queue_storage[8*sizeof(input_t)];
static QueueHandle_t queue;
static atomic_bool accepting;
static bool buttons,dimmed,off;
static int64_t last_frame,last_activity,held_at;
static int held_key=-1;
static uint32_t sequence=37;
static const char *maps[]={"沙城仓库","海港货站","霓虹基地"};
static int64_t now_ms(void){return esp_timer_get_time()/1000;}
static lv_obj_t *label(lv_obj_t *parent,const char *str,int x,int y,int width,uint32_t color)
{
    lv_obj_t *o=ui_pixel_label(parent,str,&pocket_breach_zh_16,color);
    lv_obj_set_pos(o,x,y);lv_obj_set_width(o,width);lv_obj_set_style_text_align(o,LV_TEXT_ALIGN_CENTER,0);return o;
}
static void set_text(lv_obj_t *o,const char *str){if(strcmp(lv_label_get_text(o),str))lv_label_set_text(o,str);}
static void line(const char *str,int y,uint32_t color){label(panel,str,0,y,198,color);}
static void overlay(void)
{
    if(overlay_root){lv_obj_delete(overlay_root);overlay_root=NULL;panel=NULL;}
    if(game.page==PB_FIGHT||game.page==PB_TRAVEL)return;
    overlay_root=lv_obj_create(screen);
    lv_obj_remove_style_all(overlay_root);lv_obj_set_size(overlay_root,240,254);
    lv_obj_remove_flag(overlay_root,LV_OBJ_FLAG_SCROLLABLE);
    panel=ui_pixel_panel_create(overlay_root,10,66,220,182,UI_PAPER);
    if(game.page==PB_HOME){
        line("三键出击，十二波突围",2,UI_INK);
        line("上键左转  下键右转",35,UI_INK);
        line("靠近自动锁定  确定开火",59,UI_INK);
        line("箱后会探头  金框已锁定",83,UI_INK);
        line("敌人在屏外才显示箭头",107,UI_INK);
        line(pb_runtime_audio_ok()?"戴好装备，准备出发":"音效不可用，仍可游玩",140,0x267969);
    }else if(game.page==PB_PAUSE){
        line("休息一下，战场已暂停",2,UI_INK);
        line("确定继续战斗",43,0x267969);line("上键回到首页",72,UI_INK);
        line("下键同盘重新开始",101,UI_INK);line("同一地图，同一批敌人",140,UI_INK);
    }else{
        char str[64];line(game.won?"突围成功！":"再试一次，就差一点",2,0x267969);
        snprintf(str,sizeof(str),"得分 %u  最佳 %u",game.score,game.best[game.mode]);line(str,35,UI_INK);
        snprintf(str,sizeof(str),"击破 %u  连击 %u",game.kills,game.max_combo);line(str,62,UI_INK);
        snprintf(str,sizeof(str),"命中率 %u%%",game.shots?game.hits*100/game.shots:0);line(str,89,UI_INK);
        line("确定同盘再来",119,0x267969);line("上键换一盘  下键返回",143,UI_INK);
    }
}
static void render(void)
{
    char str[80];
    pb_render(&game,pixels);lv_obj_invalidate(image);
    int guide=pb_guidance(&game);
    if(guide>=0){
        float a=pb_bearing(&game,guide);
        snprintf(str,sizeof(str),"%s，向%s转",pb_direction(a)==3?"后方敌人":"视野外敌人",a<0?"左":"右");
        set_text(threat,str);lv_obj_remove_flag(threat,LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(threat,lv_color_hex(game.hurt_ms&&guide==game.last_attacker?0xFF8175:0xFFE76C),0);
    }else lv_obj_add_flag(threat,LV_OBJ_FLAG_HIDDEN);
    int soc=pb_runtime_battery();if(soc<0)set_text(battery,"--%");else{snprintf(str,sizeof(str),"%d%%",soc>100?100:soc);set_text(battery,str);}
    if(game.page==PB_HOME){
        snprintf(str,sizeof(str),"%s / %s",maps[game.map],game.mode?"高手挑战":"轻松练习");set_text(stats,str);
        set_text(footer,"上键换图  下键难度\n确定开始突围");
    }else if(game.page==PB_FIGHT||game.page==PB_TRAVEL){
        snprintf(str,sizeof(str),"甲%u 弹%u 敌%u 第%u波",game.health,game.ammo,pb_alive(&game),game.wave);set_text(stats,str);
        if(game.page==PB_TRAVEL)set_text(footer,"区域清空，自动前进\n下一处交战点");
        else if(game.settle)set_text(footer,"全部击倒，区域清空\n准备向前推进");
        else if(game.reload)set_text(footer,"自动换弹，按箭头找敌人\n上下转向  长按确定暂停");
        else if(game.locked>=0){snprintf(str,sizeof(str),"已锁定%d号  确定开火\n上下换目标  长按确定暂停",game.locked+1);set_text(footer,str);}
        else set_text(footer,"等待探头，靠近自动锁定\n上下转向  确定开火");
    }else{set_text(stats,maps[game.map]);set_text(footer,game.page==PB_PAUSE?"慢慢来，找到自己的节奏":"把机器递给朋友，比比成绩");}
    if(!buttons)set_text(footer,"按键不可用，请重启");
}
static void start(uint32_t seed){pb_start(&game,seed);held_key=-1;}
static bool handle(input_t in,int64_t now)
{
    if(in.event!=BSP_BTN_PRESS&&in.event!=BSP_BTN_LONG)return false;
    if(in.event==BSP_BTN_LONG){if(in.key==BSP_BTN_OK&&(game.page==PB_FIGHT||game.page==PB_TRAVEL)){pb_pause(&game);return true;}return false;}
    last_activity=now;
    if(dimmed||off){dimmed=off=false;bsp_display_backlight(100);held_key=-1;return false;}
    pb_page before=game.page;
    if(game.page==PB_HOME){
        if(in.key==BSP_BTN_UP)game.map=(game.map+1)%3;
        else if(in.key==BSP_BTN_DOWN)game.mode^=1;
        else start((uint32_t)now^(++sequence*2654435761U));
    }else if(game.page==PB_PAUSE){
        if(in.key==BSP_BTN_OK)pb_pause(&game);else if(in.key==BSP_BTN_UP)pb_home(&game);else start(game.seed);
    }else if(game.page==PB_RESULT){
        if(in.key==BSP_BTN_OK)start(game.seed);else if(in.key==BSP_BTN_UP)start((uint32_t)now^(++sequence*2654435761U));else pb_home(&game);
    }else if(in.key==BSP_BTN_OK)pb_fire(&game);
    else{pb_turn(&game,in.key==BSP_BTN_UP?-0.075f:0.075f);held_key=in.key;held_at=now;}
    return game.page!=before;
}
static void frame(lv_timer_t *t)
{
    (void)t;int64_t now=now_ms(),elapsed=now-last_frame;last_frame=now;
    unsigned ms=elapsed<0?0:elapsed>80?80:(unsigned)elapsed;bool changed=false;
    input_t in;
    for(unsigned i=0;i<8&&xQueueReceive(queue,&in,0)==pdTRUE;i++){
        if(now>=in.at&&now-in.at<=600&&!changed)changed|=handle(in,now);
    }
    if(game.page==PB_FIGHT&&held_key>=0&&now-held_at>=260){
        /* One calibrated read uses the BSP's existing ADC owner. Never require chords. */
        static const uint16_t ranges[BSP_BTN_COUNT][2]=BSP_BTN_MV_TABLE;
        int mv=bsp_button_read_mv();
        if(mv>=ranges[held_key][0]&&mv<ranges[held_key][1]){pb_turn(&game,ms*0.0016f*(held_key==BSP_BTN_UP?-1:1));last_activity=now;}
        else held_key=-1;
    }
    pb_page before=game.page;if(!changed)pb_tick(&game,ms);changed|=before!=game.page;
    bool active=game.page==PB_FIGHT||game.page==PB_TRAVEL;
    pb_runtime_active(active);
    if(game.sound){pb_runtime_sound(game.sound);game.sound=PB_SILENT;}
    if(!active&&!dimmed&&now-last_activity>=60000){dimmed=true;bsp_display_backlight(20);}
    if(!active&&!off&&now-last_activity>=180000){off=true;bsp_display_backlight(0);}
    if(changed)overlay();
    if(!off)render();
}
void pocket_breach_prepare(void){if(!queue)queue=xQueueCreateStatic(8,sizeof(input_t),queue_storage,&queue_control);}
void pocket_breach_key(bsp_btn_t b,bsp_btn_ev_t ev)
{
    if(!atomic_load(&accepting)||b<BSP_BTN_UP||b>BSP_BTN_OK)return;
    if(ev!=BSP_BTN_PRESS&&ev!=BSP_BTN_LONG)return;
    input_t in={b,ev,now_ms()};(void)xQueueSend(queue,&in,0);
}
void pocket_breach_enter(bool available)
{
    if(screen)return;
    pocket_breach_prepare();buttons=available;dimmed=off=false;held_key=-1;
    last_frame=last_activity=now_ms();pb_start(&game,37);pb_home(&game);
    screen=ui_pixel_screen_create("");label(screen,"口袋突围",6,15,145,0xFFFFFF);
    battery=label(screen,"--%",165,31,62,UI_INK);
    image=lv_image_create(screen);lv_image_set_src(image,&image_desc);lv_image_set_pivot(image,0,0);lv_image_set_scale(image,512);lv_image_set_antialias(image,false);lv_obj_set_pos(image,0,54);
    threat=label(screen,"",62,56,174,0xFFE76C);
    lv_obj_set_style_bg_color(threat,lv_color_hex(UI_INK),0);lv_obj_set_style_bg_opa(threat,LV_OPA_90,0);
    stats=label(screen,"",0,256,240,0xFFFFFF);footer=label(screen,"",2,280,236,UI_INK);
    lv_obj_set_style_bg_color(stats,lv_color_hex(UI_INK),0);lv_obj_set_style_bg_opa(stats,LV_OPA_COVER,0);
    lv_obj_set_style_bg_color(footer,lv_color_hex(UI_PAPER),0);lv_obj_set_style_bg_opa(footer,LV_OPA_COVER,0);
    overlay();render();lv_screen_load(screen);
    if(placeholder){lv_obj_delete(placeholder);placeholder=NULL;}
    timer=lv_timer_create(frame,50,NULL);xQueueReset(queue);atomic_store(&accepting,buttons);bsp_display_backlight(100);
}
void pocket_breach_exit(void)
{
    atomic_store(&accepting,false);pb_runtime_active(false);
    if(timer){lv_timer_delete(timer);timer=NULL;}
    if(screen){if(lv_screen_active()==screen){placeholder=lv_obj_create(NULL);lv_screen_load(placeholder);}lv_obj_delete(screen);}
    screen=panel=overlay_root=image=stats=footer=battery=threat=NULL;held_key=-1;xQueueReset(queue);
}
