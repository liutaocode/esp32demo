#pragma once

#include "bsp_button.h"
#include <stdbool.h>

void laoluo_quotes_enter(bool audio_available, bool buttons_available);
void laoluo_quotes_key(bsp_btn_t button, bsp_btn_ev_t event);
