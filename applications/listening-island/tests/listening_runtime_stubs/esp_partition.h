#pragma once
#include <stddef.h>
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_PARTITION_TYPE_DATA 1
typedef struct {size_t size;} esp_partition_t;
const esp_partition_t *esp_partition_find_first(unsigned type,unsigned subtype,const char *name);
esp_err_t esp_partition_read(const esp_partition_t *p,size_t offset,void *data,size_t size);
