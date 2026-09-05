#pragma once
#include <stdbool.h>
#include "bsp_button.h"
/* prepare/key are callback-safe; enter/exit require the LVGL lock. */
void ricochet_rush_prepare(void);
void ricochet_rush_enter(bool buttons_available);
void ricochet_rush_exit(void);
void ricochet_rush_key(bsp_btn_t btn, bsp_btn_ev_t ev);
