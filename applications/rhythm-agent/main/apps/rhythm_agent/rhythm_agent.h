#pragma once
#include "bsp_button.h"
#include <stdbool.h>
void rhythm_agent_prepare(void);
void rhythm_agent_enter(bool buttons_ok);
void rhythm_agent_exit(void);
void rhythm_agent_key(bsp_btn_t key,bsp_btn_ev_t event);
