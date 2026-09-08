#pragma once
#include <stdint.h>
typedef struct { const uint8_t *data; uint32_t samples; uint8_t group; } cat_clip_t;
extern const cat_clip_t cat_clips[10];
