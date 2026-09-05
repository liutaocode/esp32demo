#pragma once

#include "bsp_button.h"
#include <stdbool.h>

void tomato_bloom_enter(bool buttons_available);
void tomato_bloom_exit(void);
void tomato_bloom_key(bsp_btn_t button, bsp_btn_ev_t event);
