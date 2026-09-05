/* Actual firmware UI and LVGL, with simulated clock, buttons and battery. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/math_rail/math_rail.c"

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
            assert(!(cp >= 'A' && cp <= 'Z') && !(cp >= 'a' && cp <= 'z'));
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

static void no_label_overlap(lv_obj_t *o)
{
    for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *a = lv_obj_get_child(o, i);
        no_label_overlap(a);
        if (!lv_obj_check_type(a, &lv_label_class) || !*lv_label_get_text(a)) continue;
        lv_area_t aa; lv_obj_get_coords(a, &aa);
        for (unsigned j = i + 1; j < lv_obj_get_child_count(o); j++) {
            lv_obj_t *b = lv_obj_get_child(o, j);
            if (!lv_obj_check_type(b, &lv_label_class) || !*lv_label_get_text(b)) continue;
            lv_area_t bb; lv_obj_get_coords(b, &bb);
            assert(aa.x2 < bb.x1 || bb.x2 < aa.x1 || aa.y2 < bb.y1 || bb.y2 < aa.y1);
        }
    }
}
static void check(void) { lv_obj_update_layout(s_screen); bounds(s_screen); no_label_overlap(s_screen); }
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
    callback_active = true; math_rail_key(b, ev); callback_active = false;
}
static void key(bsp_btn_t b) { advance(220); event(b, BSP_BTN_CLICK); advance(20); }


static bsp_btn_t answer_key(bool correct)
{
    bool top = s_state.current.options[0] == s_state.current.answer;
    return top == correct ? BSP_BTN_UP : BSP_BTN_DOWN;
}
static void start(unsigned mode)
{
    mt_home(&s_state); s_state.mode = mode; render(); key(BSP_BTN_OK);
    assert(s_state.page == MT_ASK); check();
}
int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    math_rail_prepare(); math_rail_enter(true); snap("home");
    event(BSP_BTN_OK, BSP_BTN_PRESS); event(BSP_BTN_OK, BSP_BTN_DOUBLE);
    event((bsp_btn_t)99, BSP_BTN_CLICK); advance(20); assert(s_state.page == MT_HOME);
    key(BSP_BTN_DOWN); assert(s_state.mode == 1); snap("mode-twenty");
    key(BSP_BTN_DOWN); snap("mode-hundred"); key(BSP_BTN_DOWN); snap("mode-multiply");
    key(BSP_BTN_OK); snap("question");
    key(BSP_BTN_OK); assert(s_state.page == MT_PAUSED); snap("pause");
    advance(10000); assert(s_state.position == 0);
    key(BSP_BTN_UP); assert(s_state.page == MT_ASK);
    key(answer_key(true)); assert(s_state.correct == 1); snap("correct");
    key(BSP_BTN_DOWN); assert(s_state.page == MT_ASK);
    key(answer_key(false)); assert(s_state.correct == 1); snap("correction");
    advance(59000); assert(s_state.page == MT_FEEDBACK);
    advance(1100); assert(s_state.page == MT_PAUSED && backlight == 20);
    advance(120000); assert(backlight == 0);
    key(BSP_BTN_DOWN); assert(backlight == 100 && s_state.page == MT_PAUSED);
    key(BSP_BTN_UP); assert(s_state.page == MT_FEEDBACK);
    advance(220); event(BSP_BTN_OK, BSP_BTN_LONG); advance(20); assert(s_state.page == MT_HOME);
    start(0);
    advance(220); event(BSP_BTN_UP, BSP_BTN_CLICK); advance(201); assert(s_state.page == MT_ASK);
    for (unsigned i = 0; i < 9; i++) event(BSP_BTN_UP, BSP_BTN_CLICK);
    advance(20); assert(s_state.page == MT_FEEDBACK && s_state.position == 0);
    event(BSP_BTN_DOWN, BSP_BTN_CLICK); advance(20); assert(s_state.page == MT_FEEDBACK);
    key(BSP_BTN_UP); assert(s_state.page == MT_ASK && s_state.position == 1);
    for (unsigned mode = 0; mode < MT_MODES; mode++) {
        for (unsigned score = 0; score <= 10; score++) {
            start(mode);
            for (unsigned i = 0; i < MT_ROUNDS; i++) {
                key(answer_key(i < score)); check(); key(BSP_BTN_UP); check();
            }
            assert(s_state.page == MT_RESULT && s_state.correct == score);
            if (mode == 0 && score == 8) snap("result");
            if (score < 10) {
                unsigned stamps = s_state.stamps[mode];
                key(BSP_BTN_DOWN); assert(s_state.review && s_state.count == 10 - score);
                if (mode == 0 && score == 8) snap("review");
                unsigned n = s_state.count;
                for (unsigned i = 0; i < n; i++) { key(answer_key(true)); check(); key(BSP_BTN_UP); check(); }
                assert(s_state.correct == score && s_state.stamps[mode] == stamps);
                if (mode == 0 && score == 8) snap("review-result");
            }
        }
    }
    /* Render all generated hints, operators and large numbers across 400 courses. */
    for (unsigned mode = 0; mode < MT_MODES; mode++) for (unsigned seed = 0; seed < 100; seed++) {
        s_state.mode = mode; mt_start(&s_state, seed);
        for (unsigned i = 0; i < MT_ROUNDS; i++) {
            render(); check(); mt_answer(&s_state, 0); render(); check(); mt_next(&s_state);
        }
        render(); check();
    }
    mt_home(&s_state); render(); soc = -1; battery(NULL);
    assert(strcmp(lv_label_get_text(s_battery), "--%") == 0); snap("battery-unavailable");
    soc = 150; battery(NULL); assert(strcmp(lv_label_get_text(s_battery), "100%") == 0); check();
    for (unsigned i = 0; i < 50; i++) {
        math_rail_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        event(BSP_BTN_OK, BSP_BTN_CLICK); math_rail_enter(true); advance(220);
        assert(s_state.page == MT_HOME); check();
    }
    math_rail_exit(); math_rail_enter(false); key(BSP_BTN_OK);
    assert(s_state.page == MT_HOME); snap("no-buttons"); math_rail_exit();
    puts("Math Rail UI: glyphs, bounds, overlap, 4000 questions/hints, controls, pause/wake and 50 exits PASS");
}
