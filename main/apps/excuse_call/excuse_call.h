#pragma once
#include <stdbool.h>
#include "bsp_button.h"
void excuse_call_prepare(void);
void excuse_call_enter(bool buttons_available);
void excuse_call_exit(void);
void excuse_call_key(bsp_btn_t button,bsp_btn_ev_t event);
