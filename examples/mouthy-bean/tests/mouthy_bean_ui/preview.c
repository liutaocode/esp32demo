/* Runs the production renderer, synthetic peripherals only. Not a device capture. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/misc/lv_text_private.h"
#include "mouthy_bean.c"
static uint64_t fake_us;
static bool busy, heard, available=true, in_callback;
static unsigned audio_level=2300;
static uint32_t prefs;
int64_t esp_timer_get_time(void){return (int64_t)fake_us;}
uint32_t esp_random(void){return 732;}
int bsp_battery_soc(void){assert(!in_callback);return 87;}
void bsp_display_backlight(uint8_t b){(void)b;assert(!in_callback);}
void bean_runtime_start(void){}
void bean_runtime_play(int l,unsigned v){(void)l;busy=v>0;}
void bean_runtime_stop(void){busy=false;heard=false;}
void bean_runtime_listen(bool e){if(!e)heard=false;}
bool bean_runtime_busy(void){return busy;}
bool bean_runtime_sound(void){return heard;}
bool bean_runtime_ready(void){return available;}
unsigned bean_runtime_level(void){return audio_level;}
uint32_t bean_runtime_saved(void){return prefs;}
void bean_runtime_save(uint32_t p){prefs=p;}
bool bean_runtime_storage_ok(void){return true;}
static void check_labels(lv_obj_t *o){
    if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
    if(lv_obj_check_type(o,&lv_label_class)){
        const char *s=lv_label_get_text(o);lv_area_t a;lv_obj_get_coords(o,&a);
        assert(a.x1>=0 && a.y1>=0 && a.x2<240 && a.y2<320);
        lv_point_t dim;const lv_font_t *f=lv_obj_get_style_text_font(o,0);
        lv_text_get_size(&dim,s,f,0,lv_obj_get_style_text_line_space(o,0),lv_obj_get_width(o),LV_TEXT_FLAG_NONE);
        if(dim.y>lv_obj_get_height(o))fprintf(stderr,"Clipped: %s %d > %d\n",s,(int)dim.y,(int)lv_obj_get_height(o));
        assert(dim.y<=lv_obj_get_height(o));
        for(uint32_t i=0;s[i];){uint32_t cp=lv_text_encoded_next(s,&i);if(cp=='\n')continue;
            assert(!(cp>='a' && cp<='z') && !(cp>='A' && cp<='Z'));
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
static void check(void){lv_obj_update_layout(screen);check_labels(screen);}
static void snap(const char *name){
    check();lv_draw_buf_t *b=lv_snapshot_take(screen,LV_COLOR_FORMAT_RGB888);assert(b);
    char path[100];snprintf(path,sizeof(path),"%s.ppm",name);FILE *f=fopen(path,"wb");assert(f);
    fprintf(f,"P6\n%u %u\n255\n",b->header.w,b->header.h);
    for(unsigned y=0;y<b->header.h;y++)for(unsigned x=0;x<b->header.w;x++){
        uint8_t *p=b->data+y*b->header.stride+x*3;uint8_t rgb[]={p[2],p[1],p[0]};fwrite(rgb,1,3,f);
    }
    fclose(f);lv_draw_buf_destroy(b);
}
static void advance(unsigned ms){fake_us+=(uint64_t)ms*1000;lv_tick_inc(ms);tick(timer);}
static void key(bsp_btn_t k,bsp_btn_ev_t e){advance(200);in_callback=true;mouthy_bean_key(k,e);in_callback=false;advance(40);}
int main(void){
    lv_init();assert(lv_display_create(240,320));mouthy_bean_enter(true);advance(200);snap("idle");
    key(BSP_BTN_UP,BSP_BTN_PRESS);assert(state.phase==BEAN_IDLE);
    key(BSP_BTN_UP,BSP_BTN_CLICK);assert(state.phase==BEAN_THINK);snap("thinking");
    advance(state.think_ms);assert(state.phase==BEAN_TALK && busy);snap("talking");
    key(BSP_BTN_OK,BSP_BTN_LONG);assert(page==1 && !busy);snap("settings");
    key(BSP_BTN_DOWN,BSP_BTN_CLICK);key(BSP_BTN_DOWN,BSP_BTN_CLICK);key(BSP_BTN_OK,BSP_BTN_CLICK);assert(page==2);snap("album");
    for(unsigned i=0;i<8;i++){key(BSP_BTN_DOWN,BSP_BTN_CLICK);check();}
    key(BSP_BTN_OK,BSP_BTN_LONG);assert(page==0);advance(500);
    heard=true;advance(40);assert(state.phase==BEAN_LISTEN);
    for(unsigned i=0;i<8;i++){advance(40);assert(!lv_obj_has_flag(ears[0],LV_OBJ_FLAG_HIDDEN));check();}
    assert(lv_obj_get_y(ears[0])<=46);snap("listening");
    heard=false;advance(1100);assert(state.phase==BEAN_THINK);advance(state.think_ms);busy=false;advance(4000);
    for(unsigned face=0;face<8;face++)for(unsigned line=0;line<BEAN_LINES;line++){
        state.face=face;state.phase=BEAN_TALK;state.line=line;draw_face();check();
        if(line==face)snap(bean_faces[face]);
    }
    for(unsigned p=0;p<5;p++){state.phase=p;draw_face();check();}
    bean_init(&state,1,now_ms());advance(120000);assert(state.phase==BEAN_REST);snap("rest");
    key(BSP_BTN_OK,BSP_BTN_CLICK);assert(state.phase==BEAN_THINK);
    for(unsigned i=0;i<4;i++){state.volume=i;page=1;render_menu();check();}
    state.discovered=255;page=2;render_menu();snap("full-album");
    page=0;render_menu();available=false;draw_face();snap("audio-unavailable");
    for(unsigned i=0;i<20;i++){mouthy_bean_exit();assert(!screen && !timer && !battery_timer);mouthy_bean_key(BSP_BTN_OK,BSP_BTN_CLICK);mouthy_bean_enter(true);check();}
    mouthy_bean_exit();prefs=0xB1000501u;mouthy_bean_enter(true);assert(state.volume==2);advance(40);
    assert((prefs&0xFF000000u)==0xB2000000u);
    mouthy_bean_exit();prefs=0xB1000401u;mouthy_bean_enter(true);assert(state.volume==0);
    mouthy_bean_exit();mouthy_bean_enter(false);snap("buttons-unavailable");mouthy_bean_exit();
    puts("Mouthy Bean UI: all 40 lines / 8 faces, Chinese glyphs, bounds, buttons, listening, settings and lifecycle PASS");
}
