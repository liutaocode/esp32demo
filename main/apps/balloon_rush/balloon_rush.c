#include "balloon_rush.h"
#include "balloon_rush_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>

LV_FONT_DECLARE(balloon_rush_zh_16);
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_control;
static uint8_t s_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static br_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery, *s_placeholder;
static lv_obj_t *s_needle, *s_time, *s_balloon;
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_last, s_activity;
static bool s_buttons, s_dimmed, s_off, s_wake_press;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}
static lv_obj_t *label(lv_obj_t *p, const char *text, int y, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(p, text, &balloon_rush_zh_16, color);
    lv_obj_set_pos(o, 0, y); lv_obj_set_width(o, 198);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}
static lv_obj_t *balloon(int y, unsigned round, bool burst)
{
    int w = 54 + (int)round * 4, h = 45 + (int)round * 3;
    int x = (198 - w) / 2;
    if (burst) {
        for (int i = 0; i < 8; i++)
            box(s_content, 37 + (i % 4) * 39, y + (i / 4) * 39, 8, 6,
                i % 2 ? UI_ORANGE : UI_RED);
        ui_pixel_mascot_create(s_content, 80, y + 3);
        return NULL;
    }
    box(s_content, 98, y + h, 2, 13, UI_INK);
    lv_obj_t *b = box(s_content, x, y, w, h, 0xF58D98);
    lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(b, 3, 0);
    lv_obj_set_style_border_color(b, lv_color_hex(UI_INK), 0);
    box(b, 9, 7, 8, 5, 0xFFE7DA);
    box(b, w / 2 - 12, h / 2 - 3, 4, 6, UI_INK);
    box(b, w / 2 + 3, h / 2 - 3, 4, 6, UI_INK);
    box(b, w / 2 - 5, h / 2 + 8, 7, 3, UI_INK);
    return b;
}
static void render(void)
{
    s_needle = s_time = s_balloon = NULL;
    lv_obj_clean(s_content);
    if (s_state.page == BR_HOME) {
        label(s_content, "再冲一次，还是收下？", 0, UI_INK);
        balloon(28, 4, false);
        label(s_content, "指针进绿区，按确定", 108, UI_INK);
        label(s_content, "越吹越赚，失手归零", 134, UI_RED);
        lv_obj_t *best = label(s_content, "", 166, UI_SKY_DARK);
        lv_label_set_text_fmt(best, "开机最高 %lu 分", (unsigned long)s_state.best);
        lv_label_set_text(s_footer, "确定开始 · 共八轮");
    } else if (s_state.page == BR_AIM) {
        lv_obj_t *round = label(s_content, "", 0, UI_INK);
        lv_label_set_text_fmt(round, "第 %u 轮 · 待收 %lu", s_state.round,
                             (unsigned long)s_state.pot);
        s_balloon = balloon(27, s_state.round, false);
        lv_obj_t *reward = label(s_content, "", 113, UI_SKY_DARK);
        lv_label_set_text_fmt(reward, "这次成功加 %lu 分", (unsigned long)br_reward(&s_state));
        box(s_content, 7, 146, 184, 22, UI_INK);
        box(s_content, 9, 148, 180, 18, 0xF5C1AB);
        int left = (s_state.target - s_state.half_width) * 180 / 1000;
        int right = (s_state.target + s_state.half_width) * 180 / 1000;
        box(s_content, 9 + left, 148, right - left + 1, 18, UI_GRASS);
        box(s_content, 9 + s_state.target * 180 / 1000 - 2, 148, 5, 18, UI_YELLOW);
        s_needle = box(s_content, 8, 141, 3, 32, UI_INK);
        s_time = label(s_content, "", 183, UI_INK);
        lv_label_set_text(s_footer, "确定停针 · 上键暂停");
    } else if (s_state.page == BR_CHOICE) {
        label(s_content, s_state.perfect ? "正中靶心！奖励加半" : "稳稳接住！", 0, UI_GRASS_DARK);
        balloon(28, s_state.round, false);
        lv_obj_t *pot = label(s_content, "", 112, UI_INK);
        lv_label_set_text_fmt(pot, "现在收下 %lu 分", (unsigned long)s_state.pot);
        lv_obj_t *next = label(s_content, "", 140, UI_SKY_DARK);
        unsigned next_round = s_state.round + 1;
        lv_label_set_text_fmt(next, "再冲可加 %u 分", 100 * next_round * next_round);
        label(s_content, "失手，本轮积分归零", 173, UI_RED);
        lv_label_set_text(s_footer, "确定再冲 · 下键收下");
    } else if (s_state.page == BR_PAUSED) {
        label(s_content, "歇一歇，气球等你", 7, UI_INK);
        balloon(39, s_state.round, false);
        label(s_content, "上键继续挑战", 127, UI_SKY_DARK);
        label(s_content, "下键放弃回首页", 157, UI_INK);
        lv_label_set_text(s_footer, "已暂停 · 积分尚未收下");
    } else {
        const char *headline = s_state.burst ? "嘭！这次没收住" :
            s_state.clears == BR_ROUNDS ? "八轮全过！太稳了" : "见好就收，落袋为安";
        label(s_content, headline, 0, s_state.burst ? UI_RED : UI_GRASS_DARK);
        balloon(29, s_state.round, s_state.burst);
        lv_obj_t *score = label(s_content, "", 112, UI_INK);
        lv_label_set_text_fmt(score, "收下 %lu 分", (unsigned long)s_state.score);
        lv_obj_t *stats = label(s_content, "", 140, UI_SKY_DARK);
        lv_label_set_text_fmt(stats, "成功 %u 轮 · 精准 %u 次", s_state.clears, s_state.perfects);
        label(s_content, s_state.new_best ? "新纪录！换朋友试试" :
              s_state.burst ? "就差一点，再试一次" : "稳稳的，也是一种赢", 173, UI_INK);
        lv_label_set_text(s_footer, "确定再来 · 下键首页");
    }
    if (!s_buttons) lv_label_set_text(s_footer, "按键不可用，请重启");
}
static void battery(lv_timer_t *t)
{
    (void)t;
    if (!s_battery || s_state.page == BR_AIM) return;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}
