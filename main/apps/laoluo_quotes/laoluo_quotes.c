#include "laoluo_quotes.h"
#include "laoluo_adpcm.h"
#include "laoluo_quotes_audio.h"
#include "laoluo_quotes_catalog.h"
#include "laoluo_quotes_state.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "ui_pixel.h"
#include <stdint.h>

LV_FONT_DECLARE(laoluo_quotes_zh_18);

#define LAOLUO_DIM_AFTER_MS 60000U
#define LAOLUO_OFF_AFTER_MS 180000U
#define LAOLUO_SAMPLE_RATE 16000U

static const char *TAG = "laoluo_quotes";
static const uint32_t CATEGORY_COLORS[] = {
    0xF7C948, 0xF08A5D, 0x63B4D1, 0x72B56B, 0xB58BD0, 0xE76F51,
};

static laoluo_quotes_state_t s_state;
static lv_obj_t *s_screen;
static lv_obj_t *s_category;
static lv_obj_t *s_counter;
static lv_obj_t *s_quote;
static lv_obj_t *s_status;
static lv_obj_t *s_battery;
static lv_obj_t *s_battery_fill;
static lv_obj_t *s_accent;
static lv_timer_t *s_battery_timer;
static lv_timer_t *s_idle_timer;
static TaskHandle_t s_audio_task;
static bool s_audio_available;
static bool s_buttons_available;
static bool s_backlight_dimmed;
static bool s_backlight_off;
static uint64_t s_last_input_ms;
static int16_t s_pcm_buffer[512];

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

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       int width, uint32_t color, lv_text_align_t align)
{
    lv_obj_t *object = ui_pixel_label(parent, text, &laoluo_quotes_zh_18, color);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, width);
    lv_obj_set_style_text_align(object, align, 0);
    return object;
}

static void refresh_battery(lv_timer_t *timer)
{
    (void)timer;
    if (!s_battery) return;
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(s_battery, "--%");
        lv_obj_set_style_bg_opa(s_battery_fill, LV_OPA_TRANSP, 0);
        return;
    }
    if (soc > 100) soc = 100;
    lv_label_set_text_fmt(s_battery, "%d%%", soc);
    lv_obj_set_width(s_battery_fill, LV_MAX(1, (34 * soc + 99) / 100));
    lv_obj_set_style_bg_color(s_battery_fill,
        lv_color_hex(soc <= 20 ? UI_RED : (soc <= 45 ? UI_YELLOW : UI_GRASS)), 0);
    lv_obj_set_style_bg_opa(s_battery_fill, LV_OPA_COVER, 0);
}

static void create_battery(void)
{
    lv_obj_t *shell = block(s_screen, 176, 33, 52, 22, UI_INK);
    lv_obj_t *inside = block(shell, 3, 3, 44, 16, 0x3E4650);
    block(s_screen, 228, 39, 4, 10, UI_INK);
    s_battery_fill = block(inside, 3, 3, 34, 10, UI_GRASS);
    s_battery = ui_pixel_label(inside, "--%", &lv_font_montserrat_14, 0xFFFFFF);
    lv_obj_set_width(s_battery, 42);
    lv_obj_set_pos(s_battery, 0, 0);
    lv_label_set_long_mode(s_battery, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_CENTER, 0);
}

static void create_speaker(lv_obj_t *parent, int x, int y)
{
    block(parent, x, y + 8, 9, 15, UI_INK);
    block(parent, x + 9, y + 4, 8, 23, UI_INK);
    block(parent, x + 17, y, 5, 31, UI_INK);
    block(parent, x + 25, y + 7, 4, 17, UI_SKY_DARK);
    block(parent, x + 32, y + 11, 4, 9, UI_SKY_DARK);
}

static void create_nav_button(int x, const char *text, uint32_t color)
{
    block(s_screen, x + 3, 277, 68, 27, UI_INK);
    lv_obj_t *button = block(s_screen, x, 274, 68, 27, color);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(UI_INK), 0);
    lv_obj_t *caption = label(button, text, 0, 2, 64, UI_INK, LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(caption, LV_LABEL_LONG_CLIP);
}

