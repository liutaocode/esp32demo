#include "stack_rush.h"
#include "stack_rush_state.h"

#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "lvgl.h"
#include "ui_pixel.h"

#include <stdatomic.h>

LV_FONT_DECLARE(stack_rush_zh_16);

typedef struct { bsp_btn_t button; int64_t at_ms; } input_t;
static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[4 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept_input;
static stack_rush_state_t s_state;
static lv_obj_t *s_screen, *s_content, *s_battery, *s_footer;
static lv_obj_t *s_moving, *s_debris;
static lv_timer_t *s_frame_timer, *s_battery_timer;
static int64_t s_last_frame, s_last_activity;
static bool s_buttons, s_dimmed, s_off;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h,
                        uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int y,
                        uint32_t color)
{
    lv_obj_t *obj = ui_pixel_label(parent, text, &stack_rush_zh_16, color);
    lv_obj_set_pos(obj, 0, y);
    lv_obj_set_width(obj, 198);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, 0);
    return obj;
}

static const char *mode_name(void)
{
    return s_state.mode ? "极速挑战" : "轻松叠叠";
}

static uint32_t floor_color(unsigned floor)
{
    static const uint32_t colors[] = {
        UI_ORANGE, 0x7557D9, 0x45C3C8, UI_GRASS, 0xF4829F
    };
    return colors[floor % 5];
}

static lv_obj_t *floor_block(lv_obj_t *parent, stack_rush_block_t b, int y,
                              uint32_t color)
{
    lv_obj_t *obj = block(parent, b.x, y, b.width, 12, color);
    /* No child objects per floor; narrow one-pixel survivors stay visible. */
    lv_obj_set_style_border_side(obj, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_border_width(obj, 2, 0);
    lv_obj_set_style_border_color(obj, lv_color_hex(UI_INK), 0);
    return obj;
}

static void draw_tower(void)
{
    lv_obj_t *field = block(s_content, 3, 53, STACK_RUSH_FIELD, 143, 0xDAEFF1);
    block(field, 0, 138, STACK_RUSH_FIELD, 5, UI_GRASS_DARK);
    for (unsigned i = 0; i < s_state.count; i++) {
        unsigned floor = s_state.floors + 1U - s_state.count + i;
        floor_block(field, s_state.tower[i], 124 - (int)i * 14,
                    floor_color(floor));
    }
    stack_rush_page_t page = s_state.page == STACK_PAUSED
        ? s_state.resume_page : s_state.page;
    if (page == STACK_MOVING) {
        s_moving = floor_block(field, s_state.moving,
                               124 - s_state.count * 14, UI_YELLOW);
    } else if (page == STACK_SETTLING && s_state.debris.width > 0) {
        s_debris = floor_block(field, s_state.debris,
                               124 - (s_state.count - 1) * 14, UI_RED);
    }
}

static void render(void)
{
    s_moving = s_debris = NULL;
    lv_obj_clean(s_content);
    if (s_state.page == STACK_READY) {
        label(s_content, "就差一点，再来一层！", 0, UI_INK);
        ui_pixel_mascot_create(s_content, 80, 32);
        label(s_content, "确定落下，叠到五十层", 88, UI_INK);
        label(s_content, "三连精准，楼层变宽", 113, UI_SKY_DARK);
        label(s_content, mode_name(), 144, UI_GRASS_DARK);
        lv_obj_t *best = label(s_content, "", 174, UI_INK);
        lv_label_set_text_fmt(best, "本次开机最高 %u 层", s_state.best[s_state.mode]);
        lv_label_set_text(s_footer, "上下选模式 / 确定开始");
    } else if (s_state.page == STACK_RESULT) {
        label(s_content, s_state.won ? "五十层！登顶成功" : "就差一点点！", 0,
              s_state.won ? UI_GRASS_DARK : UI_RED);
        lv_obj_t *score = label(s_content, "", 30, UI_INK);
        lv_obj_set_style_text_font(score, &lv_font_montserrat_20, 0);
        lv_label_set_text_fmt(score, "%02u / 50", s_state.floors);
        const char *rank = s_state.floors >= 30 ? "天空建筑师" :
                           s_state.floors >= 15 ? "城市造梦家" :
                           s_state.floors >= 5 ? "叠楼小能手" : "地基练习生";
        label(s_content, rank, 61, UI_SKY_DARK);
        lv_obj_t *stats = label(s_content, "", 89, UI_INK);
        lv_label_set_text_fmt(stats, "精准 %u 次 / 最高 %u 层", s_state.perfects,
                             s_state.best[s_state.mode]);
        for (unsigned i = 0; i < 4; i++) {
            stack_rush_block_t b = { (int16_t)(46 + i * 6), (int16_t)(106 - i * 12) };
            floor_block(s_content, b, 160 - (int)i * 12, floor_color(i));
        }
        label(s_content, s_state.new_best ? "新纪录！给朋友挑战" : mode_name(),
              180, UI_GRASS_DARK);
        lv_label_set_text(s_footer, "确定再来 / 下键换模式");
    } else {
        lv_obj_t *score = label(s_content, "", 0, UI_INK);
        lv_label_set_text_fmt(score, "%u 层 / 连准 %u", s_state.floors, s_state.streak);
        const char *feedback = s_state.page == STACK_PAUSED ? "已暂停，楼不会倒" :
            s_state.restored ? "三连精准！宽度恢复" :
            s_state.perfect ? "精准！稳稳接住" :
            s_state.floors ? "再叠一层！" : "看准下方，确定落下";
        label(s_content, feedback, 27, s_state.perfect ? UI_GRASS_DARK : UI_SKY_DARK);
        draw_tower();
        if (s_state.page == STACK_PAUSED) {
            lv_obj_t *overlay = block(s_content, 0, 91, 198, 51, UI_PAPER);
            label(overlay, "上键继续 / 下键返回", 16, UI_INK);
        }
        lv_label_set_text(s_footer, "确定落下 / 上键暂停");
    }
    if (!s_buttons) lv_label_set_text(s_footer, "按键不可用");
}

static void refresh_battery(lv_timer_t *timer)
{
    (void)timer;
    /* I2C reads can wait: sample between rounds, never during timing play. */
    if (!s_battery || s_state.page == STACK_MOVING ||
        s_state.page == STACK_SETTLING) return;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}

static bool handle_input(bsp_btn_t button, int64_t now)
{
    s_last_activity = now;
    bool was_off = s_off;
    if (s_dimmed) bsp_display_backlight(100);
    s_dimmed = s_off = false;
    if (was_off) return false; /* This entire press is wake-only. */
    if (s_state.page == STACK_READY) {
        if (button == BSP_BTN_OK) stack_rush_start(&s_state);
        else stack_rush_select(&s_state);
    } else if (s_state.page == STACK_RESULT) {
        if (button == BSP_BTN_OK) stack_rush_start(&s_state);
        else if (button == BSP_BTN_DOWN) stack_rush_home(&s_state);
        else return false;
    } else if (button == BSP_BTN_UP) {
        stack_rush_pause(&s_state);
    } else if (button == BSP_BTN_DOWN && s_state.page == STACK_PAUSED) {
        stack_rush_home(&s_state);
    } else if (button == BSP_BTN_OK) {
        return stack_rush_drop(&s_state);
    } else return false;
    return true;
}

static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms();
    uint32_t elapsed = (uint32_t)(now - s_last_frame);
    s_last_frame = now;
    input_t input;
    bool dirty = false;
    bool handled = false;
    /* At most one action per drawn frame. Judge against the displayed block,
       not a later position after input/render latency. Drop stale queued keys. */
    for (unsigned i = 0; i < 4 && xQueueReceive(s_queue, &input, 0) == pdTRUE; i++) {
        if (!handled && now - input.at_ms <= 200) {
            dirty = handle_input(input.button, now);
            handled = true;
        }
    }
    stack_rush_page_t old_page = s_state.page;
    if (!dirty) stack_rush_tick(&s_state, elapsed);
    dirty |= old_page != s_state.page;

    int64_t idle = now - s_last_activity;
    if (!s_dimmed && idle >= 60000) {
        if (s_state.page == STACK_MOVING || s_state.page == STACK_SETTLING) {
            stack_rush_pause(&s_state);
            dirty = true;
        }
        bsp_display_backlight(20);
        s_dimmed = true;
    }
    if (!s_off && idle >= 180000) {
        bsp_display_backlight(0);
        s_off = true;
    }
    if (dirty) render();
    if (s_moving) lv_obj_set_x(s_moving, s_state.moving.x);
    if (s_debris) {
        unsigned t = s_state.settle_ms;
        lv_obj_set_y(s_debris, 124 - (s_state.count - 1) * 14 +
                     (int)(t * t * 110U / (STACK_RUSH_SETTLE_MS * STACK_RUSH_SETTLE_MS)));
    }
}

