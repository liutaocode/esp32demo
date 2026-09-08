#pragma once
#include "rhythm_state.h"
#include <stddef.h>
#define RA_RATE 16000
typedef struct { uint32_t offset, bytes, samples; int16_t predictor; uint8_t step; } ra_clip_t;
extern const ra_clip_t ra_clips[4];
extern const size_t ra_voice_bytes;
int16_t ra_tone(unsigned key, unsigned sample);
void ra_audio_start(bool available);
void ra_audio_demo(const ra_pattern_t *p, unsigned volume, bool voice);
void ra_audio_note(unsigned key, unsigned volume);
void ra_audio_feedback(bool passed, unsigned volume, bool voice);
void ra_audio_stop(void);
bool ra_audio_busy(void);
bool ra_audio_ready(void);
int ra_audio_note_index(void);
