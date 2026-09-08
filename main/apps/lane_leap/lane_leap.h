#pragma once
#include <stdbool.h>
#include "bsp_button.h"
/* prepare/key are callback-safe. enter/exit require the LVGL lock. */
void lane_leap_prepare(void);
void lane_leap_enter(bool buttons_available);
void lane_leap_exit(void);
void lane_leap_key(bsp_btn_t button, bsp_btn_ev_t event);
