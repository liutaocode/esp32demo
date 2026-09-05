#include "perfect_slice.h"
#include "perfect_slice_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"
#include <stdatomic.h>

LV_FONT_DECLARE(perfect_slice_zh_16);
#define CREAM 0xFFF5D9
#define PINK 0xF19BAA
#define BERRY 0xC34868
#define MINT 0x338578

typedef struct { bsp_btn_t button; int64_t at; } slice_input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[4 * sizeof(slice_input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;
static slice_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_footer, *s_battery;
static lv_obj_t *s_knife, *s_left, *s_right, *s_placeholder;
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_last_frame, s_last_activity;
static bool s_buttons, s_dimmed, s_off;

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
    lv_obj_t *o = ui_pixel_label(p, str, &perfect_slice_zh_16, color);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_width(o, w);
    lv_obj_set_style_text_align(o, LV_TEXT_ALIGN_CENTER, 0);
    return o;
}

static lv_obj_t *line(const char *str, int y, uint32_t color)
{
    return text(s_content, str, 0, y, 198, color);
}

/* Intersect each decoration with a slice; no negative/overflowing children. */
static void decoration(lv_obj_t *p, int origin, int width, int x, int y,
                       int w, int h, uint32_t color)
{
    int left = x > origin ? x : origin;
    int right = x + w < origin + width ? x + w : origin + width;
    if (right > left) box(p, left - origin, y, right - left, h, color);
}

static lv_obj_t *cake_piece(int origin, int width, int y)
{
    lv_obj_t *p = box(s_content, 9 + origin, y, width, 64, UI_INK);
    decoration(p, origin, width, 2, 3, 176, 55, 0xD99A58);
    decoration(p, origin, width, 2, 15, 176, 9, CREAM);
    decoration(p, origin, width, 2, 33, 176, 8, BERRY);
    decoration(p, origin, width, 2, 41, 176, 9, CREAM);
    decoration(p, origin, width, 2, 3, 176, 13, PINK);
    for (int x = 8; x < SLICE_WIDTH - 8; x += 22) {
        decoration(p, origin, width, x, 9, 8, 12, PINK);
        decoration(p, origin, width, x + 4, 5, 6, 3, CREAM);
        decoration(p, origin, width, x + 5, 27, 3, 3, 0xB57740);
        decoration(p, origin, width, x + 2, 53, 3, 3, 0xB57740);
    }
    return p;
}

static void cake(int y, unsigned cut)
{
    box(s_content, 0, y + 65, 198, 5, 0xB1C7CA);
    box(s_content, 4, y + 64, 190, 3, 0xFFFFFF);
    s_left = cake_piece(0, cut, y);
    s_right = cake_piece(cut, SLICE_WIDTH - cut, y);
}

static const char *mode_name(void) { return s_state.mode ? "高手挑战" : "轻松练手"; }

static const char *grade(void)
{
    if (s_state.perfect) return s_state.streak >= 3 ? "连击！手感来了" : "刚刚好！漂亮一刀";
    if (s_state.error <= 6) return "就差一点点！";
    if (s_state.error <= 15) return "切歪了，下刀稳住";
    return "这块归我，大块归你";
}

static void render(void)
{
    s_knife = s_left = s_right = NULL;
    lv_obj_clean(s_content);
    if (s_state.page == SLICE_HOME) {
        line("一人一半，真能切准？", 0, UI_INK);
        cake(42, 90);
        lv_obj_set_x(s_left, 5);
        lv_obj_set_x(s_right, 103);
        line("看准比例，确定下刀", 120, UI_INK);
        line("十刀挑战，连准加分", 144, MINT);
        for (unsigned i = 0; i < 2; i++) {
            lv_obj_t *choice = box(s_content, i * 102, 172, 96, 28,
                                   s_state.mode == i ? MINT : 0xE4E7DA);
            text(choice, i ? "高手挑战" : "轻松练手", 0, 4, 96,
                 s_state.mode == i ? 0xFFFFFF : UI_INK);
        }
        lv_label_set_text(s_footer, "上下选模式 / 确定开始");
    } else if (s_state.page == SLICE_RESULT) {
        line(s_state.new_best ? "新纪录！这刀有点东西" : "十刀完成！今日手感", 0, MINT);
        lv_obj_t *score = line("", 27, BERRY);
        lv_obj_set_style_text_font(score, &lv_font_montserrat_20, 0);
        lv_label_set_text_fmt(score, "%u", s_state.score);
        line(s_state.score >= 1000 ? "称号：蛋糕分配大师" :
             s_state.score >= 750 ? "称号：端水艺术家" :
             s_state.score >= 450 ? "称号：甜点练习生" : "称号：随缘切糕人", 54, UI_INK);
        ui_pixel_mascot_create(s_content, 80, 80);
        lv_obj_t *stats = line("", 139, UI_INK);
        lv_label_set_text_fmt(stats, "精准 %u 刀 / 最高 %u", s_state.perfects, s_state.best[s_state.mode]);
        line(mode_name(), 164, MINT);
        line("把这份手感传给朋友", 187, BERRY);
        lv_label_set_text(s_footer, "确定再来 / 下键返回");
    } else {
        slice_page_t page = s_state.page == SLICE_PAUSED ? s_state.resume : s_state.page;
        lv_obj_t *stats = line("", 0, UI_INK);
        lv_label_set_text_fmt(stats, "第 %u/10 刀    得分 %u", s_state.round + 1, s_state.score);
        lv_obj_t *target = line("", 28, BERRY);
        lv_label_set_text_fmt(target, "左边要 %u%%", s_state.target);
        if (page == SLICE_PLAY) {
            cake(87, 90);
            if (!s_state.mode) {
                int x = 9 + (int)slice_target_x(&s_state);
                for (int y = 77; y < 159; y += 10) box(s_content, x - 1, y, 2, 5, MINT);
            }
            s_knife = box(s_content, 9 + slice_position(&s_state) - 5, 62, 10, 99, UI_INK);
            lv_obj_set_style_bg_opa(s_knife, LV_OPA_TRANSP, 0);
            box(s_knife, 0, 0, 10, 19, UI_INK);
            box(s_knife, 2, 2, 6, 12, 0x8D6650);
            box(s_knife, 4, 19, 3, 78, 0xFFFFFF);
            box(s_knife, 7, 19, 1, 78, UI_INK);
            line(s_state.mode ? "凭眼力，切出指定比例" : "绿线是目标，看准下刀", 174, MINT);
            lv_label_set_text(s_footer, "确定下刀 / 上键暂停");
        } else {
            cake(83, s_state.cut);
            lv_obj_t *actual = line("", 55, UI_INK);
            lv_label_set_text_fmt(actual, "切出 %u%%   本刀 +%u",
                (s_state.cut * 100U + SLICE_WIDTH / 2) / SLICE_WIDTH, s_state.points);
            line(grade(), 161, s_state.perfect ? MINT : BERRY);
            lv_obj_t *combo = line("", 187, UI_INK);
            if (s_state.streak >= 2) lv_label_set_text_fmt(combo, "连续精准 %u 刀！", s_state.streak);
            else lv_label_set_text(combo, "按确定，继续挑战");
            lv_label_set_text(s_footer, s_state.round == 9 ? "确定看成绩 / 上键暂停" : "确定下一刀 / 上键暂停");
        }
        if (s_state.page == SLICE_PAUSED) {
            lv_obj_t *p = box(s_content, 0, 72, 198, 86, CREAM);
            text(p, "已暂停，蛋糕等你", 0, 16, 198, UI_INK);
            text(p, "上键继续 / 下键返回", 0, 48, 198, MINT);
            lv_label_set_text(s_footer, "歇一会儿，手会更稳");
        }
    }
    if (!s_buttons) lv_label_set_text(s_footer, "按键不可用，请重启");
}

static void battery(lv_timer_t *timer)
{
    (void)timer;
    /* Avoid a blocking I2C sample during time-sensitive play. */
    if (!s_battery || s_state.page == SLICE_PLAY) return;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}

static bool handle(bsp_btn_t b, int64_t now)
{
    s_last_activity = now;
    bool wake_only = s_dimmed || s_off;
    if (wake_only) bsp_display_backlight(100);
    s_dimmed = s_off = false;
    if (wake_only) return false;
    switch (s_state.page) {
    case SLICE_HOME:
        if (b == BSP_BTN_OK) slice_start(&s_state);
        else s_state.mode ^= 1;
        return true;
    case SLICE_RESULT:
        if (b == BSP_BTN_OK) slice_start(&s_state);
        else if (b == BSP_BTN_DOWN) slice_home(&s_state);
        else return false;
        return true;
    case SLICE_PAUSED:
        if (b == BSP_BTN_UP) slice_pause(&s_state);
        else if (b == BSP_BTN_DOWN) slice_home(&s_state);
        else return false;
        return true;
    default:
        if (b == BSP_BTN_UP) { slice_pause(&s_state); return true; }
        if (b == BSP_BTN_OK) return s_state.page == SLICE_PLAY ? slice_cut(&s_state) : slice_next(&s_state);
        return false;
    }
}

static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms(), elapsed = now - s_last_frame;
    s_last_frame = now;
    bool dirty = false, handled = false;
    /* A long render stall must not teleport the blade past the player's aim. */
    if (elapsed > 200 && s_state.page == SLICE_PLAY) {
        slice_pause(&s_state);
        dirty = true;
    }
    slice_input_t input;
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &input, 0) == pdTRUE; i++) {
        if (!handled && now - input.at <= 200 && !dirty) {
            dirty = handle(input.button, now);
            handled = true;
        }
    }
    /* Judge a press against the last displayed position, before moving it. */
    if (!dirty) slice_tick(&s_state, elapsed > 50 ? 50 : elapsed < 0 ? 0 : (uint32_t)elapsed);
    if (!s_dimmed && now - s_last_activity >= 60000) {
        if (s_state.page == SLICE_PLAY || s_state.page == SLICE_REVEAL) slice_pause(&s_state);
        dirty = true;
        bsp_display_backlight(20);
        s_dimmed = true;
    }
    if (!s_off && now - s_last_activity >= 180000) {
        bsp_display_backlight(0);
        s_off = true;
    }
    if (dirty) render();
    if (s_knife && s_state.page == SLICE_PLAY) lv_obj_set_x(s_knife, 4 + slice_position(&s_state));
    if (s_left && s_state.page == SLICE_REVEAL) {
        int gap = s_state.reveal_ms * 6U / SLICE_REVEAL_MS;
        lv_obj_set_x(s_left, 9 - gap);
        lv_obj_set_x(s_right, 9 + s_state.cut + gap);
        lv_obj_set_y(s_right, 83 + gap / 2);
    }
}

