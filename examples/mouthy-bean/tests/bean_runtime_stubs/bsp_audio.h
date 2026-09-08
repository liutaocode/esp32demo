#pragma once
#include <stddef.h>
#include <stdint.h>
#define ESP_OK 0
int bsp_audio_init(void);
int bsp_audio_set_format(uint32_t,uint8_t,uint8_t);
int bsp_audio_read(void *,size_t);
int bsp_audio_write(const void *,size_t);
void bsp_audio_set_volume(uint8_t);
