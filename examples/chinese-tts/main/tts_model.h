#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TTS_TEXT_BYTES 241
#define TTS_SPEED_COUNT 6

typedef struct { const char *category; const char *text; } tts_sample_t;
extern const tts_sample_t tts_samples[];
extern const size_t tts_sample_count;
typedef struct { size_t selected; unsigned speed; } tts_model_t;
void tts_model_move(tts_model_t *model, int direction);
void tts_model_speed(tts_model_t *model);
/* Bounded, strict UTF-8 validation before passing input to the binary parser. */
bool tts_text_valid(const char *text);
