#pragma once

#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t offset;
    uint32_t adpcm_size;
    uint32_t sample_count;
    int16_t initial_predictor;
    uint8_t initial_step_index;
} minecraft_guide_audio_clip_t;

extern const uint8_t minecraft_guide_audio_data[];
extern const size_t minecraft_guide_audio_data_size;
extern const minecraft_guide_audio_clip_t minecraft_guide_audio_clips[];
extern const size_t minecraft_guide_audio_clip_count;
