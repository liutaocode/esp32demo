#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define NC_AUDIO_RATE 16000
#define NC_AUDIO_COUNT 29
typedef struct { uint32_t offset, size, samples; int16_t predictor; uint8_t step; } nc_clip_t;
extern const nc_clip_t nc_clips[NC_AUDIO_COUNT];
extern const size_t nc_audio_bytes;
bool nc_clip_valid(const nc_clip_t *c, size_t size);
void nc_audio_start(bool available);
void nc_audio_play(unsigned clip, unsigned volume);
void nc_audio_stop(void);
bool nc_audio_ready(void);
bool nc_audio_busy(void);
