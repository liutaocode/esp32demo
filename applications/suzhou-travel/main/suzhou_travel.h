#pragma once

#include "bsp_button.h"
#include <stdbool.h>

void suzhou_travel_enter(bool buttons_available, bool battery_available,
                         bool audio_available);
void suzhou_travel_exit(void);
void suzhou_travel_key(bsp_btn_t button, bsp_btn_ev_t event);
