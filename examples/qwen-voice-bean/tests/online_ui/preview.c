/* Production LVGL renderer with synthetic status, never a device capture. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/misc/lv_text_private.h"
#include "online_ui.c"
static online_status_t fixture;
static int battery_value=87;
static unsigned toggles,cancels,volumes,setups;
static int64_t clock_us=2200000;
int64_t esp_timer_get_time(void){return clock_us;}
int bsp_battery_soc(void){return battery_value;}
void online_snapshot(online_status_t *s){*s=fixture;}
void online_toggle_mic(void){toggles++;}
void online_cancel(void){cancels++;}
void online_volume(void){volumes++;}
void online_setup(void){setups++;}
static void check_labels(lv_obj_t *o){
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
    if(lv_obj_check_type(o,&lv_label_class)){
        const char *s=lv_label_get_text(o);lv_area_t a;lv_obj_get_coords(o,&a);
        assert(a.x1>=0 && a.y1>=0 && a.x2<240 && a.y2<320);
        lv_point_t dim;const lv_font_t *f=lv_obj_get_style_text_font(o,0);
        lv_text_get_size(&dim,s,f,0,lv_obj_get_style_text_line_space(o,0),lv_obj_get_width(o),LV_TEXT_FLAG_NONE);
        if(lv_label_get_long_mode(o)==LV_LABEL_LONG_DOT)dim.y=lv_obj_get_height(o);
        if(dim.y>lv_obj_get_height(o))fprintf(stderr,"Clipped: %s %d > %d\n",s,(int)dim.y,(int)lv_obj_get_height(o));
        assert(dim.y<=lv_obj_get_height(o));
        for(uint32_t i=0;s[i];){uint32_t cp=lv_text_encoded_next(s,&i);if(cp=='\n')continue;
            lv_font_glyph_dsc_t d;assert(lv_font_get_glyph_dsc(f,&d,cp,0) && !d.is_placeholder);
        }
        lv_obj_t *p=lv_obj_get_parent(o);lv_area_t parent;lv_obj_get_coords(p,&parent);
        assert(a.x1>=parent.x1 && a.y1>=parent.y1 && a.x2<=parent.x2 && a.y2<=parent.y2);
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++) {
        lv_obj_t *a=lv_obj_get_child(o,i);
        if(lv_obj_has_flag(a,LV_OBJ_FLAG_HIDDEN) || !lv_obj_check_type(a,&lv_label_class))continue;
        lv_area_t x;lv_obj_get_coords(a,&x);
        for(unsigned j=i+1;j<lv_obj_get_child_count(o);j++) {
            lv_obj_t *b=lv_obj_get_child(o,j);
            if(lv_obj_has_flag(b,LV_OBJ_FLAG_HIDDEN) || !lv_obj_check_type(b,&lv_label_class))continue;
            lv_area_t y;lv_obj_get_coords(b,&y);
            assert(x.x2<y.x1 || y.x2<x.x1 || x.y2<y.y1 || y.y2<x.y1);
        }
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++)check_labels(lv_obj_get_child(o,i));
}
static void check(void){lv_obj_update_layout(lv_screen_active());check_labels(lv_screen_active());}
static void snap(const char *name){
    check();lv_draw_buf_t *b=lv_snapshot_take(lv_screen_active(),LV_COLOR_FORMAT_RGB888);assert(b);
    char path[100];snprintf(path,sizeof(path),"%s.ppm",name);FILE *f=fopen(path,"wb");assert(f);
    fprintf(f,"P6\n%u %u\n255\n",b->header.w,b->header.h);
    for(unsigned y=0;y<b->header.h;y++)for(unsigned x=0;x<b->header.w;x++){
        uint8_t *p=b->data+y*b->header.stride+x*3;uint8_t rgb[]={p[2],p[1],p[0]};fwrite(rgb,1,3,f);
    }
    fclose(f);lv_draw_buf_destroy(b);
}
int main(void) {
    lv_init();assert(lv_display_create(240,320));fixture.volume=75;
    online_ui_enter(true);
    const char *messages[]={"Qwen-Bean-1234\n密码 12345678","正在连接后端","确定：开始对话","我在认真听","正在请教 Agent","Qwen Audio Agent","后端消息过大或分片无效"};
    const char *names[]={"setup","connecting","ready","listening","thinking","speaking","error"};
    for(unsigned i=0;i<7;i++){
        fixture.phase=i;fixture.mic=i>=3 && i<=5;fixture.level=19000;
        snprintf(fixture.message,sizeof(fixture.message),"%s",messages[i]);refresh(NULL);snap(names[i]);
    }
    battery_value=-1;update_battery(NULL);check();
    online_ui_key(BSP_BTN_OK,BSP_BTN_PRESS);assert(!toggles);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(toggles==1);
    online_ui_key(BSP_BTN_UP,BSP_BTN_CLICK);refresh(NULL);assert(volumes==1);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);assert(cancels==1);
    online_ui_key(BSP_BTN_OK,BSP_BTN_LONG);refresh(NULL);assert(setups==0);
    snap("settings-empty");
    fixture.configured=true;fixture.password_set=true;strcpy(fixture.ssid,"Home Wi-Fi");strcpy(fixture.host,"192.0.2.10");
    refresh(NULL);snap("settings");
    strcpy(fixture.ssid,"12345678901234567890123456789012");refresh(NULL);check();
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);snap("confirm");assert(setups==0);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(setups==0 && menu.page==ONLINE_SETTINGS);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_DOWN,BSP_BTN_CLICK);refresh(NULL);
    online_ui_key(BSP_BTN_OK,BSP_BTN_CLICK);refresh(NULL);assert(setups==1);
    fixture.phase=ONLINE_LISTENING;fixture.mic=true;refresh(NULL);
    assert(!lv_obj_has_flag(ears[0],LV_OBJ_FLAG_HIDDEN));
    fixture.phase=ONLINE_SPEAKING;fixture.level=1000;refresh(NULL);check();int y=lv_obj_get_height(mouth);
    clock_us+=5000000;fixture.level=19000;refresh(NULL);check();
    assert(lv_obj_get_height(mouth)>y && lv_obj_get_x(pupils[0])==22 && lv_obj_get_y(pupils[0])==25);
    assert(lv_obj_has_flag(ears[0],LV_OBJ_FLAG_HIDDEN));
    buttons_available=false;refresh(NULL);check();
    puts("Online UI: all phases, glyphs, clipping, bounds and controls PASS");
}
