#pragma once
#include "idiom_pet_state.h"
#include <stddef.h>

#define IP_AUDIO_RATE 16000
#define IP_AUDIO_NONE (-2)
#define IP_AUDIO_STOP (-1)
typedef struct {
    uint32_t offset, size, samples;
    int16_t predictor;
    uint8_t step;
} ip_audio_clip_t;
extern const ip_audio_clip_t ip_audio_clips[IP_QUESTIONS];
extern const size_t ip_audio_size;
bool ip_audio_clip_valid(const ip_audio_clip_t *clip, size_t size);
/* Pure UI transition policy: question id, STOP or NONE. */
int ip_audio_transition(ip_page_t before, const ip_state_t *after);
