#pragma once

#include <stdbool.h>
#include "bsp_button.h"

/* Prepare once before registering BSP buttons. Queue storage lives forever. */
void stack_rush_prepare_input(void);
/* Enter/exit run under the LVGL lock. Exit stops timers before deleting UI. */
void stack_rush_enter(bool buttons_available);
void stack_rush_exit(void);
/* Nonblocking button-task entry point; never touches LVGL. */
void stack_rush_key(bsp_btn_t button, bsp_btn_ev_t event);
