#pragma once
#include "bsp_button.h"
#include "night_call_state.h"
void night_call_prepare(void);
void night_call_enter(const nc_state_t *state, bool buttons_ok);
void night_call_exit(void);
void night_call_key(bsp_btn_t btn, bsp_btn_ev_t ev);
