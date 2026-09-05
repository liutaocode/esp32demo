/* Execute the production LVGL page with simulated board I/O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/pocket_pond/pocket_pond.c"
#include "src/misc/lv_text_private.h"

static int64_t fake_us;
static int backlight, soc = 87;
static bool callback_active;
static unsigned storage_status, saves;
static pp_progress_t persisted;
uint32_t esp_random(void) { return 12345; }
pp_progress_t pp_storage_init(void) { assert(!callback_active); return persisted; }
void pp_storage_save(pp_progress_t p) { assert(!callback_active); persisted = p; saves++; }
unsigned pp_storage_status(void) { return storage_status; }
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
    /* Text rows must not overlap each other, even when all fit the panel. */
    if (o == s_content) for (unsigned i = 0; i < lv_obj_get_child_count(o); i++) {
        lv_obj_t *a = lv_obj_get_child(o, i);
        if (!lv_obj_check_type(a, &lv_label_class)) continue;
        lv_area_t aa; lv_obj_get_coords(a, &aa);
        for (unsigned j = i + 1; j < lv_obj_get_child_count(o); j++) {
            lv_obj_t *b = lv_obj_get_child(o, j);
            if (!lv_obj_check_type(b, &lv_label_class)) continue;
            lv_area_t bb; lv_obj_get_coords(b, &bb);
            assert(aa.y2 < bb.y1 || bb.y2 < aa.y1 || aa.x2 < bb.x1 || bb.x2 < aa.x1);
        }
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
    callback_active = true; pocket_pond_key(b, ev); callback_active = false;
}
static void key(bsp_btn_t b) { post(b, BSP_BTN_CLICK); advance(300); }
static void ordered(void)
{
    pp_start(&s_state, 1); s_view = ROUND;
    for (unsigned i = 0; i < PP_CARDS; i++) s_state.deck[i] = i < PP_FISH ? i : PP_FISH;
    render();
}
int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    pocket_pond_prepare(); pocket_pond_enter(true); snap("home");
    key(BSP_BTN_OK); assert(s_state.page == PP_PLAY); snap("play");
    ordered();
    for (unsigned i = 0; i < PP_FISH; i++) {
        key(BSP_BTN_OK); check();
        if (i == 4) snap("new-fish");
    }
    assert(s_state.page == PP_RESULT && s_state.score == 29 && saves == 1); snap("record");
    key(BSP_BTN_UP); assert(s_view == ALBUM);
    for (unsigned i = 0; i < PP_FISH; i++) {
        for (unsigned count = 0; count <= 999; count += count < 20 ? 1 : 979) {
            s_state.progress.caught[i] = count; s_fish = i; render(); check();
        }
    }
    s_state.progress.caught[8] = 20; render(); snap("album");
    for (unsigned st = 0; st < 3; st++) { storage_status = st; advance(300); check(); }
    snap("storage-failed"); storage_status = 0;
    key(BSP_BTN_OK); assert(s_view == ROUND);
    ordered(); key(BSP_BTN_OK); key(BSP_BTN_DOWN); assert(s_state.page == PP_RESULT); snap("banked");
    ordered(); s_state.deck[1] = s_state.deck[2] = PP_FISH;
    key(BSP_BTN_OK); key(BSP_BTN_OK); assert(s_state.waves == 1); snap("wave");
    pp_state_t held = s_state;
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(300); assert(s_view == HOME);
    assert(memcmp(&held,&s_state,sizeof(held))==0);
    key(BSP_BTN_OK); assert(s_view==ROUND && s_state.cursor == held.cursor);
    advance(30000); assert(backlight == 20);
    advance(60000); assert(backlight == 0);
    assert(memcmp(&held,&s_state,sizeof(held))==0);
    key(BSP_BTN_OK); assert(backlight == 100 && s_state.cursor == held.cursor);
    key(BSP_BTN_OK); assert(s_state.lost); snap("lost");
    ordered();
    post(BSP_BTN_OK,BSP_BTN_PRESS); advance(300); assert(s_state.cursor == 0);
    post(BSP_BTN_OK,BSP_BTN_DOUBLE); advance(300); assert(s_state.cursor == 1);
    for(unsigned i=0;i<8;i++) post(BSP_BTN_OK,BSP_BTN_CLICK);
    advance(300); assert(s_state.cursor==2);
    post(BSP_BTN_OK,BSP_BTN_CLICK); advance(30); assert(s_state.cursor==2);
    post(BSP_BTN_OK,BSP_BTN_CLICK); advance(1000); assert(s_state.cursor==2);
    for(unsigned fish=0;fish<PP_FISH;fish++) {
        ordered(); s_state.last=fish; s_state.score=29; s_state.cursor=10; s_state.waves=1;
        render(); check();
    }
    soc=-1; battery(NULL); check(); assert(strcmp(lv_label_get_text(s_battery),"--%") == 0);
    soc=100; battery(NULL); check();
    pocket_pond_exit(); post(BSP_BTN_OK,BSP_BTN_CLICK);
    assert(!s_screen && !s_timer && !s_battery_timer);
    pocket_pond_enter(false); snap("buttons-failed");
    pocket_pond_exit(); lv_deinit();
    puts("Pocket Pond production LVGL: Chinese glyphs, bounds, lifecycle, input, persistence states PASS");
}
