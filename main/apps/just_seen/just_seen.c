#include "just_seen.h"
#include "just_seen_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>

LV_FONT_DECLARE(just_seen_zh_16);
static const char *ANIMALS[] = {"小猫", "兔子", "青蛙", "小熊", "小猪", "狐狸"};
static const char *MODES[] = {"记上一只", "记前两只", "记前三只"};
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_control;
static uint8_t s_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static js_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery;
static lv_timer_t *s_timer, *s_battery_timer;
static bool s_buttons, s_off, s_dimmed;
static int64_t s_activity, s_guard;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0); lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0); lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}
static lv_obj_t *label(lv_obj_t *p, const char *text, int y, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(p, text, &just_seen_zh_16, color);
    lv_obj_set_pos(o, 0, y); lv_obj_set_width(o, 198);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}
/* Original pixel animals: names and silhouettes distinguish every card. */
static void animal(unsigned a, int y)
{
    static const uint32_t colors[] = {0xF3BE5E, 0xF6DAD7, 0x9CCD74, 0xC29A76, 0xF29DAC, 0xF08B4E};
    lv_obj_t *tile = box(s_content, 53, y, 92, 80, 0xE2ECE5);
    uint32_t c = colors[a];
    int ear_y = a == 1 ? 3 : 14, ear_h = a == 1 ? 33 : 20;
    box(tile, 17, ear_y, 16, ear_h, UI_INK);
    box(tile, 59, ear_y, 16, ear_h, UI_INK);
    box(tile, 21, ear_y + 4, 8, ear_h - 4, c);
    box(tile, 63, ear_y + 4, 8, ear_h - 4, c);
    box(tile, 12, 28, 68, 45, UI_INK);
    box(tile, 16, 32, 60, 37, c);
    if (a == 5) {
        box(tile, 18, 48, 22, 17, 0xFFF3D6); box(tile, 52, 48, 22, 17, 0xFFF3D6);
    }
    if (a == 2) {
        box(tile, 20, 24, 14, 17, 0xFFFFFF); box(tile, 58, 24, 14, 17, 0xFFFFFF);
    }
    box(tile, 25, a == 2 ? 28 : 43, 6, 8, UI_INK);
    box(tile, 61, a == 2 ? 28 : 43, 6, 8, UI_INK);
    box(tile, 40, 57, 12, 4, UI_INK);
    if (a == 4) {
        box(tile, 34, 51, 24, 14, 0xD96587);
        box(tile, 39, 55, 4, 6, UI_INK); box(tile, 49, 55, 4, 6, UI_INK);
    } else if (a == 0) {
        box(tile, 7, 52, 17, 3, UI_INK); box(tile, 68, 52, 17, 3, UI_INK);
    } else if (a == 3) {
        box(tile, 33, 51, 26, 13, 0xF5DDB1); box(tile, 42, 54, 8, 5, UI_INK);
    }
}
static void render(void)
{
    lv_obj_clean(s_content);
    const char *footer = "长按确定回首页";
    lv_obj_t *o;
    switch (s_state.page) {
    case JS_HOME:
        label(s_content, "上一只，你还记得吗", 0, UI_INK);
        animal(0, 26);
        label(s_content, MODES[s_state.back - 1], 113, UI_SKY_DARK);
        label(s_content, "上下换难度 · 确定开始", 139, UI_INK);
        o = label(s_content, "", 167, UI_INK);
        lv_label_set_text_fmt(o, "本档开机最佳 %u / 8", s_state.best[s_state.back - 1]);
        label(s_content, "请停车后玩", 190, 0xA33B32);
        footer = "每局八题 · 不限时间";
        break;
    case JS_LEARN:
        o = label(s_content, "", 0, UI_SKY_DARK);
        lv_label_set_text_fmt(o, "先记住  %u / %u", s_state.cursor + 1, s_state.back);
        animal(js_current(&s_state), 29);
        label(s_content, ANIMALS[js_current(&s_state)], 118, UI_INK);
        label(s_content, "按出现顺序记住它们", 148, UI_INK);
        label(s_content, "看好了再按确定", 180, UI_SKY_DARK);
        break;
    case JS_ASK:
        o = label(s_content, "", 0, UI_SKY_DARK);
        lv_label_set_text_fmt(o, "第 %u / 8 题 · 连对 %u", s_state.round + 1, s_state.streak);
        o = label(s_content, "", 26, UI_INK);
        lv_label_set_text_fmt(o, "往前第 %u 只一样吗？", s_state.back);
        animal(js_current(&s_state), 53);
        label(s_content, ANIMALS[js_current(&s_state)], 140, UI_INK);
        box(s_content, 1, 172, 96, 34, 0xD9EDC6);
        box(s_content, 101, 172, 96, 34, 0xF5DFC7);
        o = label(s_content, "上键 相同", 179, 0x31581D); lv_obj_set_width(o, 98);
        o = label(s_content, "下键 不同", 179, 0x714426); lv_obj_set_x(o, 100); lv_obj_set_width(o, 98);
        footer = "确定暂停 · 长按回首页";
        break;
    case JS_FEEDBACK:
        label(s_content, s_state.last_correct ? "记住啦！" : "再看一眼就好", 0,
              s_state.last_correct ? 0x427421 : 0xA33B32);
        animal(js_expected(&s_state), 28);
        o = label(s_content, "", 116, UI_INK);
        lv_label_set_text_fmt(o, "往前第 %u 只：%s", s_state.back, ANIMALS[js_expected(&s_state)]);
        o = label(s_content, "", 145, UI_INK);
        lv_label_set_text_fmt(o, "这只是%s · %s", ANIMALS[js_current(&s_state)],
                             js_current(&s_state) == js_expected(&s_state) ? "相同" : "不同");
        label(s_content, s_state.round == 7 ? "确定查看成绩" : "确定继续下一只", 182, UI_SKY_DARK);
        break;
    case JS_PAUSE:
        label(s_content, "已暂停，先忙你的", 5, UI_INK);
        ui_pixel_mascot_create(s_content, 80, 44);
        label(s_content, "不扣分，也不催你", 116, UI_INK);
        label(s_content, "确定回来 · 下键结束", 147, UI_SKY_DARK);
        label(s_content, "回来先回顾，再继续", 180, UI_INK);
        break;
    case JS_RESULT:
        label(s_content, s_state.correct == 8 ? "八题全对！" : "又记住了一点", 0, 0x427421);
        animal(s_state.correct % JS_ANIMALS, 27);
        o = label(s_content, "", 114, UI_INK);
        lv_label_set_text_fmt(o, "答对 %u / 8 · 最长连对 %u", s_state.correct, s_state.longest);
        o = label(s_content, "", 142, UI_SKY_DARK);
        lv_label_set_text_fmt(o, "前 %u 只 · 题组 %04u", s_state.back, s_state.code);
        label(s_content, "上键同题 · 确定新题", 175, UI_INK);
        footer = "下键回首页 · 换个难度";
        break;
    }
    lv_label_set_text(s_footer, s_buttons ? footer : "按键不可用，请重启");
}
static void battery(lv_timer_t *timer)
{
    (void)timer;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}
