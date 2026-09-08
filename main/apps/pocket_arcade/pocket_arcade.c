/* 口袋游戏厅 —— 十二个离线小游戏的大厅页。
   这一层只做三件事:把按键排进队列、按帧驱动纯 C 的大厅状态机、
   把状态机吐出来的场景清单同步到一池 LVGL 对象上。 */
#include "pocket_arcade.h"
#include "pa_hub.h"
#include "pa_storage.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ui_pixel.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(pocket_arcade_zh_16);
LV_FONT_DECLARE(pocket_arcade_zh_12);

enum { FRAME_MS = 33, DIM_MS = 60000, OFF_MS = 180000, SAVE_DELAY_MS = 1200 };

typedef struct { bsp_btn_t button; bsp_btn_ev_t event; int64_t at; } input_t;

static StaticQueue_t s_queue_control;
static uint8_t s_queue_storage[6 * sizeof(input_t)];
static QueueHandle_t s_queue;
static atomic_bool s_accept;

static pa_hub_t s_hub;
static pa_scene_t s_scene, s_shown;
static lv_obj_t *s_screen, *s_content, *s_battery, *s_footer;
static lv_obj_t *s_rect_layer, *s_text_layer;
static lv_obj_t *s_rect[PA_MAX_RECTS], *s_label[PA_MAX_TEXTS];
static lv_style_t s_block_style;
static bool s_style_ready;
static uint8_t s_rect_used, s_label_used;
static char s_footer_shown[PA_FOOTER_MAX];
static lv_timer_t *s_timer, *s_battery_timer;
static int64_t s_last_frame, s_last_activity;
static uint32_t s_save_ms;
static bool s_buttons, s_dimmed, s_off, s_suppress_long;

static int64_t now_ms(void) { return esp_timer_get_time() / 1000; }

static const lv_font_t *font_for(uint8_t index)
{
    switch (index) {
    case PA_FONT_ZH12:  return &pocket_arcade_zh_12;
    case PA_FONT_NUM14: return &lv_font_montserrat_14;
    case PA_FONT_NUM20: return &lv_font_montserrat_20;
    default:            return &pocket_arcade_zh_16;
    }
}

static lv_text_align_t align_for(uint8_t index)
{
    if (index == PA_CENTER) return LV_TEXT_ALIGN_CENTER;
    if (index == PA_RIGHT) return LV_TEXT_ALIGN_RIGHT;
    return LV_TEXT_ALIGN_LEFT;
}

/* 矩形和文字各占一层。池子里的对象按需创建,而 LVGL 的绘制顺序就是子对象
   顺序,所以晚建的矩形会压住早建的文字 —— 分两层就永远不会。 */
static lv_obj_t *layer_create(void)
{
    lv_obj_t *layer = lv_obj_create(s_content);
    lv_obj_remove_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(layer, 0, 0);
    lv_obj_set_size(layer, PA_VIEW_W, PA_VIEW_H);
    lv_obj_set_style_pad_all(layer, 0, 0);
    lv_obj_set_style_border_width(layer, 0, 0);
    lv_obj_set_style_bg_opa(layer, LV_OPA_TRANSP, 0);
    return layer;
}

/* 只玩前几个游戏时,永远不会为方块塔那一百多个格子付内存。 */
static lv_obj_t *rect_object(unsigned index)
{
    if (s_rect[index]) return s_rect[index];
    lv_obj_t *obj = lv_obj_create(s_rect_layer);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_style(obj, &s_block_style, 0);
    s_rect[index] = obj;
    return obj;
}

static lv_obj_t *label_object(unsigned index)
{
    if (s_label[index]) return s_label[index];
    lv_obj_t *obj = lv_label_create(s_text_layer);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
    s_label[index] = obj;
    return obj;
}

