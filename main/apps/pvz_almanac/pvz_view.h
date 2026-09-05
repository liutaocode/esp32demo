#pragma once
#include "pvz_state.h"
#include "lvgl.h"
lv_obj_t *pvz_view_create(void);
void pvz_view_render(const pvz_state_t *state, bool buttons_ok);
void pvz_view_status(int voice_status);
void pvz_view_battery(int soc);
void pvz_view_destroy(void);
