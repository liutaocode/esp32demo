#include "ricochet_rush.h"
#include "ricochet_rush_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(ricochet_rush_zh_16);
#define NAVY 0x172C42
#define TEAL 0x187967
#define GOLD 0xFFCF68
#define CORAL 0xE76B60

typedef struct { bsp_btn_t button; int64_t at; } rr_input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[4 * sizeof(rr_input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static rr_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery, *s_placeholder;
static lv_obj_t *s_board, *s_stats, *s_notice;
static lv_obj_t *s_tiles[RR_CELLS], *s_numbers[RR_CELLS], *s_balls[RR_BALLS], *s_dots[17];
static unsigned s_tile_style[RR_CELLS];
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_last_frame, s_last_activity;
static bool s_buttons, s_dimmed, s_off;
static uint32_t s_seed_counter = 12345;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static lv_obj_t *box(lv_obj_t *p, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}

static lv_obj_t *text(lv_obj_t *p, const char *str, int x, int y, int w, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(p, str, &ricochet_rush_zh_16, color);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}

static lv_obj_t *line(const char *str, int y, uint32_t color)
{
    return text(s_content, str, 0, y, 198, color);
}
static void visible(lv_obj_t *o, bool show)
{
    if (show) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
static void set_text(lv_obj_t *o, const char *str)
{
    if (strcmp(lv_label_get_text(o), str)) lv_label_set_text(o, str);
}
static uint32_t new_seed(void) { return (uint32_t)now_ms() ^ (++s_seed_counter * 2654435761U); }
static void board_create(void)
{
    s_stats = line("", 0, UI_INK);
    s_board = box(s_content, 0, 26, RR_WIDTH, RR_HEIGHT, NAVY);
    for (unsigned i = 0; i < RR_CELLS; i++) {
        float x,y,w,h; rr_rect(i,&x,&y,&w,&h);
        s_tiles[i] = box(s_board, (int)x, (int)y, (int)w, (int)h, GOLD);
        lv_obj_set_style_radius(s_tiles[i], 3, 0);
        s_numbers[i] = text(s_tiles[i], "", 0, 0, (int)w, NAVY);
        lv_obj_set_style_text_font(s_numbers[i], &lv_font_montserrat_14, 0);
        s_tile_style[i] = 0xffffffffU;
    }
    for (int x = 2; x < 195; x += 11) box(s_board, x, 157, 6, 1, CORAL);
    for (unsigned i = 0; i < 17; i++) {
        s_dots[i] = box(s_board, 96, 160, 3, 3, 0x87DFB9);
        lv_obj_set_style_radius(s_dots[i], LV_RADIUS_CIRCLE, 0);
    }
    for (unsigned i = 0; i < RR_BALLS; i++) {
        s_balls[i] = box(s_board, 96, 163, 5, 5, 0xFFFFFF);
        lv_obj_set_style_radius(s_balls[i], LV_RADIUS_CIRCLE, 0);
    }
    s_notice = text(s_board, "", 1, 70, 196, 0xFFFFFF);
    lv_obj_set_style_bg_color(s_notice, lv_color_hex(NAVY), 0);
    lv_obj_set_style_bg_opa(s_notice, LV_OPA_90, 0);
}
static void board_update(void)
{
    char str[64];
    snprintf(str, sizeof(str), "第%u轮  球%u  分%u", s_state.round, s_state.balls, s_state.score);
    set_text(s_stats, str);
    for (unsigned i = 0; i < RR_CELLS; i++) {
        unsigned style = s_state.hp[i] | (s_state.pickup[i] ? 0x10000U : 0) |
                         (s_state.flash[i] ? 0x20000U : 0);
        visible(s_tiles[i], s_state.hp[i] || s_state.pickup[i]);
        if (style == s_tile_style[i]) continue;
        s_tile_style[i] = style;
        bool pickup = s_state.pickup[i];
        uint32_t color = pickup ? 0x78D8AC : s_state.hp[i] > 14 ? CORAL :
                         s_state.hp[i] > 7 ? 0xECA86C : GOLD;
        if (s_state.flash[i]) color = 0xFFFFFF;
        lv_obj_set_style_bg_color(s_tiles[i], lv_color_hex(color), 0);
        lv_obj_set_style_radius(s_tiles[i], pickup ? 9 : 3, 0);
        if (pickup) set_text(s_numbers[i], "+");
        else { snprintf(str, sizeof(str), "%u", s_state.hp[i]); set_text(s_numbers[i], str); }
    }
    float xy[34];
    unsigned dots = s_state.page == RR_AIM ? rr_preview(&s_state, xy, 17) : 0;
    for (unsigned i = 0; i < 17; i++) {
        visible(s_dots[i], i < dots);
        if (i < dots) lv_obj_set_pos(s_dots[i], (int)xy[2*i] - 1, (int)xy[2*i+1] - 1);
    }
    for (unsigned i = 0; i < RR_BALLS; i++) {
        bool aim_ball = s_state.page == RR_AIM && i == 0;
        bool flying = s_state.page == RR_FLIGHT && s_state.ball[i].active;
        visible(s_balls[i], aim_ball || flying);
        if (aim_ball) lv_obj_set_pos(s_balls[i], (int)s_state.launch_x - 2, 164);
        else if (flying) lv_obj_set_pos(s_balls[i], (int)s_state.ball[i].x - 2, (int)s_state.ball[i].y - 2);
    }
    visible(s_notice, s_state.page == RR_SETTLE);
    if (s_state.page == RR_SETTLE) {
        if (s_state.gained) snprintf(str, sizeof(str), "命中%u次  加球%u颗", s_state.hits, s_state.gained);
        else snprintf(str, sizeof(str), "命中%u次  再弹一轮", s_state.hits);
        set_text(s_notice, str);
    }
    if (s_state.page == RR_FLIGHT) {
        snprintf(str, sizeof(str), "连弹%u次 / 下键%s", s_state.hits, s_state.fast ? "常速" : "加速");
        set_text(s_footer, str);
    }
}
static void render(void)
{
    /* Keep the board object pool across aim/flight/settle transitions. */
    if (s_board && (s_state.page == RR_AIM || s_state.page == RR_FLIGHT || s_state.page == RR_SETTLE)) {
        board_update();
        if (s_state.page == RR_AIM) set_text(s_footer, "确定发射 / 上键暂停");
        if (s_state.page == RR_SETTLE) set_text(s_footer, s_state.recalled ? "小球归队，准备下一轮" : "小球归队，砖块下一行");
        return;
    }
    s_board = s_stats = s_notice = NULL;
    lv_obj_clean(s_content);
    if (s_state.page == RR_HOME) {
        line("一键发射，弹到停不下", 0, TEAL);
        lv_obj_t *art = box(s_content, 0, 29, 137, 67, NAVY);
        for (unsigned i = 0; i < 4; i++) {
            lv_obj_t *tile = box(art, 7 + i * 32, 8, 25, 22, i == 2 ? CORAL : GOLD);
            lv_obj_t *n = text(tile, i == 2 ? "8" : "3", 0, 2, 25, NAVY);
            lv_obj_set_style_text_font(n, &lv_font_montserrat_14, 0);
            box(art, 12 + i * 29, 48 - i * 5, 4, 4, 0xFFFFFF);
        }
        ui_pixel_mascot_create(s_content, 153, 40);
        line("看准虚线，确定发射", 105, UI_INK);
        line("吃加号加球，砖别触底", 128, UI_INK);
        line("上键暂停，下键调方向", 151, TEAL);
        for (unsigned i = 0; i < 2; i++) {
            lv_obj_t *p = box(s_content, i * 102, 176, 96, 27, s_state.mode == i ? TEAL : 0xE5DECA);
            text(p, i ? "高手挑战" : "轻松开弹", 0, 3, 96, s_state.mode == i ? 0xFFFFFF : UI_INK);
        }
        set_text(s_footer, "上下选模式 / 确定开始");
    } else if (s_state.page == RR_PAUSED) {
        line("已暂停，小球等你", 5, TEAL);
        ui_pixel_mascot_create(s_content, 80, 37);
        line("确定发射，上键暂停", 99, UI_INK);
        line("瞄准时：下键调方向", 124, UI_INK);
        line("飞行时：下键切加速", 149, UI_INK);
        line("守住红线，挑战三十轮", 177, TEAL);
        set_text(s_footer, "上键继续 / 下键返回");
    } else if (s_state.page == RR_RESULT) {
        line(s_state.won ? "三十轮通关！太能弹了" : "就差一点，再弹一轮", 0, TEAL);
        lv_obj_t *score = line("", 28, CORAL);
        lv_obj_set_style_text_font(score, &lv_font_montserrat_20, 0);
        lv_label_set_text_fmt(score, "%u", s_state.score);
        line(s_state.won ? "称号：反弹大魔王" : s_state.round >= 20 ? "称号：弹球高手" :
             s_state.round >= 10 ? "称号：反弹新星" : "称号：开弹练习生", 56, UI_INK);
        lv_obj_t *stats = line("", 85, UI_INK);
        lv_label_set_text_fmt(stats, "抵达%u轮  消除%u块", s_state.round, s_state.cleared);
        stats = line("", 112, UI_INK);
        lv_label_set_text_fmt(stats, "单轮最多命中%u次", s_state.best_hits);
        stats = line("", 139, TEAL);
        lv_label_set_text_fmt(stats, "%s %u", s_state.new_best ? "新纪录" : "本次开机最高", s_state.best[s_state.mode]);
        line("确定同盘再来，练准角度", 169, UI_INK);
        line("上键换一盘，下键返回", 190, TEAL);
        set_text(s_footer, "把机器递给朋友，来比比");
    } else {
        board_create(); board_update();
        if (s_state.page == RR_AIM) set_text(s_footer, "确定发射 / 上键暂停");
        if (s_state.page == RR_SETTLE) set_text(s_footer, s_state.recalled ? "小球归队，准备下一轮" : "小球归队，砖块下一行");
    }
    if (!s_buttons) set_text(s_footer, "按键不可用，请重启");
}
static void battery(lv_timer_t *timer)
{
    (void)timer;
    /* Sampling may block on I2C: skip while the player judges a moving aim. */
    if (!s_battery || s_state.page == RR_AIM || s_state.page == RR_FLIGHT) return;
    int soc = bsp_battery_soc();
    if (soc < 0) set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}
static bool handle(bsp_btn_t b, int64_t now)
{
    s_last_activity = now;
    bool wake = s_dimmed || s_off;
    if (wake) bsp_display_backlight(100);
    s_dimmed = s_off = false;
    if (wake) return false;
    switch (s_state.page) {
    case RR_HOME:
        if (b == BSP_BTN_OK) rr_start(&s_state, new_seed()); else s_state.mode ^= 1;
        return true;
    case RR_PAUSED:
        if (b == BSP_BTN_UP) rr_pause(&s_state);
        else if (b == BSP_BTN_DOWN) rr_home(&s_state);
        else return false;
        return true;
    case RR_RESULT:
        if (b == BSP_BTN_OK) rr_start(&s_state, s_state.seed);
        else if (b == BSP_BTN_UP) rr_start(&s_state, new_seed());
        else rr_home(&s_state);
        return true;
    default:
        if (b == BSP_BTN_UP) { rr_pause(&s_state); return true; }
        if (b == BSP_BTN_DOWN) { rr_reverse(&s_state); return false; }
        return b == BSP_BTN_OK && rr_fire(&s_state);
    }
}
static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms(), elapsed = now - s_last_frame;
    s_last_frame = now;
    bool dirty = false, handled = false;
    /* A slow LCD refresh/USB capture is not a pause request. The bounded
     * tick below limits catch-up without repeatedly pausing after a redraw. */
    rr_input_t input;
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &input, 0) == pdTRUE; i++) {
        if (!handled && now >= input.at && now - input.at <= 1000 && !dirty) {
            dirty = handle(input.button, now); handled = true;
        }
    }
    rr_page_t before = s_state.page;
    if (!dirty) rr_tick(&s_state, elapsed < 0 ? 0 : elapsed > 50 ? 50 : (unsigned)elapsed);
    dirty |= s_state.page != before;
    bool idle_page = s_state.page == RR_HOME || s_state.page == RR_PAUSED || s_state.page == RR_RESULT;
    if (idle_page && !s_dimmed && now - s_last_activity >= 60000) {
        dirty = true;
        bsp_display_backlight(20); s_dimmed = true;
    }
    if (idle_page && !s_off && now - s_last_activity >= 180000) { bsp_display_backlight(0); s_off = true; }
    if (dirty) render();
    else if (s_board) board_update();
}
void ricochet_rush_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(rr_input_t), s_queue_storage, &s_queue_control);
}
void ricochet_rush_key(bsp_btn_t b, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_PRESS || !atomic_load(&s_accept)) return;
    if (b != BSP_BTN_OK && b != BSP_BTN_UP && b != BSP_BTN_DOWN) return;
    rr_input_t input = {b, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}
