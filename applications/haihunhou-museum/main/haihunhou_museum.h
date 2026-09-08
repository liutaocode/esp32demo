#pragma once

#include "bsp_button.h"
#include <stdbool.h>

void haihunhou_museum_enter(bool buttons_available, bool audio_available);
void haihunhou_museum_key(bsp_btn_t button, bsp_btn_ev_t event);
