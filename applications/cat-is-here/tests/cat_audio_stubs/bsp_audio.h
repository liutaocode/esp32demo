#pragma once
#include <stdint.h>
#include <stddef.h>
#define ESP_OK 0
int bsp_audio_init(void);
int bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t channels);
void bsp_audio_set_volume(uint8_t value);
int bsp_audio_write(const void *pcm,size_t bytes);
