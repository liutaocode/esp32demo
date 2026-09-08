#pragma once
#include <stdbool.h>
#include "bsp_button.h"
void pocket_hype_prepare(void);
void pocket_hype_enter(bool buttons_available);
void pocket_hype_exit(void);
void pocket_hype_key(bsp_btn_t button, bsp_btn_ev_t event);
