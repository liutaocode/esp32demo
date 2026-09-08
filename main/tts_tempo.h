#pragma once
#include <stddef.h>
#include <stdint.h>
/* Integer waveform-similarity overlap/add, 16 kHz mono. speed 2 is unchanged.
 * Capacity must be at least tts_tempo_capacity(input_count). Returns 0 on error. */
size_t tts_tempo_capacity(size_t samples);
size_t tts_tempo_process(const int16_t *input, size_t samples, int16_t *output,
                         size_t capacity, unsigned speed);
