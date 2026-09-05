#include "tomato_bloom.h"
#include "tomato_bloom_state.h"

#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "ui_pixel.h"

#include <stddef.h>
#include <stdint.h>

LV_FONT_DECLARE(tomato_bloom_zh_16);

#define ACTIVE_DIM_AFTER_MS 30000ULL
#define IDLE_DIM_AFTER_MS 60000ULL
#define IDLE_OFF_AFTER_MS 180000ULL

static tomato_bloom_state_t s_state;
static lv_obj_t *s_screen;
static lv_obj_t *s_content;
static lv_obj_t *s_battery;
static lv_obj_t *s_time;
static lv_obj_t *s_progress_fill;
static lv_obj_t *s_plant_holder;
static lv_timer_t *s_clock_timer;
static lv_timer_t *s_battery_timer;
static lv_timer_t *s_power_timer;
static uint64_t s_last_clock_second;
static uint64_t s_last_activity_ms;
static uint8_t s_growth_stage;
static bool s_buttons_available;
static bool s_dimmed;
static bool s_screen_off;

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int width, int height,
                       uint32_t color)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_radius(object, 0, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    return object;
}

static lv_obj_t *transparent_box(lv_obj_t *parent, int x, int y,
                                 int width, int height)
{
    lv_obj_t *object = block(parent, x, y, width, height, UI_PAPER);
    lv_obj_set_style_bg_opa(object, LV_OPA_TRANSP, 0);
    return object;
}

static lv_obj_t *make_label(lv_obj_t *parent, const char *text,
                            const lv_font_t *font, uint32_t color,
                            int x, int y, int width, lv_text_align_t align)
{
    lv_obj_t *label = ui_pixel_label(parent, text, font, color);
    lv_obj_set_pos(label, x, y);
    lv_obj_set_width(label, width);
    lv_obj_set_style_text_align(label, align, 0);
    return label;
}

static void draw_leaf(lv_obj_t *parent, int x, int y, bool left)
{
    int direction = left ? -1 : 1;
    block(parent, x + direction * 7, y, 9, 6, UI_GRASS_DARK);
    block(parent, x + direction * 12, y + 3, 8, 7, UI_GRASS);
}

static void draw_tomato(lv_obj_t *parent, int x, int y, int size,
                        uint32_t color)
{
    block(parent, x + 4, y, size - 8, 4, UI_GRASS_DARK);
    block(parent, x, y + 4, size, size - 8, color);
    block(parent, x + 4, y + size - 4, size - 8, 4, color);
    block(parent, x + 4, y + 7, 4, 4, 0xFFFFFF);
}

static void draw_plant(lv_obj_t *parent, uint8_t stage)
{
    lv_obj_clean(parent);
    block(parent, 8, 65, 84, 8, 0x75452E);
    block(parent, 20, 73, 60, 5, 0x5A3A24);

    if (stage == 0U) {
        block(parent, 47, 59, 7, 6, 0x5A3A24);
        return;
    }

    int stem_top = stage == 1U ? 51 : (stage == 2U ? 37 : 25);
    block(parent, 48, stem_top, 5, 65 - stem_top, UI_GRASS_DARK);
    draw_leaf(parent, 48, 49, true);
    if (stage >= 2U) draw_leaf(parent, 52, 39, false);
    if (stage >= 3U) {
        draw_leaf(parent, 48, 29, true);
        draw_tomato(parent, stage == 3U ? 39 : 34,
                    stage == 3U ? 16 : 8, stage == 3U ? 20 : 30,
                    stage == 3U ? UI_ORANGE : UI_RED);
    }
}

static void format_time(char *buffer, size_t size, uint32_t seconds)
{
    uint32_t minutes = seconds / 60U;
    uint32_t remainder = seconds % 60U;
    lv_snprintf(buffer, size, "%02lu:%02lu", (unsigned long)minutes,
                (unsigned long)remainder);
}

static void refresh_battery(lv_timer_t *timer)
{
    (void)timer;
    if (!s_battery) return;
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(s_battery, "--%");
    } else {
        if (soc > 100) soc = 100;
        lv_label_set_text_fmt(s_battery, "%d%%", soc);
    }
}

static void create_hint(const char *text)
{
    lv_obj_t *hint = block(s_content, 13, 194, 188, 25, UI_INK);
    make_label(hint, text, &tomato_bloom_zh_16, 0xFFFFFF,
               0, 3, 188, LV_TEXT_ALIGN_CENTER);
}

