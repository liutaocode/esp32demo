/* Execute the production LVGL page with simulated board I/O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/lane_leap/lane_leap.c"
#include "src/misc/lv_text_private.h"

static int64_t fake_us;
static int backlight, soc = 87;
static bool callback_active;
int64_t esp_timer_get_time(void) { return fake_us; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t value) { assert(!callback_active); backlight = value; }

static void bounds(lv_obj_t *o)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    lv_area_t a; lv_obj_get_coords(o, &a);
    if (!s_board || o == s_board || !lv_obj_has_flag(s_board, LV_OBJ_FLAG_HIDDEN)) {
        /* Road art intentionally scrolls through its clipped parent. */
        lv_obj_t *p = o; bool road_art = false;
        while (p) { if (p == s_board) road_art = true; p = lv_obj_get_parent(p); }
        if (!road_art) assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
    }
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *str = lv_label_get_text(o);
        const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        lv_point_t size;
        lv_text_get_size(&size, str, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(o)) fprintf(stderr, "Text overflow: %s (%d > %d)\n", str, (int)size.x, (int)lv_obj_get_width(o));
        assert(size.x <= lv_obj_get_width(o));
        uint32_t i = 0;
        while (str[i]) {
            uint32_t cp = lv_text_encoded_next(str, &i);
            if (cp == '\n') continue;
            lv_font_glyph_dsc_t d;
            assert(lv_font_get_glyph_dsc(font, &d, cp, 0) && !d.is_placeholder);
        }
        assert(lv_obj_get_scroll_bottom(o) <= 0);
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) bounds(lv_obj_get_child(o, i));
}

static void check(void) { lv_obj_update_layout(s_screen); bounds(s_screen); }
static void snap(const char *name)
{
    check();
    lv_draw_buf_t *b = lv_snapshot_take(s_screen, LV_COLOR_FORMAT_RGB888); assert(b);
    char path[100]; snprintf(path, sizeof(path), "%s.ppm", name);
    FILE *f = fopen(path, "wb"); assert(f);
    fprintf(f, "P6\n%u %u\n255\n", b->header.w, b->header.h);
    for (unsigned y = 0; y < b->header.h; y++) for (unsigned x = 0; x < b->header.w; x++) {
        uint8_t *p = b->data + y * b->header.stride + x * 3;
        uint8_t rgb[] = {p[2], p[1], p[0]}; fwrite(rgb, 1, 3, f);
    }
    fclose(f); lv_draw_buf_destroy(b);
}
static void advance(unsigned ms) { fake_us += (int64_t)ms * 1000; lv_tick_inc(ms); frame(s_timer); }
static void post(bsp_btn_t b, bsp_btn_ev_t ev)
{
    callback_active = true; lane_leap_key(b, ev); callback_active = false;
}
static void key(bsp_btn_t b) { post(b, BSP_BTN_PRESS); advance(20); }
static void race(void) { for (unsigned i = 0; i < 80 && s_state.page == LL_READY; i++) advance(20); }
int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    lane_leap_prepare(); lane_leap_enter(true); snap("home");
    key(BSP_BTN_DOWN); assert(s_state.mode == 1); snap("expert-home");
    post(BSP_BTN_OK, BSP_BTN_CLICK); post(BSP_BTN_OK, BSP_BTN_DOUBLE); advance(20);
    assert(s_state.page == LL_HOME);
    key(BSP_BTN_UP); key(BSP_BTN_OK); assert(s_state.page == LL_READY); snap("ready");
    race(); assert(s_state.page == LL_RACING);
    key(BSP_BTN_UP); assert(s_state.lane == 0); key(BSP_BTN_OK); assert(s_state.jump_ms);
    key(BSP_BTN_DOWN); assert(s_state.lane == 1);
    post(BSP_BTN_UP, BSP_BTN_PRESS); post(BSP_BTN_DOWN, BSP_BTN_PRESS); advance(20); assert(s_state.lane == 1);
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(20); assert(s_state.page == LL_PAUSED); snap("pause");
    unsigned time = s_state.elapsed_ms; advance(60000); assert(time == s_state.elapsed_ms && backlight == 20);
    key(BSP_BTN_OK); assert(s_state.page == LL_PAUSED && backlight == 100);
    key(BSP_BTN_OK); assert(s_state.page == LL_READY); race();
    for (unsigned i = 0; i < 5; i++) { advance(400); assert(s_state.page == LL_RACING); }
    /* Production-rendered art fixtures; these are explicitly not device captures. */
    memset(s_state.row, 0, sizeof(s_state.row));
    s_state.spawn_ms = 10000;
    s_state.row[0] = (ll_row_t){.active = true, .y = 70, .kind = {LL_TRUCK, LL_COIN, LL_BARRIER}};
    s_state.row[1] = (ll_row_t){.active = true, .y = 128, .kind = {LL_COIN, LL_BARRIER, LL_AIR_COIN}};
    s_state.jump_ms = 0; s_state.cooldown_ms = 0; s_state.notice_ms = 0;
    s_state.lane = 0; render(); snap("race");
    s_state.lane = 2; s_state.jump_ms = 380; s_state.notice = 2; s_state.notice_ms = 400;
    render(); snap("airborne");
    s_state.row[1].kind[1] = LL_GAP; s_state.lane = 1; s_state.notice = 3;
    render(); snap("gap");
    for (unsigned notice = 0; notice < 5; notice++) {
        s_state.notice = notice; s_state.notice_ms = 500; render(); check();
    }
    s_state.score = 999999; s_state.coins = 9999; s_state.streak = 9999;
    s_state.notice_ms = 0; s_state.elapsed_ms = 9999999;
    render(); snap("boundary");
    s_state.page = LL_RESULT; s_state.best_streak = 9999; s_state.jumps = 9999;
    s_state.new_best = true; s_state.cleared = 80; s_state.result_ms = 0;
    render(); snap("result-boundary");
    key(BSP_BTN_OK); assert(s_state.page == LL_RESULT);
    for (unsigned i = 0; i < 30; i++) advance(20);
    s_state.score = 860; s_state.coins = 12; s_state.best_streak = 18; s_state.jumps = 5;
    render(); snap("result");
    uint32_t seed = s_state.seed; key(BSP_BTN_OK); assert(s_state.seed == seed && s_state.page == LL_READY);
    race(); post(BSP_BTN_OK, BSP_BTN_LONG); advance(20); key(BSP_BTN_DOWN); assert(s_state.page == LL_HOME);
    s_state.best[0] = 999999; render(); check();
    for (soc = -1; soc <= 101; soc++) { battery(NULL); check(); }
    soc = -1; battery(NULL); snap("battery-unknown");
    post(BSP_BTN_OK, BSP_BTN_PRESS); advance(501); assert(s_state.page == LL_HOME);
    advance(180000); assert(backlight == 0); key(BSP_BTN_OK); assert(backlight == 100 && s_state.page == LL_HOME);
    for (unsigned i = 0; i < 50; i++) {
        lane_leap_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        post(BSP_BTN_OK, BSP_BTN_PRESS); assert(s_queue->count == 0);
        lane_leap_enter(true); assert(!s_placeholder);
        key(BSP_BTN_OK); race();
        for (unsigned j = 0; j < 100; j++) advance(20);
        check();
    }
    lane_leap_exit(); lane_leap_enter(false); key(BSP_BTN_OK);
    assert(s_state.page == LL_HOME); snap("no-buttons"); lane_leap_exit();
    puts("Lane Leap UI: production render, Chinese glyphs, text bounds, ordered keys, pause, stalls, sleep, 50 lifecycle cycles PASS");
}
