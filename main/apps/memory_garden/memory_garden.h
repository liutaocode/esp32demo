#pragma once

#include "bsp_button.h"
#include <stdbool.h>

void memory_garden_enter(bool buttons_available);
void memory_garden_exit(void);
void memory_garden_key(bsp_btn_t button, bsp_btn_ev_t event);