void ricochet_rush_enter(bool buttons_available)
{
    if (s_screen) return;
    ricochet_rush_prepare();
    s_buttons = buttons_available; s_dimmed = s_off = false;
    s_last_frame = s_last_activity = now_ms();
    rr_home(&s_state);
    s_screen = ui_pixel_screen_create("");
    text(s_screen, "再弹一轮", 5, 15, 151, 0xFFFFFF);
    s_battery = text(s_screen, "--%", 162, 31, 65, UI_INK);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 220, 226, UI_PAPER);
    s_footer = text(s_screen, "", 4, 294, 232, UI_INK);
    render(); battery(NULL);
    s_timer = lv_timer_create(frame, 20, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen);
    if (s_placeholder) { lv_obj_delete(s_placeholder); s_placeholder = NULL; }
    bsp_display_backlight(100);
    xQueueReset(s_queue); atomic_store(&s_accept, buttons_available);
}
void ricochet_rush_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    if (s_screen) {
        if (lv_screen_active() == s_screen) {
            s_placeholder = lv_obj_create(NULL); lv_screen_load(s_placeholder);
        }
        lv_obj_delete(s_screen);
    }
    s_screen = s_content = s_footer = s_battery = s_board = s_stats = s_notice = NULL;
    xQueueReset(s_queue);
}
