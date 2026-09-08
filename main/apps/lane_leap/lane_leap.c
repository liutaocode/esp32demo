#include "lane_leap.h"
#include "lane_leap_state.h"
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

LV_FONT_DECLARE(lane_leap_zh_16);
LV_FONT_DECLARE(lane_leap_zh_12);
#define ROAD 0x20334C
#define TEAL 0x187967
#define GOLD 0xFFD16B
#define CYAN 0x6FE8E1
#define CORAL 0xF17E73

typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } ll_input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[8 * sizeof(ll_input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static ll_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery, *s_placeholder;
static lv_obj_t *s_board, *s_stats, *s_notice, *s_car, *s_shadow, *s_meter;
static lv_obj_t *s_items[LL_ROWS][LL_LANES], *s_marks[10];
static ll_kind_t s_styles[LL_ROWS][LL_LANES];
static lv_timer_t *s_timer, *s_battery_timer;
static bool s_buttons, s_dimmed, s_off;
static int64_t s_last_frame, s_last_activity;
static uint32_t s_seed_counter = 7421;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 0, 0); lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}
static lv_obj_t *text(lv_obj_t *p, const char *str, int x, int y, int w, uint32_t color, bool small)
{
    lv_obj_t *o = ui_pixel_label(p, str, small ? &lane_leap_zh_12 : &lane_leap_zh_16, color);
    lv_obj_set_pos(o, x, y); lv_obj_set_width(o, w);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}
static void line(const char *str, int y, uint32_t color) { text(s_content, str, 0, y, 198, color, false); }
static void visible(lv_obj_t *o, bool show)
{
    if (show) lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}
