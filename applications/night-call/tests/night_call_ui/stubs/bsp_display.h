#pragma once
#include <stdint.h>
void bsp_display_backlight(uint8_t level);
#include <stdbool.h>
bool bsp_lvgl_lock(unsigned timeout);
void bsp_lvgl_unlock(void);
