#pragma once
#include <stdbool.h>
#include "bsp_button.h"

void perfect_slice_prepare(void);
/* Enter/exit require the LVGL lock. Key only queues a nonblocking event. */
void perfect_slice_enter(bool buttons_available);
void perfect_slice_exit(void);
void perfect_slice_key(bsp_btn_t button, bsp_btn_ev_t event);
