#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t offset;
    uint32_t adpcm_size;
    uint32_t sample_count;
    int16_t initial_predictor;
    uint8_t initial_step_index;
} haihunhou_tts_clip_t;

extern const uint8_t haihunhou_tts_audio_data[];
extern const size_t haihunhou_tts_audio_data_size;
extern const haihunhou_tts_clip_t haihunhou_tts_clips[];
extern const size_t haihunhou_tts_clip_count;
