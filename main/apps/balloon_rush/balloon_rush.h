#pragma once
#include <stdbool.h>
#include "bsp_button.h"
/* Prepare before registering the button callback. Queue lifetime is static. */
void balloon_rush_prepare(void);
/* Called with the LVGL lock held. Exit cancels all UI timers. */
void balloon_rush_enter(bool buttons_available);
void balloon_rush_exit(void);
/* Nonblocking; accepts press edges, ignores click/double release events. */
void balloon_rush_key(bsp_btn_t button, bsp_btn_ev_t event);
