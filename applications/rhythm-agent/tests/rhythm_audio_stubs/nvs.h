#pragma once
#include <stdint.h>
typedef unsigned nvs_handle_t;
#define NVS_READWRITE 1
int nvs_open(const char *name,int mode,nvs_handle_t *handle);
int nvs_get_u32(nvs_handle_t handle,const char *key,uint32_t *value);
int nvs_set_u32(nvs_handle_t handle,const char *key,uint32_t value);
int nvs_commit(nvs_handle_t handle);
