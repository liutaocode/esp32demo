#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "bean_state.h"
typedef struct { uint32_t offset, size, samples; int16_t predictor; uint8_t step; } bean_clip_t;
extern const bean_clip_t bean_clips[BEAN_LINES];
extern const size_t bean_audio_bytes;
void bean_runtime_start(void);
void bean_runtime_play(int line, unsigned volume);
void bean_runtime_stop(void);
void bean_runtime_listen(bool enabled);
bool bean_runtime_busy(void);
bool bean_runtime_sound(void);
bool bean_runtime_ready(void);
unsigned bean_runtime_level(void);
uint32_t bean_runtime_saved(void);
void bean_runtime_save(uint32_t packed);
bool bean_runtime_storage_ok(void);
