#pragma once
#include "listening_state.h"
#include <stddef.h>
typedef struct { uint32_t offset,bytes,samples; int16_t predictor; uint8_t step,bank; } li_clip_t;
extern const char *const li_topics[LI_TOPICS];
extern const char *const li_meanings[LI_COUNT];
extern const char *const li_english[LI_COUNT];
extern const li_clip_t li_clips[LI_COUNT];
extern const size_t li_bank_sizes[2];
extern const uint32_t li_resource_crc;
bool li_clip_valid(const li_clip_t *c, size_t bank_size);
