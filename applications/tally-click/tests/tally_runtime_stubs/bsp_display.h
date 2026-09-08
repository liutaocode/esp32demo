#pragma once
#include <stdbool.h>
bool bsp_lvgl_lock(unsigned timeout);
void bsp_lvgl_unlock(void);