void stack_rush_prepare_input(void)
{
    if (!s_queue) s_queue = xQueueCreateStatic(4, sizeof(input_t),
                                               s_queue_storage, &s_queue_control);
}

void stack_rush_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_PRESS || !atomic_load(&s_accept_input)) return;
    input_t input = { button, now_ms() };
    (void)xQueueSend(s_queue, &input, 0);
}

void stack_rush_enter(bool buttons_available)
{
    if (s_screen) return;
    stack_rush_prepare_input();
    s_buttons = buttons_available;
    s_dimmed = s_off = false;
    s_last_frame = s_last_activity = now_ms();
    stack_rush_home(&s_state); /* Records live in RAM across re-entry. */
    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = label(s_screen, "再叠一层", 15, 0xFFFFFF);
    lv_obj_set_width(title, 151);
    lv_obj_set_x(title, 5);
    s_battery = ui_pixel_label(s_screen, "--%", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_battery, 163, 31);
    lv_obj_set_width(s_battery, 65);
    lv_label_set_long_mode(s_battery, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);
    s_content = ui_pixel_panel_create(s_screen, 8, 52, 220, 226, UI_PAPER);
    s_footer = label(s_screen, "", 294, UI_INK);
    lv_obj_set_x(s_footer, 4);
    lv_obj_set_width(s_footer, 232);
    render();
    refresh_battery(NULL);
    s_frame_timer = lv_timer_create(frame, 20, NULL);
    s_battery_timer = lv_timer_create(refresh_battery, 10000, NULL);
    lv_screen_load(s_screen);
    bsp_display_backlight(100);
    xQueueReset(s_queue);
    atomic_store(&s_accept_input, buttons_available);
}

void stack_rush_exit(void)
{
    atomic_store(&s_accept_input, false);
    if (s_frame_timer) lv_timer_delete(s_frame_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_frame_timer = s_battery_timer = NULL;
    s_moving = s_debris = s_content = s_battery = s_footer = NULL;
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}