static void dispatch(input_t input)
{
    if (input.event == BSP_BTN_LONG) { js_home(&s_state); return; }
    bsp_btn_t b = input.button;
    switch (s_state.page) {
    case JS_HOME:
        if (b == BSP_BTN_OK) js_start(&s_state, esp_random() % 9000);
        else js_mode(&s_state, b == BSP_BTN_DOWN ? 1 : -1);
        break;
    case JS_LEARN: case JS_FEEDBACK:
        if (b == BSP_BTN_OK) js_next(&s_state);
        break;
    case JS_ASK:
        if (b == BSP_BTN_OK) js_pause(&s_state);
        else js_answer(&s_state, b == BSP_BTN_UP);
        break;
    case JS_PAUSE:
        if (b == BSP_BTN_OK) js_resume(&s_state);
        else if (b == BSP_BTN_DOWN) js_home(&s_state);
        break;
    case JS_RESULT:
        if (b == BSP_BTN_DOWN) js_home(&s_state);
        else js_start(&s_state, b == BSP_BTN_UP ? s_state.code - 1000 : esp_random() % 9000);
        break;
    }
}
static void tick(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms();
    input_t input;
    bool changed = false;
    while (xQueueReceive(s_queue, &input, 0) == pdTRUE) {
        if (!s_buttons || now - input.at > 250) continue;
        bool sleeping = s_off || s_dimmed;
        s_activity = now;
        if (sleeping) {
            bsp_display_backlight(100); s_off = s_dimmed = false;
            s_guard = now + 700; continue;
        }
        if (now < s_guard) continue;
        dispatch(input); changed = true; s_guard = now + 200;
    }
    if (now - s_activity >= 30000 && s_state.page != JS_PAUSE) {
        js_page_t before = s_state.page; js_pause(&s_state);
        if (before != s_state.page) changed = true;
    }
    if (now - s_activity >= 180000 && !s_off) {
        bsp_display_backlight(0); s_off = true;
    } else if (now - s_activity >= 60000 && !s_dimmed && !s_off) {
        bsp_display_backlight(20); s_dimmed = true;
    }
    if (changed) render();
}
void just_seen_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t), s_storage, &s_control);
}
void just_seen_enter(bool buttons_available)
{
    just_seen_prepare(); xQueueReset(s_queue);
    s_buttons = buttons_available; s_off = s_dimmed = false;
    js_init(&s_state); s_activity = now_ms(); s_guard = 0;
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = label(s_screen, "刚才哪只", 15, 0xFFFFFF);
    lv_obj_set_x(title, 12); lv_obj_set_width(title, 136);
    s_battery = label(s_screen, "--%", 30, UI_INK);
    lv_obj_set_x(s_battery, 163); lv_obj_set_width(s_battery, 70);
    s_content = ui_pixel_panel_create(s_screen, 10, 53, 220, 234, UI_PAPER);
    s_footer = label(s_screen, "", 294, UI_INK);
    lv_obj_set_x(s_footer, 10); lv_obj_set_width(s_footer, 220);
    render(); battery(NULL);
    s_timer = lv_timer_create(tick, 20, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen); atomic_store(&s_accept, true);
}
void just_seen_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = s_content = s_footer = s_battery = NULL;
}
void just_seen_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept) || !s_queue) return;
    if (button < BSP_BTN_UP || button > BSP_BTN_OK) return;
    if (event != BSP_BTN_PRESS && !(button == BSP_BTN_OK && event == BSP_BTN_LONG)) return;
    input_t input = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}