static const char *preset_name(uint8_t preset)
{
    static const char *const NAMES[TOMATO_BLOOM_PRESET_COUNT] = {
        "快速发芽", "经典番茄", "深度扎根"
    };
    return preset < TOMATO_BLOOM_PRESET_COUNT ? NAMES[preset] : NAMES[1];
}

static void render_setup(void)
{
    make_label(s_content, "种下专注", &tomato_bloom_zh_16,
               UI_INK, 0, 4, 210, LV_TEXT_ALIGN_CENTER);
    make_label(s_content, "一次计时，一次小胜利。", &tomato_bloom_zh_16,
               UI_SKY_DARK, 0, 31, 210, LV_TEXT_ALIGN_CENTER);

    s_plant_holder = transparent_box(s_content, 55, 48, 100, 80);
    draw_plant(s_plant_holder, 4U);

    lv_obj_t *preset = ui_pixel_panel_create(s_content, 17, 132, 176, 55,
                                              UI_YELLOW);
    lv_obj_t *duration = make_label(preset, "", &tomato_bloom_zh_16,
                                    UI_INK, 0, 2, 154,
                                    LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(duration, "<  %u 分钟  >",
                          (unsigned)tomato_bloom_focus_minutes(&s_state));
    make_label(preset, preset_name(s_state.preset), &tomato_bloom_zh_16,
               UI_SKY_DARK, 0, 28, 154, LV_TEXT_ALIGN_CENTER);
    create_hint(s_buttons_available ? "上下选择 / 确定开始"
                                    : "按键不可用");
}

static void create_progress(void)
{
    lv_obj_t *track = block(s_content, 13, 62, 184, 16, UI_INK);
    block(track, 3, 3, 178, 10, UI_MUTED);
    s_progress_fill = block(track, 4, 4, 1, 8, UI_GRASS);
}

static void update_active_visuals(void)
{
    if (s_time) {
        char time_text[12];
        format_time(time_text, sizeof(time_text), s_state.remaining_seconds);
        lv_label_set_text(s_time, time_text);
    }

    uint16_t progress = tomato_bloom_progress_per_mille(&s_state);
    if (s_progress_fill) {
        int width = (176 * progress) / 1000;
        lv_obj_set_width(s_progress_fill, LV_MAX(1, width));
        lv_obj_set_style_bg_opa(s_progress_fill,
                                width == 0 ? LV_OPA_TRANSP : LV_OPA_COVER, 0);
    }

    if (s_plant_holder &&
        (s_state.page == TOMATO_BLOOM_FOCUS ||
         s_state.page == TOMATO_BLOOM_FOCUS_PAUSED)) {
        uint8_t stage = tomato_bloom_growth_stage(&s_state);
        if (stage != s_growth_stage) {
            s_growth_stage = stage;
            draw_plant(s_plant_holder, stage);
        }
    }
}

static void render_focus(void)
{
    bool paused = s_state.page == TOMATO_BLOOM_FOCUS_PAUSED;
    lv_obj_t *round = make_label(s_content, "", &tomato_bloom_zh_16,
                                 UI_SKY_DARK, 0, 2, 210,
                                 LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(round, "专注番茄 %u / %u",
                          (unsigned)(s_state.completed_sessions %
                                     TOMATO_BLOOM_SET_SIZE + 1U),
                          TOMATO_BLOOM_SET_SIZE);
    s_time = make_label(s_content, "", &lv_font_montserrat_20,
                        paused ? UI_ORANGE : UI_INK, 0, 27, 210,
                        LV_TEXT_ALIGN_CENTER);
    create_progress();
    s_plant_holder = transparent_box(s_content, 55, 83, 100, 80);
    s_growth_stage = UINT8_MAX;
    make_label(s_content, paused ? "已暂停，番茄很安全"
                                 : "放下手机，让它生长。",
               &tomato_bloom_zh_16, paused ? UI_ORANGE : UI_SKY_DARK,
               0, 169, 210, LV_TEXT_ALIGN_CENTER);
    create_hint(paused ? "确定继续 / 长按重置"
                       : "确定暂停 / 长按重置");
    update_active_visuals();
}

static void render_harvest(void)
{
    make_label(s_content, "收获一颗番茄！", &tomato_bloom_zh_16,
               UI_RED, 0, 5, 210, LV_TEXT_ALIGN_CENTER);
    make_label(s_content, "你守住了一个小承诺。", &tomato_bloom_zh_16,
               UI_SKY_DARK, 0, 35, 210, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *garden = transparent_box(s_content, 20, 57, 170, 56);
    uint8_t visible = s_state.completed_sessions > 4U
        ? 4U : s_state.completed_sessions;
    for (uint8_t index = 0; index < visible; index++) {
        draw_tomato(garden, 7 + index * 41, 9, 28, UI_RED);
    }

    lv_obj_t *count = make_label(s_content, "", &tomato_bloom_zh_16,
                                 UI_INK, 0, 116, 210,
                                 LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(count, "收获 %u 颗 / 专注 %lu 分钟",
                          (unsigned)s_state.completed_sessions,
                          (unsigned long)s_state.focused_minutes);
    make_label(s_content, tomato_bloom_rank(&s_state),
               &tomato_bloom_zh_16, UI_GRASS_DARK,
               0, 148, 210, LV_TEXT_ALIGN_CENTER);

    lv_obj_t *next = make_label(s_content, "", &tomato_bloom_zh_16,
                                UI_SKY_DARK, 0, 171, 210,
                                LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(next, "接下来：休息 %u 分钟",
                          (unsigned)tomato_bloom_next_break_minutes(&s_state));
    create_hint("确定休息 / 截图分享");
}

static void render_break(void)
{
    bool paused = s_state.page == TOMATO_BLOOM_BREAK_PAUSED;
    make_label(s_content, "给自己浇浇水", &tomato_bloom_zh_16,
               UI_SKY_DARK, 0, 5, 210, LV_TEXT_ALIGN_CENTER);
    s_time = make_label(s_content, "", &lv_font_montserrat_20,
                        paused ? UI_ORANGE : UI_INK, 0, 37, 210,
                        LV_TEXT_ALIGN_CENTER);
    create_progress();
    lv_obj_t *mascot = ui_pixel_mascot_create(s_content, 86, 92);
    lv_obj_set_style_transform_scale(mascot, 300, 0);
    lv_obj_set_style_transform_pivot_x(mascot, 19, 0);
    lv_obj_set_style_transform_pivot_y(mascot, 24, 0);
    make_label(s_content, paused ? "休息已暂停" : "远眺、呼吸、喝水。",
               &tomato_bloom_zh_16, paused ? UI_ORANGE : UI_GRASS_DARK,
               0, 166, 210, LV_TEXT_ALIGN_CENTER);
    create_hint(paused ? "确定继续 / 长按跳过"
                       : "确定暂停 / 长按跳过");
    update_active_visuals();
}

static void render_ready(void)
{
    make_label(s_content, "能量已恢复", &tomato_bloom_zh_16,
               UI_GRASS_DARK, 0, 10, 210, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *mascot = ui_pixel_mascot_create(s_content, 86, 55);
    ui_pixel_mascot_jump(mascot);
    make_label(s_content, "再种一颗吗？", &tomato_bloom_zh_16,
               UI_INK, 0, 116, 210, LV_TEXT_ALIGN_CENTER);
    lv_obj_t *stats = ui_pixel_panel_create(s_content, 25, 143, 160, 43,
                                             UI_YELLOW);
    lv_obj_t *label = make_label(stats, "", &tomato_bloom_zh_16,
                                 UI_INK, 0, 7, 138,
                                 LV_TEXT_ALIGN_CENTER);
    lv_label_set_text_fmt(label, "%u 颗番茄 / %lu 分钟",
                          (unsigned)s_state.completed_sessions,
                          (unsigned long)s_state.focused_minutes);
    create_hint("确定开始下一轮");
}

static void render(void)
{
    s_time = NULL;
    s_progress_fill = NULL;
    s_plant_holder = NULL;
    s_growth_stage = UINT8_MAX;
    lv_obj_clean(s_content);

    switch (s_state.page) {
    case TOMATO_BLOOM_SETUP:
        render_setup();
        break;
    case TOMATO_BLOOM_FOCUS:
    case TOMATO_BLOOM_FOCUS_PAUSED:
        render_focus();
        break;
    case TOMATO_BLOOM_HARVEST:
        render_harvest();
        break;
    case TOMATO_BLOOM_BREAK:
    case TOMATO_BLOOM_BREAK_PAUSED:
        render_break();
        break;
    case TOMATO_BLOOM_READY:
        render_ready();
        break;
    }
}

static bool timer_is_running(void)
{
    return s_state.page == TOMATO_BLOOM_FOCUS ||
           s_state.page == TOMATO_BLOOM_BREAK;
}

static void clock_tick(lv_timer_t *timer)
{
    (void)timer;
    uint64_t now = (uint64_t)esp_timer_get_time() / 1000000ULL;
    if (s_last_clock_second == 0U) {
        s_last_clock_second = now;
        return;
    }
    uint64_t elapsed = now - s_last_clock_second;
    if (elapsed == 0U) return;
    s_last_clock_second = now;

    if (!timer_is_running()) return;
    uint32_t seconds = elapsed > UINT32_MAX ? UINT32_MAX : (uint32_t)elapsed;
    tomato_bloom_event_t event = tomato_bloom_state_tick(&s_state, seconds);
    if (event == TOMATO_BLOOM_FOCUS_FINISHED ||
        event == TOMATO_BLOOM_BREAK_FINISHED) {
        bsp_display_backlight(100);
        s_dimmed = false;
        s_screen_off = false;
        s_last_activity_ms = now * 1000ULL;
        render();
    } else {
        update_active_visuals();
    }
}

static void power_tick(lv_timer_t *timer)
{
    (void)timer;
    uint64_t now_ms = (uint64_t)esp_timer_get_time() / 1000ULL;
    uint64_t idle_ms = now_ms - s_last_activity_ms;
    bool active = timer_is_running();
    uint64_t dim_after = active ? ACTIVE_DIM_AFTER_MS : IDLE_DIM_AFTER_MS;

    if (!active && !s_screen_off && idle_ms >= IDLE_OFF_AFTER_MS) {
        bsp_display_backlight(0);
        s_screen_off = true;
        s_dimmed = true;
    } else if (!s_dimmed && idle_ms >= dim_after) {
        bsp_display_backlight(20);
        s_dimmed = true;
    }
}

void tomato_bloom_enter(bool buttons_available)
{
    s_buttons_available = buttons_available;
    s_dimmed = false;
    s_screen_off = false;
    s_last_clock_second = (uint64_t)esp_timer_get_time() / 1000000ULL;
    s_last_activity_ms = (uint64_t)esp_timer_get_time() / 1000ULL;
    tomato_bloom_state_init(&s_state);

    s_screen = ui_pixel_screen_create("");
    make_label(s_screen, "番茄花园", &tomato_bloom_zh_16,
               0xFFFFFF, 5, 15, 151, LV_TEXT_ALIGN_CENTER);
    s_battery = make_label(s_screen, "--%", &lv_font_montserrat_14,
                           UI_INK, 162, 31, 68, LV_TEXT_ALIGN_RIGHT);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 224, 226, 0xF7F1E3);
    render();
    refresh_battery(NULL);

    s_clock_timer = lv_timer_create(clock_tick, 250, NULL);
    s_battery_timer = lv_timer_create(refresh_battery, 10000, NULL);
    s_power_timer = lv_timer_create(power_tick, 1000, NULL);
    lv_screen_load(s_screen);
}

void tomato_bloom_exit(void)
{
    if (s_clock_timer) lv_timer_delete(s_clock_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    if (s_power_timer) lv_timer_delete(s_power_timer);
    s_clock_timer = NULL;
    s_battery_timer = NULL;
    s_power_timer = NULL;
    s_content = NULL;
    s_battery = NULL;
    s_time = NULL;
    s_progress_fill = NULL;
    s_plant_holder = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}

void tomato_bloom_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if ((event != BSP_BTN_CLICK && event != BSP_BTN_LONG) ||
        !s_buttons_available) {
        return;
    }

    bool was_off = s_screen_off;
    s_last_activity_ms = (uint64_t)esp_timer_get_time() / 1000ULL;
    if (s_dimmed) {
        bsp_display_backlight(100);
        s_dimmed = false;
        s_screen_off = false;
    }
    if (was_off) return;

    if (event == BSP_BTN_LONG && button == BSP_BTN_OK) {
        if (tomato_bloom_state_reset_timer(&s_state) != TOMATO_BLOOM_NO_CHANGE) {
            s_last_clock_second = (uint64_t)esp_timer_get_time() / 1000000ULL;
            render();
        }
        return;
    }
    if (event != BSP_BTN_CLICK) return;

    tomato_bloom_event_t change = TOMATO_BLOOM_NO_CHANGE;
    if (s_state.page == TOMATO_BLOOM_SETUP &&
        (button == BSP_BTN_UP || button == BSP_BTN_DOWN)) {
        change = tomato_bloom_state_move(&s_state,
            button == BSP_BTN_UP ? -1 : 1);
    } else if (button == BSP_BTN_OK) {
        change = tomato_bloom_state_confirm(&s_state);
        s_last_clock_second = (uint64_t)esp_timer_get_time() / 1000000ULL;
    }

    if (change != TOMATO_BLOOM_NO_CHANGE) render();
}
