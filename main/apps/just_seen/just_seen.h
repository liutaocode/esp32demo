#pragma once
#include <stdbool.h>
#include "bsp_button.h"
void just_seen_prepare(void);
/* Enter/exit require the LVGL lock. Input only enqueues, never accesses UI. */
void just_seen_enter(bool buttons_available);
void just_seen_exit(void);
void just_seen_key(bsp_btn_t button, bsp_btn_ev_t event);
