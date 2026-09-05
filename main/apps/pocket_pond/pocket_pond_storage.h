#pragma once
#include "pocket_pond_state.h"
/* App-lifetime worker owns NVS only, never UI objects. Initialize before LVGL lock. */
pp_progress_t pp_storage_init(void);
void pp_storage_save(pp_progress_t progress);
/* 0 saved, 1 pending, 2 unavailable/failed. */
unsigned pp_storage_status(void);
