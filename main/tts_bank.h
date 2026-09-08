#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef struct {
    const uint8_t *data;
    size_t size, metadata_size;
    uint32_t count, frame_bytes, max_samples;
} tts_bank_t;
typedef struct {
    const uint8_t *packets;
    uint32_t samples;
    uint16_t skip, frames;
} tts_syllable_t;
bool tts_bank_open(tts_bank_t *bank, const uint8_t *data, size_t size);
bool tts_bank_syllable(const tts_bank_t *bank, uint32_t index, tts_syllable_t *syllable);
uint32_t tts_bank_u32(const uint8_t *data);
