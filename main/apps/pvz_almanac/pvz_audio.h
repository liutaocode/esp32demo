#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "pvz_catalog.h"
#define PVZ_AUDIO_COUNT (PVZ_COUNT * 2)
typedef struct { uint32_t offset, size, samples; int16_t predictor; uint8_t step; } pvz_clip_t;
extern const pvz_clip_t pvz_clips[PVZ_AUDIO_COUNT];
extern const size_t pvz_audio_size;
bool pvz_clip_valid(const pvz_clip_t *clip, size_t size);
/* Runtime functions: only the audio worker calls the blocking BSP functions. */
bool pvz_audio_start(bool available);
void pvz_audio_request(int clip); /* -1 cancels; latest request wins. */
int pvz_audio_status(void); /* 0 idle, 1 playing, 2 finished, -1 unavailable/error */
void pvz_audio_stop(void); /* joins worker; never call in a button callback */
