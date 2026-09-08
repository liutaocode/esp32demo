#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "night_call.h"
#include "night_call_audio.h"
#include "night_call_storage.h"
#include "fap_screenshot.h"
#include "esp_log.h"
static void on_key(bsp_btn_t b,bsp_btn_ev_t e,void *arg) { (void)arg; night_call_key(b,e); }
void app_main(void) {
    ESP_LOGI("night_call","Night Call 1.0.0 starting");
    nc_state_t state; nc_storage_start(&state); night_call_prepare();
    bsp_i2c_init();
    if(bsp_display_init()!=ESP_OK || !bsp_lvgl_init()) { ESP_LOGE("night_call","display unavailable"); return; }
    bsp_display_backlight(80);
    bool audio=bsp_audio_init()==ESP_OK;
    bool battery=bsp_battery_init()==ESP_OK;
    nc_audio_start(audio);
    bool buttons=bsp_button_init(on_key,NULL)==ESP_OK;
    if(bsp_lvgl_lock(1000)) { night_call_enter(&state,buttons); bsp_lvgl_unlock(); }
    fap_screenshot_start();
    ESP_LOGI("night_call","ready: audio=%d buttons=%d battery=%d",audio,buttons,battery);
}
