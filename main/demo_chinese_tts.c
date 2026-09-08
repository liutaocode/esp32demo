#include "tts_demo.h"
#include "tts_model.h"
#include "tts_runtime.h"
#include "bsp_battery.h"
#include "ui_pixel.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdio.h>

LV_FONT_DECLARE(chinese_tts_zh_16);
static lv_obj_t *screen, *category, *sentence, *status, *detail, *speed_label, *battery, *mascot;
static lv_timer_t *timer;
static QueueHandle_t keys;
static portMUX_TYPE key_guard = portMUX_INITIALIZER_UNLOCKED;
static tts_model_t model;
static bool available;
static esp_err_t local_error;
typedef struct { bsp_btn_t button; bsp_btn_ev_t event; } key_t;

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y, int width, uint32_t color) {
    lv_obj_t *obj = ui_pixel_label(parent, text, &chinese_tts_zh_16, color);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_width(obj, width);
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);
    return obj;
}
static void refresh_selection(void) {
    lv_label_set_text_fmt(category, "%02u/%02u  %s", (unsigned)model.selected + 1,
        (unsigned)tts_sample_count, tts_samples[model.selected].category);
    lv_label_set_text(sentence, tts_samples[model.selected].text);
    lv_label_set_text_fmt(speed_label, "语速 %u/5", model.speed);
}
static void tick(lv_timer_t *t) {
    (void)t;
    key_t key;
    while (xQueueReceive(keys, &key, 0) == pdTRUE) {
        if (key.event == BSP_BTN_CLICK) {
            if (key.button == BSP_BTN_UP || key.button == BSP_BTN_DOWN) {
                tts_runtime_stop();
                tts_model_move(&model, key.button == BSP_BTN_UP ? -1 : 1);
                refresh_selection();
            } else if (key.button == BSP_BTN_OK && available && tts_runtime_snapshot().status != TTS_LOADING) {
                if (tts_runtime_snapshot().status == TTS_SPEAKING ||
                    tts_runtime_snapshot().status == TTS_PREPARING) { tts_runtime_stop(); }
                else {
                    local_error = tts_runtime_say(tts_samples[model.selected].text, model.speed);
                    if (local_error == ESP_OK) ui_pixel_mascot_jump(mascot);
                }
            }
        } else if (key.event == BSP_BTN_LONG && key.button == BSP_BTN_OK) {
            tts_runtime_stop();
            tts_model_speed(&model); refresh_selection();
        }
    }
    tts_snapshot_t snap = tts_runtime_snapshot();
    if (!available || local_error != ESP_OK || snap.status == TTS_ERROR) {
        lv_label_set_text(status, snap.error == ESP_ERR_INVALID_SIZE ? "句子太长，请分句" : "暂时无法朗读");
        lv_label_set_text_fmt(detail, "%s", esp_err_to_name(snap.error != ESP_OK ? snap.error : local_error));
    } else {
        const char *text = snap.status == TTS_LOADING ? "正在加载语音库…" :
            snap.status == TTS_PREPARING ? "正在准备 · 确定取消" :
            snap.status == TTS_SPEAKING ? "正在朗读 · 确定停止" :
            "按确定，听听这一句";
        lv_label_set_text(status, text);
        lv_label_set_text_fmt(detail, "音频 %lu.%lus  合成 %lums",
            (unsigned long)(snap.audio_ms / 1000), (unsigned long)(snap.audio_ms % 1000 / 100),
            (unsigned long)snap.synth_ms);
    }
}
void tts_demo_enter(bool audio_ok, bool buttons_ok) {
    if (screen) return;
    model = (tts_model_t){.speed = 2};
    keys = xQueueCreate(12, sizeof(key_t));
    available = audio_ok && keys;
    local_error = available ? tts_runtime_start() : ESP_ERR_INVALID_STATE;
    available = available && local_error == ESP_OK;
    screen = ui_pixel_screen_create("SAY HELLO");
    battery = label(screen, "--%", 181, 31, 55, 0xFFFFFF);
    int soc = bsp_battery_soc();
    if (soc >= 0) lv_label_set_text_fmt(battery, "%d%%", soc);
    lv_obj_t *panel = ui_pixel_panel_create(screen, 12, 58, 210, 154, UI_PAPER);
    category = label(panel, "", 0, 0, 188, UI_SKY_DARK);
    sentence = label(panel, "", 0, 35, 186, UI_INK);
    lv_obj_set_style_text_line_space(sentence, 5, 0);
    mascot = ui_pixel_mascot_create(screen, 16, 216);
    status = label(screen, "", 64, 221, 171, 0xFFFFFF);
    speed_label = label(screen, "", 64, 248, 140, UI_YELLOW);
    detail = ui_pixel_label(screen, "", &lv_font_montserrat_14, 0xFFFFFF);
    /* Chinese units use the bundled font too. */
    lv_obj_set_style_text_font(detail, &chinese_tts_zh_16, 0);
    lv_obj_set_pos(detail, 12, 266);
    lv_obj_set_width(detail, 224);
    label(screen, buttons_ok ? "上下选句  确定播放/停止" : "按键不可用", 12, 296, 222, UI_INK);
    label(panel, "长按确定：切换语速", 0, 113, 188, UI_SKY_DARK);
    refresh_selection();
    lv_screen_load(screen);
    if (keys) timer = lv_timer_create(tick, 50, NULL);
    else lv_label_set_text(status, "内存不足，请重启");
}
void tts_demo_key(bsp_btn_t button, bsp_btn_ev_t event) {
    key_t key = {button, event};
    portENTER_CRITICAL(&key_guard);
    if (keys) xQueueSend(keys, &key, 0);
    portEXIT_CRITICAL(&key_guard);
}
void tts_demo_exit(void) {
    if (!screen) return;
    if (timer) { lv_timer_delete(timer); timer = NULL; }
    portENTER_CRITICAL(&key_guard);
    QueueHandle_t old = keys; keys = NULL;
    portEXIT_CRITICAL(&key_guard);
    if (old) vQueueDelete(old);
    /* Worker never touches LVGL. Keep its resources alive if codec I/O has not exited. */
    if (!tts_runtime_shutdown(3000)) return;
    lv_obj_t *empty = lv_obj_create(NULL);
    lv_screen_load(empty);
    lv_obj_delete(screen); screen = NULL;
}
