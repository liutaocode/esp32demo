#pragma once
#include <stdbool.h>
#include "bsp_button.h"
void math_rail_prepare(void);
/* Enter and exit require the BSP LVGL lock. */
void math_rail_enter(bool buttons_available);
void math_rail_exit(void);
void math_rail_key(bsp_btn_t button, bsp_btn_ev_t event);
