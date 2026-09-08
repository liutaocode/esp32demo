#pragma once
#include <stdbool.h>
#include "bsp_button.h"
#include "jelly_squeeze_state.h"

/* prepare precedes callback registration; enter/exit require the LVGL lock.
 * key only enqueues an event and can run in the button task. */
void jelly_squeeze_prepare(void);
void jelly_squeeze_enter(bool buttons_available, js_progress_t saved);
void jelly_squeeze_exit(void);
void jelly_squeeze_key(bsp_btn_t button, bsp_btn_ev_t event);
