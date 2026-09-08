#pragma once
#include "cat_state.h"
#include "minecraft_adpcm.h"
typedef struct {
    cat_sound_t kind, pending;
    uint32_t sample, count, fade_left, rng;
    minecraft_adpcm_state_t decoder;
    uint8_t clip, last[3], used;
    bool loop;
} cat_voice_t;
void cat_voice_start(cat_voice_t *v,cat_sound_t kind);
void cat_voice_stop(cat_voice_t *v);
size_t cat_voice_render(cat_voice_t *v,int16_t *out,size_t capacity);
