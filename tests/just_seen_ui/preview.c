/* Render the production app with real LVGL and simulated board peripherals. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "src/misc/lv_text_private.h"
#include "apps/just_seen/just_seen.c"
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
    callback_active = true; just_seen_key(b, e); callback_active = false; advance(20);
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
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *a = lv_obj_get_child(o, i);
        if (!lv_obj_check_type(a, &lv_label_class) || !lv_label_get_text(a)[0]) continue;
        lv_area_t x; lv_obj_get_coords(a, &x);
        for (unsigned j = i + 1; j < lv_obj_get_child_count(o); j++) {
            lv_obj_t *b = lv_obj_get_child(o, j);
            if (!lv_obj_check_type(b, &lv_label_class) || !lv_label_get_text(b)[0]) continue;
            lv_area_t y; lv_obj_get_coords(b, &y);
            assert(x.x2 < y.x1 || y.x2 < x.x1 || x.y2 < y.y1 || y.y2 < x.y1);
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
        if (!(b.x1 >= a.x1 && b.y1 >= a.y1 && b.x2 <= a.x2 && b.y2 <= a.y2)) fprintf(stderr, "Child bounds: %d,%d-%d,%d content %d,%d-%d,%d text=%s\n", b.x1,b.y1,b.x2,b.y2,a.x1,a.y1,a.x2,a.y2,lv_obj_check_type(lv_obj_get_child(s_content,i),&lv_label_class)?lv_label_get_text(lv_obj_get_child(s_content,i)):"box");
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
    just_seen_prepare(); just_seen_enter(true); snap("home");
    key_event(BSP_BTN_OK, BSP_BTN_CLICK); key_event(BSP_BTN_OK, BSP_BTN_DOUBLE);
    assert(s_state.page == JS_HOME);
    for (unsigned mode = 1; mode <= 3; mode++) {
        s_state.back = mode; render(); check();
        advance(300); key(BSP_BTN_OK); assert(s_state.page == JS_LEARN); snap("learn");
        for (unsigned i = 0; i < mode; i++) { advance(300); key(BSP_BTN_OK); check(); }
        assert(s_state.page == JS_ASK); snap(mode == 1 ? "question" : "hard-question");
        for (unsigned r = 0; r < 8; r++) {
            advance(300); key(BSP_BTN_OK); assert(s_state.page == JS_PAUSE); snap("pause");
            unsigned round = s_state.round;
            advance(300); key(BSP_BTN_OK); assert(s_state.page == JS_LEARN);
            for (unsigned i = 0; i < mode; i++) { advance(300); key(BSP_BTN_OK); check(); }
            assert(s_state.page == JS_ASK && s_state.round == round);
            bool answer = js_current(&s_state) == js_expected(&s_state);
            if (r == 3) answer = !answer;
            advance(300); key(answer ? BSP_BTN_UP : BSP_BTN_DOWN);
            assert(s_state.page == JS_FEEDBACK); snap(r == 3 ? "miss" : "correct");
            key(BSP_BTN_OK); assert(s_state.page == JS_FEEDBACK);
            advance(300); key(BSP_BTN_OK); check();
        }
        assert(s_state.page == JS_RESULT && s_state.correct == 7); snap("result");
        uint8_t old[11]; memcpy(old, s_state.cards, sizeof old);
        advance(300); key(BSP_BTN_UP); assert(!memcmp(old, s_state.cards, mode + 8));
        advance(300); key_event(BSP_BTN_OK, BSP_BTN_LONG); assert(s_state.page == JS_HOME);
    }
    /* All six silhouettes, maximum score, all modes and battery extremes. */
    for (unsigned a = 0; a < 6; a++) {
        js_start(&s_state, 42); s_state.cards[0] = a; render(); check();
    }
    s_state.page = JS_RESULT; s_state.correct = s_state.longest = 8; s_state.code = 9999;
    render(); snap("perfect");
    js_home(&s_state); render(); advance(300); key(BSP_BTN_OK);
    advance(30000); assert(s_state.page == JS_PAUSE);
    advance(60000); assert(backlight == 20);
    key(BSP_BTN_OK); assert(s_state.page == JS_PAUSE && backlight == 100);
    advance(180000); assert(backlight == 0);
    key(BSP_BTN_OK); assert(s_state.page == JS_PAUSE && backlight == 100);
    advance(1000); key_event(BSP_BTN_OK, BSP_BTN_LONG); assert(s_state.page == JS_HOME);
    soc = -1; battery(NULL); assert(!strcmp(lv_label_get_text(s_battery), "--%")); check();
    soc = 101; battery(NULL); assert(!strcmp(lv_label_get_text(s_battery), "100%")); check();
    advance(300); just_seen_key(BSP_BTN_OK, BSP_BTN_PRESS); advance(251);
    assert(s_state.page == JS_HOME);
    for (int i = 0; i < 8; i++) just_seen_key(BSP_BTN_OK, BSP_BTN_PRESS);
    advance(20); assert(s_state.page == JS_LEARN);
    for (int i = 0; i < 50; i++) {
        just_seen_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        just_seen_key(BSP_BTN_OK, BSP_BTN_PRESS);
        just_seen_enter(true); assert(s_state.page == JS_HOME); check();
    }
    just_seen_exit(); just_seen_enter(false); key(BSP_BTN_OK);
    assert(s_state.page == JS_HOME); snap("no-buttons");
    puts("Just Seen UI: glyphs, bounds, all modes, wake, replay, queue, 50 exits PASS");
}
