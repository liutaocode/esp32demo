/* Production LVGL renderer: synthetic peripherals, never a USB capture. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/misc/lv_text_private.h"
#include "apps/code_theater/code_theater.c"
static int64_t fake_us;
static int soc = 87;
static bool callback_active;
int64_t esp_timer_get_time(void) { return fake_us; }
static uint32_t entropy=98765;
uint32_t esp_random(void) { return entropy; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t b) { (void)b; assert(!callback_active); }
static void advance(unsigned ms) { fake_us += (int64_t)ms * 1000; lv_tick_inc(ms); tick(s_timer); }
static void raw(bsp_btn_t b,bsp_btn_ev_t e)
{
    callback_active=true; code_theater_key(b,e); callback_active=false; advance(60);
}
static void key_event(bsp_btn_t b,bsp_btn_ev_t e)
{
    advance(800); raw(b,BSP_BTN_PRESS); raw(b,e);
}
static void key(bsp_btn_t b) { key_event(b,BSP_BTN_CLICK); }
static void check_labels(lv_obj_t *o)
{
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *text = lv_label_get_text(o);
        assert(!strstr(text,"离线") && !strstr(text,"模拟") && !strstr(text,"小项目"));
        lv_area_t a; lv_obj_get_coords(o, &a);
        assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
        lv_point_t size;
        const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(o)) fprintf(stderr, "Overflow: %s width=%d allowed=%d\n", text, (int)size.x, (int)lv_obj_get_width(o));
        assert(size.x <= lv_obj_get_width(o));
        assert(size.y <= lv_obj_get_height(o));
        for (uint32_t i = 0; text[i];) {
            uint32_t cp = lv_text_encoded_next(text, &i);
            assert(o == s_title || (!(cp >= 'A' && cp <= 'Z') && !(cp >= 'a' && cp <= 'z')));
            lv_font_glyph_dsc_t dsc;
            assert(lv_font_get_glyph_dsc(font, &dsc, cp, 0) && !dsc.is_placeholder);
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *a = lv_obj_get_child(o, i);
        if (!lv_obj_check_type(a, &lv_label_class) || !lv_label_get_text(a)[0]) continue;
        lv_area_t x; lv_obj_get_coords(a, &x);
        for (unsigned j = i + 1; j < lv_obj_get_child_count(o); j++) {
            lv_obj_t *b = lv_obj_get_child(o, j);
            if (!lv_obj_check_type(b, &lv_label_class) || !lv_label_get_text(b)[0]) continue;
            lv_area_t y; lv_obj_get_coords(b, &y);
            if (!(x.x2 < y.x1 || y.x2 < x.x1 || x.y2 < y.y1 || y.y2 < x.y1)) fprintf(stderr, "Overlap: %s and %s\n", lv_label_get_text(a), lv_label_get_text(b));
            assert(x.x2 < y.x1 || y.x2 < x.x1 || x.y2 < y.y1 || y.y2 < x.y1);
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) check_labels(lv_obj_get_child(o, i));
}
static void check(void) { lv_obj_update_layout(s_screen); check_labels(s_screen); }
static void snap(const char *name)
{
    check();
    lv_draw_buf_t *b = lv_snapshot_take(s_screen, LV_COLOR_FORMAT_RGB888); assert(b);
    char path[120]; snprintf(path, sizeof(path), "%s.ppm", name);
    FILE *f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n%u %u\n255\n", b->header.w, b->header.h);
    for (unsigned y = 0; y < b->header.h; y++) for (unsigned x = 0; x < b->header.w; x++) {
        uint8_t *p = b->data + y * b->header.stride + x * 3;
        uint8_t rgb[] = {p[2], p[1], p[0]}; fwrite(rgb, 1, 3, f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}
static unsigned interaction_frame;
static void record_interaction(unsigned frames)
{
    for (unsigned i=0;i<frames;i++) {
        advance(120); char path[40]; snprintf(path,sizeof path,"interaction-%03u",interaction_frame++);
        snap(path);
    }
}
static void rapid_ban(void)
{
    advance(800);
    for (unsigned i=0;i<7;i++) {
        raw(BSP_BTN_OK,BSP_BTN_PRESS);
        assert(s_state.access==(i<6 ? CT_ACCESS_OK : CT_BANNED));
        raw(BSP_BTN_OK,BSP_BTN_CLICK); advance(120);
    }
    assert(!s_appeal_armed && s_state.access==CT_BANNED);
}
int main(void)
{
    lv_init(); assert(lv_display_create(240,320));
    code_theater_prepare(); code_theater_enter(true);
    assert(!strcmp(lv_label_get_text(s_title),"Claude Code"));
    for (unsigned f=0;f<380;f++) {
        advance(120); char name[40]; snprintf(name,sizeof name,"motion-%03u",f); snap(name);
    }
    assert(s_state.access==CT_ACCESS_OK);
    code_theater_exit(); code_theater_enter(true);
    for (int i=0;i<20;i++) advance(60);
    snap("boot");
    key(BSP_BTN_UP); assert(s_state.phase==CT_INTERRUPTED); snap("interrupted"); record_interaction(10);
    unsigned old_y=lv_obj_get_y(s_arms[1]); advance(180); check();
    assert((int)old_y!=lv_obj_get_y(s_arms[1])); /* Companion remains alive when interrupted. */
    key(BSP_BTN_UP); assert(s_state.phase!=CT_INTERRUPTED);
    unsigned seen=0;
    for (unsigned i=0;i<600;i++) {
        advance(120); seen|=1u<<s_state.phase;
        if (s_state.phase==CT_EDIT) snap("coding");
        if (s_state.phase==CT_WAIT) snap("choice");
        if (s_state.phase==CT_DONE) snap("delivery");
    }
    for (unsigned i=0;i<2000 && s_state.phase!=CT_WAIT;i++) advance(120);
    assert(s_state.phase==CT_WAIT);
    for (unsigned i=0;i<12;i++) advance(60);
    snap("choice");
    key_event(BSP_BTN_OK,BSP_BTN_LONG); assert(s_page==1 && s_state.paused); snap("cards");
    unsigned paused_phase=s_state.phase; advance(20000); assert(s_state.phase==paused_phase);
    key(BSP_BTN_OK); assert(!s_page && !s_state.paused);
    rapid_ban(); snap("banned"); record_interaction(12);
    assert(!strcmp(lv_label_get_text(s_rows[0]),"我们发现您的账户异常，"));
    assert(!strcmp(lv_label_get_text(s_rows[1]),"已经停止了您的访问权限。"));
    entropy=1; key(BSP_BTN_OK); assert(s_state.access==CT_APPEAL_FAILED); snap("appeal-failed"); record_interaction(16);
    entropy=0; key(BSP_BTN_OK); assert(s_state.access==CT_APPEAL_FAILED);
    uint32_t remaining=s_state.ban_left; advance(remaining-1); assert(s_state.access==CT_APPEAL_FAILED);
    advance(1); assert(s_state.access==CT_ACCESS_OK); snap("recovered"); record_interaction(10);
    rapid_ban(); entropy=2; key(BSP_BTN_OK); assert(s_state.access==CT_ACCESS_OK); snap("appeal-passed"); record_interaction(10);
    for (unsigned task=0;task<CT_TASKS;task++) for (unsigned bug=0;bug<CT_CARDS;bug++) {
        ct_init(&s_state,1+task*31+bug);
        s_state.project=(task+bug)%CT_TASKS; s_state.task=task; s_state.bug=bug;
        for (unsigned t=0;t<60 && !s_state.completed;t++) {
            ct_tick(&s_state,1000); seen|=1u<<s_state.phase;
            for (unsigned frame=0;frame<8;frame++) {
                s_motion=frame*180; s_scroll=frame%3; s_reveal=80; render(); check();
            }
        }
        assert(s_state.completed==1);
    }
    assert((seen & ((1u<<(CT_COMPACT+1))-1)) == ((1u<<(CT_COMPACT+1))-1));
    for (unsigned p=0;p<CT_PHASES;p++) for (unsigned frame=0;frame<20;frame++) {
        s_state.phase=p; s_state.elapsed=0; s_motion=frame*180; s_scroll=0; render(); check();
    }
    s_state.phase=CT_DONE; s_state.completed=s_state.boosts=9999; s_state.cards=4095;
    s_page=1; render(); check(); s_card_page^=1; render(); snap("full-cards");
    s_page=0; render(); check();
    soc=-1; battery(NULL); check(); assert(!strcmp(lv_label_get_text(s_battery),"--%"));
    soc=101; battery(NULL); check(); assert(!strcmp(lv_label_get_text(s_battery),"100%"));
    ct_init(&s_state,1); s_guard=0; s_state.phase=CT_THINK;
    code_theater_key(BSP_BTN_UP,BSP_BTN_PRESS); advance(251); assert(s_state.burst==0);
    /* Seven queued PRESS events still count even if UI dispatch is guarded. */
    for (unsigned i=0;i<7;i++) code_theater_key(BSP_BTN_OK,BSP_BTN_PRESS);
    advance(60); assert(s_state.access==CT_BANNED);
    for (unsigned i=0;i<40;i++) {
        code_theater_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        code_theater_key(BSP_BTN_OK,BSP_BTN_CLICK);
        code_theater_enter(true); assert(s_page==0 && !s_state.paused); check();
    }
    /* PRESS + PRESS + DOUBLE are exactly two physical clicks. */
    raw(BSP_BTN_OK,BSP_BTN_PRESS); raw(BSP_BTN_OK,BSP_BTN_PRESS); raw(BSP_BTN_OK,BSP_BTN_DOUBLE);
    assert(s_state.burst==2 && s_state.access==CT_ACCESS_OK);
    code_theater_exit(); code_theater_enter(false);
    for (unsigned i=0;i<7;i++) raw(BSP_BTN_OK,BSP_BTN_PRESS);
    assert(s_state.burst==0);
    for (unsigned i=0;i<1000;i++) advance(60);
    assert(s_state.completed>0 && s_state.access==CT_ACCESS_OK); snap("no-buttons");
    code_theater_exit();
    puts("Code Theater UI: companion poses, scene catalog, physical-click counting, both appeals, exact recovery, glyphs, geometry and lifecycle PASS");
}
