/* Actual firmware UI and LVGL, with simulated clock, buttons and battery. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/down_100/down_100.c"

static int64_t fake_us;
static int backlight, soc = 87;
static bool callback_active;
static bool audio_active;
static unsigned audio_count[D100_SOUND_COUNT];
void d100_audio_prepare(void) { assert(!callback_active); }
void d100_audio_active(bool active) { assert(!callback_active); audio_active = active; }
void d100_audio_play(d100_sound_t sound)
{
    assert(!callback_active);
    if (sound != D100_SOUND_NONE) { assert(audio_active); audio_count[sound]++; }
}
int64_t esp_timer_get_time(void) { return fake_us; }
uint32_t esp_random(void) { return 2026; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t v) { assert(!callback_active); backlight = v; }

static void bounds(lv_obj_t *o)
{
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    lv_area_t a; lv_obj_get_coords(o, &a);
    assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
    if (lv_obj_check_type(o, &lv_label_class)) {
        if (s_state.page == D100_PLAY) assert(o == s_stats || o == s_battery);
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

static void check(void)
{
    lv_obj_update_layout(s_screen); bounds(s_screen);
    if (s_state.page == D100_PLAY) {
        lv_area_t area; lv_obj_get_coords(s_field, &area);
        assert(area.x1 == 0 && area.y1 == 26 && area.x2 == 239 && area.y2 == 319);
        assert(lv_obj_has_flag(s_footer, LV_OBJ_FLAG_HIDDEN));
        assert(lv_obj_has_flag(s_content, LV_OBJ_FLAG_HIDDEN));
        /* Active play has only the HUD, battery and geometric tile artwork. */
        assert(lv_obj_get_child_count(s_game) == 3);
        for (int i = 0; i < D100_ROWS; i++) for (int lane = 0; lane < 3; lane++)
            assert(!lv_obj_check_type(s_art[i][lane], &lv_label_class));
    }
}
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
    callback_active = true; down_100_key(b, ev); callback_active = false;
}
static void key(bsp_btn_t b) { event(b, BSP_BTN_PRESS); advance(20); }

