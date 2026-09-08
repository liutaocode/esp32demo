/* Execute the production LVGL page with simulated board I/O. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "apps/jelly_squeeze/jelly_squeeze.c"
#include "src/misc/lv_text_private.h"

static int64_t fake_us;
static int backlight, soc = 68;
static bool callback_active;
static uint32_t random_value = 4242;
static js_progress_t written;
static unsigned writes;

int64_t esp_timer_get_time(void) { return fake_us; }
uint32_t esp_random(void) { return random_value; }
int bsp_battery_soc(void) { assert(!callback_active); return soc; }
void bsp_display_backlight(uint8_t value) { assert(!callback_active); backlight = value; }
void js_storage_save(js_progress_t progress) { written = progress; writes++; }

/* 闸门是唯一故意越出父容器的对象：它从赛道上方滑进来，由 LVGL 裁剪。
 * 单独核对它的横向对齐、底边不越过地面，以及每块钢板都在闸门自己的范围内。 */
static void gate_bounds(void)
{
    lv_area_t gate, arena;
    lv_obj_get_coords(s_gate, &gate);
    lv_obj_get_coords(s_arena, &arena);
    assert(gate.x1 == arena.x1 && gate.x2 == arena.x2);
    assert(gate.y2 - arena.y1 <= JS_GROUND && gate.y1 >= arena.y1 - JS_GATE_H);
    for (unsigned i = 0; i < lv_obj_get_child_count(s_gate); i++) {
        lv_obj_t *part = lv_obj_get_child(s_gate, i);
        assert(lv_obj_get_x(part) >= 0 && lv_obj_get_y(part) >= 0);
        assert(lv_obj_get_x(part) + lv_obj_get_width(part) <= JS_FIELD_W);
        assert(lv_obj_get_y(part) + lv_obj_get_height(part) <= JS_GATE_H);
    }
}

