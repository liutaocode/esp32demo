#pragma once
#include "cat_state.h"
typedef struct { cat_state_t cat; unsigned page, selection; bool wake_key[3]; uint32_t dance_stop_guard; } cat_control_t;
void cat_control_init(cat_control_t *c,uint32_t seed,const cat_profile_t *saved);
/* key: up=0/down=1/confirm=2; event: press=0/click=1/double=2/long=3. */
cat_sound_t cat_control_key(cat_control_t *c,unsigned key,unsigned event);
void cat_control_tick(cat_control_t *c,uint32_t ms);
unsigned cat_control_brightness(const cat_control_t *c);
