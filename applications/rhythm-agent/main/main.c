#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "rhythm_agent.h"
#include "rhythm_audio.h"
#include "rhythm_service.h"
#include "fap_screenshot.h"
#include "esp_log.h"
static void on_key(bsp_btn_t key,bsp_btn_ev_t event,void *unused) {
    (void)unused; rhythm_agent_key(key,event);
}
void app_main(void) {
    (void)bsp_i2c_init();
    if(bsp_display_init()!=ESP_OK || !bsp_lvgl_init()) return;
    bsp_display_backlight(85); rhythm_agent_prepare();
    bool buttons=bsp_button_init(on_key,NULL)==ESP_OK;
    bool audio=bsp_audio_init()==ESP_OK;
    (void)bsp_battery_init(); ra_service_start(); ra_audio_start(audio);
    if(bsp_lvgl_lock(1000)) {rhythm_agent_enter(buttons);bsp_lvgl_unlock();}
    fap_screenshot_start();
    ESP_LOGI("rhythm_agent","ready; buttons=%d audio=%d",buttons,audio);
}
