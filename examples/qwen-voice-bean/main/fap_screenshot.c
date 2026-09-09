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

/* Optional local application QA input; screen capture itself stays read-only. */
__attribute__((weak)) bool fap_application_command(const char *line)
{
    (void)line;
    return false;
}


#define FAP_CMD "FAP_SCREENSHOT_V1"
#define FAP_CMD_LEN (sizeof(FAP_CMD) - 1)
#define FAP_LINE_MAX 24
#define FAP_TASK_STACK 8192
#define FAP_TASK_PRIO 3
#define FAP_TX_CHUNK 512
#define FAP_SNAP_BYTES ((uint32_t)BSP_LCD_W * BSP_LCD_H * 2)

/*
 * A capture streams the screen out band by band as LVGL renders it, so nothing
 * here holds a full frame. The 240x320 RGB565 frame would be 150 KB of internal
 * RAM, and on this board that is the same memory the radio stack and the audio
 * DMA descriptors need — an application had to choose between screen capture
 * and going online. Streaming removes the choice.
 *
 * The display renders in partial mode into a full-width 20-row buffer, so a
 * full-screen invalidation arrives as consecutive top-to-bottom bands. That is
 * exactly the row order the wire format needs. It is verified rather than
 * assumed: a capture renders twice, first probing the band geometry without
 * sending anything, and only then streaming the bytes.
 */
typedef enum { FAP_MODE_IDLE = 0, FAP_MODE_PROBE, FAP_MODE_STREAM } fap_mode_t;

static bool s_started;
static fap_mode_t s_mode;        /* Only mutated while the UI lock is held. */
static int s_expect_y;           /* Next row the band sequence must start on. */
static bool s_bands_ok;
static uint32_t s_sent_bytes;
static bool write_all(const void *data, size_t length);

static void mirror_flush_event(lv_event_t *event)
{
    if (s_mode == FAP_MODE_IDLE) return;

    lv_display_t *display = lv_event_get_target(event);
    const lv_area_t *area = lv_event_get_param(event);
    lv_draw_buf_t *draw_buf = lv_display_get_buf_active(display);
    if (!area || !draw_buf || !draw_buf->data ||
        lv_display_get_color_format(display) != LV_COLOR_FORMAT_RGB565) {
        s_bands_ok = false;
        return;
    }

    /* Anything but a full-width band starting exactly where the previous one
       ended would produce a skewed image, so refuse instead of guessing. */
    if (area->x1 != 0 || area->x2 != BSP_LCD_W - 1 ||
        area->y1 != s_expect_y || area->y2 < area->y1 || area->y2 >= BSP_LCD_H) {
        s_bands_ok = false;
        return;
    }

    if (s_mode == FAP_MODE_STREAM && s_bands_ok) {
        /* The event fires before the display port byte-swaps the buffer, so
           the rows are still little-endian, matching the announced format. */
        const uint8_t *source = draw_buf->data;
        uint32_t stride = lv_display_get_render_mode(display) == LV_DISPLAY_RENDER_MODE_PARTIAL
            ? lv_draw_buf_width_to_stride(lv_area_get_width(area), LV_COLOR_FORMAT_RGB565)
            : draw_buf->header.stride;
        uint32_t row_bytes = (uint32_t)BSP_LCD_W * 2;
        for (int y = area->y1; y <= area->y2; y++) {
            if (!write_all(source + (uint32_t)(y - area->y1) * stride, row_bytes)) {
                s_bands_ok = false;
                return;
            }
            s_sent_bytes += row_bytes;
        }
    }

    s_expect_y = area->y2 + 1;
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

/* Render the whole screen once with the callback in the given mode. */
static bool render_pass(fap_mode_t mode)
{
    s_mode = mode;
    s_expect_y = 0;
    s_bands_ok = true;
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(lv_display_get_default());
    s_mode = FAP_MODE_IDLE;
    return s_bands_ok && s_expect_y == BSP_LCD_H;
}

static void send_screen(void)
{
    if (!bsp_lvgl_lock(2000)) {
        ESP_LOGE(TAG, "could not lock UI for screen capture");
        return;
    }

    /* First pass proves the band geometry while nothing has been sent yet, so a
       display configuration this code cannot stream fails loudly and cleanly
       instead of putting a skewed frame on the wire. */
    if (!render_pass(FAP_MODE_PROBE)) {
        bsp_lvgl_unlock();
        ESP_LOGE(TAG, "display does not render full-width top-to-bottom bands; "
                      "capture unavailable");
        return;
    }

    char header[64];
    int header_length = snprintf(header, sizeof(header),
        "%s %d %d RGB565LE %lu\n", FAP_CMD, BSP_LCD_W, BSP_LCD_H,
        (unsigned long)FAP_SNAP_BYTES);

    esp_log_level_set("*", ESP_LOG_NONE);
    s_sent_bytes = 0;
    bool sent = write_all(header, (size_t)header_length) && render_pass(FAP_MODE_STREAM);

    /* The probe already proved this pass would cover the frame, so a short
       stream means the link failed underneath. Pad it out anyway: the host is
       waiting for exactly FAP_SNAP_BYTES and must not be left desynchronised. */
    if (s_sent_bytes < FAP_SNAP_BYTES) {
        static const uint8_t filler[FAP_TX_CHUNK] = { 0 };
        while (s_sent_bytes < FAP_SNAP_BYTES) {
            uint32_t left = FAP_SNAP_BYTES - s_sent_bytes;
            uint32_t chunk = left > FAP_TX_CHUNK ? FAP_TX_CHUNK : left;
            if (!write_all(filler, chunk)) break;
            s_sent_bytes += chunk;
        }
        sent = false;
    }
    esp_log_level_set("*", ESP_LOG_INFO);
    bsp_lvgl_unlock();

    if (sent) ESP_LOGI(TAG, "screen capture sent (%lu bytes)", (unsigned long)s_sent_bytes);
    else ESP_LOGE(TAG, "screen capture incomplete");
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
                window[used] = '\0';
                if (used) (void)fap_application_command(window);
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
