#pragma once
#include <stdint.h>
typedef unsigned nvs_handle_t;
#define NVS_READWRITE 1
int nvs_open(const char *, int, nvs_handle_t *);
int nvs_get_u32(nvs_handle_t, const char *, uint32_t *);
int nvs_set_u32(nvs_handle_t, const char *, uint32_t);
int nvs_commit(nvs_handle_t);
