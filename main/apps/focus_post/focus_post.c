#include "focus_post.h"
#include "focus_post_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>

LV_FONT_DECLARE(focus_post_zh_16);
LV_FONT_DECLARE(focus_post_zh_24);
static const char *ANIMALS[] = {"小猫", "兔子", "青蛙"};
static const char *PACES[] = {"慢速", "中速", "快速"};
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_control;
static uint8_t s_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static fp_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery, *s_bar;
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
    lv_obj_t *o = ui_pixel_label(p, text, &focus_post_zh_16, color);
    lv_obj_set_pos(o, 0, y); lv_obj_set_width(o, 198);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}
static lv_obj_t *big_label(const char *text, int y, uint32_t color)
{
    lv_obj_t *o = label(s_content, text, y, color);
    lv_obj_set_style_text_font(o, &focus_post_zh_24, 0);
    return o;
}
/* Integer-scaled code art keeps the same silhouettes in the small target badge
   and large visitor, without a bitmap allocation or an animation. */
static void pixel(lv_obj_t *tile, int scale, int x, int y, int w, int h, uint32_t color)
{
    box(tile, x * scale / 100, y * scale / 100,
        (w * scale + 99) / 100, (h * scale + 99) / 100, color);
}
static void animal_at(unsigned a, int x, int y, int scale)
{
    static const uint32_t colors[] = {0xF3BE5E, 0xF6DAD7, 0x9CCD74};
    lv_obj_t *tile = box(s_content, x, y, 92 * scale / 100, 80 * scale / 100, 0xE2ECE5);
    uint32_t c = colors[a];
    int ear_y = a == 1 ? 3 : 14, ear_h = a == 1 ? 33 : 20;
    pixel(tile, scale, 17, ear_y, 16, ear_h, UI_INK);
    pixel(tile, scale, 59, ear_y, 16, ear_h, UI_INK);
    pixel(tile, scale, 21, ear_y + 4, 8, ear_h - 4, c);
    pixel(tile, scale, 63, ear_y + 4, 8, ear_h - 4, c);
    pixel(tile, scale, 12, 28, 68, 45, UI_INK);
    pixel(tile, scale, 16, 32, 60, 37, c);
    if (a == 2) {
        pixel(tile, scale, 20, 24, 14, 17, 0xFFFFFF);
        pixel(tile, scale, 58, 24, 14, 17, 0xFFFFFF);
    }
    pixel(tile, scale, 25, a == 2 ? 28 : 43, 6, 8, UI_INK);
    pixel(tile, scale, 61, a == 2 ? 28 : 43, 6, 8, UI_INK);
    pixel(tile, scale, 40, 57, 12, 4, UI_INK);
    if (a == 0) {
        pixel(tile, scale, 7, 52, 17, 3, UI_INK);
        pixel(tile, scale, 68, 52, 17, 3, UI_INK);
    }
}
static void hero(unsigned a, int y, int scale)
{
    animal_at(a, (198 - 92 * scale / 100) / 2, y, scale);
}
static void action(const char *text)
{
    box(s_content, 3, 173, 192, 38, 0xD9EDC6);
    big_label(text, 177, 0x31583D);
}
static void target_badge(void)
{
    box(s_content, 0, 0, 198, 36, 0xDAEAE4);
    animal_at(s_state.target, 3, 0, 45);
    lv_obj_t *o = big_label("", 3, UI_INK);
    lv_obj_set_x(o, 49); lv_obj_set_width(o, 145);
    lv_label_set_text_fmt(o, "只送%s", ANIMALS[s_state.target]);
}
static void progress(void)
{
    for (unsigned i = 0; i < FP_ROUNDS; i++)
        box(s_content, 5 + i * 16, 39, 12, 4,
            i <= s_state.round ? 0x356044 : 0xD6E0D7);
}
static void render(void)
{
    s_bar = NULL; lv_obj_clean(s_content);
    const char *footer = "";
    lv_obj_t *o;
    switch (s_state.page) {
    case FP_HOME:
        hero(0, 2, 165);
        for (unsigned i = 0; i < 3; i++) {
            bool selected = i == s_state.pace;
            box(s_content, 3 + i * 65, 140, 62, 27, selected ? 0x356044 : 0xE0E8DF);
            o = label(s_content, PACES[i], 144, selected ? 0xFFFFFF : UI_INK);
            lv_obj_set_x(o, 3 + i * 65); lv_obj_set_width(o, 62);
        }
        action("确定开始");
        footer = "上下换速度";
        break;
    case FP_RULE:
        o = big_label("", 0, UI_INK);
        lv_label_set_text_fmt(o, "只送%s", ANIMALS[s_state.target]);
        hero(s_state.target, 36, 155);
        action(s_state.tutorial ? "确定试试" : "确定出发");
        break;
    case FP_READY:
        hero(s_state.target, 5, 165);
        big_label("松开手", 167, UI_INK);
        footer = "下键暂停";
        break;
    case FP_PRACTICE: case FP_VISITOR:
        target_badge();
        if (!s_state.tutorial) progress();
        hero(fp_current(&s_state), 47, 140);
        if (!(s_state.tutorial && !s_state.practice)) {
            box(s_content, 9, 162, 180, 6, 0xD6E0D7);
            s_bar = box(s_content, 9, 162, 180, 6, 0x63A181);
        }
        /* Only practice reveals the action. Scored play still requires the
           child to compare the visitor with the persistent target badge. */
        action(s_state.tutorial ? (s_state.practice ? "等一等" : "按确定") : "一样才按");
        footer = "下键暂停";
        break;
    case FP_FEEDBACK:
        big_label(s_state.correct ? (s_state.pressed ? "送到啦" : "等对啦") :
            (s_state.pressed ? "这次等一等" : "这次要送信"), 0, s_state.correct ? 0x356044 : UI_INK);
        hero(fp_current(&s_state), 36, 155);
        action(s_state.tutorial && !s_state.correct ? "确定重试" : "确定继续");
        break;
    case FP_PAUSE:
        big_label("休息一下", 0, UI_INK);
        hero(s_state.target, 36, 155);
        action("确定继续");
        footer = "下键回家";
        break;
    case FP_RESULT:
        big_label("送完啦", 0, UI_INK);
        hero(s_state.target, 32, 45);
        o = big_label("", 78, 0x356044);
        lv_label_set_text_fmt(o, "送对 %u / 6", s_state.delivered);
        o = big_label("", 117, 0x356044);
        lv_label_set_text_fmt(o, "等对 %u / 6", s_state.waited);
        action("确定休息");
        footer = "上键重练 · 下键换题";
        break;
    }
    lv_label_set_text(s_footer, s_buttons ? footer : "按键异常，请重启");
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
    if (input.event == BSP_BTN_LONG) { fp_home(&s_state); return; }
    bsp_btn_t b = input.button;
    switch (s_state.page) {
    case FP_HOME:
        if (b == BSP_BTN_OK) fp_start(&s_state, esp_random(), true);
        else s_state.pace = (s_state.pace + (b == BSP_BTN_DOWN ? 1 : 2)) % 3;
        break;
    case FP_PAUSE:
        if (b == BSP_BTN_OK) fp_resume(&s_state, input.at);
        else if (b == BSP_BTN_DOWN) fp_home(&s_state);
        break;
    case FP_RESULT:
        if (b == BSP_BTN_OK) fp_home(&s_state);
        else fp_start(&s_state, b == BSP_BTN_UP ? s_state.seed : esp_random(), false);
        break;
    default:
        if (b == BSP_BTN_DOWN) fp_pause(&s_state, input.at);
        else if (b == BSP_BTN_OK) fp_ok(&s_state, input.at);
        break;
    }
}
static void tick(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms(); input_t input;
    bool changed = false;
    while (xQueueReceive(s_queue, &input, 0) == pdTRUE) {
        if (!s_buttons || now - input.at > 250) continue;
        bool sleeping = s_off || s_dimmed;
        s_activity = now;
        if (sleeping) {
            bsp_display_backlight(100); s_off = s_dimmed = false;
            s_guard = now + 700; continue;
        }
        if (input.at < s_guard || input.at < s_state.opened) continue;
        fp_page_t before = s_state.page;
        dispatch(input); changed = true;
        if (before != s_state.page || before == FP_HOME) s_guard = now + 220;
    }
    fp_page_t before = s_state.page;
    fp_tick(&s_state, now);
    if (before != s_state.page) { changed = true; s_guard = s_state.page == FP_FEEDBACK ? now + 220 : now; }
    if (now - s_activity >= 30000 && s_state.page != FP_PAUSE) {
        before = s_state.page; fp_pause(&s_state, now);
        if (before != s_state.page) changed = true;
    }
    if (now - s_activity >= 180000 && !s_off) {
        bsp_display_backlight(0); s_off = true;
    } else if (now - s_activity >= 60000 && !s_dimmed && !s_off) {
        bsp_display_backlight(20); s_dimmed = true;
    }
    if (changed) render();
    if (s_bar && !(s_state.tutorial && !s_state.practice)) {
        int64_t left = s_state.deadline > now ? s_state.deadline - now : 0;
        int width = (int)(180 * left / fp_duration(&s_state));
        lv_obj_set_width(s_bar, width < 1 ? 1 : width > 180 ? 180 : width);
    }
}
void focus_post_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t), s_storage, &s_control);
}
void focus_post_enter(bool buttons_available)
{
    focus_post_prepare(); xQueueReset(s_queue);
    s_buttons = buttons_available; s_off = s_dimmed = false;
    fp_init(&s_state); s_activity = now_ms(); s_guard = 0;
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = label(s_screen, "小邮差", 15, 0xFFFFFF);
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
void focus_post_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = s_content = s_footer = s_battery = s_bar = NULL;
}
void focus_post_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept) || !s_queue) return;
    if (button < BSP_BTN_UP || button > BSP_BTN_OK) return;
    if (event != BSP_BTN_PRESS && !(button == BSP_BTN_OK && event == BSP_BTN_LONG)) return;
    input_t input = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}
