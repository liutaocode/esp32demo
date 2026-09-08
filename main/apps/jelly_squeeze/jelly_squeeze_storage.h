#pragma once
#include "jelly_squeeze_state.h"

/* App-lifetime worker owns NVS only, never UI objects. Initialize before the LVGL lock. */
js_progress_t js_storage_init(void);
void js_storage_save(js_progress_t progress);
