#pragma once
#include <stdbool.h>
#include "bsp_button.h"

void clean_sweep_prepare(void);
/* 进入和退出需要持有 LVGL 锁;按键回调只投递事件,不碰 LVGL 对象。 */
void clean_sweep_enter(bool buttons_available);
void clean_sweep_exit(void);
void clean_sweep_key(bsp_btn_t button, bsp_btn_ev_t event);
