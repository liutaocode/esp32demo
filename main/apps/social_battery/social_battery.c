#include "social_battery.h"
#include "social_battery_state.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "ui_pixel.h"
#include <stdatomic.h>

LV_FONT_DECLARE(social_battery_zh_16);
LV_FONT_DECLARE(social_battery_zh_28);

typedef struct {
    const char *title;
    const char *message;
    const char *caption;
    uint32_t color;
    uint8_t bars;
} badge_t;

static const badge_t BADGES[SOCIAL_BATTERY_MODE_COUNT] = {
    {"欢迎搭话", "可以打断我\n也可以一起发呆", "今日份好奇心已上线", 0xA7D93E, 4},
    {"慢速营业", "不是不理你\n是我正在缓冲", "回复很慢，善意满格", 0xFFD928, 2},
    {"请勿打扰", "让我安静一会儿\n谢谢你给我空间", "暂时离线，与世界无关", 0xFFB23E, 1},
    {"求个抱抱", "先问我愿不愿意\n陪我坐坐也很好", "今日需要一点点温柔", 0xF5A9C4, 1},
};

static social_battery_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_battery, *s_time, *s_fill;
static lv_timer_t *s_tick, *s_battery_tick;
static bool s_buttons_available, s_off;
static uint8_t s_brightness;
static uint64_t s_last_activity;
static lv_obj_t *s_footer;
/* Single button-task producer, single LVGL-task consumer; never block input. */
static social_battery_input_t s_inputs[8];
static atomic_uint s_input_head, s_input_tail;
static atomic_bool s_accept_input;

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000U;
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text,
                        const lv_font_t *font, uint32_t color,
                        int x, int y, int width)
{
    lv_obj_t *obj = ui_pixel_label(parent, text, font, color);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, width);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    return obj;
}

static void hint(const char *first, const char *second)
{
    label(s_content, first, &social_battery_zh_16, UI_INK, 0, 178, 202);
    label(s_content, second, &social_battery_zh_16, UI_SKY_DARK, 0, 201, 202);
}

static void draw_battery(uint8_t bars, uint32_t color)
{
    box(s_content, 35, 38, 124, 58, UI_INK);
    box(s_content, 159, 55, 9, 23, UI_INK);
    box(s_content, 40, 43, 114, 48, UI_PAPER);
    for (uint8_t i = 0; i < 4; ++i) {
        box(s_content, 44 + i * 27, 47, 23, 40, i < bars ? color : UI_MUTED);
    }
    /* A tiny face makes the sign legible without pretending to measure mood. */
    box(s_content, 79, 57, 5, 7, UI_INK);
    box(s_content, 113, 57, 5, 7, UI_INK);
    box(s_content, 91, 74, 15, 4, UI_INK);
}

static void update_timer(void)
{
    if (s_time) {
        uint32_t seconds = social_battery_seconds(&s_state);
        lv_label_set_text_fmt(s_time, "%02lu:%02lu",
            (unsigned long)(seconds / 60U), (unsigned long)(seconds % 60U));
    }
    if (s_fill) {
        int width = 174 * social_battery_progress(&s_state) / 100;
        lv_obj_set_width(s_fill, width > 0 ? width : 1);
        lv_obj_set_style_bg_opa(s_fill, width > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, 0);
    }
}

static void render(void)
{
    s_time = NULL;
    s_fill = NULL;
    lv_obj_clean(s_content);
    const badge_t *badge = &BADGES[s_state.mode];
    if (s_state.page == SOCIAL_BADGE) {
        label(s_content, s_state.locked ? "展示已锁定" : "我的社交电量",
              &social_battery_zh_16, UI_SKY_DARK, 0, 0, 202);
        draw_battery(badge->bars, badge->color);
        label(s_content, badge->title, &social_battery_zh_28, UI_INK, 0, 104, 202);
        label(s_content, badge->message, &social_battery_zh_16, UI_INK, 0, 141, 202);
        hint(badge->caption, s_state.locked ? "长按确定解锁"
                                          : "上下换牌 / 确定锁定");
    } else if (s_state.page == SOCIAL_SETUP) {
        label(s_content, "给自己留白", &social_battery_zh_28, UI_INK, 0, 4, 202);
        ui_pixel_mascot_create(s_content, 82, 43);
        lv_obj_t *duration = label(s_content, "", &social_battery_zh_28,
                                    UI_INK, 0, 103, 202);
        lv_label_set_text_fmt(duration, "%u 分钟", social_battery_minutes(&s_state));
        label(s_content, "这段时间，先照顾自己", &social_battery_zh_16,
              UI_SKY_DARK, 0, 146, 202);
        hint("上下选时长 / 确定开始", "长按确定返回");
    } else if (s_state.page == SOCIAL_RECHARGING) {
        label(s_content, s_state.paused ? "充电已暂停" : "人类充电中",
              &social_battery_zh_28, UI_INK, 0, 5, 202);
        label(s_content, "请把这段安静留给我", &social_battery_zh_16,
              UI_SKY_DARK, 0, 48, 202);
        s_time = label(s_content, "", &lv_font_montserrat_20, UI_INK, 0, 85, 202);
        lv_obj_t *track = box(s_content, 10, 121, 182, 20, UI_INK);
        box(track, 4, 4, 174, 12, UI_MUTED);
        s_fill = box(track, 4, 4, 1, 12, UI_GRASS);
        label(s_content, "喝口水，看看窗外", &social_battery_zh_16,
              UI_SKY_DARK, 0, 152, 202);
        hint(s_state.paused ? "确定继续" : "确定暂停", "长按确定结束");
        update_timer();
    } else {
        label(s_content, "充电时间到", &social_battery_zh_28, UI_INK, 0, 5, 202);
        draw_battery(4, UI_GRASS);
        label(s_content, "状态由你决定", &social_battery_zh_16,
              UI_INK, 0, 112, 202);
        label(s_content, "想继续安静，也很好", &social_battery_zh_16,
              UI_SKY_DARK, 0, 144, 202);
        hint("不会自动切换社交状态", "确定返回原来的牌");
    }
    if (s_footer) lv_label_set_text(s_footer, !s_buttons_available ? "按键不可用" :
        s_state.page == SOCIAL_BADGE && !s_state.locked ? "长按确定：独处充电" :
        "你不必随时在线");
}

