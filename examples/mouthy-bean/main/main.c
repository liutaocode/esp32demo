#include "mouthy_bean.h"
#include "bean_runtime.h"
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "fap_screenshot.h"
#include "esp_log.h"

static void on_key(bsp_btn_t key, bsp_btn_ev_t event, void *user)
{
    (void)user;
    mouthy_bean_key(key, event);
}

void app_main(void)
{
    bsp_i2c_init();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE("mouthy_bean", "Display initialization failed");
        return;
    }
    mouthy_bean_prepare();
    bean_runtime_start();
    bool buttons = bsp_button_init(on_key, NULL) == ESP_OK;
    bsp_battery_init();
    if (bsp_lvgl_lock(1000)) {
        mouthy_bean_enter(buttons);
        bsp_lvgl_unlock();
    }
    bsp_display_backlight(100);
    fap_screenshot_start();
}
