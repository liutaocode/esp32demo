#pragma once
#include "online_state.h"
#include "esp_err.h"
bool online_load_config(online_config_t *config);
/* Boot-only provisioning mode: returns after starting AP/server. Save reboots. */
esp_err_t online_start_setup(char *description,size_t size);
