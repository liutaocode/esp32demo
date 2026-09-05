#pragma once
#include <stdbool.h>
#include "bsp_button.h"
/* Prepare before registering the button callback; key only queues input. */
void pvz_almanac_prepare(void);
/* enter/exit execute while holding the LVGL lock, outside button callbacks. */
void pvz_almanac_enter(bool audio_ok, bool buttons_ok);
void pvz_almanac_exit(void);
void pvz_almanac_key(bsp_btn_t btn, bsp_btn_ev_t event);
