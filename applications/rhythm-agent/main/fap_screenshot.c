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
#define FAP_TASK_STACK 8192
#define FAP_TASK_PRIO 3
#define FAP_TX_CHUNK 512
#define FAP_SNAP_BYTES ((uint32_t)BSP_LCD_W * BSP_LCD_H * 2)

static uint8_t s_frame_pixels[FAP_SNAP_BYTES] __attribute__((aligned(64)));
static bool s_started;
static bool s_capture_frozen; /* Guarded by the LVGL lock. */

/*
 * Keep a copy of each area immediately before the display port byte-swaps and
 * sends it to the panel. This avoids rendering a second off-screen copy of the
 * complete object tree when the host requests a screenshot.
 */
static void mirror_flush_event(lv_event_t *event)
{
    if (s_capture_frozen) return;
    lv_display_t *display = lv_event_get_target(event);
    const lv_area_t *area = lv_event_get_param(event);
    lv_draw_buf_t *draw_buf = lv_display_get_buf_active(display);
    if (!area || !draw_buf || !draw_buf->data ||
        lv_display_get_color_format(display) != LV_COLOR_FORMAT_RGB565) {
        return;
    }

    int x1 = area->x1 < 0 ? 0 : area->x1;
    int y1 = area->y1 < 0 ? 0 : area->y1;
    int x2 = area->x2 >= BSP_LCD_W ? BSP_LCD_W - 1 : area->x2;
    int y2 = area->y2 >= BSP_LCD_H ? BSP_LCD_H - 1 : area->y2;
    if (x1 > x2 || y1 > y2) return;

    const uint8_t *source = draw_buf->data;
    uint32_t source_stride = draw_buf->header.stride;
    uint32_t source_x = (uint32_t)(x1 - area->x1) * 2;
    uint32_t copy_bytes = (uint32_t)(x2 - x1 + 1) * 2;
    for (int y = y1; y <= y2; y++) {
        uint32_t source_y = (uint32_t)(y - area->y1);
        uint8_t *destination = s_frame_pixels +
            ((uint32_t)y * BSP_LCD_W + (uint32_t)x1) * 2;
        memcpy(destination, source + source_y * source_stride + source_x,
               copy_bytes);
    }
}

static bool write_all(const void *data, size_t length)
{
    const uint8_t *cursor = data;
    while (length > 0) {
        size_t wanted = length > FAP_TX_CHUNK ? FAP_TX_CHUNK : length;
        int written = usb_serial_jtag_write_bytes(cursor, wanted,
                                                   pdMS_TO_TICKS(2000));
        if (written <= 0) return false;
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

    /* Force one complete normal refresh so every row in the mirror is fresh. */
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(lv_display_get_default());
    s_capture_frozen = true;
    bsp_lvgl_unlock();

    char header[64];
    int header_length = snprintf(header, sizeof(header),
        "%s %d %d RGB565LE %lu\n", FAP_CMD, BSP_LCD_W, BSP_LCD_H,
        (unsigned long)FAP_SNAP_BYTES);

    esp_log_level_set("*", ESP_LOG_NONE);
    bool sent = write_all(header, (size_t)header_length) &&
                write_all(s_frame_pixels, FAP_SNAP_BYTES);
    esp_log_level_set("*", ESP_LOG_INFO);
    if (bsp_lvgl_lock(0)) { s_capture_frozen = false; bsp_lvgl_unlock(); }
    if (sent) ESP_LOGI(TAG, "screen capture sent");
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
        ESP_LOGE(TAG, "could not install screen mirror");
        return;
    }
    lv_display_t *display = lv_display_get_default();
    if (!display) {
        bsp_lvgl_unlock();
        ESP_LOGE(TAG, "display is not ready for screen capture");
        return;
    }
    lv_display_add_event_cb(display, mirror_flush_event,
                            LV_EVENT_FLUSH_START, NULL);
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
