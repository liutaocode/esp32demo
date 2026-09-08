#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Byte offsets refer to the original UTF-8 file. Pause retries the current sentence. */
typedef struct { bool active, paused, waiting; uint32_t cursor, next; unsigned speed; } eb_speech_t;
void eb_speech_begin(eb_speech_t *s, uint32_t offset);
void eb_speech_pause(eb_speech_t *s);
void eb_speech_stop(eb_speech_t *s);
void eb_speech_completed(eb_speech_t *s);
/* Returns consumed source bytes. Normalizes whitespace and splits on punctuation,
   never in a UTF-8 character; output has at most capacity-1 bytes. */
size_t eb_speech_chunk(const char *source, size_t bytes, char *out, size_t capacity);

unsigned eb_speech_volume_step(unsigned volume, int direction);
