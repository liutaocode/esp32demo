#pragma once
#include <stdbool.h>
#include "bsp_button.h"
void fruit_merge_prepare(void);
void fruit_merge_enter(bool buttons_available);
void fruit_merge_exit(void);
void fruit_merge_key(bsp_btn_t button, bsp_btn_ev_t event);
