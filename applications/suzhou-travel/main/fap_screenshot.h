#pragma once

#include "esp_err.h"

struct _lv_display_t;

/* Start the observational FAP_SCREENSHOT_V1 service on the USB console. */
esp_err_t fap_screenshot_init(struct _lv_display_t *display);
