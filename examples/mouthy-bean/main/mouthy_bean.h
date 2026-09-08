#pragma once
#include "bsp_button.h"
void mouthy_bean_prepare(void);
void mouthy_bean_enter(bool buttons);
void mouthy_bean_exit(void);
void mouthy_bean_key(bsp_btn_t key, bsp_btn_ev_t event);
