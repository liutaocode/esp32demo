/* Execute the production LVGL page with simulated board I/O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/perfect_slice/perfect_slice.c"
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
    callback_active = true; perfect_slice_key(b, ev); callback_active = false;
}
static void key(bsp_btn_t b) { post(b, BSP_BTN_PRESS); advance(20); }
static void reveal_done(void) { for (unsigned i = 0; i < 20; i++) advance(20); }

int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    perfect_slice_prepare(); perfect_slice_enter(true); snap("home");
    key(BSP_BTN_DOWN); assert(s_state.mode == 1); snap("expert-home");
    post(BSP_BTN_OK, BSP_BTN_CLICK); post(BSP_BTN_OK, BSP_BTN_LONG); advance(20);
    assert(s_state.page == SLICE_HOME);
    key(BSP_BTN_UP); key(BSP_BTN_OK); advance(200); snap("play");
    key(BSP_BTN_UP); assert(s_state.page == SLICE_PAUSED); snap("paused");
    advance(60000); assert(s_state.page == SLICE_PAUSED && backlight == 20);
    key(BSP_BTN_UP); assert(s_state.page == SLICE_PAUSED && backlight == 100);
    key(BSP_BTN_UP); assert(s_state.page == SLICE_PLAY);
    advance(201); assert(s_state.page == SLICE_PAUSED); /* Stalled render. */
    key(BSP_BTN_UP);
    for (unsigned i = 0; i < 10; i++) {
        s_state.phase = (slice_target_x(&s_state) - 1) * 1000U;
        key(BSP_BTN_OK); assert(s_state.page == SLICE_REVEAL && s_state.perfect);
        unsigned score = s_state.score;
        for (unsigned j = 0; j < 8; j++) post(BSP_BTN_OK, BSP_BTN_PRESS);
        advance(20); assert(s_state.page == SLICE_REVEAL && s_state.score == score);
        reveal_done(); if (i == 0) snap("perfect");
        key(BSP_BTN_OK);
    }
    assert(s_state.page == SLICE_RESULT && s_state.score == 1150); snap("result");
    for (unsigned score = 0; score <= 1150; score += 50) {
        s_state.score = score; s_state.new_best = false; render(); check();
    }
    for (soc = 0; soc <= 100; soc += 100) { battery(NULL); check(); }
    soc = 87;
    /* Check every cut boundary, target, mode and feedback string at max score. */
    for (unsigned mode = 0; mode < 2; mode++) for (unsigned round = 0; round < 10; round++) {
        s_state.mode = mode; slice_start(&s_state);
        for (unsigned r = 0; r < round; r++) {
            slice_cut(&s_state); slice_tick(&s_state, 400); slice_next(&s_state);
        }
        render(); check();
        slice_state_t base = s_state;
        for (unsigned x = 1; x < SLICE_WIDTH; x++) {
            s_state = base; s_state.phase = (x - 1) * 1000; s_state.streak = 9; s_state.score = 1030;
            slice_cut(&s_state); render(); reveal_done(); check();
        }
    }
    s_state.mode = 1; slice_start(&s_state); s_state.phase = 50000;
    render(); snap("expert");
    key(BSP_BTN_OK); reveal_done(); snap("miss");
    /* Stale queued events cannot cut or advance after an idle wake. */
    slice_home(&s_state); render(); post(BSP_BTN_OK, BSP_BTN_PRESS); advance(250);
    assert(s_state.page == SLICE_HOME);
    advance(180000); assert(backlight == 0);
    key(BSP_BTN_OK); assert(backlight == 100 && s_state.page == SLICE_HOME);
    soc = -1; battery(NULL); assert(strcmp(lv_label_get_text(s_battery), "--%") == 0); snap("battery-unknown");
    for (unsigned i = 0; i < 50; i++) {
        perfect_slice_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        post(BSP_BTN_OK, BSP_BTN_PRESS); assert(s_queue->count == 0);
        perfect_slice_enter(true); assert(!s_placeholder && s_state.page == SLICE_HOME);
    }
    perfect_slice_exit(); perfect_slice_enter(false); key(BSP_BTN_OK);
    assert(s_state.page == SLICE_HOME); snap("no-buttons");
    perfect_slice_exit();
    puts("Perfect Slice UI: glyphs, text bounds, all cuts, input, pause, wake, fallbacks, 50 exits PASS");
}
