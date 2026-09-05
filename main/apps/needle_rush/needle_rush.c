#include "needle_rush.h"
#include "needle_rush_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>

LV_FONT_DECLARE(needle_rush_zh_16);
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static nr_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_battery, *s_footer, *s_shot;
static lv_obj_t *s_lines[NR_MAX_PINS], *s_tips[NR_MAX_PINS];
static lv_point_precise_t s_points[NR_MAX_PINS][2];
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_last_frame, s_last_activity;
static bool s_buttons, s_dimmed, s_off, s_suppress_long;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, uint32_t color, int radius)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_radius(o, radius, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}
static lv_obj_t *text(lv_obj_t *p, const char *value, int y, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(p, value, &needle_rush_zh_16, color);
    lv_obj_set_pos(o, 0, y); lv_obj_set_width(o, 198);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}
static const char *mode_name(void) { return s_state.mode ? "一命高手" : "三次机会"; }
static void pin_position(unsigned i, int angle)
{
    int sn = lv_trigo_sin(angle), cs = lv_trigo_cos(angle);
    s_points[i][0] = (lv_point_precise_t){99 + cs * 20 / 32768, 60 + sn * 20 / 32768};
    s_points[i][1] = (lv_point_precise_t){99 + cs * 49 / 32768, 60 + sn * 49 / 32768};
    lv_line_set_points(s_lines[i], s_points[i], 2);
    lv_obj_set_pos(s_tips[i], (int)s_points[i][1].x - 4, (int)s_points[i][1].y - 4);
}
static void wheel(bool home)
{
    lv_obj_t *field = box(s_content, 0, 29, 198, home ? 132 : 146, 0xE8F4E7, 12);
    unsigned count = home ? 7 : s_state.count;
    /* The dotted firing lane makes the impact position unambiguous. */
    for (int y = 86; y < (home ? 122 : 143); y += 10) box(field, 98, y, 2, 4, 0x9BC4AF, 0);
    for (unsigned i = 0; i < count; i++) {
        uint32_t color = home || i < s_state.initial_count ? UI_INK : 0x168B79;
        if (!home && s_state.page == NR_FEEDBACK && s_state.hit &&
            nr_distance(nr_angle(&s_state, i), NR_IMPACT) <= NR_CLEARANCE) color = UI_RED;
        s_lines[i] = lv_line_create(field);
        lv_obj_set_style_line_width(s_lines[i], 2, 0);
        lv_obj_set_style_line_color(s_lines[i], lv_color_hex(color), 0);
        s_tips[i] = box(field, 0, 0, 9, 9, color, LV_RADIUS_CIRCLE);
        pin_position(i, home ? (int)i * 45 + 115 : (int)(nr_angle(&s_state, i) / 1000));
    }
    box(field, 77, 38, 44, 44, UI_INK, LV_RADIUS_CIRCLE);
    box(field, 80, 41, 38, 38, UI_YELLOW, LV_RADIUS_CIRCLE);
    lv_obj_t *left = text(field, "", 49, UI_INK);
    lv_obj_set_x(left, 77); lv_obj_set_width(left, 44);
    if (home) lv_label_set_text(left, "针");
    else {
        lv_obj_set_style_text_font(left, &lv_font_montserrat_20, 0);
        lv_label_set_text_fmt(left, "%u", s_state.remaining);
    }
    s_shot = box(field, 95, home ? 121 : 132, 9, 9, UI_ORANGE, LV_RADIUS_CIRCLE);
    box(s_shot, 3, 0, 3, 5, UI_INK, 0);
    if (!home && s_state.page == NR_FEEDBACK) {
        if (!s_state.hit) lv_obj_add_flag(s_shot, LV_OBJ_FLAG_HIDDEN);
        else {
            lv_obj_set_y(s_shot, 105);
            lv_obj_set_style_bg_color(s_shot, lv_color_hex(UI_RED), 0);
        }
    }
}
static void render(void)
{
    s_shot = NULL;
    for (unsigned i = 0; i < NR_MAX_PINS; i++) s_lines[i] = s_tips[i] = NULL;
    lv_obj_clean(s_content);
    if (s_state.page == NR_HOME) {
        text(s_content, "插进空隙，别碰到针", 0, UI_INK);
        wheel(true);
        /* Home has one illustration and two fixed text rows. */
        lv_obj_t *hint = text(s_content, mode_name(), 166, 0x168B79);
        (void)hint;
        lv_obj_t *best = text(s_content, "", 186, UI_INK);
        lv_label_set_text_fmt(best, "本次最高 %u 分", s_state.best[s_state.mode]);
        lv_label_set_text(s_footer, "上下选模式 / 确定开始");
    } else if (s_state.page == NR_RESULT) {
        text(s_content, s_state.won ? "十关全过！针不错" : "就差一点，针可惜", 0,
             s_state.won ? 0x168B79 : UI_RED);
        lv_obj_t *score = text(s_content, "", 34, UI_INK);
        lv_obj_set_style_text_font(score, &lv_font_montserrat_20, 0);
        lv_label_set_text_fmt(score, "%u", s_state.score);
        text(s_content, s_state.won ? "见缝插针大师" : s_state.level >= 5 ? "空隙捕手" : "稳住就能赢", 65, 0x168B79);
        lv_obj_t *stats = text(s_content, "", 96, UI_INK);
        lv_label_set_text_fmt(stats, "过关 %u / 连中 %u", s_state.won ? NR_LEVELS : s_state.level, s_state.best_streak);
        lv_obj_t *seed = text(s_content, "", 124, UI_INK);
        lv_label_set_text_fmt(seed, "同题挑战 %04u", s_state.challenge);
        text(s_content, s_state.new_best ? "新纪录！给朋友试试" : "确定同题，再破纪录", 154, 0x168B79);
        text(s_content, "上键换题 / 下键返回", 184, UI_INK);
        lv_label_set_text(s_footer, "确定再来 / 长按返回");
    } else if (s_state.page == NR_PAUSED) {
        text(s_content, "已暂停，空隙等着你", 3, UI_INK);
        ui_pixel_mascot_create(s_content, 80, 43);
        text(s_content, "上键继续", 114, 0x168B79);
        text(s_content, "下键返回开始页", 148, UI_INK);
        text(s_content, "圆盘数字是剩余针数", 184, UI_INK);
        lv_label_set_text(s_footer, "歇一会儿，手会更稳");
    } else {
        lv_obj_t *stats = text(s_content, "", 0, UI_INK);
        lv_label_set_text_fmt(stats, "第 %02u/10 关   机会 %u", s_state.level + 1, s_state.lives);
        wheel(false);
        const char *feedback = "看准空隙，确定发射";
        uint32_t color = UI_INK;
        if (s_state.page == NR_FEEDBACK) {
            feedback = s_state.hit ? "撞针了！慢一点" : !s_state.remaining ? "过关！下一关反向转" :
                s_state.streak % 3 == 0 ? "三连稳准！加五分" : "稳稳插中！";
            color = s_state.hit ? UI_RED : 0x168B79;
        }
        lv_obj_t *status = text(s_content, feedback, 181, color);
        if (s_state.page == NR_SPIN && s_state.score)
            lv_label_set_text_fmt(status, "得分 %u / 连中 %u", s_state.score, s_state.streak);
        lv_label_set_text(s_footer, "确定发射 / 上键暂停");
    }
    if (!s_buttons) lv_label_set_text(s_footer, "按键不可用，请重启");
}
static void battery(lv_timer_t *timer)
{
    (void)timer;
    if (!s_battery || (s_state.page != NR_HOME && s_state.page != NR_RESULT && s_state.page != NR_PAUSED)) return;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}
