#pragma once
#include "bsp_button.h"
#include <stdbool.h>
void tally_start(bool buttons_ok);
void tally_key(bsp_btn_t button,bsp_btn_ev_t event);
