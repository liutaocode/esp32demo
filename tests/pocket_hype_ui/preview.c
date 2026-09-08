/* Render and exercise the production LVGL page, without touching USB hardware. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/pocket_hype/pocket_hype.c"
#include "src/misc/lv_text_private.h"
static int64_t fake_us;
static int soc = 87, backlight;
static bool callback_active, audio_ready = true;
static unsigned plays, stops, last_clip, last_volume;
static bool last_voice;
int64_t esp_timer_get_time(void) { return fake_us; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t n) { assert(!callback_active); backlight = n; }
bool ph_audio_ready(void) { return audio_ready; }
bool ph_audio_busy(void) { return false; }
void ph_audio_stop(void) { assert(!callback_active); stops++; }
void ph_audio_play(unsigned clip, unsigned volume, bool voice) {
    assert(!callback_active && clip < 18 && volume < 4); plays++; last_clip = clip; last_volume = volume; last_voice = voice;
}
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
static void check(void) { lv_obj_update_layout(s_screen); bounds(s_screen); }
static void snap(const char *name) {
    check(); lv_draw_buf_t *b = lv_snapshot_take(s_screen, LV_COLOR_FORMAT_RGB888); assert(b);
    char path[100]; snprintf(path,sizeof(path),"%s.ppm",name);
    FILE *f = fopen(path,"wb"); assert(f); fprintf(f,"P6\n%u %u\n255\n",b->header.w,b->header.h);
    for(unsigned y=0;y<b->header.h;y++) for(unsigned x=0;x<b->header.w;x++) {
        uint8_t *p=b->data+y*b->header.stride+x*3; uint8_t rgb[]={p[2],p[1],p[0]}; fwrite(rgb,1,3,f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}
static void advance(unsigned ms) { fake_us += (int64_t)ms*1000; lv_tick_inc(ms); if (s_timer) frame(s_timer); }
static void post(bsp_btn_t b, bsp_btn_ev_t e) { callback_active=true; pocket_hype_key(b,e); callback_active=false; }
static void key(bsp_btn_t b) { post(b,BSP_BTN_PRESS); advance(40); if(b==BSP_BTN_OK) { post(b,BSP_BTN_CLICK); advance(40); } }
static void hold(void) { post(BSP_BTN_OK,BSP_BTN_PRESS); advance(500); post(BSP_BTN_OK,BSP_BTN_LONG); advance(40); }
int main(void) {
    lv_init(); assert(lv_display_create(240,320)); pocket_hype_prepare(); pocket_hype_enter(true); snap("home");
    key(BSP_BTN_OK); assert(plays==1 && last_clip==0 && last_voice && last_volume==2); snap("applause");
    post(BSP_BTN_OK,BSP_BTN_PRESS); post(BSP_BTN_OK,BSP_BTN_PRESS); post(BSP_BTN_OK,BSP_BTN_DOUBLE); advance(40);
    assert(plays==2 && last_clip==2); snap("full-house");
    for(unsigned s=0;s<7;s++) {
        s_state.selected=s; s_state.active=false; render(); check();
        for(unsigned tier=0;tier<3;tier++) { key(BSP_BTN_OK); advance(120); check(); }
    }
    s_state.selected=6; s_state.active=false; render(); snap("surprise");
    unsigned before=plays; hold(); assert(s_state.page==PH_MENU && plays==before); snap("settings");
    key(BSP_BTN_OK); assert(s_state.volume==3); key(BSP_BTN_OK); assert(s_state.volume==0);
    key(BSP_BTN_DOWN); key(BSP_BTN_OK); assert(!s_state.voice);
    key(BSP_BTN_DOWN); key(BSP_BTN_DOWN); key(BSP_BTN_OK); assert(s_state.page==PH_HELP); snap("help");
    key(BSP_BTN_OK); assert(s_state.page==PH_MENU); key(BSP_BTN_UP); key(BSP_BTN_OK); assert(s_state.page==PH_CHALLENGE); snap("challenge");
    for(unsigned round=0;round<6;round++) {
        s_state.selected=(s_state.order[round]+(round==0))%6;
        key(BSP_BTN_OK); assert(s_state.page==PH_FEEDBACK); snap(round==0?"try-this":"good-catch");
        assert(last_volume==0 && !last_voice); key(BSP_BTN_OK);
    }
    assert(s_state.page==PH_RESULT && s_state.score==5); snap("result");
    for(unsigned scene=0;scene<6;scene++) for(unsigned cue=0;cue<3;cue++) {
        s_state.page=PH_CHALLENGE; s_state.round=0; s_state.order[0]=scene; s_state.cues[0]=cue; render(); check();
    }
    hold(); assert(s_state.page==PH_LIVE); advance(180000); assert(backlight==0);
    before=plays; key(BSP_BTN_OK); assert(plays==before && backlight==85); key(BSP_BTN_OK); assert(plays==before+1);
    advance(180000); before=plays; hold(); assert(plays==before && s_state.page==PH_LIVE); key(BSP_BTN_OK); assert(plays==before+1);
    advance(180000); before=plays;
    post(BSP_BTN_OK,BSP_BTN_PRESS); advance(40); post(BSP_BTN_OK,BSP_BTN_PRESS); advance(40);
    post(BSP_BTN_OK,BSP_BTN_DOUBLE); advance(40); assert(plays==before);
    key(BSP_BTN_OK); assert(plays==before+1);
    audio_ready=false; advance(40); snap("audio-unavailable"); soc=-1; battery(NULL); snap("battery-unknown");
    pocket_hype_exit(); before=plays; key(BSP_BTN_OK); assert(plays==before);
    pocket_hype_enter(false); snap("buttons-unavailable"); pocket_hype_exit();
    for(int i=0;i<10;i++) { pocket_hype_enter(true); key(BSP_BTN_OK); pocket_hype_exit(); }
    puts("Pocket Hype production UI: all scenes, tiers, cues, pages, Chinese glyphs, clipping, key gestures, wake suppression and lifecycle PASS");
}
