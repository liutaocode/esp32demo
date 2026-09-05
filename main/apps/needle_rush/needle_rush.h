#pragma once
#include <stdbool.h>
#include "bsp_button.h"

void needle_rush_prepare(void);
/* Enter/exit require the LVGL lock. Keys only enqueue, never touch LVGL. */
void needle_rush_enter(bool buttons_available);
void needle_rush_exit(void);
void needle_rush_key(bsp_btn_t button, bsp_btn_ev_t event);
