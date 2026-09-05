/* Render the production app with real LVGL and simulated board peripherals. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/misc/lv_text_private.h"
#include "apps/balloon_rush/balloon_rush.c"
static int64_t fake_us;
static int soc = 87, backlight;
static bool callback_active;
int64_t esp_timer_get_time(void) { return fake_us; }
uint32_t esp_random(void) { return 1234; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t b) { assert(!callback_active); backlight = b; }
static void advance(unsigned ms) { fake_us += (int64_t)ms * 1000; lv_tick_inc(ms); tick(s_timer); }
static void key_event(bsp_btn_t b, bsp_btn_ev_t e)
{
    callback_active = true; balloon_rush_key(b, e); callback_active = false; advance(20);
}
static void key(bsp_btn_t b) { key_event(b, BSP_BTN_PRESS); }
static void check_labels(lv_obj_t *o)
{
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *text = lv_label_get_text(o);
        lv_area_t a; lv_obj_get_coords(o, &a);
        assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
        lv_point_t size;
        const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        lv_text_get_size(&size, text, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(o)) fprintf(stderr, "Overflow: %s width=%d allowed=%d\n", text, (int)size.x, (int)lv_obj_get_width(o));
        assert(size.x <= lv_obj_get_width(o));
        for (uint32_t i = 0; text[i];) {
            uint32_t cp = lv_text_encoded_next(text, &i);
            assert(!(cp >= 'A' && cp <= 'Z') && !(cp >= 'a' && cp <= 'z'));
            lv_font_glyph_dsc_t dsc;
            assert(lv_font_get_glyph_dsc(font, &dsc, cp, 0) && !dsc.is_placeholder);
        }
    }
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) check_labels(lv_obj_get_child(o, i));
}
static void check(void)
{
    lv_obj_update_layout(s_screen); check_labels(s_screen);
    /* All app panel objects must remain inside its actual content rectangle. */
    lv_area_t a; lv_obj_get_content_coords(s_content, &a);
    for (unsigned i = 0; i < lv_obj_get_child_count(s_content); i++) {
        lv_area_t b; lv_obj_get_coords(lv_obj_get_child(s_content, i), &b);
        assert(b.x1 >= a.x1 && b.y1 >= a.y1 && b.x2 <= a.x2 && b.y2 <= a.y2);
    }
}
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
    lv_init(); assert(lv_display_create(240, 320));
    balloon_rush_prepare(); balloon_rush_enter(true); snap("home");
    key_event(BSP_BTN_OK, BSP_BTN_CLICK); key_event(BSP_BTN_OK, BSP_BTN_DOUBLE);
    assert(s_state.page == BR_HOME);
    key(BSP_BTN_OK); advance(1100); snap("aim");
    key(BSP_BTN_UP); uint32_t phase = s_state.phase; advance(15000);
    assert(s_state.page == BR_PAUSED && s_state.phase == phase); snap("pause");
    key(BSP_BTN_UP); s_state.needle = s_state.target; key(BSP_BTN_OK);
    assert(s_state.page == BR_CHOICE); snap("choice");
    key(BSP_BTN_OK); assert(s_state.page == BR_CHOICE); /* No accidental double action. */
    advance(500); key(BSP_BTN_DOWN); assert(s_state.score == 150); snap("collected");
    key(BSP_BTN_OK); s_state.needle = 0; key(BSP_BTN_OK); assert(s_state.burst); snap("burst");
    key(BSP_BTN_OK);
    for (unsigned r = 1; r <= BR_ROUNDS; r++) {
        assert(s_state.round == r);
        advance(1000); check();
        s_state.needle = s_state.target; key(BSP_BTN_OK); check();
        if (r < BR_ROUNDS) { advance(500); key(BSP_BTN_OK); }
    }
    assert(s_state.score == 30600); snap("victory");
    key(BSP_BTN_OK); advance(BR_TIMEOUT); assert(s_state.burst);
    key(BSP_BTN_DOWN); advance(60000); assert(backlight == 20);
    key(BSP_BTN_OK); assert(backlight == 100 && s_state.page == BR_HOME);
    key_event(BSP_BTN_OK, BSP_BTN_LONG); assert(s_state.page == BR_HOME);
    key(BSP_BTN_OK); assert(s_state.page == BR_AIM);
    key_event(BSP_BTN_OK, BSP_BTN_LONG); assert(s_state.page == BR_HOME);
    advance(180000); assert(backlight == 0);
    key(BSP_BTN_OK); assert(s_state.page == BR_HOME && backlight == 100);
    key_event(BSP_BTN_OK, BSP_BTN_LONG); assert(s_state.page == BR_HOME);
    soc = -1; battery(NULL); assert(!strcmp(lv_label_get_text(s_battery), "--%")); snap("unknown-battery");
    /* Queued old presses expire, and a burst of presses only acts once. */
    balloon_rush_key(BSP_BTN_OK, BSP_BTN_PRESS); advance(201); assert(s_state.page == BR_HOME);
    for (int i = 0; i < 8; i++) balloon_rush_key(BSP_BTN_OK, BSP_BTN_PRESS);
    advance(20); assert(s_state.page == BR_AIM);
    for (int i = 0; i < 50; i++) {
        balloon_rush_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        balloon_rush_key(BSP_BTN_OK, BSP_BTN_PRESS);
        balloon_rush_enter(true); assert(s_state.page == BR_HOME && !s_placeholder); check();
    }
    balloon_rush_exit(); balloon_rush_enter(false); key(BSP_BTN_OK);
    assert(s_state.page == BR_HOME); snap("no-buttons");
    puts("Balloon Rush UI: glyphs, bounds, eight rounds, input, wake, queue, 50 exits PASS");
}
