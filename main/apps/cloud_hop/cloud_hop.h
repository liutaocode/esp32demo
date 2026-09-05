#pragma once
#include <stdbool.h>
#include "bsp_button.h"

/* prepare precedes callback registration; enter/exit require the LVGL lock.
 * key only enqueues an event and can run in the button task. */
void cloud_hop_prepare(void);
void cloud_hop_enter(bool buttons_available);
void cloud_hop_exit(void);
void cloud_hop_key(bsp_btn_t button, bsp_btn_ev_t event);
