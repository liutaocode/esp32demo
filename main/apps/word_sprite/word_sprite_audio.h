#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WS_SAMPLE_RATE 16000
#define WS_VOICE_MEANING_BASE 41
#define WS_AUDIO_COUNT 77
enum { WS_VOICE_WELCOME = 36, WS_VOICE_CORRECT, WS_VOICE_RETRY,
       WS_VOICE_FINISH, WS_VOICE_REVIEW };
typedef struct {
    uint32_t offset, size, samples;
    int16_t predictor;
    uint8_t step;
} ws_clip_t;
extern const ws_clip_t ws_clips[WS_AUDIO_COUNT];
extern const size_t ws_audio_size;
bool ws_clip_valid(const ws_clip_t *clip, size_t size);
bool ws_explanation_clips(unsigned word, int cue, int clips[3]);
