// main/main.c —— Pocket Bookshelf TTS application entry.
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "ebook.h"
#include "fap_screenshot.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include <string.h>

static const char *TAG = "main";
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    ebook_key(btn, ev);
}

void app_main(void) {
    ESP_LOGI(TAG, "Pocket Bookshelf TTS starting");
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "wake-up cause: %d", wakeup);
    }

    bsp_i2c_init();
    bsp_i2c_scan();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed; check SPI wiring "
                      "(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    bool buttons_ok = (bsp_button_init(on_key, NULL) == ESP_OK);
    bool audio_ok = (bsp_audio_init() == ESP_OK);
    bool battery_ok = (bsp_battery_init() == ESP_OK);

    ebook_prepare();
    if (bsp_lvgl_lock(1000)) {
        ebook_enter(buttons_ok);
        bsp_lvgl_unlock();
    }

    fap_screenshot_start();

    ESP_LOGI(TAG, "ready: buttons=%d audio=%d battery=%d",
             buttons_ok, audio_ok, battery_ok);
}

/* Serial test controls follow the physical key path; no codec work here. */
void fap_app_command(const char *line) {
    if (!strcmp(line, "BOOK_OK")) ebook_key(BSP_BTN_OK, BSP_BTN_CLICK);
    else if (!strcmp(line, "BOOK_UP")) ebook_key(BSP_BTN_UP, BSP_BTN_CLICK);
    else if (!strcmp(line, "BOOK_DOWN")) ebook_key(BSP_BTN_DOWN, BSP_BTN_CLICK);
    else if (!strcmp(line, "BOOK_BACK")) ebook_key(BSP_BTN_OK, BSP_BTN_LONG);
}
