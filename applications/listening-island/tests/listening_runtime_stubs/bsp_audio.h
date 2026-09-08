#pragma once
#include "esp_partition.h"
int bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t channels);
void bsp_audio_set_volume(uint8_t volume);
int bsp_audio_write(const void *data,size_t bytes);
