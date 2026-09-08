#pragma once

#include "bsp_button.h"
#include <stdbool.h>

void minecraft_guide_enter(bool audio_available, bool buttons_available);
void minecraft_guide_key(bsp_btn_t btn, bsp_btn_ev_t ev);