void perfect_slice_prepare(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(slice_input_t), s_queue_storage, &s_queue_control);
}

void perfect_slice_key(bsp_btn_t b, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_PRESS || !atomic_load(&s_accept)) return;
    if (b != BSP_BTN_OK && b != BSP_BTN_UP && b != BSP_BTN_DOWN) return;
    slice_input_t input = {b, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}

void perfect_slice_enter(bool buttons_available)
{
    if (s_screen) return;
    perfect_slice_prepare();
    s_buttons = buttons_available;
    s_dimmed = s_off = false;
    s_last_frame = s_last_activity = now_ms();
    slice_home(&s_state);
    s_screen = ui_pixel_screen_create("");
    text(s_screen, "一刀刚好", 5, 15, 151, 0xFFFFFF);
    s_battery = text(s_screen, "--%", 162, 31, 65, UI_INK);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 220, 226, UI_PAPER);
    s_footer = text(s_screen, "", 4, 294, 232, UI_INK);
    render();
    battery(NULL);
    s_timer = lv_timer_create(frame, 20, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen);
    if (s_placeholder) { lv_obj_delete(s_placeholder); s_placeholder = NULL; }
    bsp_display_backlight(100);
    xQueueReset(s_queue);
    atomic_store(&s_accept, buttons_available);
}

void perfect_slice_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    /* Keep the display on a valid screen while this page is released. */
    if (s_screen) {
        if (lv_screen_active() == s_screen) {
            s_placeholder = lv_obj_create(NULL);
            lv_screen_load(s_placeholder);
        }
        lv_obj_delete(s_screen);
    }
    s_screen = s_content = s_footer = s_battery = s_knife = s_left = s_right = NULL;
    xQueueReset(s_queue);
}
