#pragma once
#include "down_100_state.h"
#include <stddef.h>

enum { D100_AUDIO_RATE = 16000 };
typedef enum { D100_SOUND_NONE, D100_SOUND_START, D100_SOUND_GEM,
    D100_SOUND_HEAL, D100_SOUND_CRACK, D100_SOUND_HURT,
    D100_SOUND_LOSE, D100_SOUND_CLEAR, D100_SOUND_COUNT } d100_sound_t;
d100_sound_t d100_sound_event(const d100_state_t *before, const d100_state_t *after);
size_t d100_sound_samples(d100_sound_t sound);
size_t d100_sound_render(d100_sound_t sound, size_t offset, int16_t *pcm, size_t capacity);

/* Single app-lifetime worker; no LVGL or game-state references. These calls
   only publish atomic state and overwrite a one-slot queue, never PCM I/O. */
void d100_audio_prepare(void);
void d100_audio_active(bool active);
void d100_audio_play(d100_sound_t sound);
bool d100_audio_idle(void);