static void sync_scene(void)
{
    for (unsigned i = 0; i < PA_MAX_RECTS; i++) {
        bool live = i < s_scene.rects;
        bool was_live = i < s_shown.rects;
        if (!live) {
            if (was_live && s_rect[i]) lv_obj_add_flag(s_rect[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const pa_rect_t *want = &s_scene.rect[i];
        if (was_live && memcmp(want, &s_shown.rect[i], sizeof(*want)) == 0) continue;
        lv_obj_t *obj = rect_object(i);
        if (!obj) continue;
        lv_obj_set_pos(obj, want->x, want->y);
        lv_obj_set_size(obj, want->w, want->h);
        lv_obj_set_style_radius(obj, want->radius, 0);
        lv_obj_set_style_bg_color(obj, lv_color_hex(want->color), 0);
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    for (unsigned i = 0; i < PA_MAX_TEXTS; i++) {
        bool live = i < s_scene.texts;
        bool was_live = i < s_shown.texts;
        if (!live) {
            if (was_live && s_label[i]) lv_obj_add_flag(s_label[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        const pa_text_t *want = &s_scene.text[i];
        if (was_live && memcmp(want, &s_shown.text[i], sizeof(*want)) == 0) continue;
        lv_obj_t *obj = label_object(i);
        if (!obj) continue;
        lv_obj_set_pos(obj, want->x, want->y);
        lv_obj_set_width(obj, want->w);
        lv_obj_set_style_text_font(obj, font_for(want->font), 0);
        lv_obj_set_style_text_color(obj, lv_color_hex(want->color), 0);
        lv_obj_set_style_text_align(obj, align_for(want->align), 0);
        lv_label_set_text(obj, want->text);
        lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_scene.rects > s_rect_used) s_rect_used = s_scene.rects;
    if (s_scene.texts > s_label_used) s_label_used = s_scene.texts;
    s_shown = s_scene;

    const char *footer = s_buttons ? s_scene.footer : "按键不可用，请重启设备";
    if (strcmp(footer, s_footer_shown) != 0) {
        snprintf(s_footer_shown, sizeof(s_footer_shown), "%s", footer);
        lv_label_set_text(s_footer, footer);
    }
}

static void render(void)
{
    pa_hub_draw(&s_hub, &s_scene);
    sync_scene();
}

static void battery(lv_timer_t *timer)
{
    (void)timer;
    if (!s_battery) return;
    int soc = bsp_battery_soc();
    if (soc < 0) lv_label_set_text(s_battery, "--%");
    else lv_label_set_text_fmt(s_battery, "%d%%", soc > 100 ? 100 : soc);
}

/* 把一次硬件按键翻译成大厅认识的键位。返回 false 表示这个事件不该用。 */
static bool translate(input_t input, pa_key_t *out)
{
    bool click_mode = pa_hub_wants_click(&s_hub);
    if (input.button != BSP_BTN_OK) {
        if (input.event != BSP_BTN_PRESS) return false;
        *out = (input.button == BSP_BTN_UP) ? PA_KEY_UP : PA_KEY_DOWN;
        return true;
    }
    if (click_mode) {
        if (input.event == BSP_BTN_CLICK) { *out = PA_KEY_OK; return true; }
        if (input.event == BSP_BTN_DOUBLE) { *out = PA_KEY_OK2; return true; }
        return false;
    }
    if (input.event != BSP_BTN_PRESS) return false;
    *out = PA_KEY_OK;
    return true;
}

static void handle(input_t input, int64_t now)
{
    s_last_activity = now;
    if (s_off) {
        s_off = s_dimmed = false;
        s_suppress_long = true;
        bsp_display_backlight(100);
        return;
    }
    if (s_dimmed) { s_dimmed = false; bsp_display_backlight(100); }
    if (input.event == BSP_BTN_LONG) {
        pa_key_t held = (input.button == BSP_BTN_UP)   ? PA_KEY_UP
                      : (input.button == BSP_BTN_DOWN) ? PA_KEY_DOWN
                                                       : PA_KEY_OK;
        if (!s_suppress_long) pa_hub_long(&s_hub, held);
        return;
    }
    s_suppress_long = false;
    pa_key_t key;
    if (translate(input, &key)) pa_hub_key(&s_hub, key);
}

static void frame(lv_timer_t *timer)
{
    (void)timer;
    int64_t now = now_ms();
    uint32_t elapsed = (uint32_t)(now - s_last_frame);
    if (elapsed > 120U) elapsed = 120U;   /* 卡顿时一帧最多推进这么多游戏时间 */
    s_last_frame = now;

    input_t input;
    for (unsigned i = 0; i < 6 && xQueueReceive(s_queue, &input, 0) == pdTRUE; i++) {
        if (now - input.at > 400) continue;
        handle(input, now);
    }
    pa_hub_tick(&s_hub, elapsed);

    if (s_hub.record_dirty) {
        s_hub.record_dirty = false;
        s_save_ms = SAVE_DELAY_MS;
    }
    if (s_save_ms) {
        s_save_ms = (s_save_ms > elapsed) ? s_save_ms - elapsed : 0;
        if (!s_save_ms) pa_storage_save(&s_hub.record);
    }

    if (!s_dimmed && now - s_last_activity >= DIM_MS) {
        /* 熄屏前先把这一局收掉,免得人走了以后分数还在自己往下掉。 */
        if (s_hub.page == PA_PAGE_PLAY) pa_hub_long(&s_hub, PA_KEY_OK);
        s_dimmed = true;
        bsp_display_backlight(20);
    }
    if (!s_off && now - s_last_activity >= OFF_MS) {
        s_off = true;
        bsp_display_backlight(0);
    }
    render();
}

void pocket_arcade_prepare(void)
{
    if (!s_queue)
        s_queue = xQueueCreateStatic(6, sizeof(input_t), s_queue_storage, &s_queue_control);
}

void pocket_arcade_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (!atomic_load(&s_accept)) return;
    /* 上下键收按下沿和长按(长按用来翻页),确定键还要单击和双击。 */
    if (button != BSP_BTN_OK && event != BSP_BTN_PRESS && event != BSP_BTN_LONG) return;
    input_t input = {button, event, now_ms()};
    (void)xQueueSend(s_queue, &input, 0);
}

void pocket_arcade_enter(bool buttons_available)
{
    if (s_screen) return;
    pocket_arcade_prepare();
    if (!s_style_ready) {
        lv_style_init(&s_block_style);
        lv_style_set_pad_all(&s_block_style, 0);
        lv_style_set_border_width(&s_block_style, 0);
        lv_style_set_bg_opa(&s_block_style, LV_OPA_COVER);
        s_style_ready = true;
    }
    pa_record_t stored = pa_storage_init();
    pa_hub_init(&s_hub, &stored, (uint32_t)now_ms() * 2654435761U + 1U);

    s_buttons = buttons_available;
    s_dimmed = s_off = s_suppress_long = false;
    s_save_ms = 0;
    s_rect_used = s_label_used = 0;
    s_last_frame = s_last_activity = now_ms();
    memset(&s_shown, 0, sizeof(s_shown));
    memset(s_rect, 0, sizeof(s_rect));
    memset(s_label, 0, sizeof(s_label));
    s_footer_shown[0] = '\0';

    s_screen = ui_pixel_screen_create("");
    lv_obj_t *title = ui_pixel_label(s_screen, "口袋游戏厅", &pocket_arcade_zh_16, 0xFFFFFF);
    lv_obj_set_pos(title, 5, 15);
    lv_obj_set_width(title, 151);
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    s_battery = ui_pixel_label(s_screen, "--%", &lv_font_montserrat_14, UI_INK);
    lv_obj_set_pos(s_battery, 163, 31);
    lv_obj_set_width(s_battery, 65);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_RIGHT, 0);

    s_content = ui_pixel_panel_create(s_screen, 8, 52, 220, 226, UI_PAPER);
    s_rect_layer = layer_create();
    s_text_layer = layer_create();
    s_footer = ui_pixel_label(s_screen, "", &pocket_arcade_zh_16, UI_INK);
    lv_obj_set_pos(s_footer, 4, 294);
    lv_obj_set_width(s_footer, 232);
    lv_obj_set_style_text_align(s_footer, LV_TEXT_ALIGN_CENTER, 0);

    render();
    battery(NULL);
    s_timer = lv_timer_create(frame, FRAME_MS, NULL);
    s_battery_timer = lv_timer_create(battery, 10000, NULL);
    lv_screen_load(s_screen);
    bsp_display_backlight(100);
    xQueueReset(s_queue);
    atomic_store(&s_accept, buttons_available);
    ESP_LOGI("arcade", "ready: games=%d buttons=%d free heap=%u",
             PA_GAME_COUNT, buttons_available,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

void pocket_arcade_exit(void)
{
    atomic_store(&s_accept, false);
    if (s_timer) lv_timer_delete(s_timer);
    if (s_battery_timer) lv_timer_delete(s_battery_timer);
    s_timer = s_battery_timer = NULL;
    if (s_save_ms) pa_storage_save(&s_hub.record);
    s_save_ms = 0;
    /* 这两个数字加上剩余堆是设备上判断对象池是否吃紧的唯一依据。 */
    ESP_LOGI("arcade", "object high water: rects=%u labels=%u free heap=%u",
             (unsigned)s_rect_used, (unsigned)s_label_used,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    s_content = s_battery = s_footer = NULL;
    s_rect_layer = s_text_layer = NULL;
    memset(s_rect, 0, sizeof(s_rect));
    memset(s_label, 0, sizeof(s_label));
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
}
