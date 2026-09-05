#pragma once

#include "bsp_button.h"
#include <stdbool.h>

void vibe_check_enter(bool buttons_available, bool audio_available);
void vibe_check_key(bsp_btn_t button, bsp_btn_ev_t event);
