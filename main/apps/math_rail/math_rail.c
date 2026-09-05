#include "math_rail.h"
#include "math_rail_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>
#include <stdio.h>

LV_FONT_DECLARE(math_rail_zh_16);
LV_FONT_DECLARE(math_rail_zh_26);
#define LARGE (&math_rail_zh_26)
#define FONT (&math_rail_zh_16)
#define INK UI_INK
#define GREEN 0x286340
#define BLUE 0x245E8C

typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at_ms; } input_t;
static QueueHandle_t s_queue;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[4 * sizeof(input_t)];
static atomic_bool s_accept;
static mt_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery;
static lv_timer_t *s_timer, *s_battery_timer;
static bool s_buttons, s_dimmed, s_off;
static int64_t s_last_activity, s_guard_until;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static lv_obj_t *rect(lv_obj_t *p, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0); lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0); lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}
static lv_obj_t *label(lv_obj_t *p, const char *t, int x, int y, int w, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(p, t, FONT, color);
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, w);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}
static void line(const char *t, int y, uint32_t color) { label(s_content, t, 0, y, 198, color); }
static void strip(const char *t, int y, uint32_t bg)
{
    rect(s_content, 0, y, 198, 30, bg);
    label(s_content, t, 0, y + 3, 198, INK);
}
/* Ten compact carriages fit the same 198-pixel safe area as the answer cards. */
static void train(int y, unsigned cars)
{
    rect(s_content, 0, y + 25, 198, 3, 0x9D7856);
    for (int x = 2; x < 198; x += 15) rect(s_content, x, y + 28, 7, 3, 0x9D7856);
    rect(s_content, 0, y + 5, 30, 17, BLUE);
    rect(s_content, 3, y, 14, 15, BLUE);
    rect(s_content, 6, y + 3, 8, 8, 0xB9F3FF);
    rect(s_content, 23, y + 1, 5, 8, INK);
    rect(s_content, 3, y + 21, 7, 5, INK); rect(s_content, 21, y + 21, 7, 5, INK);
    for (unsigned i = 0; i < MT_ROUNDS; i++) {
        int x = 34 + (int)i * 16;
        rect(s_content, x, y + 9, 13, 13, i < cars ? 0xEDA838 : 0xD9DDD3);
        if (i < cars) rect(s_content, x + 3, y + 12, 7, 5, 0xFFFFFF);
        rect(s_content, x + 2, y + 22, 4, 4, INK); rect(s_content, x + 8, y + 22, 4, 4, INK);
    }
}
static void big(const char *text, int y, uint32_t color)
{
    lv_obj_t *o = label(s_content, text, 0, y, 198, color);
    lv_obj_set_style_text_font(o, LARGE, 0);
}
static void option(unsigned choice, int y)
{
    char value[16];
    rect(s_content, 0, y, 198, 44, choice == 0 ? 0xD2EDC1 : 0xD8E7F6);
    label(s_content, choice == 0 ? "上键" : "下键", 12, y + 11, 42, INK);
    snprintf(value, sizeof(value), "%u", s_state.current.options[choice]);
    lv_obj_t *o = label(s_content, value, 60, y + 5, 130, INK);
    lv_obj_set_style_text_font(o, LARGE, 0);
}
static void hint(char *buf, size_t size)
{
    mt_question_t q = s_state.current;
    if (q.op == MT_ADD && q.a % 10 && q.a % 10 + q.b > 10)
        snprintf(buf, size, "先加 %u，再加 %u", 10 - q.a % 10, q.b - (10 - q.a % 10));
    else if (q.op == MT_SUB && q.a % 10 && q.b > q.a % 10)
        snprintf(buf, size, "先减 %u，再减 %u", q.a % 10, q.b - q.a % 10);
    else if (q.op == MT_DIV)
        snprintf(buf, size, "想乘法：%u × %u = %u", q.answer, q.b, q.a);
    else if (q.op == MT_MUL) snprintf(buf, size, "%u 个 %u 相加", q.b, q.a);
    else snprintf(buf, size, "%s", q.op == MT_ADD ? "把两个数合起来" : "从前面的数往回减");
}
static void render(void)
{
    lv_obj_clean(s_content);
    lv_label_set_text(s_footer, "长按确定回首页");
    char buf[80];
    unsigned stamps = s_state.stamps[s_state.mode];
    switch (s_state.page) {
    case MT_HOME:
        train(36, stamps);
        ui_pixel_mascot_create(s_content, 80, 78);
        line("答对一题，接一节车厢", 4, GREEN);
        line(mt_mode_name(s_state.mode), 133, BLUE);
        snprintf(buf, sizeof(buf), "本次旅程  %u / 6 站", stamps); line(buf, 158, INK);
        strip(s_buttons ? "上下换档 · 确定出发" : "按键不可用", 184, UI_YELLOW);
        lv_label_set_text(s_footer, "每站十题 · 不限时");
        break;
    case MT_ASK:
        train(0, s_state.review ? s_state.review_correct : s_state.correct);
        snprintf(buf, sizeof(buf), "%s  %u / %u", s_state.review ? "错题加练" : mt_station_name(stamps),
                 s_state.position + 1, s_state.count); line(buf, 36, BLUE);
        snprintf(buf, sizeof(buf), "%u %s %u = ?", s_state.current.a, mt_operator(s_state.current.op), s_state.current.b);
        big(buf, 65, INK);
        option(0, 110); option(1, 165);
        lv_label_set_text(s_footer, "上下直接选 · 确定暂停");
        break;
    case MT_FEEDBACK:
        line(s_state.last_correct ? "答对啦，接上车厢！" : "没关系，一起算一遍", 5,
             s_state.last_correct ? GREEN : BLUE);
        snprintf(buf, sizeof(buf), "%u %s %u = %u", s_state.current.a,
                 mt_operator(s_state.current.op), s_state.current.b, s_state.current.answer);
        big(buf, 42, INK);
        hint(buf, sizeof(buf)); line(buf, 85, BLUE);
        train(118, s_state.review ? s_state.review_correct : s_state.correct);
        if (s_state.review) line("错题练习不改变首轮分", 156, GREEN);
        else { snprintf(buf, sizeof(buf), "已接 %u 节 · 连对 %u 题", s_state.correct, s_state.streak); line(buf, 156, GREEN); }
        strip("上键或下键 · 继续", 184, UI_YELLOW);
        break;
    case MT_PAUSED:
        line("小火车休息中", 10, GREEN);
        ui_pixel_mascot_create(s_content, 80, 46);
        line("题目会等你，不扣分", 112, INK);
        strip("上键或确定 · 继续", 147, UI_YELLOW);
        strip("下键 · 回首页", 184, UI_MUTED);
        break;
    case MT_RESULT:
        line(s_state.review ? "错题又练了一遍" : s_state.correct == 10 ? "满载到站！" : s_state.correct >= 8 ? "新车站盖章啦！" : "再接几节就到站", 0, GREEN);
        snprintf(buf, sizeof(buf), "%u / %u", s_state.review ? s_state.review_correct : s_state.correct,
                 s_state.review ? s_state.count : MT_ROUNDS); big(buf, 25, INK);
        train(62, s_state.review ? s_state.review_correct : s_state.correct);
        if (s_state.review) line("加练不改变首轮成绩", 103, BLUE);
        else { snprintf(buf, sizeof(buf), "最高连对 %u · 最佳 %u", s_state.best_streak, s_state.best[s_state.mode]); line(buf, 103, BLUE); }
        if (s_state.earned && !s_state.review) snprintf(buf, sizeof(buf), "收到 %s 车票", mt_station_name(stamps - 1));
        else if (stamps == MT_STATIONS) snprintf(buf, sizeof(buf), "六站集齐，继续出发！");
        else snprintf(buf, sizeof(buf), "首轮对八题，收集车票");
        line(buf, 130, GREEN);
        strip("上键 · 新的一站", 150, UI_YELLOW);
        strip(s_state.miss_count ? "下键 · 重练错题" : "下键 · 回首页", 184, UI_MUTED);
        lv_label_set_text(s_footer, "确定回首页 · 上下换档");
        break;
    }
}
static void battery(lv_timer_t *timer)
{
    (void)timer;
    if (!s_battery) return;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}
