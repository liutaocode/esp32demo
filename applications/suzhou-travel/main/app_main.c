#include "suzhou_travel.h"

#include "bsp_battery.h"
#include "bsp_audio.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "fap_screenshot.h"

static const char *TAG = "suzhou_app";

/* Button callbacks run outside LVGL, so every UI access is locked. */
static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *user_data)
{
    (void)user_data;
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    suzhou_travel_key(button, event);
    bsp_lvgl_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "Suzhou travel guide starting");

    bsp_i2c_init();
    struct _lv_display_t *display = NULL;
    if (bsp_display_init() != ESP_OK || !(display = bsp_lvgl_init())) {
        ESP_LOGE(TAG,
                 "display/LVGL init failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(90);

    const bool buttons_available = (bsp_button_init(on_key, NULL) == ESP_OK);
    const bool audio_available = (bsp_audio_init() == ESP_OK);
    const bool battery_available = (bsp_battery_init() == ESP_OK);

    if (bsp_lvgl_lock(1000)) {
        suzhou_travel_enter(buttons_available, battery_available, audio_available);
        if (fap_screenshot_init(display) != ESP_OK) {
            ESP_LOGW(TAG, "serial screenshot service unavailable");
        }
        bsp_lvgl_unlock();
    }

    ESP_LOGI(TAG, "ready: buttons=%d battery=%d audio=%d",
             buttons_available, battery_available, audio_available);
}
