/* Actual firmware UI and LVGL, with simulated clock, buttons and battery. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/math_train/math_train.c"

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
    callback_active = true; math_train_key(b, ev); callback_active = false;
}
static void key(bsp_btn_t b) { advance(220); event(b, BSP_BTN_CLICK); advance(20); }



static void submit(bool right)
{
    const mt_question_t *q = mt_current(&s_state);
    unsigned i = 0;
    while ((q->choices[i] == q->answer) != right) i++;
    while (s_state.selected != i) key(BSP_BTN_DOWN);
    check(); key(BSP_BTN_OK); check();
}
int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    math_train_prepare(); math_train_enter(true); snap("home");
    event(BSP_BTN_OK, BSP_BTN_PRESS); event(BSP_BTN_OK, BSP_BTN_DOUBLE);
    event((bsp_btn_t)99, BSP_BTN_CLICK); advance(20); assert(s_state.page == MT_HOME);
    key(BSP_BTN_UP); assert(s_state.mode == 3); snap("mode-multiply");
    key(BSP_BTN_DOWN); assert(s_state.mode == 0); key(BSP_BTN_OK); snap("question");
    submit(true); snap("correct"); key(BSP_BTN_OK); submit(false); snap("correction");
    key(BSP_BTN_UP); assert(s_state.page == MT_FEEDBACK);
    advance(60000); assert(s_state.page == MT_FEEDBACK && backlight == 20);
    advance(120000); assert(backlight == 0);
    key(BSP_BTN_OK); assert(backlight == 100 && s_state.page == MT_FEEDBACK);
    key(BSP_BTN_OK); assert(s_state.page == MT_ASK);
    advance(220); event(BSP_BTN_OK, BSP_BTN_LONG); advance(20); assert(s_state.page == MT_HOME);
    key(BSP_BTN_OK);
    advance(220); event(BSP_BTN_OK, BSP_BTN_CLICK); advance(201); assert(s_state.page == MT_ASK);
    for (unsigned i = 0; i < 9; i++) event(BSP_BTN_OK, BSP_BTN_CLICK);
    advance(20); assert(s_state.page == MT_FEEDBACK && s_state.completed == 1);
    event(BSP_BTN_OK, BSP_BTN_CLICK); advance(20); assert(s_state.page == MT_FEEDBACK);
    for (unsigned mode = 0; mode < 4; mode++) {
        mt_home(&s_state); s_state.mode = mode; mt_start(&s_state, 890 + mode); render();
        for (unsigned i = 0; i < 10; i++) { check(); submit(i % 3 != 0); key(BSP_BTN_OK); check(); }
        assert(s_state.page == MT_RESULT && mt_missed(&s_state) == 4);
        if (mode == 0) snap("result");
        key(BSP_BTN_DOWN); key(BSP_BTN_OK); assert(s_state.reviewing && s_state.review_total == 4);
        if (mode == 0) snap("review");
        for (unsigned i = 0; i < 4; i++) { submit(true); key(BSP_BTN_OK); check(); }
        assert(s_state.page == MT_RESULT && !mt_missed(&s_state) && s_state.correct == 6);
        if (mode == 0) snap("review-done");
        key(BSP_BTN_DOWN); assert(s_state.selected == 2); key(BSP_BTN_OK); assert(s_state.page == MT_HOME);
    }
    /* Check thousands of actual equations, choices and feedback combinations. */
    for (unsigned mode = 0; mode < 4; mode++) for (unsigned seed = 1; seed < 101; seed++) {
        s_state.mode = mode; mt_start(&s_state, seed);
        for (unsigned i = 0; i < 10; i++) {
            render(); check();
            s_state.selected = seed % 3; mt_answer(&s_state); render(); check(); mt_next(&s_state);
        }
        render(); check();
    }
    /* Explicit largest valid equations, scores, missing battery and failed keys. */
    mt_start(&s_state, 11); s_state.questions[0] = (mt_question_t){100, 99, 1, 1, {0,1,11}};
    render(); snap("wide-equation"); mt_answer(&s_state); render(); check();
    s_state.questions[0] = (mt_question_t){100, 0, 0, 100, {99,100,90}}; render(); check();
    mt_home(&s_state); render(); soc = -1; battery(NULL); assert(strcmp(lv_label_get_text(s_battery), "--%") == 0); check();
    soc = 150; battery(NULL); assert(strcmp(lv_label_get_text(s_battery), "100%") == 0); check();
    for (unsigned i = 0; i < 50; i++) {
        math_train_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        event(BSP_BTN_OK, BSP_BTN_CLICK); math_train_enter(true); advance(220); assert(s_state.page == MT_HOME); check();
    }
    math_train_exit(); math_train_enter(false); key(BSP_BTN_OK); assert(s_state.page == MT_HOME); snap("no-buttons");
    math_train_exit(); puts("Math Train actual LVGL: glyphs, bounds, overlap, input, idle, review and lifecycle PASS");
    return 0;
}
