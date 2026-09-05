/* Actual firmware UI and LVGL, with simulated clock, buttons and battery. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/cloud_hop/cloud_hop.c"

static int64_t fake_us;
static int backlight, soc = 87;
static bool callback_active;
int64_t esp_timer_get_time(void) { return fake_us; }
uint32_t esp_random(void) { return 2026; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t v) { assert(!callback_active); backlight = v; }

static void bounds(lv_obj_t *o)
{
    lv_area_t a; lv_obj_get_coords(o, &a);
    assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *s = lv_label_get_text(o);
        const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        int width = 0;
        while (*s) {
            uint32_t cp = (unsigned char)*s++;
            if (cp >= 0xE0) { cp = ((cp & 15) << 12) | (((unsigned char)s[0] & 63) << 6) | ((unsigned char)s[1] & 63); s += 2; }
            else if (cp >= 0xC0) { cp = ((cp & 31) << 6) | ((unsigned char)*s++ & 63); }
            lv_font_glyph_dsc_t dsc;
            assert(lv_font_get_glyph_dsc(font, &dsc, cp, 0) && !dsc.is_placeholder);
            width += dsc.adv_w;
        }
        if (width > lv_obj_get_width(o)) fprintf(stderr, "Text too wide: %s (%d > %d)\n", lv_label_get_text(o), width, (int)lv_obj_get_width(o));
        assert(width <= lv_obj_get_width(o));
        assert(lv_obj_get_height(o) <= font->line_height); /* No accidental wrapping. */
        if (lv_obj_get_parent(o) == s_content) {
            lv_area_t parent; lv_obj_get_content_coords(s_content, &parent);
            assert(a.x1 >= parent.x1 && a.x2 <= parent.x2 && a.y1 >= parent.y1 && a.y2 <= parent.y2);
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) bounds(lv_obj_get_child(o, i));
}

static void check(void) { lv_obj_update_layout(s_screen); bounds(s_screen); }
static void snap(const char *name)
{
    check(); lv_draw_buf_t *b = lv_snapshot_take(s_screen, LV_COLOR_FORMAT_RGB888); assert(b);
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
static void event(bsp_btn_t b, bsp_btn_ev_t ev)
{
    callback_active = true; cloud_hop_key(b, ev); callback_active = false;
}
static void key(bsp_btn_t b) { event(b, BSP_BTN_PRESS); advance(20); }

int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    cloud_hop_prepare(); cloud_hop_enter(true); snap("home");
    event(BSP_BTN_OK, BSP_BTN_CLICK); event(BSP_BTN_OK, BSP_BTN_LONG); event(BSP_BTN_OK, BSP_BTN_DOUBLE);
    advance(20); assert(s_state.page == CH_HOME);
    key(BSP_BTN_DOWN); assert(s_state.mode == 1); snap("sprint");
    key(BSP_BTN_UP); key(BSP_BTN_OK); assert(s_state.page == CH_AIM); snap("aim");
    /* Entire press judged at the currently displayed position, not 20 ms later. */
    s_state.cursor = s_state.target_x; int shown = s_state.cursor;
    key(BSP_BTN_OK); assert(s_state.page == CH_FLY && s_state.landing_x == shown);
    key(BSP_BTN_OK); assert(s_state.page == CH_FLY && s_state.phase_ms == 20);
    advance(180); snap("flight");
    key(BSP_BTN_UP); assert(s_state.page == CH_PAUSED); snap("pause");
    assert(strcmp(lv_label_get_text(s_footer), "上键继续 / 下键返回") == 0);
    ch_point_t p = ch_pose(&s_state); advance(10000); assert(p.y == ch_pose(&s_state).y);
    key(BSP_BTN_UP); advance(300); assert(s_state.page == CH_LANDED && s_state.perfect); snap("perfect");
    advance(CH_FEEDBACK_MS); assert(s_state.level == 1 && s_state.page == CH_AIM);
    s_state.cursor = CH_CURSOR_MIN; key(BSP_BTN_OK); advance(CH_FLIGHT_MS);
    assert(!s_state.hit && s_state.lives == 2); snap("miss"); advance(CH_FEEDBACK_MS);
    /* Stale events and a full queue cannot cause a delayed extra jump. */
    event(BSP_BTN_OK, BSP_BTN_PRESS); advance(201); assert(s_state.page == CH_AIM);
    for (int i = 0; i < 8; i++) event(BSP_BTN_OK, BSP_BTN_PRESS);
    advance(20); assert(s_state.page == CH_FLY); advance(CH_FLIGHT_MS); advance(CH_FEEDBACK_MS);
    assert(s_state.page == CH_AIM);
    advance(60000); assert(s_state.page == CH_PAUSED && backlight == 20);
    advance(120000); assert(backlight == 0);
    key(BSP_BTN_UP); assert(backlight == 100 && s_state.page == CH_PAUSED);
    key(BSP_BTN_UP); assert(s_state.page == CH_AIM);
    key(BSP_BTN_UP); key(BSP_BTN_DOWN); assert(s_state.page == CH_HOME);
    key(BSP_BTN_OK);
    for (int i = 0; i < CH_GOAL; i++) {
        s_state.cursor = s_state.target_x; key(BSP_BTN_OK);
        for (int j = 0; j < 22; j++) { advance(20); check(); }
        check(); advance(CH_FEEDBACK_MS); check();
    }
    assert(s_state.page == CH_RESULT && s_state.score == 1175); snap("victory");
    uint16_t challenge = s_state.challenge;
    key(BSP_BTN_OK); assert(s_state.challenge == challenge && s_state.score == 0);
    s_state.mode = 1; ch_start(&s_state, challenge); render();
    s_state.cursor = CH_CURSOR_MIN; key(BSP_BTN_OK); advance(CH_FLIGHT_MS); advance(CH_FEEDBACK_MS);
    assert(s_state.page == CH_RESULT && s_state.score == 0); snap("result");
    key(BSP_BTN_UP); assert(s_state.challenge != challenge);
    ch_home(&s_state); render(); soc = -1; battery(NULL);
    assert(strcmp(lv_label_get_text(s_battery), "--%") == 0); snap("battery-unavailable");
    /* Exercise every rank and the maximum numeric field widths. */
    for (int level = 0; level <= 25; level++) {
        s_state.page = CH_RESULT; s_state.level = level; s_state.score = 1175;
        s_state.perfects = 25; s_state.best_streak = 25; s_state.challenge = 9999;
        render(); check();
    }
    for (int i = 0; i < 50; i++) {
        cloud_hop_exit(); assert(!s_timer && !s_battery_timer && !s_screen);
        event(BSP_BTN_OK, BSP_BTN_PRESS); cloud_hop_enter(true); advance(20);
        assert(s_state.page == CH_HOME); check();
    }
    cloud_hop_exit(); cloud_hop_enter(false); key(BSP_BTN_OK);
    assert(s_state.page == CH_HOME); snap("no-buttons"); cloud_hop_exit();
    puts("Cloud Hop UI: glyphs, bounds, queued input, flight, replay, idle, 50 exits PASS");
}
