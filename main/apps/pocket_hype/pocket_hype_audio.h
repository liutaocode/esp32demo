#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PH_AUDIO_RATE 16000U
#define PH_AUDIO_COUNT 36U
typedef struct { uint32_t offset, size, samples; int16_t predictor; uint8_t step; } ph_clip_t;
extern const ph_clip_t ph_clips[PH_AUDIO_COUNT];
extern const size_t ph_audio_bytes;
bool ph_clip_valid(const ph_clip_t *c, size_t size);
void ph_audio_start(bool available);
void ph_audio_play(unsigned clip, unsigned volume, bool voice);
void ph_audio_stop(void);
bool ph_audio_ready(void);
bool ph_audio_busy(void);