static void set_text(lv_obj_t *o, const char *str)
{
    if (strcmp(lv_label_get_text(o), str)) lv_label_set_text(o, str);
}
static lv_obj_t *car(lv_obj_t *p, int x, int y, bool truck)
{
    lv_obj_t *o = box(p, x, y, 28, 34, truck ? CORAL : CYAN);
    lv_obj_set_style_radius(o, 5, 0);
    box(o, 0, 6, 4, 9, UI_INK); box(o, 24, 6, 4, 9, UI_INK);
    box(o, 0, 23, 4, 8, UI_INK); box(o, 24, 23, 4, 8, UI_INK);
    box(o, 7, 7, 14, truck ? 6 : 9, ROAD);
    box(o, 6, 2, 4, 3, 0xFFFFFF); box(o, 18, 2, 4, 3, 0xFFFFFF);
    box(o, 9, 20, 10, truck ? 11 : 6, truck ? 0xB34751 : 0x209899);
    return o;
}
static void item_style(lv_obj_t *o, ll_kind_t kind)
{
    lv_obj_clean(o);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    if (kind == LL_TRUCK) car(o, 7, 1, true);
    else if (kind == LL_BARRIER) {
        box(o, 2, 12, 38, 12, GOLD);
        for (int x = 5; x < 38; x += 12) box(o, x, 12, 5, 12, 0x815333);
        box(o, 5, 24, 4, 5, 0xE9DAC3); box(o, 33, 24, 4, 5, 0xE9DAC3);
    } else if (kind == LL_GAP) {
        box(o, 0, 9, 42, 20, 0x0A1527);
        box(o, 0, 7, 42, 2, CYAN); box(o, 0, 29, 42, 2, CYAN);
        for (int x = 3; x < 40; x += 9) box(o, x, 10, 3, 3, 0x4E6879);
    } else if (kind == LL_COIN || kind == LL_AIR_COIN) {
        int y = kind == LL_AIR_COIN ? 2 : 12;
        lv_obj_t *coin = box(o, 13, y, 16, 16, GOLD);
        lv_obj_set_style_radius(coin, LV_RADIUS_CIRCLE, 0);
        box(coin, 7, 4, 2, 8, 0x9D682F);
        if (kind == LL_AIR_COIN) {
            box(o, 4, 6, 7, 3, CYAN); box(o, 31, 6, 7, 3, CYAN);
            box(o, 17, 29, 9, 3, 0x13243B);
        }
    }
}
static void board_create(void)
{
    s_stats = text(s_content, "", 0, 0, 198, UI_INK, false);
    s_board = box(s_content, 0, 24, LL_WIDTH, LL_HEIGHT, ROAD);
    box(s_board, 0, 0, 3, LL_HEIGHT, 0x729989);
    box(s_board, 195, 0, 3, LL_HEIGHT, 0x729989);
    for (unsigned i = 0; i < 10; i++) s_marks[i] = box(s_board, i % 2 ? 131 : 65, 0, 2, 14, 0x50647A);
    for (unsigned r = 0; r < LL_ROWS; r++) for (unsigned l = 0; l < LL_LANES; l++) {
        s_items[r][l] = box(s_board, l * 66 + 12, 0, 42, 36, ROAD);
        s_styles[r][l] = (ll_kind_t)99;
    }
    s_shadow = box(s_board, 0, LL_PLAYER_Y + 9, 26, 9, 0x101D30);
    lv_obj_set_style_radius(s_shadow, LV_RADIUS_CIRCLE, 0);
    s_car = car(s_board, 85, LL_PLAYER_Y - 17, false);
    s_notice = text(s_board, "", 0, 4, 198, 0xFFFFFF, true);
    lv_obj_set_style_bg_color(s_notice, lv_color_hex(ROAD), 0);
    lv_obj_set_style_bg_opa(s_notice, LV_OPA_90, 0);
    s_meter = box(s_board, 0, 178, 198, 4, CYAN);
}
static void board_update(void)
{
    char str[96];
    snprintf(str, sizeof(str), "分%u  命%u  倍%u", s_state.score, s_state.hp, ll_multiplier(&s_state));
    set_text(s_stats, str);
    for (unsigned i = 0; i < 10; i++)
        lv_obj_set_y(s_marks[i], ((int)(s_state.distance * 4) + (int)(i / 2) * 42) % 210 - 15);
    for (unsigned r = 0; r < LL_ROWS; r++) for (unsigned l = 0; l < LL_LANES; l++) {
        ll_row_t *row = &s_state.row[r];
        ll_kind_t kind = row->kind[l];
        visible(s_items[r][l], row->active && !row->taken[l] && kind != LL_EMPTY);
        if (kind != s_styles[r][l]) { item_style(s_items[r][l], kind); s_styles[r][l] = kind; }
        lv_obj_set_y(s_items[r][l], (int)row->y - 18);
    }
    int x = (int)s_state.lane * 66 + 19;
    lv_obj_set_pos(s_car, x, LL_PLAYER_Y - 17 - (int)ll_height(&s_state));
    lv_obj_set_x(s_shadow, x + 1);
    lv_obj_set_style_opa(s_car, s_state.invincible_ms && s_state.invincible_ms % 200 < 100 ? LV_OPA_50 : LV_OPA_COVER, 0);
    unsigned ready = s_state.jump_ms ? LL_JUMP_MS - s_state.jump_ms : s_state.cooldown_ms ? 0 : LL_JUMP_MS;
    lv_obj_set_width(s_meter, 1 + (197 * ready / LL_JUMP_MS));
    if (s_state.page == LL_READY) {
        snprintf(str, sizeof(str), "准备出发  %u", (s_state.countdown_ms + 499) / 500);
    } else if (s_state.notice_ms) {
        const char *msg[] = {"", "金币到手", "空中金币！", "漂亮飞跃！", "擦碰！还有机会"};
        snprintf(str, sizeof(str), "%s", msg[s_state.notice <= 4 ? s_state.notice : 0]);
    } else snprintf(str, sizeof(str), "速度%u档  连续无伤%u", ll_level(&s_state), s_state.streak);
    set_text(s_notice, str);
    set_text(s_footer, "上左 下右 确定跳 长按停");
}
static void render(void)
{
    if (s_board && (s_state.page == LL_RACING || s_state.page == LL_READY)) { board_update(); return; }
    s_board = NULL;
    lv_obj_clean(s_content);
    if (s_state.page == LL_HOME) {
        line("三条车道，一跃上头", 0, TEAL);
        lv_obj_t *art = box(s_content, 0, 28, 198, 53, ROAD);
        box(art, 65, 0, 2, 53, 0x50647A); box(art, 131, 0, 2, 53, 0x50647A);
        car(art, 86, 8, false);
        lv_obj_t *o = box(art, 12, 8, 42, 36, ROAD); item_style(o, LL_BARRIER);
        o = box(art, 144, 8, 42, 36, ROAD); item_style(o, LL_AIR_COIN);
        line("上键向左，下键向右", 89, UI_INK);
        line("确定跳跃，空中可换道", 112, UI_INK);
        line("货车要躲，路障可跳", 135, TEAL);
        for (unsigned i = 0; i < 2; i++) {
            lv_obj_t *p = box(s_content, i * 102, 161, 96, 27, s_state.mode == i ? TEAL : 0xE5DECA);
            text(p, i ? "极速挑战" : "悠闲兜风", 0, 3, 96, s_state.mode == i ? 0xFFFFFF : UI_INK, false);
        }
        char str[80]; snprintf(str, sizeof(str), "开机最高 %u / 长按暂停", s_state.best[s_state.mode]);
        text(s_content, str, 0, 193, 198, TEAL, true);
        set_text(s_footer, "上下选模式 / 确定出发");
    } else if (s_state.page == LL_PAUSED) {
        line("靠边歇一会", 3, TEAL);
        for (unsigned i = 0; i < 3; i++) {
            lv_obj_t *o = box(s_content, 4, 31 + i * 40, 42, 36, UI_PAPER);
            item_style(o, i == 0 ? LL_TRUCK : i == 1 ? LL_GAP : LL_AIR_COIN);
            text(s_content, i == 0 ? "货车：换道躲开" : i == 1 ? "断路：跳跃越过" : "飞币：起跳收集", 47, 40 + i * 40, 151, UI_INK, false);
        }
        line("连续无伤，奖励翻倍", 163, TEAL);
        text(s_content, "长按确定暂停，松开再按继续", 0, 193, 198, UI_INK, true);
        set_text(s_footer, "确定继续 / 下键回首页");
    } else if (s_state.page == LL_RESULT) {
        line(s_state.new_best ? "新纪录！这把真飞了" : "再跑一趟，就能更远", 0, TEAL);
        lv_obj_t *n = text(s_content, "", 0, 28, 198, 0xC25343, false);
        lv_obj_set_style_text_font(n, &lv_font_montserrat_20, 0);
        lv_label_set_text_fmt(n, "%u", s_state.score);
        line(s_state.cleared >= 60 ? "称号：车道飞行家" : s_state.cleared >= 25 ? "称号：街头小车神" : "称号：兜风新司机", 57, UI_INK);
        char str[80]; snprintf(str, sizeof(str), "金币%u  飞跃%u", s_state.coins, s_state.jumps); line(str, 87, UI_INK);
        snprintf(str, sizeof(str), "最久无伤 %u 组", s_state.best_streak); line(str, 113, TEAL);
        text(s_content, "确定重跑同路线，练好这一关", 0, 150, 198, UI_INK, true);
        text(s_content, "上键换条路，下键回首页", 0, 175, 198, UI_INK, true);
        set_text(s_footer, "递给朋友，比比谁更能跑");
    } else { board_create(); board_update(); }
    if (!s_buttons) set_text(s_footer, "按键不可用，请重启");
}
static void battery(lv_timer_t *timer)
{
    (void)timer;
    if (!s_battery || s_state.page == LL_RACING || s_state.page == LL_READY) return;
    int soc = bsp_battery_soc();
    if (soc < 0) set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}