static void bounds(lv_obj_t *o)
{
    if (o == s_gate) { gate_bounds(); return; }
    lv_area_t a; lv_obj_get_coords(o, &a);
    assert(a.x1 >= 0 && a.y1 >= 0 && a.x2 < 240 && a.y2 < 320);
    if (lv_obj_check_type(o, &lv_label_class)) {
        const char *str = lv_label_get_text(o);
        const lv_font_t *font = lv_obj_get_style_text_font(o, 0);
        lv_point_t size;
        lv_text_get_size(&size, str, font, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        if (size.x > lv_obj_get_width(o))
            fprintf(stderr, "Text overflow: %s (%d > %d)\n", str, (int)size.x, (int)lv_obj_get_width(o));
        assert(size.x <= lv_obj_get_width(o));
        uint32_t i = 0;
        while (str[i]) {
            uint32_t cp = lv_text_encoded_next(str, &i);
            if (cp == '\n') continue;
            lv_font_glyph_dsc_t d;
            assert(lv_font_get_glyph_dsc(font, &d, cp, 0) && !d.is_placeholder);
        }
        assert(lv_obj_get_scroll_bottom(o) <= 0);
        if (lv_obj_get_parent(o) == s_content) {
            lv_area_t panel; lv_obj_get_content_coords(s_content, &panel);
            if (a.y2 > panel.y2)
                fprintf(stderr, "Panel clips %s at %d > %d\n", str, (int)a.y2, (int)panel.y2);
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
    callback_active = true; jelly_squeeze_key(b, ev); callback_active = false;
}
static void key(bsp_btn_t b)
{
    post(b, b == BSP_BTN_OK ? BSP_BTN_CLICK : BSP_BTN_PRESS);
    advance(30);
}

/* 会读洞口的玩家：把目标档位挪到本关要求的体型，然后等果冻停稳。 */
static void aim(void)
{
    int32_t want = ((int32_t)s_state.shape_index - 2) * JS_ANCHOR;
    if (s_state.page == JS_PLAY && s_state.target != want)
        key(s_state.target < want ? BSP_BTN_UP : BSP_BTN_DOWN);
    else
        advance(30);
}

/* 果冻的画面轮廓必须和判定用的宽高一致，否则玩家看到的和判到的会对不上。 */
static void assert_silhouette(void)
{
    if (!s_jelly || s_state.page == JS_FAIL) return;   /* 卡住时故意画得更扁，见 jelly_layout */
    lv_obj_update_layout(s_screen);
    js_size_t body = js_body(&s_state);
    assert(lv_obj_get_width(s_jelly) == body.w && lv_obj_get_height(s_jelly) == body.h);
    assert(lv_obj_get_x(s_jelly) == JS_FIELD_W / 2 - body.w / 2);
    assert(lv_obj_get_y(s_jelly) == JS_GROUND - body.h);
}

int main(void)
{
    lv_init(); assert(lv_display_create(240, 320));
    js_progress_t saved = {0, 0, 0};
    jelly_squeeze_prepare(); jelly_squeeze_enter(true, saved);
    assert(s_state.page == JS_HOME && s_state.unlocked == 1);
    snap("home");

    /* 只解锁一只时上键不换口味，下键始终切难度。 */
    key(BSP_BTN_UP); assert(s_state.flavor == 0);
    key(BSP_BTN_DOWN); assert(s_state.mode == 1); snap("home-dash");
    key(BSP_BTN_DOWN); assert(s_state.mode == 0);

    /* 解锁全部口味后逐个渲染首页，检查配色与解锁提示的排版。 */
    js_progress_t rich = {400, 0, 0};
    jelly_squeeze_exit(); jelly_squeeze_enter(true, rich);
    assert(s_state.unlocked == JS_FLAVORS);
    for (unsigned i = 0; i < JS_FLAVORS; i++) { check(); key(BSP_BTN_UP); }
    assert(s_state.flavor == 0);
    snap("home-flavors");
    jelly_squeeze_exit();
    js_progress_t middle = {60, 120, 90};
    jelly_squeeze_enter(true, middle);
    assert(s_state.unlocked == 4 && s_state.best[0] == 120 && s_state.best[1] == 90);
    key(BSP_BTN_UP); assert(s_state.flavor == 1); check();

    /* 单击才算确定，按下和双击不会重复开局。 */
    post(BSP_BTN_OK, BSP_BTN_PRESS); post(BSP_BTN_OK, BSP_BTN_DOUBLE); advance(30);
    assert(s_state.page == JS_HOME);
    key(BSP_BTN_OK); assert(s_state.page == JS_PLAY && s_state.lives == 3 && s_state.saves == 3);
    for (unsigned i = 0; i < 6; i++) { aim(); assert_silhouette(); }
    snap("play");

    /* 长按确定暂停，暂停期间状态完全冻结。 */
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(30);
    assert(s_state.page == JS_PAUSED);
    js_state_t frozen = s_state; advance(3000);
    assert(memcmp(&frozen, &s_state, sizeof(frozen)) == 0);
    snap("paused");
    /* 长按抬手后补来的单击不再当成一次稳住。 */
    post(BSP_BTN_OK, BSP_BTN_CLICK); advance(30);
    assert(s_state.page == JS_PAUSED && s_state.saves == 3);
    key(BSP_BTN_UP); assert(s_state.page == JS_PLAY);

    /* 稳住会立刻止住晃动，用完就不再消耗。 */
    key(BSP_BTN_UP);
    key(BSP_BTN_OK); assert(s_state.saves == 2 && s_state.vel == 0 && s_state.shape == s_state.target);
    key(BSP_BTN_OK); key(BSP_BTN_OK); assert(s_state.saves == 0);
    key(BSP_BTN_OK); assert(s_state.saves == 0);

    /* 认真玩一局：每帧都验证排版，顺带截下过门和卡住两种反馈。 */
    js_start(&s_state, 88); s_state.mode = 0; js_start(&s_state, 88); render();
    bool shot_pass = false, shot_fail = false;
    unsigned guard = 0;
    while (s_state.page != JS_RESULT && guard++ < 4000) {
        aim();
        assert_silhouette();
        if (s_state.page == JS_PASS && !shot_pass) { shot_pass = true; snap("pass"); }
        if (s_state.page == JS_FAIL && !shot_fail) { shot_fail = true; snap("fail"); }
        check();
    }
    assert(shot_pass && s_state.cleared > 0 && s_state.perfects > 0);

    /* 一局不动手：一定会撞门，把三条命耗光并走到结算。 */
    js_start(&s_state, 5); render();
    guard = 0;
    while (s_state.page != JS_RESULT && guard++ < 4000) {
        advance(30);
        if (s_state.page == JS_FAIL && !shot_fail) { shot_fail = true; snap("fail"); }
        check();
    }
    assert(shot_fail && s_state.page == JS_RESULT && s_state.lives == 0);
    snap("result");

    /* 结算三个按键：同题、换题、回首页。 */
    unsigned challenge = s_state.challenge;
    key(BSP_BTN_OK); assert(s_state.page == JS_PLAY && s_state.challenge == challenge);
    js_start(&s_state, challenge); s_state.page = JS_RESULT; render();
    key(BSP_BTN_UP); assert(s_state.challenge == (challenge + 1) % 10000 && s_state.page == JS_PLAY);
    s_state.page = JS_RESULT; render();
    key(BSP_BTN_DOWN); assert(s_state.page == JS_HOME);

    /* 高分与解锁写进存档，重复相同进度不再重写。 */
    unsigned before = writes;
    js_start(&s_state, 3); s_state.score = 999; s_state.cleared = 20;
    s_state.total_cleared = 300; s_state.page = JS_RESULT; s_state.best[0] = 999;
    advance(30);
    assert(writes == before + 1 && written.total_cleared == 300 && written.best_relaxed == 999);
    advance(30); assert(writes == before + 1);
    js_home(&s_state); render();

    /* 五种洞口 × 两种难度 × 全部形变档位：闸门与果冻的排版都要成立。 */
    for (unsigned mode = 0; mode < 2; mode++)
        for (unsigned shape = 0; shape < JS_SHAPES; shape++) {
            js_start(&s_state, 11u + shape);
            s_state.mode = (uint8_t)mode;
            s_state.shape_index = (uint8_t)shape;
            s_state.lives = mode ? 1 : 3;
            render();
            for (int32_t x = -JS_SHAPE_LIMIT; x <= JS_SHAPE_LIMIT; x += 250) {
                s_state.shape = x; s_state.target = x;
                for (unsigned phase = 0; phase <= 4; phase++) {
                    s_state.gate_ms = s_state.gate_span * phase / 4;
                    animate(); check(); assert_silhouette();
                }
            }
            /* 过门与卡住两种反馈页也要能画。 */
            for (unsigned outcome = 0; outcome < 2; outcome++) {
                s_state.page = outcome ? JS_FAIL : JS_PASS;
                s_state.combo = (uint8_t)(outcome ? 0 : 4);
                s_state.gate_ms = s_state.gate_span;
                render(); check();
            }
            s_state.page = JS_PLAY;
        }

    /* 关卡上限、分数和连击的极值排版。 */
    js_start(&s_state, 1);
    s_state.level = JS_MAX_LEVEL - 1; s_state.score = 99999;
    s_state.combo = s_state.best_combo = 255; s_state.cleared = 999; s_state.perfects = 999;
    render(); check();
    s_state.page = JS_RESULT; s_state.new_best = true; render(); check();
    snap("result-best");

    /* 电量不可用时只显示占位。 */
    soc = -1; js_home(&s_state); render(); battery(NULL); check();
    soc = 140; battery(NULL); check();
    soc = 68; battery(NULL);

    /* 过期与堆满的输入队列都不能触发操作。 */
    js_home(&s_state); render();
    post(BSP_BTN_OK, BSP_BTN_CLICK);
    fake_us += 400 * 1000; lv_tick_inc(400); frame(s_timer);
    assert(s_state.page == JS_HOME);
    for (unsigned i = 0; i < 12; i++) post(BSP_BTN_DOWN, BSP_BTN_PRESS);
    advance(30); assert(s_state.page == JS_HOME); check();

    /* 变暗、熄屏与唤醒：变暗后的第一次按键只唤醒。 */
    jelly_squeeze_exit(); jelly_squeeze_enter(true, middle);
    advance(30); assert(backlight == 100);
    s_last_activity -= 61000; advance(30); assert(backlight == 20);
    s_last_activity -= 181000; advance(30); assert(backlight == 0);
    key(BSP_BTN_OK); assert(backlight == 100 && s_state.page == JS_HOME);
    post(BSP_BTN_OK, BSP_BTN_LONG); advance(30); assert(s_state.page == JS_HOME);
    /* 计时页闲置一分钟自动暂停。 */
    key(BSP_BTN_OK); assert(s_state.page == JS_PLAY);
    s_last_activity -= 61000; advance(30);
    assert(s_state.page == JS_PAUSED && backlight == 20);

    /* 按键不可用时页面仍要画得出来，并且不接受输入。 */
    jelly_squeeze_exit(); jelly_squeeze_enter(false, saved);
    check(); assert(s_state.page == JS_HOME);
    key(BSP_BTN_OK); assert(s_state.page == JS_HOME);
    snap("no-buttons");

    for (unsigned i = 0; i < 50; i++) { jelly_squeeze_exit(); jelly_squeeze_enter(true, middle); }
    check();
    jelly_squeeze_exit();
    printf("Jelly Squeeze preview: PASS\n");
    return 0;
}
