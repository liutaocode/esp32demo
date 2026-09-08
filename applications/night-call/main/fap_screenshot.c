#include "fap_screenshot.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "fap_shot";

#define FAP_CMD "FAP_SCREENSHOT_V1"
#define FAP_CMD_LEN (sizeof(FAP_CMD) - 1)
#define FAP_LINE_MAX 24
/* lv_refr_now executes the font renderer on this task. The ESP32-C3
 * needs more than 4 KiB for the deepest Chinese label path. */
#define FAP_TASK_STACK 8192
#define FAP_TASK_PRIO 3
#define FAP_TX_CHUNK 512
#define FAP_SNAP_BYTES ((uint32_t)BSP_LCD_W * BSP_LCD_H * 2)

static bool s_started, s_hook_installed;
/* Only read/changed while holding the LVGL lock. No full-frame allocation. */
static bool s_capturing, s_capture_ok;
static int s_capture_row;
static bool write_all(const void *data, size_t length);

/* A forced full refresh emits full-width strips in increasing row order.
 * Stream each strip before the display port swaps bytes for the real panel.
 * Out-of-order/partial strips fail closed instead of returning a false capture.
 */
static void stream_flush_event(lv_event_t *event)
{
    if (!s_capturing || !s_capture_ok) return;
    lv_display_t *display = lv_event_get_target(event);
    const lv_area_t *area = lv_event_get_param(event);
    lv_draw_buf_t *buf = lv_display_get_buf_active(display);
    if (!area || !buf || !buf->data || area->x1 != 0 ||
        area->x2 != BSP_LCD_W - 1 || area->y1 != s_capture_row ||
        area->y2 < area->y1 || area->y2 >= BSP_LCD_H ||
        buf->header.stride < BSP_LCD_W * 2 ||
        lv_display_get_color_format(display) != LV_COLOR_FORMAT_RGB565) {
        s_capture_ok = false; return;
    }
    for (int y=area->y1; y<=area->y2; y++) {
        if (!write_all(buf->data + (y-area->y1)*buf->header.stride, BSP_LCD_W*2)) {
            s_capture_ok = false; return;
        }
        s_capture_row++;
    }
}

static bool write_all(const void *data, size_t length)
{
    const uint8_t *cursor = data;
    while (length > 0) {
        size_t wanted = length > FAP_TX_CHUNK ? FAP_TX_CHUNK : length;
        int written = usb_serial_jtag_write_bytes(cursor, wanted,
                                                   pdMS_TO_TICKS(2000));
        if (written <= 0 || (size_t)written > wanted) return false;
        cursor += written;
        length -= (size_t)written;
    }
    return true;
}

static void send_screen(void)
{
    if (!bsp_lvgl_lock(2000)) {
        ESP_LOGE(TAG, "could not lock UI for screen capture");
        return;
    }

    lv_display_t *display = lv_display_get_default();
    if (!display || lv_display_get_color_format(display) != LV_COLOR_FORMAT_RGB565) {
        bsp_lvgl_unlock(); return;
    }
    char header[64];
    int length = snprintf(header,sizeof(header),"%s %d %d RGB565LE %lu\n",
        FAP_CMD,BSP_LCD_W,BSP_LCD_H,(unsigned long)FAP_SNAP_BYTES);
    esp_log_level_t previous_level = esp_log_level_get("*");
    esp_log_level_set("*", ESP_LOG_NONE);
    s_capture_row=0;
    s_capture_ok=write_all(header,(size_t)length);
    s_capturing=s_capture_ok;
    if (s_capturing) {
        lv_obj_invalidate(lv_screen_active());
        lv_refr_now(display);
    }
    s_capturing=false;
    bool sent=s_capture_ok && s_capture_row==BSP_LCD_H;
    bsp_lvgl_unlock();
    esp_log_level_set("*", previous_level);
    if (sent) ESP_LOGI(TAG,"screen capture sent; stack margin=%u bytes",
                       (unsigned)uxTaskGetStackHighWaterMark(NULL));
    else ESP_LOGE(TAG,"screen capture incomplete; retry the read-only request");
}

static void screenshot_task(void *argument)
{
    (void)argument;
    char window[FAP_LINE_MAX];
    size_t used = 0;
    uint8_t input[16];

    for (;;) {
        if (!usb_serial_jtag_is_driver_installed()) {
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }

        int count = usb_serial_jtag_read_bytes(input, sizeof(input),
                                                pdMS_TO_TICKS(500));
        if (count < 0) {
            vTaskDelay(pdMS_TO_TICKS(200));
            continue;
        }
        if (count == 0) continue;

        for (int index = 0; index < count; index++) {
            char character = (char)input[index];
            if (character == '\r' || character == '\n') {
                used = 0;
                continue;
            }
            if (used < sizeof(window) - 1) {
                window[used++] = character;
            } else {
                memmove(window, window + 1, sizeof(window) - 2);
                used = sizeof(window) - 2;
                window[used++] = character;
            }
            if (used == FAP_CMD_LEN &&
                memcmp(window, FAP_CMD, FAP_CMD_LEN) == 0) {
                send_screen();
                used = 0;
            }
        }
    }
}

void fap_screenshot_start(void)
{
    if (s_started) return;

    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t config = {
            .rx_buffer_size = 256,
            .tx_buffer_size = 1024,
        };
        esp_err_t error = usb_serial_jtag_driver_install(&config);
        if (error != ESP_OK) {
            ESP_LOGE(TAG, "serial capture driver failed: %s",
                     esp_err_to_name(error));
            return;
        }
    }
    usb_serial_jtag_vfs_use_driver();

    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "could not install capture hook");
        return;
    }
    lv_display_t *display = lv_display_get_default();
    if (!display) {
        bsp_lvgl_unlock();
        ESP_LOGE(TAG, "display is not ready for screen capture");
        return;
    }
    if (!s_hook_installed) {
        lv_display_add_event_cb(display, stream_flush_event,
                                LV_EVENT_FLUSH_START, NULL);
        s_hook_installed = true;
    }
    lv_obj_invalidate(lv_screen_active());
    bsp_lvgl_unlock();

    if (xTaskCreate(screenshot_task, "fap_shot", FAP_TASK_STACK, NULL,
                    FAP_TASK_PRIO, NULL) != pdPASS) {
        ESP_LOGE(TAG, "screen capture task creation failed");
        return;
    }
    s_started = true;
    ESP_LOGI(TAG, "FAP_SCREENSHOT_V1 ready");
}
