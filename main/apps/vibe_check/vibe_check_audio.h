#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VC_AUDIO_RATE 16000
#define VC_AUDIO_COUNT 14

enum {
    VC_VOICE_WELCOME = 0,
    VC_VOICE_QUESTION_0,
    VC_VOICE_QUESTION_1,
    VC_VOICE_QUESTION_2,
    VC_VOICE_QUESTION_3,
    VC_VOICE_QUESTION_4,
    VC_VOICE_RESULT_0,
};

typedef struct {
    uint32_t offset;
    uint32_t size;
    uint32_t samples;
    int16_t predictor;
    uint8_t step;
} vibe_check_audio_clip_t;

extern const vibe_check_audio_clip_t vibe_check_audio_clips[VC_AUDIO_COUNT];
extern const size_t vibe_check_audio_size;

bool vibe_check_audio_clip_valid(const vibe_check_audio_clip_t *clip,
                                 size_t data_size);
