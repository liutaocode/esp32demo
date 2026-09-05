/* Execute the production LVGL page with simulated board I/O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/ricochet_rush/ricochet_rush.c"
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
    callback_active = true; ricochet_rush_key(b, ev); callback_active = false;
}
static void key(bsp_btn_t b) { post(b, BSP_BTN_PRESS); advance(20); }
static void until_aim(void)
{
    for(unsigned i=0;i<1200 && s_state.page!=RR_AIM && s_state.page!=RR_RESULT;i++) { advance(20); if(i%20==0)check(); }
    assert(s_state.page==RR_AIM || s_state.page==RR_RESULT);
}
int main(void)
{
    lv_init(); assert(lv_display_create(240,320));
    ricochet_rush_prepare(); ricochet_rush_enter(true); snap("home");
    key(BSP_BTN_DOWN); assert(s_state.mode==1); snap("expert-home");
    post(BSP_BTN_OK,BSP_BTN_CLICK); post(BSP_BTN_OK,BSP_BTN_DOUBLE); post(BSP_BTN_OK,BSP_BTN_LONG);
    advance(20); assert(s_state.page==RR_HOME);
    key(BSP_BTN_UP); key(BSP_BTN_OK); assert(s_state.page==RR_AIM); snap("aim");
    int direction=s_state.direction; key(BSP_BTN_DOWN); assert(s_state.direction==-direction);
    key(BSP_BTN_UP); assert(s_state.page==RR_PAUSED); snap("pause");
    advance(60000); assert(backlight==20 && s_state.page==RR_PAUSED);
    key(BSP_BTN_UP); assert(backlight==100 && s_state.page==RR_PAUSED);
    key(BSP_BTN_UP); assert(s_state.page==RR_AIM);
    /* Slow on-device refresh/capture must never masquerade as a pause key. */
    for(unsigned i=0;i<20;i++) {
        float angle=s_state.angle;
        advance(i%2 ? 800 : 201);
        assert(s_state.page==RR_AIM && s_state.angle!=angle);
    }
    advance(180000); assert(s_state.page==RR_AIM && backlight==100);
    s_state.angle=20;
    post(BSP_BTN_OK,BSP_BTN_PRESS); advance(400); assert(s_state.page==RR_FLIGHT);
    for(unsigned i=0;i<10;i++) {
        unsigned before=s_state.flight_ms;
        advance(350); assert(s_state.page==RR_FLIGHT && s_state.flight_ms-before==50);
    }
    for(unsigned i=0;i<25;i++)advance(20); snap("flight");
    unsigned launched=s_state.launched;
    for(unsigned i=0;i<8;i++)post(BSP_BTN_OK,BSP_BTN_PRESS);
    advance(20); assert(s_state.launched>=launched && s_state.page==RR_FLIGHT);
    key(BSP_BTN_DOWN); assert(s_state.fast); snap("fast");
    key(BSP_BTN_UP); assert(s_state.page==RR_PAUSED);
    unsigned flight=s_state.flight_ms; advance(100); assert(s_state.flight_ms==flight);
    key(BSP_BTN_UP); until_aim(); snap("round-two");
    /* Production-rendered boundary fixtures, clearly distinct from USB captures. */
    for(unsigned i=0;i<RR_CELLS;i++) { s_state.hp[i]=i%3?24:1; s_state.pickup[i]=false; }
    s_state.hp[15]=0; s_state.pickup[15]=true;
    s_state.round=30; s_state.balls=24; s_state.score=99999;
    s_state.launch_x=8; s_state.angle=65; render(); snap("danger");
    for(int angle=-65;angle<=65;angle++) { s_state.angle=angle; board_update(); check(); }
    s_state.launch_x=190;
    for(int angle=-65;angle<=65;angle++) { s_state.angle=angle; board_update(); check(); }
    s_state.page=RR_SETTLE; s_state.hits=999; s_state.gained=7; s_state.settle_ms=0;
    render(); advance(800); assert(s_state.page==RR_SETTLE && s_state.settle_ms==50); snap("settle");
    s_state.page=RR_RESULT; s_state.cleared=120; s_state.best_hits=999; s_state.best[0]=99999;
    s_state.new_best=true; s_state.won=false; render(); snap("result");
    s_state.won=true; render(); snap("victory");
    for(unsigned r=1;r<=30;r++) { s_state.round=r; s_state.won=false; s_state.new_best=false; render(); check(); }
    uint32_t seed=s_state.seed; key(BSP_BTN_OK); assert(s_state.page==RR_AIM && s_state.seed==seed);
    s_state.page=RR_RESULT; render(); key(BSP_BTN_UP); assert(s_state.seed!=seed);
    key(BSP_BTN_UP); key(BSP_BTN_DOWN); assert(s_state.page==RR_HOME);
    for(soc=-1;soc<=101;soc++) { battery(NULL); check(); }
    soc=-1; battery(NULL); snap("battery-unknown");
    post(BSP_BTN_OK,BSP_BTN_PRESS); advance(1001); assert(s_state.page==RR_HOME);
    advance(180000); assert(backlight==0); key(BSP_BTN_OK); assert(backlight==100 && s_state.page==RR_HOME);
    for(unsigned i=0;i<50;i++) {
        ricochet_rush_exit(); assert(!s_timer && !s_battery_timer && !s_screen);
        post(BSP_BTN_OK,BSP_BTN_PRESS); assert(s_queue->count==0);
        ricochet_rush_enter(true); assert(!s_placeholder && s_state.page==RR_HOME);
        key(BSP_BTN_OK); s_state.balls=24; key(BSP_BTN_OK);
        for(unsigned j=0;j<100;j++)advance(20);
        check(); key(BSP_BTN_UP); assert(s_state.page==RR_PAUSED);
    }
    ricochet_rush_exit(); ricochet_rush_enter(false); key(BSP_BTN_OK);
    assert(s_state.page==RR_HOME); snap("no-buttons"); ricochet_rush_exit();
    puts("Ricochet Rush UI: Chinese glyphs, bounds, aim extremes, input, pause, slow-refresh regression, active idle, sleep, fallbacks, 50 exits PASS");
}
