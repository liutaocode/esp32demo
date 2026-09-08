#pragma once
#include <stdbool.h>
#include "bsp_button.h"
void deadline_station_prepare(void);
void deadline_station_enter(bool buttons_available);
void deadline_station_exit(void);
void deadline_station_key(bsp_btn_t button, bsp_btn_ev_t event);
