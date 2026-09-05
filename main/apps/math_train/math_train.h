#pragma once
#include <stdbool.h>
#include "bsp_button.h"
void math_train_prepare(void);
/* Enter/exit require the LVGL lock. Key only enqueues and never blocks. */
void math_train_enter(bool buttons_available);
void math_train_exit(void);
void math_train_key(bsp_btn_t button, bsp_btn_ev_t event);