static void render_quote(void)
{
    if (laoluo_quotes_catalog_count == 0) return;
    const laoluo_quote_t *quote = &laoluo_quotes_catalog[s_state.index];
    uint32_t accent = CATEGORY_COLORS[s_state.index %
        (sizeof(CATEGORY_COLORS) / sizeof(CATEGORY_COLORS[0]))];

    lv_label_set_text(s_category, quote->category);
    lv_obj_set_style_bg_color(s_category, lv_color_hex(accent), 0);
    lv_label_set_text_fmt(s_counter, "第 %u 条 / 共 %u 条",
                          (unsigned)s_state.index + 1U,
                          (unsigned)laoluo_quotes_catalog_count);
    lv_label_set_text(s_quote, quote->text);
    lv_obj_set_style_bg_color(s_accent, lv_color_hex(accent), 0);
    if (s_buttons_available) {
        lv_label_set_text(s_status,
            s_audio_available ? "合成语音 / 非本人录音" : "语音不可用");
    }
}

static void set_audio_status(const char *text)
{
    if (!bsp_lvgl_lock(500)) return;
    if (s_status) lv_label_set_text(s_status, text);
    bsp_lvgl_unlock();
}

static bool play_quote(size_t quote_index, uint32_t *next_request)
{
    *next_request = 0;
    if (quote_index >= laoluo_audio_clip_count ||
        quote_index >= laoluo_quotes_catalog_count) {
        ESP_LOGE(TAG, "missing TTS clip for quote %u", (unsigned)quote_index);
        set_audio_status("语音不可用");
        return true;
    }

    const laoluo_audio_clip_t *clip = &laoluo_audio_clips[quote_index];
    if (clip->sample_count == 0 ||
        clip->offset + clip->adpcm_size > laoluo_audio_data_size ||
        (clip->sample_count / 2U) > clip->adpcm_size) {
        ESP_LOGE(TAG, "invalid TTS clip for quote %u", (unsigned)quote_index);
        set_audio_status("语音不可用");
        return true;
    }
    if (bsp_audio_set_format(LAOLUO_SAMPLE_RATE, 16, 1) != ESP_OK) {
        set_audio_status("语音不可用");
        return true;
    }

    bsp_audio_set_volume(75);
    set_audio_status("正在播放…");
    laoluo_adpcm_state_t decoder;
    laoluo_adpcm_init(&decoder, clip->initial_predictor,
                       clip->initial_step_index);
    size_t sample = 0;
    while (sample < clip->sample_count) {
        if (xTaskNotifyWait(0, UINT32_MAX, next_request, 0) == pdTRUE) {
            return false;
        }
        size_t count = 0;
        while (count < sizeof(s_pcm_buffer) / sizeof(s_pcm_buffer[0]) &&
               sample < clip->sample_count) {
            if (sample == 0) {
                s_pcm_buffer[count++] = (int16_t)decoder.predictor;
            } else {
                size_t nibble = sample - 1U;
                uint8_t packed = laoluo_audio_data[clip->offset + nibble / 2U];
                uint8_t code = (nibble & 1U) ? (packed >> 4) : (packed & 0x0fU);
                s_pcm_buffer[count++] = laoluo_adpcm_decode(&decoder, code);
            }
            sample++;
        }
        if (bsp_audio_write(s_pcm_buffer, count * sizeof(s_pcm_buffer[0])) != ESP_OK) {
            ESP_LOGE(TAG, "TTS playback failed at sample %u", (unsigned)sample);
            set_audio_status("播放失败");
            return true;
        }
    }
    set_audio_status("朗读完毕 / 确定重播");
    return true;
}

static void audio_task(void *argument)
{
    (void)argument;
    for (;;) {
        uint32_t request = 0;
        xTaskNotifyWait(0, UINT32_MAX, &request, portMAX_DELAY);
        while (request > 0 && request <= laoluo_audio_clip_count) {
            uint32_t next_request = 0;
            play_quote(request - 1U, &next_request);
            request = next_request;
        }
    }
}

static void request_current_audio(void)
{
    if (!s_audio_task || laoluo_quotes_catalog_count == 0) return;
    laoluo_quotes_state_mark_played(&s_state);
    xTaskNotify(s_audio_task, (uint32_t)s_state.index + 1U,
                eSetValueWithOverwrite);
}

