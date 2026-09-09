#pragma once
#include <stdbool.h>
#include "bsp_button.h"
void online_ui_enter(bool buttons_ok);
void online_ui_key(bsp_btn_t key,bsp_btn_ev_t ev);
