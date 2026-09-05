#pragma once
#include <stdbool.h>
#include "bsp_button.h"
void focus_post_prepare(void);
void focus_post_enter(bool buttons_available);
void focus_post_exit(void);
void focus_post_key(bsp_btn_t button, bsp_btn_ev_t event);
