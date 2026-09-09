#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "online_ui.h"
#include "online_runtime.h"
#include "fap_screenshot.h"
static void on_key(bsp_btn_t key,bsp_btn_ev_t event,void *arg) {
    (void)arg;online_ui_key(key,event);
}
void app_main(void) {
    bsp_i2c_init();
    if(bsp_display_init()!=ESP_OK || !bsp_lvgl_init())return;
    bsp_display_backlight(100);
    bool audio_ok=bsp_audio_init()==ESP_OK;
    bsp_battery_init();
    online_start(audio_ok);
    bool buttons_ok=bsp_button_init(on_key,NULL)==ESP_OK;
    if(bsp_lvgl_lock(1000)){online_ui_enter(buttons_ok);bsp_lvgl_unlock();}
    fap_screenshot_start();
}