static void stop_audio(void)
{
    if (s_audio_task) xTaskNotify(s_audio_task, 0, eSetValueWithOverwrite);
}

static void idle_tick(lv_timer_t *timer)
{
    (void)timer;
    uint64_t now_ms = (uint64_t)esp_timer_get_time() / 1000ULL;
    uint64_t idle_ms = now_ms - s_last_input_ms;
    if (!s_backlight_off && idle_ms >= LAOLUO_OFF_AFTER_MS) {
        bsp_display_backlight(0);
        s_backlight_off = true;
        s_backlight_dimmed = true;
    } else if (!s_backlight_dimmed && idle_ms >= LAOLUO_DIM_AFTER_MS) {
        bsp_display_backlight(20);
        s_backlight_dimmed = true;
    }
}

void laoluo_quotes_enter(bool audio_available, bool buttons_available)
{
    s_audio_available = audio_available;
    s_buttons_available = buttons_available;
    laoluo_quotes_state_init(&s_state, esp_random(), laoluo_quotes_catalog_count);
    s_screen = ui_pixel_screen_create("");
    label(s_screen, "老罗语录", 19, 15, 124, UI_INK, LV_TEXT_ALIGN_CENTER);
    label(s_screen, "老罗语录", 17, 13, 124, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    create_battery();

    lv_obj_t *card = ui_pixel_panel_create(s_screen, 8, 60, 224, 201, UI_PAPER);
    s_accent = block(card, 0, 0, 5, 185, UI_YELLOW);
    s_category = label(card, "", 10, 5, 52, UI_INK, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(s_category, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(s_category, 5, 0);
    lv_obj_set_style_pad_right(s_category, 5, 0);
    s_counter = label(card, "", 70, 7, 132, UI_SKY_DARK, LV_TEXT_ALIGN_RIGHT);

    block(card, 10, 35, 192, 3, UI_INK);
    s_quote = label(card, "", 10, 49, 192, UI_INK, LV_TEXT_ALIGN_LEFT);
    lv_label_set_long_mode(s_quote, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(s_quote, 5, 0);

    create_speaker(card, 12, 153);
    label(card, "— 罗永浩", 52, 157, 146, UI_SKY_DARK, LV_TEXT_ALIGN_RIGHT);

    create_nav_button(8, "上一个", 0xC7E7F4);
    create_nav_button(86, "朗读", UI_YELLOW);
    create_nav_button(164, "下一个", 0xC7E7F4);
    s_status = label(s_screen,
        buttons_available ? (audio_available ? "合成语音 / 非本人录音"
                                             : "语音不可用")
                          : "按键不可用",
        8, 302, 224, 0xFFFFFF, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_bg_color(s_status, lv_color_hex(0x403225), 0);
    lv_obj_set_style_bg_opa(s_status, LV_OPA_COVER, 0);

    render_quote();
    refresh_battery(NULL);
    s_battery_timer = lv_timer_create(refresh_battery, 10000, NULL);
    s_idle_timer = lv_timer_create(idle_tick, 1000, NULL);
    s_last_input_ms = (uint64_t)esp_timer_get_time() / 1000ULL;
    lv_screen_load(s_screen);

    if (audio_available && !s_audio_task &&
        xTaskCreate(audio_task, "laoluo_tts", 5120, NULL, 4,
                    &s_audio_task) != pdPASS) {
        s_audio_available = false;
        lv_label_set_text(s_status, "语音不可用");
    }
}

void laoluo_quotes_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_CLICK || !s_buttons_available) return;
    s_last_input_ms = (uint64_t)esp_timer_get_time() / 1000ULL;
    bool was_off = s_backlight_off;
    if (s_backlight_dimmed) {
        bsp_display_backlight(100);
        s_backlight_dimmed = false;
        s_backlight_off = false;
    }
    if (was_off) return;

    if (button == BSP_BTN_UP || button == BSP_BTN_DOWN) {
        stop_audio();
        laoluo_quotes_state_move(&s_state,
            button == BSP_BTN_UP ? -1 : 1, laoluo_quotes_catalog_count);
        render_quote();
    } else if (button == BSP_BTN_OK && s_audio_available) {
        request_current_audio();
    }
}