int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    down_100_prepare(); down_100_enter(true); snap("home");
    event(BSP_BTN_OK, BSP_BTN_CLICK); event(BSP_BTN_OK, BSP_BTN_LONG); event(BSP_BTN_OK, BSP_BTN_DOUBLE);
    advance(20); assert(s_state.page == D100_HOME);
    key(BSP_BTN_UP); assert(s_state.mode == 1); snap("extreme");
    key(BSP_BTN_UP); key(BSP_BTN_DOWN); assert(s_state.page == D100_HELP); snap("help");
    key(BSP_BTN_UP); key(BSP_BTN_OK); assert(s_state.page == D100_PLAY); snap("play");
    key(BSP_BTN_DOWN); assert(s_state.lane == 2 && s_state.support == -1);
    for (int i = 0; i < 10; i++) { advance(20); check(); }
    snap("fall");
    key(BSP_BTN_OK); assert(s_state.page == D100_PAUSE); snap("pause");
    uint32_t active = s_state.active_ms; advance(2000); assert(s_state.active_ms == active);
    key(BSP_BTN_OK); assert(s_state.page == D100_PLAY);
    /* Real-device redraws and serial captures can take hundreds of ms.
       Resume must keep playing, with bounded physics rather than forced pause. */
    const unsigned delays[] = {201, 220, 500, 1500, 300};
    for (unsigned i = 0; i < sizeof(delays) / sizeof(delays[0]); i++) {
        uint32_t old_ms = s_state.active_ms;
        advance(delays[i]);
        assert(s_state.page == D100_PLAY);
        assert(s_state.active_ms - old_ms == 100);
        check();
    }
    key(BSP_BTN_OK); assert(s_state.page == D100_PAUSE);
    key(BSP_BTN_UP); assert(s_state.floor == 0 && s_state.page == D100_PLAY);
    event(BSP_BTN_DOWN, BSP_BTN_PRESS); advance(201);
    assert(s_state.lane == 1 && s_state.page == D100_PLAY); /* Stale input discarded. */
    for (int i = 0; i < 8; i++) event(BSP_BTN_UP, BSP_BTN_PRESS);
    advance(20); assert(s_state.lane == 0); advance(20); assert(s_state.lane == 0);
    advance(60000); assert(s_state.page == D100_PAUSE && backlight == 20);
    advance(120000); assert(backlight == 0);
    key(BSP_BTN_OK); assert(backlight == 100 && s_state.page == D100_PAUSE);
    key(BSP_BTN_DOWN); assert(s_state.page == D100_HOME);
    key(BSP_BTN_OK);
    /* Real UI runs a complete course: all frames checked for glyphs and bounds. */
    unsigned frames = 0;
    while (s_state.page == D100_PLAY && frames++ < 20000) {
        if (s_state.support >= 0) {
            d100_row_t *r = &s_state.rows[s_state.support];
            int hole = -1;
            for (int lane = 0; lane < 3; lane++) if (r->tile[lane] == D100_HOLE) hole = lane;
            assert(hole >= 0); key(hole > s_state.lane ? BSP_BTN_DOWN : BSP_BTN_UP);
        } else if (s_state.lane != 1) key(s_state.lane < 1 ? BSP_BTN_DOWN : BSP_BTN_UP);
        else advance(20);
        check();
        if (s_state.floor == 12 && s_state.support >= 0) snap("deep");
        if (s_state.floor == 30 && s_state.support >= 0) snap("supply");
        if (s_state.floor % 20 == 16 && s_state.support >= 0) {
            char name[24]; snprintf(name, sizeof(name), "stage-%u", d100_stage(s_state.floor) + 1);
            snap(name);
        }
    }
    assert(s_state.page == D100_RESULT && s_state.floor == 100); snap("victory");
    assert(audio_count[D100_SOUND_START] && audio_count[D100_SOUND_GEM] && audio_count[D100_SOUND_CLEAR]);
    uint16_t challenge = s_state.challenge;
    key(BSP_BTN_OK); assert(s_state.challenge == challenge && s_state.floor == 0);
    /* Stationary player loses to the moving ceiling. */
    for (int i = 0; i < 160; i++) advance(20);
    assert(s_state.page == D100_RESULT && s_state.notice == D100_CEILING); snap("result");
    key(BSP_BTN_UP); assert(s_state.challenge != challenge);
    s_state.rows[0].tile[2] = D100_SPIKE; key(BSP_BTN_DOWN);
    assert(s_state.health == 2); snap("hurt");
    key(BSP_BTN_OK); key(BSP_BTN_UP);
    s_state.rows[0].tile[2] = D100_CRACK; key(BSP_BTN_DOWN); snap("crack");
    key(BSP_BTN_OK); key(BSP_BTN_UP);
    s_state.rows[0].tile[2] = D100_GEM; key(BSP_BTN_DOWN); snap("gem");
    key(BSP_BTN_OK); key(BSP_BTN_UP);
    s_state.rows[0].tile[1] = D100_HEAL;
    s_state.rows[0].tile[2] = D100_HEAL;
    s_state.health = 1; live_update(); snap("two-supplies");
    uint16_t untouched = s_art_style[0][1];
    key(BSP_BTN_DOWN); assert(s_state.health == 2);
    assert(audio_count[D100_SOUND_HEAL] && audio_count[D100_SOUND_HURT] && audio_count[D100_SOUND_LOSE]);
    assert(s_art_style[0][1] == untouched && s_art_style[0][2] != untouched);
    snap("one-supply-left");
    key(BSP_BTN_UP); assert(s_state.health == 3); snap("supplies-collected");
    key(BSP_BTN_OK); key(BSP_BTN_UP);
    s_state.rows[0].tile[1] = D100_GEM;
    s_state.rows[0].tile[2] = D100_GEM;
    live_update(); untouched = s_art_style[0][1];
    key(BSP_BTN_DOWN); assert(s_state.gems == 1 && s_art_style[0][1] == untouched);
    snap("one-gem-left"); key(BSP_BTN_UP); assert(s_state.gems == 2);
    key(BSP_BTN_OK); key(BSP_BTN_UP);
    s_state.rows[0].tile[0] = D100_GEM;
    s_state.rows[0].tile[2] = D100_SPIKE;
    s_state.rows[1].tile[0] = D100_HEAL;
    s_state.rows[1].tile[2] = D100_CRACK;
    live_update(); snap("items");
    /* Numeric stress covers text overflow at maximum possible public values. */
    s_state.score = 9999; s_state.floor = 100; s_state.gems = 200;
    s_state.max_combo = 100; s_state.challenge = 9999; live_update(); check();
    s_state.page = D100_RESULT; render(); check();
    key(BSP_BTN_DOWN); soc = -1; battery(NULL); snap("battery-unavailable");
    for (int i = 0; i < 60; i++) {
        down_100_exit(); assert(!s_screen && !s_timer && !s_battery_timer);
        event(BSP_BTN_OK, BSP_BTN_PRESS); down_100_enter(true); advance(20);
        assert(s_state.page == D100_HOME); check();
    }
    down_100_exit(); down_100_enter(false); key(BSP_BTN_OK);
    assert(s_state.page == D100_HOME); snap("no-buttons");
    down_100_exit(); assert(!audio_active);
    puts("Down 100 UI: full course, glyphs, layout, input, pause, audio dispatch, teardown PASS");
}
