#include "cloud_hop.h"
#include "cloud_hop_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"

#include <stdatomic.h>

LV_FONT_DECLARE(cloud_hop_zh_16);
typedef struct { bsp_btn_t button; int64_t at_ms; } input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static ch_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery, *s_cloud, *s_cursor;
static lv_timer_t *s_timer, *s_battery_timer;
static bool s_buttons, s_dimmed, s_off;
static int64_t s_last_frame, s_last_activity;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static lv_obj_t *rect(lv_obj_t *p, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *o = lv_obj_create(p);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y); lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}

static lv_obj_t *text(lv_obj_t *p, const char *str, int y, uint32_t color)
{
    lv_obj_t *o = ui_pixel_label(p, str, &cloud_hop_zh_16, color);
    lv_obj_set_pos(o, 0, y); lv_obj_set_width(o, CH_FIELD);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}

static const char *mode_name(void) { return s_state.mode ? "一命冲刺" : "三次机会"; }

static lv_obj_t *cloud(lv_obj_t *p, int x, int y, bool sad)
{
    lv_obj_t *o = rect(p, x - 11, y, 22, 20, UI_INK);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    uint32_t color = sad ? 0xFFC3C7 : 0xFFFFFF;
    rect(o, 5, 0, 12, 3, UI_INK);
    rect(o, 2, 3, 18, 14, UI_INK);
    rect(o, 0, 7, 22, 7, UI_INK);
    rect(o, 4, 4, 14, 11, color);
    rect(o, 2, 8, 18, 5, color);
    rect(o, 6, 8, 2, sad ? 2 : 4, UI_INK);
    rect(o, 14, 8, 2, sad ? 2 : 4, UI_INK);
    rect(o, 10, sad ? 12 : 13, 3, 2, UI_INK);
    rect(o, 4, 12, 3, 2, 0xF590B0);
    rect(o, 16, 12, 3, 2, 0xF590B0);
    rect(o, 5, 17, 4, 3, UI_INK);
    rect(o, 14, 17, 4, 3, UI_INK);
    return o;
}

static void island(lv_obj_t *p, int center, int y, int width, bool gold)
{
    int x = center - width / 2;
    rect(p, x, y, width + 1, 5, UI_INK);
    rect(p, x + 1, y + 1, width - 1, 3, UI_GRASS);
    rect(p, x + 4, y + 5, width - 7, 7, 0x9F7255);
    rect(p, x + 8, y + 12, width - 15, 3, 0x70543D);
    if (gold) rect(p, center - CH_PERFECT, y, CH_PERFECT * 2 + 1, 4, UI_YELLOW);
}

static void scene(void)
{
    lv_obj_t *field = rect(s_content, 0, 32, CH_FIELD, 142, 0xD8F0F7);
    rect(field, 8, 20, 23, 3, 0xB8DBEC);
    rect(field, 158, 80, 28, 3, 0xB8DBEC);
    rect(field, 25, 90, 13, 3, 0xB8DBEC);
    rect(field, 83, 16, 4, 4, 0xFFFFFF);
    island(field, s_state.current_x, 126, 24, false);
    island(field, s_state.target_x, 48, s_state.target_width, true);
    ch_point_t pose = ch_pose(&s_state);
    s_cloud = cloud(field, pose.x, pose.y, !s_state.hit && s_state.page == CH_LANDED);
    ch_page_t page = s_state.page == CH_PAUSED ? s_state.resume : s_state.page;
    if (page == CH_AIM) {
        s_cursor = rect(field, s_state.cursor - 4, 28, 9, 14, UI_INK);
        lv_obj_set_style_bg_opa(s_cursor, LV_OPA_TRANSP, 0);
        rect(s_cursor, 3, 0, 3, 8, 0xDD4863);
        rect(s_cursor, 0, 6, 9, 3, 0xDD4863);
        rect(s_cursor, 2, 9, 5, 2, 0xDD4863);
        rect(s_cursor, 4, 11, 1, 3, 0xDD4863);
    }
}

