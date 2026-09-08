#pragma once
#include <stddef.h>
#include <stdint.h>
#define EC_AUDIO_RATE 16000U
#define EC_LOOP_SAMPLES 72000U
void ec_pcm(unsigned ring, uint32_t offset, int16_t *pcm, size_t count);
void ec_audio_prepare(void);
/* Single UI owner sends desired playback; worker alone touches codec, never UI. */
void ec_audio_play(unsigned ring, int64_t deadline_ms);
void ec_audio_stop(void);
/* 0 preparing, 1 ready, -1 unavailable. */
int ec_audio_status(void);