static bool handle(input_t input, int64_t now)
{
    s_last_activity = now;
    if (s_off) {
        s_off = s_dimmed = false; s_suppress_long = true;
        bsp_display_backlight(100); return false;
    }
    if (s_dimmed) { s_dimmed = false; bsp_display_backlight(100); }
    if (input.event == BSP_BTN_LONG) {
        if (s_suppress_long) return false;
        nr_home(&s_state); return true;
    }
    s_suppress_long = false;
    if (s_state.page == NR_HOME) {
        if (input.button == BSP_BTN_OK) nr_start(&s_state, (unsigned)(now % 10000));
        else s_state.mode ^= 1U;
    } else if (s_state.page == NR_RESULT) {
        if (input.button == BSP_BTN_OK) nr_start(&s_state, s_state.challenge);
        else if (input.button == BSP_BTN_UP) nr_start(&s_state, (s_state.challenge + 1) % 10000);
        else nr_home(&s_state);
    } else if (input.button == BSP_BTN_UP) nr_pause(&s_state);
    else if (input.button == BSP_BTN_DOWN && s_state.page == NR_PAUSED) nr_home(&s_state);
    else if (input.button == BSP_BTN_OK) return nr_fire(&s_state);
    else return false;
    return true;
}
static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms();
    uint32_t elapsed = (uint32_t)(now - s_last_frame);
    s_last_frame = now;
    input_t input;
    bool handled = false, dirty = false;
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &input, 0) == pdTRUE; i++) {
        if (!handled && now - input.at <= 200) {
            dirty = handle(input, now); handled = true;
        }
    }
    nr_page_t previous = s_state.page;
    if (!dirty) nr_tick(&s_state, elapsed);
    dirty |= previous != s_state.page;
    if (!s_dimmed && now - s_last_activity >= 60000) {
        if (s_state.page == NR_SPIN || s_state.page == NR_FLIGHT || s_state.page == NR_FEEDBACK) {
            nr_pause(&s_state); dirty = true;
        }
        s_dimmed = true; bsp_display_backlight(20);
    }
    if (!s_off && now - s_last_activity >= 180000) {
        s_off = true; bsp_display_backlight(0);
    }
    if (dirty) render();
    if (s_state.page == NR_SPIN)
        for (unsigned i = 0; i < s_state.count; i++) pin_position(i, (int)(nr_angle(&s_state, i) / 1000));
    if (s_shot && s_state.page == NR_FLIGHT)
        lv_obj_set_y(s_shot, 132 - (int)(s_state.elapsed * 27 / NR_FLIGHT_MS));
}
void needle_rush_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t), s_queue_storage, &s_queue_control);
}
void needle_rush_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept)) return;
    if (event != BSP_BTN_PRESS && !(button == BSP_BTN_OK && event == BSP_BTN_LONG)) return;
    input_t input = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}
void needle_rush_enter(bool buttons_available)
{
    if (s_screen) return;
    needle_rush_prepare(); nr_home(&s_state);
    s_buttons = buttons_available;
    s_dimmed = s_off = s_suppress_long = false;
    s_last_frame = s_last_activity = now_ms();
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = text(s_screen, "再插一针", 15, 0xFFFFFF);
    lv_obj_set_x(title, 5); lv_obj_set_width(title, 151);
    s_battery = ui_pixel_label(s_screen, "--%", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_battery, 163, 31); lv_obj_set_width(s_battery, 65);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 220, 226, UI_PAPER);
    s_footer = text(s_screen, "", 294, UI_INK);
    lv_obj_set_x(s_footer, 4); lv_obj_set_width(s_footer, 232);
    render(); battery(NULL);
    s_timer = lv_timer_create(frame, 20, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen); bsp_display_backlight(100);
    xQueueReset(s_queue); atomic_store(&s_accept, buttons_available);
}
void needle_rush_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    s_content = s_battery = s_footer = s_shot = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}
