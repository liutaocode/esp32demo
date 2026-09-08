#pragma once
#include <stddef.h>
typedef int esp_err_t;
typedef unsigned nvs_handle_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NVS_NOT_FOUND 2
#define NVS_READWRITE 1
esp_err_t nvs_open(const char *name,int mode,nvs_handle_t *h);
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *data,size_t *size);
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size);
esp_err_t nvs_commit(nvs_handle_t h);
