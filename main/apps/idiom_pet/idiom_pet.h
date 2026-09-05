#pragma once

#include <stdbool.h>
#include "bsp_button.h"

/* Call once from app_main before button registration, outside the LVGL lock. */
void idiom_pet_prepare(void);
/* These lifecycle functions require the LVGL lock. */
void idiom_pet_enter(bool buttons_available);
void idiom_pet_exit(void);
/* Nonblocking event delivery; no LVGL or storage calls. */
void idiom_pet_key(bsp_btn_t button, bsp_btn_ev_t event);
