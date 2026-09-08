#pragma once
#include "tally_feedback.h"
#include <stdint.h>
#include <stdbool.h>
#define TC_SOUND_RATE 16000u
#define TC_AUDIO_FRAMES 240u
#define TC_CROSSFADE_FRAMES 80u
unsigned tc_sound_length(tc_feedback sound);
int16_t tc_sound_sample(tc_feedback sound,unsigned index);
typedef struct {
    tc_feedback current,previous;
    unsigned position,previous_position,fade;
} tc_mixer;
void tc_mixer_request(tc_mixer *m,tc_feedback sound);
void tc_mixer_render(tc_mixer *m,int16_t *out,unsigned count);
bool tc_mixer_busy(const tc_mixer *m);
void tc_audio_start(void);
void tc_audio_request(tc_feedback sound);
bool tc_audio_quiet(void);