static void refresh_battery(lv_timer_t *timer)
{
    (void)timer;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}

static void brightness(uint8_t value)
{
    if (s_brightness != value) bsp_display_backlight(value);
    s_brightness = value;
    s_off = value == 0;
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    uint64_t now = now_ms();
    bool redraw = false;
    unsigned tail = atomic_load_explicit(&s_input_tail, memory_order_relaxed);
    unsigned head = atomic_load_explicit(&s_input_head, memory_order_acquire);
    while (tail != head) {
        social_battery_input_t input = s_inputs[tail % 8U];
        ++tail;
        s_last_activity = now;
        bool was_off = s_off;
        brightness(100);
        if (!was_off) redraw |= social_battery_input(&s_state, input, now);
    }
    atomic_store_explicit(&s_input_tail, tail, memory_order_release);
    social_battery_page_t page = s_state.page;
    if (social_battery_tick(&s_state, now)) {
        if (page != s_state.page) {
            s_last_activity = now;
            redraw = true;
        } else update_timer();
    }
    if (redraw) render();
    uint64_t idle = now - s_last_activity;
    bool keep_visible = s_state.locked || s_state.page == SOCIAL_RECHARGING ||
                        s_state.page == SOCIAL_READY;
    brightness(!keep_visible && idle >= 180000U ? 0 : idle >= 60000U ? 35 : 100);
}

void social_battery_enter(bool buttons_available)
{
    if (s_screen) return;
    s_buttons_available = buttons_available;
    atomic_store(&s_input_head, 0);
    atomic_store(&s_input_tail, 0);
    social_battery_init(&s_state);
    s_last_activity = now_ms();
    s_brightness = 0;
    brightness(100);
    s_screen = ui_pixel_screen_create("");
    label(s_screen, "社交电量牌", &social_battery_zh_16, 0xFFFFFF, 5, 15, 151);
    s_battery = label(s_screen, "--%", &lv_font_montserrat_14, UI_INK, 165, 31, 65);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 224, 244, UI_PAPER);
    s_footer = label(s_screen, "",
          &social_battery_zh_16, UI_INK, 5, 301, 230);
    render();
    refresh_battery(NULL);
    s_tick = lv_timer_create(tick, 50, NULL);
    s_battery_tick = lv_timer_create(refresh_battery, 10000, NULL);
    lv_screen_load(s_screen);
    atomic_store(&s_accept_input, buttons_available);
}

void social_battery_exit(void)
{
    atomic_store(&s_accept_input, false);
    if (s_tick) lv_timer_delete(s_tick);
    if (s_battery_tick) lv_timer_delete(s_battery_tick);
    s_tick = NULL;
    s_battery_tick = NULL;
    s_time = NULL;
    s_fill = NULL;
    s_content = NULL;
    s_battery = NULL;
    s_footer = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}

void social_battery_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept_input)) return;
    social_battery_input_t input;
    if (event == BSP_BTN_LONG && button == BSP_BTN_OK) input = SOCIAL_HOLD;
    else if (event != BSP_BTN_CLICK) return;
    else if (button == BSP_BTN_UP) input = SOCIAL_UP;
    else if (button == BSP_BTN_DOWN) input = SOCIAL_DOWN;
    else if (button == BSP_BTN_OK) input = SOCIAL_OK;
    else return;
    unsigned head = atomic_load_explicit(&s_input_head, memory_order_relaxed);
    unsigned tail = atomic_load_explicit(&s_input_tail, memory_order_acquire);
    if (head - tail >= 8U) return;
    s_inputs[head % 8U] = input;
    atomic_store_explicit(&s_input_head, head + 1U, memory_order_release);
}
