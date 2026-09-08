#pragma once
#include <stdbool.h>
#include "bsp_button.h"

void pocket_arcade_prepare(void);
/* 进入和退出需要持有 LVGL 锁;按键回调只投递事件,不碰 LVGL 对象。 */
void pocket_arcade_enter(bool buttons_available);
void pocket_arcade_exit(void);
void pocket_arcade_key(bsp_btn_t button, bsp_btn_ev_t event);
