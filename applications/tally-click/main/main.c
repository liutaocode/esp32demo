#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "fap_screenshot.h"
#include "tally/tally_runtime.h"
#include "tally/tally_ui.h"
#include "tally/tally_sound.h"
static void on_key(bsp_btn_t b,bsp_btn_ev_t e,void *user)
{ (void)user; tally_key(b,e); }
void app_main(void)
{
    bsp_i2c_init();
    if (bsp_display_init()!=ESP_OK || !bsp_lvgl_init()) return;
    bsp_display_backlight(90);
    bsp_battery_init();
    bool buttons_ok=bsp_button_init(on_key,NULL)==ESP_OK;
    if (!bsp_lvgl_lock(1000)) return;
    tc_ui_create();
    bsp_lvgl_unlock();
    tc_audio_start();
    tally_start(buttons_ok);
    fap_screenshot_start();
}
