#pragma once
#include <stdbool.h>
#include <stdint.h>
#define PB_W 120
#define PB_H 100
#define PB_MAP 12
#define PB_ENEMIES 4
#define PB_PI 3.14159265359f
typedef enum { PB_HOME, PB_FIGHT, PB_TRAVEL, PB_PAUSE, PB_RESULT } pb_page;
typedef enum { PB_SILENT, PB_SHOT, PB_HIT, PB_BREAK, PB_RELOAD, PB_HURT, PB_CLEAR } pb_sound;
typedef struct { float x,y; unsigned hp, attack, flash, death_ms, hide_ms, rise_ms; bool cover; } pb_enemy;
typedef struct {
    pb_page page, resume;
    unsigned map, mode, wave, health, ammo, score, best[2], combo, max_combo, kills, shots, hits;
    unsigned cooldown, reload, recoil, damage, settle, stable, elapsed, travel_ms;
    unsigned hurt_ms, lock_grace;
    int locked, last_attacker, skip_lock;
    uint32_t seed, rng;
    float x,y,base,aim;
    float hurt_bearing;
    bool won;
    pb_enemy enemy[PB_ENEMIES];
    pb_sound sound;
} pb_game;
void pb_home(pb_game *g);
void pb_start(pb_game *g, uint32_t seed);
void pb_turn(pb_game *g, float amount);
void pb_fire(pb_game *g);
void pb_tick(pb_game *g, unsigned ms);
void pb_pause(pb_game *g);
int pb_wall(unsigned map, int x, int y);
float pb_ray(unsigned map, float x,float y,float dx,float dy,int *side,float *tex);
int pb_target(const pb_game *g);
float pb_bearing(const pb_game *g, int enemy);
int pb_threat(const pb_game *g);
int pb_guidance(const pb_game *g);
bool pb_in_view(const pb_game *g, int enemy);
bool pb_exposed(const pb_enemy *enemy);
unsigned pb_alive(const pb_game *g);
void pb_crate(const pb_game *g, int enemy, float *x, float *y, float *hx, float *hy);
int pb_direction(float bearing);
void pb_render(const pb_game *g, uint16_t *pixels);
void pb_pcm(pb_sound sound, unsigned offset, int16_t *out, unsigned count);
unsigned pb_sound_samples(pb_sound sound);