static void render(void)
{
    s_cloud = s_cursor = NULL;
    lv_obj_clean(s_content);
    if (s_state.page == CH_HOME) {
        text(s_content, "就差一点，再跳一步！", 0, UI_INK);
        lv_obj_t *sky = rect(s_content, 0, 28, CH_FIELD, 72, 0xD8F0F7);
        island(sky, 39, 54, 48, false);
        island(sky, 151, 29, 54, true);
        cloud(sky, 89, 14, false);
        for (int i = 0; i < 5; i++) rect(sky, 49 + i * 17, 43 - i * 5, 3, 3, 0x87BED3);
        text(s_content, "红标对准浮岛，确定跳", 107, UI_INK);
        text(s_content, "踩中金色，连击加分", 132, UI_SKY_DARK);
        rect(s_content, 30, 157, 138, 24, UI_YELLOW);
        text(s_content, mode_name(), 159, UI_INK);
        lv_obj_t *best = text(s_content, "", 187, UI_GRASS_DARK);
        lv_label_set_text_fmt(best, "本次最高 %u 分", s_state.best[s_state.mode]);
        lv_label_set_text(s_footer, "上下选模式 / 确定开始");
    } else if (s_state.page == CH_RESULT) {
        text(s_content, s_state.level == CH_GOAL ? "二十五岛，全部拿下！" : "就差这一步！", 0, UI_INK);
        lv_obj_t *score = text(s_content, "", 28, UI_SKY_DARK);
        lv_obj_set_style_text_font(score, &lv_font_montserrat_20, 0);
        lv_label_set_text_fmt(score, "%03u", s_state.score);
        text(s_content, s_state.level == CH_GOAL ? "云端大满贯" :
             s_state.level >= 15 ? "天空漫游家" :
             s_state.level >= 5 ? "浮岛小高手" : "起跳练习生", 55, UI_GRASS_DARK);
        lv_obj_t *line = text(s_content, "", 82, UI_INK);
        lv_label_set_text_fmt(line, "过岛 %u / 精准 %u", s_state.level, s_state.perfects);
        line = text(s_content, "", 108, UI_INK);
        lv_label_set_text_fmt(line, "最高连击 %u / %s", s_state.best_streak, s_state.mode ? "一命" : "三次");
        rect(s_content, 10, 135, 178, 28, 0xD8F0F7);
        line = text(s_content, "", 139, UI_SKY_DARK);
        lv_label_set_text_fmt(line, "同题挑战 %04u", s_state.challenge);
        text(s_content, s_state.new_best ? "新纪录！递给朋友试试" : "拍下成绩，递给朋友", 174, UI_INK);
        lv_label_set_text(s_footer, "确定同题 / 上换题 / 下返回");
    } else {
        lv_obj_t *stats = text(s_content, "", 0, UI_INK);
        lv_label_set_text_fmt(stats, "%02u岛  %u分  机会%u", s_state.level, s_state.score, s_state.lives);
        scene();
        const char *feedback = s_state.page == CH_PAUSED ? "休息一下，云会等你" :
            s_state.page == CH_FLY ? "起跳！" :
            s_state.page == CH_LANDED ? (s_state.hit ?
                (s_state.perfect ? (s_state.streak >= 3 ? "连击！稳得像开了挂" : "正中金色！精准落地") : "接住了！再跳一步") :
                (s_state.lives ? "没接住，还有机会" : "没接住，差一点点")) : "红标对准浮岛，确定跳";
        text(s_content, feedback, 182, s_state.page == CH_LANDED && !s_state.hit ? 0xB42A44 : UI_SKY_DARK);
        if (s_state.page == CH_PAUSED) {
            lv_obj_t *overlay = rect(s_content, 0, 75, CH_FIELD, 59, UI_INK);
            rect(overlay, 2, 2, CH_FIELD - 4, 55, UI_PAPER);
            text(overlay, "已暂停", 6, UI_INK);
            text(overlay, "上键继续 / 下键返回", 32, UI_SKY_DARK);
        } else if (s_state.page == CH_LANDED && !s_state.hit) {
            lv_obj_t *miss = text(s_content, "", 89, 0xB42A44);
            lv_label_set_text_fmt(miss, "还差 %d 像素", ch_miss_distance(&s_state));
        }
        lv_label_set_text(s_footer, s_state.page == CH_PAUSED ?
            "上键继续 / 下键返回" : "确定起跳 / 上键暂停");
    }
    if (!s_buttons) lv_label_set_text(s_footer, "按键不可用，请重启检查");
}

