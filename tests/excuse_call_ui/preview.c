#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/misc/lv_text_private.h"
#include "apps/excuse_call/excuse_call.c"
static int64_t fake_us;
static int soc=87, status=1;
static unsigned plays, stops;
static int64_t audio_deadline;
int64_t esp_timer_get_time(void) { return fake_us; }
int bsp_battery_soc(void) { return soc; }
void ec_audio_prepare(void) {}
void ec_audio_play(unsigned ring,int64_t deadline) { assert(ring<5); plays++; audio_deadline=deadline; }
void ec_audio_stop(void) { stops++; }
int ec_audio_status(void) { return status; }
static void advance(unsigned ms) { fake_us+=(int64_t)ms*1000; lv_tick_inc(ms); frame(NULL); }
static void key(bsp_btn_t b) { excuse_call_key(b,BSP_BTN_PRESS); advance(20); }
static void check_labels(lv_obj_t *o)
{
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *text = lv_label_get_text(o);
        assert(!strstr(text,"借过") && !strstr(text,"确定") && !strstr(text,"上下") && !strstr(text,"铃声") && !strstr(text,"秒后"));
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
            assert((!(cp >= 'A' && cp <= 'Z') && !(cp >= 'a' && cp <= 'z')));
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
int main(void)
{
    lv_init(); assert(lv_display_create(240,320));
    excuse_call_prepare(); excuse_call_enter(true); snap("home"); assert(!plays);
    for(unsigned r=0;r<5;r++) { key(BSP_BTN_UP); check(); }
    assert(s_state.ring==0 && !plays);
    key(BSP_BTN_OK); assert(s_state.ringing && plays==1); snap("ringing");
    excuse_call_key(BSP_BTN_OK,BSP_BTN_CLICK); excuse_call_key(BSP_BTN_OK,BSP_BTN_LONG);
    excuse_call_key(BSP_BTN_OK,BSP_BTN_DOUBLE); advance(20); assert(s_state.ringing && plays==1);
    uint64_t deadline=s_state.deadline;
    for(unsigned r=0;r<5;r++) { key(BSP_BTN_DOWN); assert(s_state.deadline==deadline && audio_deadline==(int64_t)deadline); check(); }
    for(unsigned f=0;f<30;f++) { advance(90); check(); }
    key(BSP_BTN_OK); assert(!s_state.ringing && stops==1); snap("stopped");
    key(BSP_BTN_OK); advance(59999); assert(s_state.ringing); advance(1);
    assert(!s_state.ringing && s_state.timed_out); snap("timeout");
    key(BSP_BTN_OK); assert(s_state.ringing); fake_us=(int64_t)s_state.deadline*1000;
    excuse_call_key(BSP_BTN_OK,BSP_BTN_PRESS); advance(0); assert(!s_state.ringing);
    for(unsigned c=0;c<3;c++) for(unsigned r=0;r<5;r++) {
        s_state.caller=c; s_state.ring=r; render(); check();
    }
    for(unsigned i=0;i<50;i++) { excuse_call_exit(); assert(!s_screen && !s_timer && !s_battery_timer); excuse_call_key(BSP_BTN_OK,BSP_BTN_PRESS); excuse_call_enter(true); check(); }
    status=-1; render(); snap("no-audio"); key(BSP_BTN_OK); assert(!s_state.ringing);
    status=0; render(); check(); status=1;
    soc=-1; battery(NULL); check(); soc=100; battery(NULL); check();
    excuse_call_exit(); excuse_call_enter(false); key(BSP_BTN_OK); key(BSP_BTN_DOWN); assert(!s_state.ringing && s_state.ring==0); snap("no-buttons");
    excuse_call_exit(); puts("Excuse Call production UI: physical events, lifecycle, deadline, all glyphs and label geometry PASS");
}
