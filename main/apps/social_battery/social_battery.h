#pragma once

#include "bsp_button.h"
#include <stdbool.h>

/* Enter/exit require the LVGL lock. Key only enqueues and never touches LVGL. */
void social_battery_enter(bool buttons_available);
void social_battery_exit(void);
void social_battery_key(bsp_btn_t button, bsp_btn_ev_t event);
