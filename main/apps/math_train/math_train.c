#include "math_train.h"
#include "math_train_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>
#include <stdio.h>

LV_FONT_DECLARE(math_train_zh_18);
LV_FONT_DECLARE(math_train_math_28);
#define FONT (&math_train_zh_18)
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

static void option(const char *t, int y, bool selected, bool enabled)
{
    lv_obj_t *o = rect(s_content, 0, y, 198, 29,
                        !enabled ? 0xE2E4DE : selected ? UI_YELLOW : 0xE1ECF0);
    if (selected) {
        lv_obj_set_style_border_width(o, 2, 0);
        lv_obj_set_style_border_color(o, lv_color_hex(INK), 0);
    }
    if (selected) rect(s_content, 8, y + 11, 6, 6, INK);
    label(s_content, t, 19, y + 2, 173, enabled ? INK : 0x67736A);
}
static void train(int y)
{
    /* Original locomotive and five star wagons, drawn without icon fonts. */
    rect(s_content, 0, y + 37, 198, 3, 0x897558);
    for (int x = 4; x < 198; x += 15) rect(s_content, x, y + 40, 9, 3, 0x897558);
    rect(s_content, 1, y + 10, 39, 23, INK);
    rect(s_content, 5, y + 14, 31, 15, 0xD47535);
    rect(s_content, 4, y, 21, 25, INK);
    rect(s_content, 8, y + 4, 13, 13, 0xB9F3FF);
    rect(s_content, 30, y + 3, 7, 9, INK);
    rect(s_content, 7, y + 30, 9, 9, INK);
    rect(s_content, 28, y + 30, 9, 9, INK);
    for (unsigned i = 0; i < 5; i++) {
        int x = 45 + i * 31;
        rect(s_content, x, y + 14, 29, 19, INK);
        rect(s_content, x + 3, y + 17, 23, 13, 0xD0E7D8);
        rect(s_content, x + 4, y + 32, 6, 6, INK);
        rect(s_content, x + 20, y + 32, 6, 6, INK);
        for (unsigned j = 0; j < 2; j++) {
            unsigned n = i * 2 + j;
            uint32_t c = n < s_state.correct ? 0xB97912 : 0x91AB9A;
            rect(s_content, x + 5 + j * 11, y + 22, 8, 3, c);
            rect(s_content, x + 8 + j * 11, y + 19, 3, 9, c);
        }
    }
}
static void equation(bool reveal, int y)
{
    const mt_question_t *q = mt_current(&s_state);
    char t[64];
    if (reveal) snprintf(t, sizeof(t), "%u %s %u = %u", q->a, mt_operator(q->op), q->b, q->answer);
    else snprintf(t, sizeof(t), "%u %s %u = ?", q->a, mt_operator(q->op), q->b);
    lv_obj_t *o = ui_pixel_label(s_content, t, &math_train_math_28, INK);
    lv_obj_set_pos(o, 0, y); lv_obj_set_width(o, 198);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
}
static void render(void)
{
    lv_obj_clean(s_content);
    lv_label_set_text(s_footer, "上下选择 · 确定作答");
    char buf[80];
    switch (s_state.page) {
    case MT_HOME:
        line("算对一题，添颗星", 0, GREEN);
        train(30);
        for (unsigned i = 0; i < MT_MODES; i++)
            option(mt_mode_name(i), 83 + i * 33, s_state.selected == i, true);
        lv_label_set_text(s_footer, s_buttons ? "上下选难度 · 确定出发" : "按键不可用");
        break;
    case MT_ASK:
        snprintf(buf, sizeof(buf), s_state.reviewing ? "错题再练  %u / %u" : "第 %u 题  /  %u",
                 s_state.cursor + 1, s_state.reviewing ? s_state.review_total : MT_COUNT);
        line(buf, 0, BLUE); train(27); equation(false, 76);
        for (unsigned i = 0; i < 3; i++) {
            snprintf(buf, sizeof(buf), "%u", mt_current(&s_state)->choices[i]);
            option(buf, 113 + i * 33, s_state.selected == i, true);
        }
        break;
    case MT_FEEDBACK:
        line(s_state.last_correct ? (s_state.reviewing ? "订正成功！" : "答对啦，添颗星！") : "没关系，看这里", 0,
             s_state.last_correct ? GREEN : BLUE);
        train(28); equation(true, 81);
        if (!s_state.last_correct) {
            snprintf(buf, sizeof(buf), "刚才选了 %u", mt_current(&s_state)->choices[s_state.selected]);
            line(buf, 122, BLUE);
            line("记住算式，稍后再练", 151, GREEN);
        } else if (s_state.reviewing) {
            line("这一题已经学会啦", 128, GREEN);
        } else {
            snprintf(buf, sizeof(buf), "连续答对 %u 题", s_state.streak); line(buf, 123, GREEN);
            line(s_state.streak && s_state.streak % 3 == 0 ? "连对三题，真棒！" : "慢慢想，也能亮晶晶", 151, BLUE);
        }
        option("确定继续", 183, true, true);
        lv_label_set_text(s_footer, "长按确定键回首页");
        break;
    case MT_RESULT:
        line(mt_rank(&s_state), 0, GREEN); train(27);
        snprintf(buf, sizeof(buf), "本趟答对 %u / 10", s_state.correct); line(buf, 72, INK);
        if (s_state.reviewing) snprintf(buf, sizeof(buf), "待订正 %u 题", mt_missed(&s_state));
        else snprintf(buf, sizeof(buf), "最高连对 %u · 最佳 %u", s_state.best_streak, s_state.records[s_state.mode]);
        line(buf, 97, BLUE);
        option("再来一趟", 123, s_state.selected == 0, true);
        snprintf(buf, sizeof(buf), "错题再练  %u 题", mt_missed(&s_state));
        option(buf, 155, s_state.selected == 1, mt_missed(&s_state) > 0);
        option("更换难度", 187, s_state.selected == 2, true);
        lv_label_set_text(s_footer, s_state.reviewing ? "订正不改变本趟成绩" : "最佳仅记录本次开机");
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
    if (wake) return false;
    if (in.event == BSP_BTN_LONG) { mt_home(&s_state); return true; }
    if (in.button != BSP_BTN_OK) {
        mt_move(&s_state, in.button == BSP_BTN_DOWN ? 1 : -1); return s_state.page != MT_FEEDBACK;
    }
    switch (s_state.page) {
    case MT_HOME: mt_start(&s_state, esp_random()); break;
    case MT_ASK: return mt_answer(&s_state);
    case MT_FEEDBACK: return mt_next(&s_state);
    case MT_RESULT:
        if (s_state.selected == 0) mt_start(&s_state, esp_random());
        else if (s_state.selected == 1) return mt_review(&s_state);
        else mt_home(&s_state);
        break;
    }
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
        bsp_display_backlight(20); s_dimmed = true;
    }
    if (!s_off && idle >= 180000) { bsp_display_backlight(0); s_off = true; }
    if (dirty) render();
}
void math_train_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t), s_queue_storage, &s_queue_control);
}
void math_train_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept)) return;
    if (button != BSP_BTN_UP && button != BSP_BTN_DOWN && button != BSP_BTN_OK) return;
    if (event != BSP_BTN_CLICK && !(button == BSP_BTN_OK && event == BSP_BTN_LONG)) return;
    input_t in = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &in, 0);
}
void math_train_enter(bool buttons_available)
{
    if (s_screen) return;
    math_train_prepare();
    s_buttons = buttons_available; s_off = s_dimmed = false;
    s_last_activity = now_ms(); s_guard_until = s_last_activity;
    mt_init(&s_state);
    s_screen = ui_pixel_screen_create("");
    label(s_screen, "口算小火车", 5, 13, 151, 0xFFFFFF);
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
void math_train_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    s_content = s_footer = s_battery = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}