static uint32_t new_seed(void) { return (uint32_t)now_ms() ^ (++s_seed_counter * 2654435761U); }
static void handle(ll_input_t input, int64_t now)
{
    s_last_activity = now;
    if (s_dimmed || s_off) {
        bsp_display_backlight(100); s_dimmed = s_off = false;
        xQueueReset(s_queue); return;
    }
    if (input.event == BSP_BTN_LONG) {
        if (s_state.page == LL_RACING || s_state.page == LL_READY) ll_pause(&s_state);
        return;
    }
    bsp_btn_t b = input.button;
    switch (s_state.page) {
    case LL_HOME:
        if (b == BSP_BTN_OK) ll_start(&s_state, new_seed()); else s_state.mode ^= 1;
        break;
    case LL_RACING:
        if (b == BSP_BTN_OK) ll_jump(&s_state); else ll_move(&s_state, b == BSP_BTN_UP ? -1 : 1);
        break;
    case LL_PAUSED:
        if (b == BSP_BTN_OK) ll_pause(&s_state); else if (b == BSP_BTN_DOWN) ll_home(&s_state);
        break;
    case LL_RESULT:
        if (s_state.result_ms < 450) break;
        if (b == BSP_BTN_OK) ll_start(&s_state, s_state.seed);
        else if (b == BSP_BTN_UP) ll_start(&s_state, new_seed()); else ll_home(&s_state);
        break;
    case LL_READY: break;
    }
}
static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms(), elapsed = now - s_last_frame; s_last_frame = now;
    ll_page_t before = s_state.page;
    unsigned mode = s_state.mode;
    ll_input_t input;
    for (unsigned i = 0; i < 8 && xQueueReceive(s_queue, &input, 0) == pdTRUE; i++) {
        if (now < input.at || now - input.at > 500) continue;
        ll_page_t page = s_state.page;
        handle(input, now);
        if (page != s_state.page) { xQueueReset(s_queue); break; }
    }
    ll_tick(&s_state, elapsed < 0 ? 0 : elapsed > 100 ? 100 : (unsigned)elapsed);
    if (before != s_state.page || mode != s_state.mode) render();
    else if (s_board) board_update();
    if (s_state.page == LL_HOME || s_state.page == LL_PAUSED || s_state.page == LL_RESULT) {
        if (!s_off && now - s_last_activity >= 180000) { bsp_display_backlight(0); s_off = true; }
        else if (!s_off && !s_dimmed && now - s_last_activity >= 60000) { bsp_display_backlight(20); s_dimmed = true; }
    } else s_last_activity = now;
}
void lane_leap_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(8, sizeof(ll_input_t), s_queue_storage, &s_queue_control);
}
void lane_leap_key(bsp_btn_t b, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept)) return;
    if (b != BSP_BTN_OK && b != BSP_BTN_UP && b != BSP_BTN_DOWN) return;
    if (event != BSP_BTN_PRESS && !(event == BSP_BTN_LONG && b == BSP_BTN_OK)) return;
    ll_input_t input = {b, event, now_ms()}; (void)xQueueSend(s_queue, &input, 0);
}
void lane_leap_enter(bool buttons_available)
{
    if (s_screen) return;
    lane_leap_prepare(); s_buttons = buttons_available; s_dimmed = s_off = false;
    s_last_frame = s_last_activity = now_ms(); ll_home(&s_state);
    s_screen = ui_pixel_screen_create("");
    text(s_screen, "飞跃车道", 5, 15, 151, 0xFFFFFF, false);
    s_battery = text(s_screen, "--%", 164, 31, 63, UI_INK, false);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 220, 230, UI_PAPER);
    s_footer = text(s_screen, "", 4, 294, 232, UI_INK, false);
    render(); battery(NULL);
    s_timer = lv_timer_create(frame, 20, NULL); s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen);
    if (s_placeholder) { lv_obj_delete(s_placeholder); s_placeholder = NULL; }
    bsp_display_backlight(100); xQueueReset(s_queue); atomic_store(&s_accept, buttons_available);
}
void lane_leap_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    if (s_screen) {
        if (lv_screen_active() == s_screen) { s_placeholder = lv_obj_create(NULL); lv_screen_load(s_placeholder); }
        lv_obj_delete(s_screen);
    }
    s_screen = s_content = s_footer = s_battery = s_board = NULL;
    if (s_queue) xQueueReset(s_queue);
}