static bool handle(input_t in, int64_t now)
{
    if (in.event == BSP_BTN_LONG && s_wake_press) return false;
    if (in.event == BSP_BTN_PRESS) s_wake_press = false;
    s_activity = now;
    bool wake = s_dimmed;
    if (wake) bsp_display_backlight(100);
    s_dimmed = s_off = false;
    if (wake) { s_wake_press = true; return false; }
    if (in.event == BSP_BTN_LONG) { br_home(&s_state); return true; }
    if (s_state.page == BR_HOME || s_state.page == BR_RESULT) {
        if (in.button == BSP_BTN_OK) br_start(&s_state, esp_random());
        else if (in.button == BSP_BTN_DOWN) br_home(&s_state);
        else return false;
    } else if (in.button == BSP_BTN_UP) br_pause(&s_state);
    else if (s_state.page == BR_PAUSED && in.button == BSP_BTN_DOWN) br_home(&s_state);
    else if (s_state.page == BR_AIM && in.button == BSP_BTN_OK) return br_stop(&s_state);
    else if (s_state.page == BR_CHOICE && in.button == BSP_BTN_OK) return br_continue(&s_state);
    else if (s_state.page == BR_CHOICE && in.button == BSP_BTN_DOWN) return br_collect(&s_state);
    else return false;
    return true;
}
static void tick(lv_timer_t *t)
{
    (void)t;
    int64_t now = now_ms();
    uint32_t ms = (uint32_t)(now - s_last);
    s_last = now;
    input_t in;
    bool handled = false, dirty = false;
    /* One press per frame, scored against the last displayed needle. */
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &in, 0) == pdTRUE; i++) {
        if (!handled && now >= in.at && now - in.at <= 200) {
            dirty = handle(in, now);
            handled = true;
        }
    }
    br_page_t old = s_state.page;
    if (!dirty) br_tick(&s_state, ms);
    dirty |= old != s_state.page;
    int64_t idle = now - s_activity;
    if (!s_dimmed && idle >= 60000) {
        if (s_state.page == BR_AIM || s_state.page == BR_CHOICE) {
            br_pause(&s_state); dirty = true;
        }
        bsp_display_backlight(20); s_dimmed = true;
    }
    if (!s_off && idle >= 180000) { bsp_display_backlight(0); s_off = true; }
    if (dirty) render();
    if (s_needle) {
        lv_obj_set_x(s_needle, 8 + s_state.needle * 180 / 1000);
        lv_label_set_text_fmt(s_time, "剩余 %lu 秒 · 绿区才安全",
                             (unsigned long)((br_remaining(&s_state) + 999) / 1000));
    }
    if (s_balloon) lv_obj_set_y(s_balloon, 27 + (s_state.elapsed / 180 % 3));
}
void balloon_rush_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t), s_storage, &s_control);
}
void balloon_rush_key(bsp_btn_t b, bsp_btn_ev_t e)
{
    if (!atomic_load(&s_accept)) return;
    if (e != BSP_BTN_PRESS && !(b == BSP_BTN_OK && e == BSP_BTN_LONG)) return;
    input_t in = { b, e, now_ms() };
    (void)xQueueSend(s_queue, &in, 0);
}
void balloon_rush_enter(bool buttons_available)
{
    if (s_screen) return;
    balloon_rush_prepare();
    br_home(&s_state);
    s_buttons = buttons_available;
    s_dimmed = s_off = s_wake_press = false;
    s_last = s_activity = now_ms();
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = label(s_screen, "见好就收", 15, 0xFFFFFF);
    lv_obj_set_x(title, 5); lv_obj_set_width(title, 151);
    s_battery = ui_pixel_label(s_screen, "--%", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_battery, 163, 31); lv_obj_set_width(s_battery, 65);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 220, 226, UI_PAPER);
    s_footer = label(s_screen, "", 294, UI_INK);
    lv_obj_set_x(s_footer, 4); lv_obj_set_width(s_footer, 232);
    render(); battery(NULL);
    s_timer = lv_timer_create(tick, 20, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen);
    if (s_placeholder) { lv_obj_delete(s_placeholder); s_placeholder = NULL; }
    bsp_display_backlight(100);
    xQueueReset(s_queue);
    atomic_store(&s_accept, buttons_available);
}
void balloon_rush_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    if (s_screen && lv_screen_active() == s_screen) {
        s_placeholder = lv_obj_create(NULL);
        lv_screen_load(s_placeholder);
    }
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = s_content = s_footer = s_battery = NULL;
    s_needle = s_time = s_balloon = NULL;
}