static bool handle(input_t in, int64_t now)
{
    s_last_activity = now;
    bool wake = s_dimmed || s_off;
    if (wake) bsp_display_backlight(100);
    s_dimmed = s_off = false;
    if (wake) return false; /* Wake is never an answer or a resume. */
    if (in.event == BSP_BTN_LONG) { mt_home(&s_state); return true; }
    bsp_btn_t b = in.button;
    if (s_state.page == MT_HOME) {
        if (b == BSP_BTN_OK) mt_start(&s_state, esp_random());
        else mt_mode(&s_state, b == BSP_BTN_DOWN ? 1 : -1);
    } else if (s_state.page == MT_PAUSED) {
        if (b == BSP_BTN_DOWN) mt_home(&s_state);
        else mt_pause(&s_state);
    } else if (s_state.page == MT_RESULT) {
        if (b == BSP_BTN_UP) mt_start(&s_state, esp_random());
        else if (b == BSP_BTN_DOWN && s_state.miss_count) mt_review(&s_state);
        else mt_home(&s_state);
    } else if (s_state.page == MT_ASK) {
        if (b == BSP_BTN_OK) mt_pause(&s_state);
        else return mt_answer(&s_state, b == BSP_BTN_UP ? 0 : 1);
    } else return mt_next(&s_state);
    return true;
}
static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms();
    input_t in; bool dirty = false, handled = false;
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &in, 0) == pdTRUE; i++) {
        if (!handled && now - in.at_ms <= 200 && in.at_ms >= s_guard_until) {
            dirty = handle(in, now); handled = true;
            s_guard_until = now + 180; /* Do not spill rapid presses into a new page. */
        }
    }
    int64_t idle = now - s_last_activity;
    if (!s_dimmed && idle >= 60000) {
        if (s_state.page == MT_ASK || s_state.page == MT_FEEDBACK) {
            mt_pause(&s_state); dirty = true;
        }
        bsp_display_backlight(20); s_dimmed = true;
    }
    if (!s_off && idle >= 180000) { bsp_display_backlight(0); s_off = true; }
    if (dirty) render();
}
void math_rail_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t), s_queue_storage, &s_queue_control);
}
void math_rail_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept)) return;
    if (button != BSP_BTN_UP && button != BSP_BTN_DOWN && button != BSP_BTN_OK) return;
    if (event != BSP_BTN_CLICK && !(button == BSP_BTN_OK && event == BSP_BTN_LONG)) return;
    input_t in = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &in, 0);
}
void math_rail_enter(bool buttons_available)
{
    if (s_screen) return;
    math_rail_prepare();
    s_buttons = buttons_available; s_off = s_dimmed = false;
    s_last_activity = now_ms(); s_guard_until = s_last_activity;
    mt_init(&s_state);
    s_screen = ui_pixel_screen_create("");
    label(s_screen, "口算旅行号", 5, 13, 151, 0xFFFFFF);
    s_battery = ui_pixel_label(s_screen, "--%", &lv_font_montserrat_14, INK);
    lv_obj_set_pos(s_battery, 164, 31); lv_obj_set_width(s_battery, 64);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    /* 220 - 2*(4 border + 7 padding) = 198 usable pixels. */
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 220, 238, UI_PAPER);
    s_footer = label(s_screen, "", 3, 297, 234, INK);
    render(); battery(NULL);
    s_timer = lv_timer_create(frame, 20, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen); bsp_display_backlight(100);
    xQueueReset(s_queue); atomic_store(&s_accept, buttons_available);
}
void math_rail_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    s_content = s_footer = s_battery = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}
