#pragma once
#include "listening_state.h"
#include "lvgl.h"
void li_ui_create(li_state_t *state);
void li_ui_render(li_state_t *state,int battery,bool audio,int saving);
