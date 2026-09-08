#pragma once
#include "night_call_state.h"
/* Start/load before LVGL initialization. Workers never access UI objects. */
void nc_storage_start(nc_state_t *state);
void nc_storage_save(const nc_state_t *state);
/* 0 saved, 1 writing, -1 unavailable/corrupt; failed writes remain visible. */
int nc_storage_status(void);