static void battery(lv_timer_t *timer)
{
    (void)timer;
    /* Keep potentially blocking I2C reads out of timing play. */
    if (!s_battery || (s_state.page != CH_HOME && s_state.page != CH_RESULT && s_state.page != CH_PAUSED)) return;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}

static bool handle(bsp_btn_t key, int64_t now)
{
    s_last_activity = now;
    bool wake = s_dimmed;
    if (wake) bsp_display_backlight(100);
    s_off = s_dimmed = false;
    if (wake) return false;
    if (s_state.page == CH_HOME) {
        if (key == BSP_BTN_OK) ch_start(&s_state, esp_random() % 10000);
        else s_state.mode ^= 1;
    } else if (s_state.page == CH_RESULT) {
        if (key == BSP_BTN_OK) ch_start(&s_state, s_state.challenge);
        else if (key == BSP_BTN_UP) ch_start(&s_state, (s_state.challenge + 1U) % 10000);
        else ch_home(&s_state);
    } else if (key == BSP_BTN_UP) ch_pause(&s_state);
    else if (key == BSP_BTN_DOWN && s_state.page == CH_PAUSED) ch_home(&s_state);
    else if (key == BSP_BTN_OK) return ch_jump(&s_state);
    else return false;
    return true;
}

static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms();
    uint32_t elapsed = (uint32_t)(now - s_last_frame);
    s_last_frame = now;
    input_t in;
    bool dirty = false, handled = false;
    /* One action per visible frame; old presses cannot spill into a new jump.
       Judge the displayed cursor before advancing time. */
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &in, 0) == pdTRUE; i++) {
        if (!handled && now - in.at_ms <= 200) {
            dirty = handle(in.button, now); handled = true;
        }
    }
    int64_t idle = now - s_last_activity;
    if (!s_dimmed && idle >= 60000) {
        if (s_state.page == CH_AIM || s_state.page == CH_FLY || s_state.page == CH_LANDED) {
            ch_pause(&s_state); dirty = true;
        }
        bsp_display_backlight(20); s_dimmed = true;
    }
    if (!s_off && idle >= 180000) { bsp_display_backlight(0); s_off = true; }
    ch_page_t before = s_state.page;
    if (!dirty) ch_tick(&s_state, elapsed);
    dirty |= before != s_state.page;
    if (dirty) render();
    if (s_cursor) lv_obj_set_x(s_cursor, s_state.cursor - 4);
    if (s_cloud) {
        ch_point_t p = ch_pose(&s_state);
        lv_obj_set_pos(s_cloud, p.x - 11, p.y);
    }
}

void cloud_hop_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t), s_queue_storage, &s_queue_control);
}

void cloud_hop_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_PRESS || !atomic_load(&s_accept)) return;
    if (button != BSP_BTN_UP && button != BSP_BTN_DOWN && button != BSP_BTN_OK) return;
    input_t in = {button, now_ms()};
    (void)xQueueSend(s_queue, &in, 0);
}

void cloud_hop_enter(bool buttons_available)
{
    if (s_screen) return;
    cloud_hop_prepare();
    s_buttons = buttons_available; s_dimmed = s_off = false;
    s_last_frame = s_last_activity = now_ms();
    ch_home(&s_state);
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = text(s_screen, "再跳一步", 15, 0xFFFFFF);
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

void cloud_hop_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    s_content = s_footer = s_battery = s_cloud = s_cursor = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}
