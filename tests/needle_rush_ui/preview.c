/* Execute the production LVGL page with simulated board I/O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/needle_rush/needle_rush.c"
#include "src/misc/lv_text_private.h"

static int64_t fake_us;
static int backlight, soc = 87;
static bool callback_active;
int64_t esp_timer_get_time(void) { return fake_us; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t value) { assert(!callback_active); backlight = value; }

static void bounds(lv_obj_t *o)
{
    lv_area_t a; lv_obj_get_coords(o, &a);
    assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
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
        /* Also test clipping by the content panel, not just display edges. */
        if (lv_obj_get_parent(o) == s_content) {
            lv_area_t panel; lv_obj_get_content_coords(s_content, &panel);
            if (a.y2 > panel.y2) fprintf(stderr, "Panel clips %s at %d > %d\n", str, (int)a.y2, (int)panel.y2);
            assert(a.x1 >= panel.x1 && a.x2 <= panel.x2 && a.y1 >= panel.y1 && a.y2 <= panel.y2);
        }
        /* Product UI must not leak English words. */
        for (const char *c = str; *c; c++) assert(!(*c >= 'A' && *c <= 'Z') && !(*c >= 'a' && *c <= 'z'));
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
    callback_active = true; needle_rush_key(b, ev); callback_active = false;
}
static void key(bsp_btn_t b) { post(b, BSP_BTN_PRESS); advance(20); }
static void settle(void) {
    for (unsigned i = 0; i < 30 && (s_state.page == NR_FLIGHT || s_state.page == NR_FEEDBACK); i++) advance(20);
}
static bool safe(void) {
    for (unsigned i = 0; i < s_state.count; i++)
        if (nr_distance(nr_angle(&s_state, i), NR_IMPACT) <= NR_CLEARANCE) return false;
    return true;
}
int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    needle_rush_prepare(); needle_rush_enter(true); snap("home");
    key(BSP_BTN_DOWN); assert(s_state.mode == 1); snap("expert-home");
    key(BSP_BTN_UP); key(BSP_BTN_OK); snap("play");
    post(BSP_BTN_OK, BSP_BTN_CLICK); post(BSP_BTN_OK, BSP_BTN_DOUBLE); advance(20);
    assert(s_state.page == NR_SPIN);
    key(BSP_BTN_UP); assert(s_state.page == NR_PAUSED); snap("paused");
    nr_state_t paused = s_state; advance(3000);
    assert(memcmp(&paused, &s_state, sizeof(paused)) == 0);
    key(BSP_BTN_UP); assert(s_state.page == NR_SPIN);
    nr_start(&s_state, 1); render();
    unsigned shots = 0, iterations = 0;
    while (s_state.page != NR_RESULT && iterations++ < 100000) {
        if (s_state.page == NR_SPIN && safe()) {
            key(BSP_BTN_OK); assert(s_state.page == NR_FLIGHT && !s_state.hit);
            shots++;
            for (unsigned j = 0; j < 8; j++) post(BSP_BTN_OK, BSP_BTN_PRESS);
            advance(20); assert(s_state.page == NR_FLIGHT);
            if (shots == 1) { snap("flight"); }
        } else advance(20);
        if (s_state.page == NR_FEEDBACK && s_state.elapsed == 0) {
            check();
            if (shots == 3) snap("combo");
            if (!s_state.remaining) snap("level-clear");
        }
        if (s_state.page == NR_SPIN) check();
    }
    assert(s_state.won && s_state.score == 720); snap("win");
    unsigned challenge = s_state.challenge;
    key(BSP_BTN_OK); assert(s_state.challenge == challenge && s_state.page == NR_SPIN);
    for (unsigned miss = 0; miss < 3; miss++) {
        s_state.phase = NR_IMPACT; s_state.pins[0] = 0;
        render(); key(BSP_BTN_OK); assert(s_state.hit);
        for (unsigned i = 0; i < 6; i++) advance(20);
        assert(s_state.page == NR_FEEDBACK); snap("miss"); settle();
    }
    assert(s_state.page == NR_RESULT && !s_state.won); snap("result");
    key(BSP_BTN_UP); assert(s_state.challenge == (challenge + 1) % 10000);
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(20); assert(s_state.page == NR_HOME);
    /* Every visible wheel angle and both spin directions fit the panel. */
    for (unsigned level = 0; level < NR_LEVELS; level++) {
        nr_start(&s_state, 9); s_state.level = level; s_state.count = 13;
        for (unsigned i = 0; i < s_state.count; i++) s_state.pins[i] = i * NR_TURN / 13;
        render();
        for (unsigned angle = 0; angle < 360; angle++) {
            s_state.phase = (int32_t)angle * 1000;
            for (unsigned i = 0; i < s_state.count; i++) pin_position(i, (int)(nr_angle(&s_state, i) / 1000));
            check();
        }
    }
    /* Idle auto-pause, dim/off, first press wake-only, stale/queued input. */
    nr_start(&s_state, 0); render(); advance(60000);
    assert(s_state.page == NR_PAUSED && backlight == 20);
    advance(120000); assert(backlight == 0);
    key(BSP_BTN_OK); assert(s_state.page == NR_PAUSED && backlight == 100);
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(20); assert(s_state.page == NR_PAUSED);
    key(BSP_BTN_UP); assert(s_state.page == NR_SPIN);
    nr_home(&s_state); render(); post(BSP_BTN_OK, BSP_BTN_PRESS); advance(250);
    assert(s_state.page == NR_HOME);
    for (soc = 0; soc <= 100; soc++) { battery(NULL); check(); }
    soc = -1; battery(NULL); assert(strcmp(lv_label_get_text(s_battery), "--%") == 0); snap("battery-unknown");
    for (unsigned i = 0; i < 50; i++) {
        needle_rush_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        unsigned count = s_queue->count; post(BSP_BTN_OK, BSP_BTN_PRESS); assert(s_queue->count == count);
        needle_rush_enter(true); assert(s_state.page == NR_HOME && !s_queue->count);
    }
    needle_rush_exit(); needle_rush_enter(false); key(BSP_BTN_OK);
    assert(s_state.page == NR_HOME); snap("no-buttons");
    needle_rush_exit();
    puts("Needle Rush UI: glyphs, Chinese-only text, bounds, 3600 wheel layouts, input, idle, 50 exits PASS");
}
