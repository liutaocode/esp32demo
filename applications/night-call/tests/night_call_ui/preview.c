/* Host renders use the exact production LVGL object tree and subset fonts. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/night_call/night_call.c"
#include "src/misc/lv_text_private.h"
static int64_t fake_us;
static int soc=87, backlight, storage_status;
static bool callback_active, audio_ready=true;
static unsigned plays, last_clip, saves;
int64_t esp_timer_get_time(void) { return fake_us; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t n) { assert(!callback_active); backlight=n; }
bool nc_audio_ready(void) { return audio_ready; }
bool nc_audio_busy(void) { return false; }
void nc_audio_stop(void) { assert(!callback_active); }
void nc_audio_play(unsigned clip,unsigned volume) { assert(!callback_active && clip<29 && volume<4); plays++; last_clip=clip; }
void nc_storage_save(const nc_state_t *s) { assert(!callback_active && nc_state_valid(s)); saves++; }
int nc_storage_status(void) { return storage_status; }
static void bounds(lv_obj_t *o) {
    lv_area_t a; lv_obj_get_coords(o, &a);
    /* The shared theme intentionally has decorative overhang on its title plate. */
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *str = lv_label_get_text(o); if (!str[0]) return;
        const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        lv_point_t size;
        lv_text_get_size(&size, str, font, 0, 1, lv_obj_get_width(o), LV_TEXT_FLAG_NONE);
        if (a.x1 < 0 || a.y1 < 0 || a.x2 >= 240 || a.y2 >= 320) fprintf(stderr, "Screen bounds: %s [%d %d %d %d]\n", str,a.x1,a.y1,a.x2,a.y2);
        assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
        assert(size.y <= lv_obj_get_height(o) + 1);
        uint32_t i = 0;
        while (str[i]) {
            uint32_t cp = lv_text_encoded_next(str, &i); if (cp == '\n') continue;
            lv_font_glyph_dsc_t d;
            if (!lv_font_get_glyph_dsc(font, &d, cp, 0) || d.is_placeholder) fprintf(stderr,"Missing glyph U+%04X in %s\n",cp,str);
            assert(lv_font_get_glyph_dsc(font, &d, cp, 0) && !d.is_placeholder);
        }
        assert(lv_obj_get_scroll_bottom(o) <= 0);
        lv_obj_t *parent = lv_obj_get_parent(o);
        if (parent != s_screen) {
            lv_area_t area; lv_obj_get_content_coords(parent, &area);
            if (a.x1 < area.x1 || a.x2 > area.x2 || a.y1 < area.y1 || a.y2 > area.y2) fprintf(stderr, "Parent clips: %s [%d %d %d %d] in [%d %d %d %d]\n",str,a.x1,a.y1,a.x2,a.y2,area.x1,area.y1,area.x2,area.y2);
            assert(a.x1 >= area.x1 && a.x2 <= area.x2 && a.y1 >= area.y1 && a.y2 <= area.y2);
        }
        for (const char *c = str; *c; c++) assert(!(*c >= 'A' && *c <= 'Z') && !(*c >= 'a' && *c <= 'z'));
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) bounds(lv_obj_get_child(o,i));
}
static lv_area_t occupied[64];
static unsigned occupied_count;
static void no_overlap(lv_obj_t *o) {
    if(lv_obj_check_type(o,&lv_label_class) && lv_label_get_text(o)[0]) {
        lv_area_t a;lv_obj_get_coords(o,&a);lv_point_t size;
        lv_text_get_size(&size,lv_label_get_text(o),lv_obj_get_style_text_font(o,0),0,1,lv_obj_get_width(o),LV_TEXT_FLAG_NONE);
        a.y2=a.y1+size.y-1;
        for(unsigned i=0;i<occupied_count;i++) {
            lv_area_t b=occupied[i];
            bool overlap=a.x1<=b.x2 && b.x1<=a.x2 && a.y1<=b.y2 && b.y1<=a.y2;
            if(overlap) fprintf(stderr,"Overlapping label: %s\n",lv_label_get_text(o));
            assert(!overlap);
        }
        assert(occupied_count<64);occupied[occupied_count++]=a;
    }
    for(unsigned i=0;i<lv_obj_get_child_count(o);i++) no_overlap(lv_obj_get_child(o,i));
}
static void check(void) { lv_obj_update_layout(s_screen); bounds(s_screen);occupied_count=0;no_overlap(s_screen); }
static void snap(const char *name) {
    check(); lv_draw_buf_t *b = lv_snapshot_take(s_screen, LV_COLOR_FORMAT_RGB888); assert(b);
    char path[100]; snprintf(path,sizeof(path),"%s.ppm",name);
    FILE *f = fopen(path,"wb"); assert(f); fprintf(f,"P6\n%u %u\n255\n",b->header.w,b->header.h);
    for(unsigned y=0;y<b->header.h;y++) for(unsigned x=0;x<b->header.w;x++) {
        uint8_t *p=b->data+y*b->header.stride+x*3; uint8_t rgb[]={p[2],p[1],p[0]}; fwrite(rgb,1,3,f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}
static void advance(unsigned ms) { fake_us+=(int64_t)ms*1000; lv_tick_inc(ms); if(s_timer) frame(s_timer); }
static void post(bsp_btn_t b,bsp_btn_ev_t e) { callback_active=true; night_call_key(b,e); callback_active=false; advance(40); }
static void key(bsp_btn_t b) { post(b,BSP_BTN_PRESS); post(b,BSP_BTN_CLICK); }
#include "capture.inc"
int main(void) {
    lv_init(); assert(lv_display_create(240,320)); nc_state_t fresh; nc_init(&fresh);
    night_call_prepare(); night_call_enter(&fresh,true); snap("01-home");
    key(BSP_BTN_OK); assert(s_page==CHAPTERS); snap("02-chapters");
    key(BSP_BTN_OK); assert(s_page==STORY && s_state.active && last_clip==0); snap("03-incoming");
    unsigned before=plays;
    post(BSP_BTN_UP,BSP_BTN_PRESS); post(BSP_BTN_UP,BSP_BTN_LONG); assert(plays==before+1 && s_selected==0);
    post(BSP_BTN_DOWN,BSP_BTN_LONG); assert(s_state.volume==0);
    post(BSP_BTN_OK,BSP_BTN_LONG); assert(s_page==PAUSE); snap("04-pause");
    key(BSP_BTN_DOWN); key(BSP_BTN_OK); assert(s_page==HOME && s_state.active && s_selected==0);
    key(BSP_BTN_OK); assert(s_page==STORY && s_state.node==0);
    for(unsigned n=0;n<nc_node_count;n++) {
        s_state.node=n; s_state.chapter=nc_nodes[n].chapter; s_state.active=true; s_page=STORY;
        for(unsigned selected=0;selected<2;selected++) { s_selected=selected; render(); check(); }
        char path[80]; snprintf(path,sizeof(path),"scene-%02u",n); snap(path);
    }
    s_page=ENDING; s_selected=0;
    for(int e=0;e<NC_ENDINGS;e++) { s_ending=e; render(); check(); char path[80]; snprintf(path,sizeof(path),"ending-%02d",e); snap(path); }
    for(unsigned all=0;all<2;all++) {
        s_state.clues=all?63:0; s_state.endings=all?127:0; s_page=ARCHIVE;
        for(unsigned i=0;i<NC_CLUES+NC_ENDINGS;i++) { s_archive=i; render(); check(); }
    }
    snap("05-archive");
    s_page=HELP; render(); snap("06-help");
    s_page=SETTINGS; s_selected=0;
    for(unsigned v=0;v<4;v++) { s_state.volume=v; render(); check(); } snap("07-settings");
    for(int page=HOME;page<=PAUSE;page++) { s_page=(page_t)page; s_selected=0; render(); check(); }
    test_capture();
    home(); advance(180001); assert(backlight==0 && s_asleep); before=plays;
    key(BSP_BTN_OK); assert(backlight==80 && !s_asleep && plays==before);
    audio_ready=false; advance(40); snap("08-audio-unavailable");
    storage_status=1; advance(40); snap("09-saving"); storage_status=-1; advance(40); snap("10-save-warning");
    soc=-1; battery(NULL); snap("11-battery-unknown");
    night_call_exit(); before=plays; key(BSP_BTN_OK); assert(plays==before);
    storage_status=0; audio_ready=true; night_call_enter(&fresh,false); snap("12-buttons-unavailable"); night_call_exit();
    for(int i=0;i<10;i++) { night_call_enter(&fresh,true); key(BSP_BTN_OK); night_call_exit(); }
    printf("Night Call production UI: all scenes, 7 endings, archive, gestures, Chinese glyphs, text bounds and lifecycle PASS; saves=%u\n",saves);
}
