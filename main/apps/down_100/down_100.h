#pragma once
#include <stdbool.h>
#include "bsp_button.h"
void down_100_prepare(void);
void down_100_enter(bool buttons_available);
void down_100_exit(void);
void down_100_key(bsp_btn_t button, bsp_btn_ev_t event);
