#include "fap_screenshot.h"

#include "bsp_display.h"
#include "driver/usb_serial_jtag.h"
#include "driver/usb_serial_jtag_vfs.h"
#include "esp_log.h"
#include "fap_screenshot_protocol.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <stdio.h>
#include <string.h>

#define SCREENSHOT_TASK_STACK 4096
#define SCREENSHOT_TASK_PRIORITY 3
#define USB_WRITE_TIMEOUT_MS 5000
#define USB_WRITE_CHUNK 512

static const char *TAG = "fap_capture";
static lv_display_t *s_display;
static bool s_streaming;
static bool s_stream_failed;
static size_t s_streamed_bytes;
static int32_t s_expected_y;

static bool usb_write_all(const uint8_t *data, size_t length)
{
    const TickType_t deadline = xTaskGetTickCount() +
                                pdMS_TO_TICKS(USB_WRITE_TIMEOUT_MS);
    size_t offset = 0;
    while (offset < length) {
        const size_t remaining = length - offset;
        const size_t chunk = remaining < USB_WRITE_CHUNK ?
                             remaining : USB_WRITE_CHUNK;
        const int written = usb_serial_jtag_write_bytes(
            data + offset, chunk, pdMS_TO_TICKS(20));
        if (written > 0) {
            offset += (size_t)written;
            continue;
        }
        if (written < 0 || (int32_t)(xTaskGetTickCount() - deadline) >= 0) {
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return true;
}

static void capture_flush_event(lv_event_t *event)
{
    if (!s_streaming || s_stream_failed) {
        return;
    }

    lv_display_t *display = lv_event_get_target(event);
    const lv_area_t *area = lv_event_get_param(event);
    lv_draw_buf_t *buffer = lv_display_get_buf_active(display);
    const int32_t width = lv_display_get_horizontal_resolution(display);
    const int32_t height = lv_display_get_vertical_resolution(display);
    if (!area || !buffer || !buffer->data || area->x1 != 0 ||
        area->x2 != width - 1 || area->y1 != s_expected_y ||
        area->y2 >= height) {
        s_stream_failed = true;
        return;
    }

    const size_t row_bytes = (size_t)width * 2;
    const size_t rows = (size_t)lv_area_get_height(area);
    const size_t stride = buffer->header.stride;
    for (size_t row = 0; row < rows; row++) {
        if (!usb_write_all(buffer->data + row * stride, row_bytes)) {
            s_stream_failed = true;
            return;
        }
        s_streamed_bytes += row_bytes;
    }
    s_expected_y = area->y2 + 1;
}

static void capture_current_screen(void)
{
    if (!s_display || !bsp_lvgl_lock(2000)) {
        return;
    }

    const int32_t width = lv_display_get_horizontal_resolution(s_display);
    const int32_t height = lv_display_get_vertical_resolution(s_display);
    if (width <= 0 || height <= 0 ||
        lv_display_get_color_format(s_display) != LV_COLOR_FORMAT_RGB565) {
        bsp_lvgl_unlock();
        return;
    }

    const esp_log_level_t previous_level = esp_log_get_level_master();
    esp_log_set_level_master(ESP_LOG_NONE);

    char header[80];
    const size_t payload_size = (size_t)width * (size_t)height * 2;
    const int header_length = snprintf(
        header, sizeof(header), "FAP_SCREENSHOT_V1 %ld %ld RGB565LE %u\n",
        (long)width, (long)height, (unsigned)payload_size);

    s_stream_failed = header_length <= 0 ||
                      (size_t)header_length >= sizeof(header) ||
                      !usb_write_all((const uint8_t *)header,
                                     (size_t)header_length);
    s_streamed_bytes = 0;
    s_expected_y = 0;
    s_streaming = !s_stream_failed;
    if (s_streaming) {
        lv_obj_invalidate(lv_display_get_screen_active(s_display));
        lv_refr_now(s_display);
    }
    s_streaming = false;

    if (usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(USB_WRITE_TIMEOUT_MS)) ==
        ESP_OK) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    esp_log_set_level_master(previous_level);
    bsp_lvgl_unlock();

    if (s_stream_failed || s_streamed_bytes != payload_size ||
        s_expected_y != height) {
        ESP_LOGE(TAG, "screen capture incomplete: sent=%u expected=%u y=%ld",
                 (unsigned)s_streamed_bytes, (unsigned)payload_size,
                 (long)s_expected_y);
    } else {
        ESP_LOGI(TAG, "screen capture complete: %ux%u",
                 (unsigned)width, (unsigned)height);
    }
}

static void screenshot_task(void *argument)
{
    (void)argument;
    fap_screenshot_parser_t parser;
    fap_screenshot_parser_init(&parser);
    uint8_t input[64];

    for (;;) {
        const int received = usb_serial_jtag_read_bytes(
            input, sizeof(input), pdMS_TO_TICKS(20));
        for (int i = 0; i < received; i++) {
            if (fap_screenshot_parser_feed(&parser, input[i])) {
                capture_current_screen();
            }
        }
        if (received == 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    vTaskDelete(NULL);
}

esp_err_t fap_screenshot_init(lv_display_t *display)
{
    if (!display) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_display) {
        return s_display == display ? ESP_OK : ESP_ERR_INVALID_STATE;
    }

    if (!usb_serial_jtag_is_driver_installed()) {
        usb_serial_jtag_driver_config_t config = {
            .tx_buffer_size = 4096,
            .rx_buffer_size = 256,
        };
        const esp_err_t error = usb_serial_jtag_driver_install(&config);
        if (error != ESP_OK) {
            return error;
        }
        usb_serial_jtag_vfs_use_driver();
    }

    s_display = display;
    if (xTaskCreate(screenshot_task, "fap_capture", SCREENSHOT_TASK_STACK,
                    NULL, SCREENSHOT_TASK_PRIORITY, NULL) != pdPASS) {
        s_display = NULL;
        return ESP_ERR_NO_MEM;
    }
    lv_display_add_event_cb(display, capture_flush_event,
                            LV_EVENT_FLUSH_START, NULL);
    ESP_LOGI(TAG, "FAP_SCREENSHOT_V1 ready");
    return ESP_OK;
}
