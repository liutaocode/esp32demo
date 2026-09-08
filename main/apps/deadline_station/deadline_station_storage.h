#pragma once
#include "deadline_station_state.h"
/* App-lifetime worker owns NVS only, never UI objects. Initialize before LVGL lock. */
ds_progress_t ds_storage_init(void);
void ds_storage_save(ds_progress_t progress);
/* 0 saved, 1 pending, 2 unavailable/failed. */
unsigned ds_storage_status(void);
