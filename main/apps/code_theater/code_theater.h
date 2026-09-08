#pragma once
#include <stdbool.h>
#include "bsp_button.h"
#include "lvgl.h"
void code_theater_prepare(void);
void code_theater_enter(bool buttons_available);
void code_theater_exit(void);
void code_theater_key(bsp_btn_t button, bsp_btn_ev_t event);
