#pragma once
#include <stdbool.h>
#include "bsp_button.h"
/* Prepare before locking LVGL; enter and exit with the LVGL lock held. */
void pocket_pond_prepare(void);
void pocket_pond_enter(bool buttons_available);
void pocket_pond_exit(void);
/* Callback is nonblocking and only enqueues input. */
void pocket_pond_key(bsp_btn_t button, bsp_btn_ev_t event);
