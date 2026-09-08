#pragma once
#include "bsp_button.h"
#include <stdbool.h>
void pocket_breach_prepare(void);
void pocket_breach_enter(bool buttons_available);
void pocket_breach_exit(void);
void pocket_breach_key(bsp_btn_t button,bsp_btn_ev_t event);
void pb_runtime_start(bool audio_available);
void pb_runtime_active(bool active);
void pb_runtime_sound(unsigned sound);
int pb_runtime_battery(void);
bool pb_runtime_audio_ok(void);
