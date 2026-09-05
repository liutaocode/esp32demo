#include "down_100.h"
#include "down_100_state.h"
#include "down_100_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"
#include <stdatomic.h>
#include <stdio.h>

LV_FONT_DECLARE(down_100_zh_16);
typedef struct { bsp_btn_t key; int64_t at; } input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static d100_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery, *s_stats, *s_game;
static lv_obj_t *s_field, *s_player, *s_tiles[D100_ROWS][3], *s_art[D100_ROWS][3];
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_last_frame, s_last_activity;
static int s_visual_x;
static uint16_t s_art_style[D100_ROWS][3];
static uint16_t s_drawn_floor, s_drawn_score;
static uint8_t s_drawn_health;
static bool s_buttons, s_dimmed, s_off;
static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }
static lv_obj_t *rect(lv_obj_t *p, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(p); lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}
static lv_obj_t *text(lv_obj_t *p, const char *str, int y, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(p, str, &down_100_zh_16, color);
    lv_obj_set_pos(o, 0, y); lv_obj_set_width(o, D100_FIELD_W);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}
enum { GAME_W = 240, GAME_H = 294, HUD_H = 26 };
static int game_x(int x) { return x * GAME_W / D100_FIELD_W; }
static int game_y(int y) { return y * GAME_H / D100_FIELD_H; }
static lv_obj_t *miner(lv_obj_t *p, int x, int y, bool large)
{
    static const struct { int x, y, w, h; uint32_t color; } parts[] = {
        {3, 0, 12, 3, UI_YELLOW}, {0, 3, 18, 4, UI_ORANGE},
        {7, 2, 5, 4, 0xFFFCE5}, {3, 7, 12, 8, 0xFFD2A0},
        {5, 9, 2, 3, UI_INK}, {12, 9, 2, 3, UI_INK},
        {4, 15, 10, 5, 0x57D9DE}, {2, 20, 5, 2, UI_PAPER},
        {11, 20, 5, 2, UI_PAPER}
    };
    int w = large ? 26 : 18, h = large ? 32 : 22;
    lv_obj_t *o = rect(p, x, y, w, h, UI_INK);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    for (unsigned i = 0; i < sizeof(parts) / sizeof(parts[0]); i++)
        rect(o, parts[i].x * w / 18, parts[i].y * h / 22,
             parts[i].w * w / 18, parts[i].h * h / 22, parts[i].color);
    return o;
}
static const char *mode_name(void) { return s_state.mode ? "一命极限" : "三心探险"; }
static void art_rect(lv_layer_t *layer, const lv_area_t *origin,
                     int x, int y, int w, int h, uint32_t color)
{
    lv_draw_rect_dsc_t d; lv_draw_rect_dsc_init(&d);
    d.bg_color = lv_color_hex(color); d.bg_opa = LV_OPA_COVER;
    lv_area_t area = {origin->x1 + x, origin->y1 + y,
                      origin->x1 + x + w - 1, origin->y1 + y + h - 1};
    lv_draw_rect(layer, &d, &area);
}
static void art_triangle(lv_layer_t *layer, const lv_area_t *origin,
                         int x1, int y1, int x2, int y2, int x3, int y3, uint32_t color)
{
    lv_draw_triangle_dsc_t d; lv_draw_triangle_dsc_init(&d);
    d.color = lv_color_hex(color); d.opa = LV_OPA_COVER;
    d.p[0] = (lv_point_precise_t){origin->x1 + x1, origin->y1 + y1};
    d.p[1] = (lv_point_precise_t){origin->x1 + x2, origin->y1 + y2};
    d.p[2] = (lv_point_precise_t){origin->x1 + x3, origin->y1 + y3};
    lv_draw_triangle(layer, &d);
}
static void draw_tile_art(lv_event_t *event)
{
    unsigned slot = (unsigned)(uintptr_t)lv_event_get_user_data(event);
    d100_row_t *row = &s_state.rows[slot / 3];
    unsigned lane = slot % 3, tile = row->tile[lane];
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t area; lv_obj_get_coords(lv_event_get_target_obj(event), &area);
    if (tile == D100_SPIKE) {
        for (int x = 2; x < 60; x += 13) {
            art_triangle(layer, &area, x, 24, x + 6, 5, x + 12, 24, 0xDCE6F1);
            art_triangle(layer, &area, x + 6, 5, x + 6, 24, x + 12, 24, 0x839AB7);
            art_rect(layer, &area, x + 5, 14, 2, 5, 0xFFFFFF);
        }
    } else if (tile == D100_CRACK) {
        art_rect(layer, &area, 2, 28, 62, 2, 0x9B623F);
        art_rect(layer, &area, 6, 26, 3, 3, 0xFFE0A8);
        art_rect(layer, &area, 58, 33, 3, 3, 0xFFE0A8);
        for (int i = 0; i < 5; i++)
            art_rect(layer, &area, 28 + (i % 2) * 5, 24 + i * 3,
                     row->crack_ms[lane] > d100_crack_delay(row->depth) / 2 ? 7 : 4, 3, 0x182B49);
        art_rect(layer, &area, 40, 27, 8, 2, 0x704529);
    } else if (tile == D100_GEM && !(row->used & (1U << lane))) {
        art_triangle(layer, &area, 22, 8, 28, 2, 39, 2, 0xADFFFF);
        art_triangle(layer, &area, 22, 8, 39, 2, 45, 8, 0x64E5F0);
        art_triangle(layer, &area, 22, 8, 33, 23, 33, 8, 0x20A6C2);
        art_triangle(layer, &area, 33, 8, 33, 23, 45, 8, 0x51E8E8);
        art_rect(layer, &area, 28, 5, 4, 3, 0xFFFFFF);
    } else if (tile == D100_HEAL && !(row->used & (1U << lane))) {
        art_rect(layer, &area, 26, 1, 15, 4, 0xA5EFC0);
        art_rect(layer, &area, 23, 5, 21, 18, 0x255E54);
        art_rect(layer, &area, 25, 5, 17, 15, 0xB8F5CB);
        art_rect(layer, &area, 28, 8, 4, 3, 0xE6668B);
        art_rect(layer, &area, 35, 8, 4, 3, 0xE6668B);
        art_rect(layer, &area, 28, 11, 11, 3, 0xE6668B);
        art_rect(layer, &area, 30, 14, 7, 2, 0xE6668B);
        art_rect(layer, &area, 32, 16, 3, 2, 0xE6668B);
    }
}
static uint32_t tile_color(unsigned tile)
{
    switch (tile) {
        case D100_GEM: return 0x48D8DB;
        case D100_CRACK: return 0xE7AC68;
        case D100_SPIKE: return 0xFA6C88;
        case D100_HEAL: return 0x90DF94;
        default: return 0x95B6DE;
    }
}
static void live_update(void)
{
    if (!s_field) return;
    if (s_state.floor != s_drawn_floor || s_state.score != s_drawn_score || s_state.health != s_drawn_health) {
        lv_label_set_text_fmt(s_stats, "%03u层  心%u  %u分", s_state.floor, s_state.health, s_state.score);
        static const uint32_t colors[] = {0xDDF3E8, 0xF3E8AC, 0xFFD284, 0xFFAD76, 0xFF8592};
        lv_obj_set_style_text_color(s_stats, lv_color_hex(colors[d100_stage(s_state.floor)]), 0);
        s_drawn_floor = s_state.floor; s_drawn_score = s_state.score; s_drawn_health = s_state.health;
    }
    for (int i = 0; i < D100_ROWS; i++) for (int lane = 0; lane < 3; lane++) {
        d100_row_t *r = &s_state.rows[i];
        lv_obj_t *o = s_tiles[i][lane];
        int y = r->y / 1000;
        unsigned tile = r->tile[lane];
        if (tile == D100_HOLE || y < 17 || y + 9 >= D100_FIELD_H) {
            lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_art[i][lane], LV_OBJ_FLAG_HIDDEN); continue;
        }
        lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(o, game_y(y));
        lv_obj_set_y(s_art[i][lane], game_y(y) - 24);
        lv_obj_remove_flag(s_art[i][lane], LV_OBJ_FLAG_HIDDEN);
        uint16_t style = tile | ((r->used & (1U << lane)) ? 8U : 0U) |
            (r->crack_ms[lane] > d100_crack_delay(r->depth) / 2 ? 16U : 0U);
        if (style != s_art_style[i][lane]) {
            lv_obj_set_style_bg_color(o, lv_color_hex(tile_color(tile)), 0);
            lv_obj_invalidate(s_art[i][lane]);
            s_art_style[i][lane] = style;
        }

    }
    lv_obj_set_pos(s_player, game_x(s_visual_x) - 13, game_y(s_state.feet / 1000) - 32);
    lv_obj_set_style_opa(s_player, s_state.immune_ms && (s_state.active_ms / 100) % 2 ? LV_OPA_50 : LV_OPA_COVER, 0);
}
static void scene(void)
{
    s_drawn_floor = s_drawn_score = UINT16_MAX;
    s_drawn_health = UINT8_MAX;
    s_game = rect(s_screen, 0, 0, GAME_W, 320, 0x182B49);
    rect(s_game, 0, 0, GAME_W, HUD_H, 0x102039);
    s_stats = text(s_game, "", 5, UI_PAPER);
    lv_obj_set_x(s_stats, 6); lv_obj_set_width(s_stats, 184);
    lv_obj_set_style_text_align(s_stats, LV_TEXT_ALIGN_LEFT, 0);
    s_field = rect(s_game, 0, HUD_H, GAME_W, GAME_H, 0x182B49);
    for (int x = 3; x < 198; x += 64)
        rect(s_field, game_x(x), game_y(14), 1, GAME_H - game_y(14), 0x294462);
    for (int i = 0; i < 12; i++) {
        rect(s_field, i * 20, 0, 18, game_y(5), 0xFA6C88);
        rect(s_field, i * 20 + 4, game_y(5), 10, game_y(4), 0xFA6C88);
        rect(s_field, i * 20 + 8, game_y(9), 2, game_y(3), 0xFA6C88);
    }
    for (int i = 0; i < D100_ROWS; i++) for (int lane = 0; lane < 3; lane++) {
        lv_obj_t *o = rect(s_field, game_x(7 + lane * 64), 0, game_x(56), game_y(9), 0x95B6DE);
        s_tiles[i][lane] = o;
        s_art_style[i][lane] = UINT16_MAX;
        rect(o, 2, game_y(6), game_x(52), game_y(2), 0x53708F);
        /* Draw original geometric items without labels or per-pixel objects. */
        s_art[i][lane] = rect(s_field, game_x(7 + lane * 64), 0, game_x(56), 39, 0);
        lv_obj_set_style_bg_opa(s_art[i][lane], LV_OPA_TRANSP, 0);
        lv_obj_add_event_cb(s_art[i][lane], draw_tile_art, LV_EVENT_DRAW_MAIN,
                            (void *)(uintptr_t)(i * 3 + lane));
    }
    s_player = miner(s_field, 0, 0, true);
    lv_obj_move_foreground(s_battery);
    live_update();
}
static void render(void)
{
    if (s_game) lv_obj_delete(s_game);
    s_game = s_field = s_player = s_stats = NULL;
    lv_obj_clean(s_content);
    bool playing = s_state.page == D100_PLAY || s_state.page == D100_PAUSE;
    /* Keep the shared theme for menus, with an edge-to-edge play surface. */
    for (unsigned i = 0; i < lv_obj_get_child_count(s_screen); i++) {
        lv_obj_t *child = lv_obj_get_child(s_screen, i);
        if (playing && child != s_battery) lv_obj_add_flag(child, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(child, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_pos(s_battery, playing ? 194 : 163, playing ? 5 : 31);
    lv_obj_set_width(s_battery, playing ? 40 : 65);
    lv_obj_set_style_text_color(s_battery, lv_color_hex(playing ? 0xB5D5EB : UI_INK), 0);
    if (s_state.page == D100_HOME) {
        text(s_content, "越往下，越上头", 0, UI_INK);
        lv_obj_t *world = rect(s_content, 0, 29, 198, 67, 0x182B49);
        rect(world, 7, 29, 56, 8, 0x95B6DE); rect(world, 71, 29, 56, 8, 0x95B6DE);
        rect(world, 7, 57, 56, 8, 0x90DF94); rect(world, 135, 57, 56, 8, 0x48D8DB);
        miner(world, 90, 7, false);
        text(s_content, "走向缺口，一路下到百层", 106, UI_INK);
        text(s_content, "上键左移　下键右移", 132, UI_SKY_DARK);
        rect(s_content, 30, 158, 138, 25, UI_YELLOW);
        text(s_content, mode_name(), 160, UI_INK);
        lv_obj_t *best = text(s_content, "", 189, UI_GRASS_DARK);
        lv_label_set_text_fmt(best, "本次最高 %u 分", s_state.best[s_state.mode]);
        lv_label_set_text(s_footer, "确定开始 / 上换模式 / 下玩法");
    } else if (s_state.page == D100_HELP) {
        text(s_content, "下楼生存指南", 0, UI_INK);
        text(s_content, "上键左移，下键右移", 29, UI_SKY_DARK);
        text(s_content, "下落也能移，碎板快走", 54, UI_INK);
        text(s_content, "宝石加分，碎板尽快走", 79, UI_INK);
        text(s_content, "尖刺扣心，补给恢复心", 104, UI_INK);
        text(s_content, "两秒内连下，连击加分", 129, UI_INK);
        text(s_content, "碰到顶刺，本局结束", 154, 0xB42A44);
        text(s_content, "一命模式补给不加心", 179, UI_SKY_DARK);
        lv_label_set_text(s_footer, "确定开局 / 上下返回");
    } else if (s_state.page == D100_RESULT) {
        bool clear = s_state.floor == D100_GOAL;
        text(s_content, clear ? "勇闯百层，成功到底！" : "再下一层，就破纪录", 0, UI_INK);
        lv_obj_t *n = text(s_content, "", 29, UI_SKY_DARK);
        lv_label_set_text_fmt(n, "到达 %03u 层", s_state.floor);
        n = text(s_content, "", 56, UI_INK);
        lv_label_set_text_fmt(n, "得分 %u / 宝石 %u", s_state.score, s_state.gems);
        n = text(s_content, "", 83, UI_INK);
        lv_label_set_text_fmt(n, "最高连击 %u 层", s_state.max_combo);
        text(s_content, clear ? "地下探险王" : s_state.notice == D100_CEILING ? "慢了一步，被顶刺追上" : "爱心耗尽，留意粉色刺", 111, 0xB42A44);
        rect(s_content, 10, 141, 178, 27, 0xD8F0F7);
        n = text(s_content, "", 144, UI_SKY_DARK);
        lv_label_set_text_fmt(n, "同图挑战 %04u", s_state.challenge);
        text(s_content, s_state.new_best ? "新纪录！递给朋友试试" : "记住路线，下次更深", 181, UI_GRASS_DARK);
        lv_label_set_text(s_footer, "确定同图 / 上换图 / 下返回");
    } else {
        scene();
        if (s_state.page == D100_PAUSE) {
            lv_obj_t *panel = rect(s_game, 12, 51, 216, 240, UI_PAPER);
            lv_obj_set_style_border_width(panel, 2, 0);
            lv_obj_set_style_border_color(panel, lv_color_hex(UI_INK), 0);
            const char *lines[] = {
                "已暂停", "上键左移，下键右移", "下落也能移，碎板快走",
                "宝石加分，碎板尽快走", "尖刺扣心，二十层补给",
                "两秒连下，连击加分", "碰到顶刺，本局结束",
                "确定继续 / 上键重来", "下键返回首页"
            };
            for (unsigned i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) {
                lv_obj_t *label = text(panel, lines[i], 8 + i * 25,
                    i == 0 || i >= 7 ? UI_SKY_DARK : UI_INK);
                if (i == 0) lv_label_set_text_fmt(label, "已暂停  第%u段", d100_stage(s_state.floor) + 1);
                lv_obj_set_x(label, 6); lv_obj_set_width(label, 200);
            }
        }
    }
    if (!s_buttons) lv_label_set_text(s_footer, "按键不可用，请重启检查");
}
static void battery(lv_timer_t *timer)
{
    (void)timer;
    if (!s_battery || s_state.page == D100_PLAY) return;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}
static void start(uint16_t challenge)
{
    d100_start(&s_state, challenge); s_visual_x = d100_lane_x(s_state.lane);
}
static void handle(bsp_btn_t key, int64_t now)
{
    s_last_activity = now;
    bool wake = s_dimmed;
    if (wake) bsp_display_backlight(100);
    s_dimmed = s_off = false;
    if (wake) return;
    switch (s_state.page) {
        case D100_HOME:
            if (key == BSP_BTN_OK) start(esp_random() % 10000);
            else if (key == BSP_BTN_UP) s_state.mode ^= 1;
            else s_state.page = D100_HELP;
            break;
        case D100_HELP:
            if (key == BSP_BTN_OK) start(esp_random() % 10000);
            else s_state.page = D100_HOME;
            break;
        case D100_PLAY:
            if (key == BSP_BTN_OK) d100_pause(&s_state);
            else d100_move(&s_state, key == BSP_BTN_UP ? -1 : 1);
            break;
        case D100_PAUSE:
            if (key == BSP_BTN_OK) d100_pause(&s_state);
            else if (key == BSP_BTN_UP) start(s_state.challenge);
            else s_state.page = D100_HOME;
            break;
        case D100_RESULT:
            if (key == BSP_BTN_OK) start(s_state.challenge);
            else if (key == BSP_BTN_UP) start((s_state.challenge + 1U) % 10000);
            else s_state.page = D100_HOME;
            break;
    }
}
static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms();
    uint32_t elapsed = (uint32_t)(now - s_last_frame); s_last_frame = now;
    d100_page_t before = s_state.page;
    d100_state_t previous = s_state;
    uint8_t old_mode = s_state.mode;
    bool handled = false;
    input_t in;
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &in, 0) == pdTRUE; i++) {
        if (!handled && now - in.at <= 200) { handle(in.key, now); handled = true; }
    }
    /* Rendering and USB capture can exceed 200 ms on the device. A slow frame
       must never act as a pause key. d100_tick already caps unseen catch-up. */
    int64_t idle = now - s_last_activity;
    if (!s_dimmed && idle >= 60000) {
        if (s_state.page == D100_PLAY) d100_pause(&s_state);
        bsp_display_backlight(20); s_dimmed = true;
    }
    if (!s_off && idle >= 180000) { bsp_display_backlight(0); s_off = true; }
    if (before == D100_PLAY && s_state.page == D100_PLAY) d100_tick(&s_state, elapsed);
    if (s_state.page != before)
        d100_audio_active(s_state.page == D100_PLAY || s_state.page == D100_RESULT);
    d100_audio_play(d100_sound_event(&previous, &s_state));
    /* Lane changes are discrete; no long holds or simultaneous buttons needed. */
    s_visual_x = d100_lane_x(s_state.lane);
    if (s_state.page != before || s_state.mode != old_mode) render();
    if (s_state.page == D100_PLAY) live_update();
}
void down_100_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t), s_queue_storage, &s_queue_control);
    d100_audio_prepare();
}
void down_100_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_PRESS || !atomic_load(&s_accept)) return;
    if (button != BSP_BTN_UP && button != BSP_BTN_DOWN && button != BSP_BTN_OK) return;
    input_t in = {button, now_ms()}; (void)xQueueSend(s_queue, &in, 0);
}
void down_100_enter(bool buttons_available)
{
    if (s_screen) return;
    down_100_prepare(); s_buttons = buttons_available;
    s_dimmed = s_off = false; s_state.page = D100_HOME;
    d100_audio_active(false);
    s_last_frame = s_last_activity = now_ms();
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = text(s_screen, "勇闯地下100层", 15, 0xFFFFFF);
    lv_obj_set_x(title, 5); lv_obj_set_width(title, 151);
    s_battery = ui_pixel_label(s_screen, "--%", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_battery, 163, 31); lv_obj_set_width(s_battery, 65);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 8, 50, 220, 230, UI_PAPER);
    s_footer = text(s_screen, "", 294, UI_INK);
    lv_obj_set_x(s_footer, 0); lv_obj_set_width(s_footer, 240);
    render(); battery(NULL);
    s_timer = lv_timer_create(frame, 20, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen); bsp_display_backlight(100);
    xQueueReset(s_queue); atomic_store(&s_accept, buttons_available);
}
void down_100_exit(void)
{
    atomic_store(&s_accept, false);
    d100_audio_active(false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    s_content = s_footer = s_battery = s_game = s_field = s_player = s_stats = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}
