#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum { CAT_SIT, CAT_LOOK, CAT_GROOM, CAT_STRETCH, CAT_SLEEP,
    CAT_APPROACH, CAT_PAT, CAT_ROLL, CAT_PLAY, CAT_SNUGGLE, CAT_GIFT, CAT_DANCING, CAT_POSES } cat_pose_t;
typedef enum { CAT_CALL, CAT_STROKE, CAT_TOY } cat_action_t;
typedef enum { CAT_SILENT, CAT_MEW, CAT_PURR, CAT_CHIRP, CAT_DANCE } cat_sound_t;
enum { CAT_SAVE_BYTES = 32, CAT_MEMORIES = 6 };
typedef struct {
    uint8_t name, toy, coat, sound, pocket, memories, last_toy;
    uint32_t seed, minutes;
} cat_profile_t;
typedef struct {
    cat_profile_t profile;
    cat_pose_t pose;
    uint32_t rng, pose_ms, pose_duration, idle_ms, total_remainder, pet_gap;
    uint32_t gift_wait, interaction_cooldown;
    uint8_t pet_chain, variant;
    bool dirty;
} cat_state_t;
void cat_init(cat_state_t *cat, uint32_t seed, const cat_profile_t *saved);
void cat_tick(cat_state_t *cat, uint32_t elapsed_ms);
cat_sound_t cat_act(cat_state_t *cat, cat_action_t action);
cat_sound_t cat_dance(cat_state_t *cat, bool start);
void cat_option(cat_state_t *cat, unsigned row);
void cat_encode(const cat_profile_t *profile, uint8_t out[CAT_SAVE_BYTES]);
bool cat_decode(cat_profile_t *profile, const uint8_t *data, size_t len);
const char *cat_name(const cat_state_t *cat);
const char *cat_message(const cat_state_t *cat);
const char *cat_memory(unsigned index);
const char *cat_toy_name(unsigned index);
