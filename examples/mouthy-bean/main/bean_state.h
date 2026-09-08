#pragma once
#include <stdbool.h>
#include <stdint.h>
#define BEAN_LINES 40
#define BEAN_FACES 8
/* Pure monotonic state machine. No speech recognition or recorded audio. */
typedef enum { BEAN_IDLE, BEAN_LISTEN, BEAN_THINK, BEAN_TALK, BEAN_REST } bean_phase_t;
typedef struct {
    bean_phase_t phase;
    uint32_t rng, discovered;
    uint64_t since, last_sound, last_action, next_idle;
    int line, face, last_key, streak;
    unsigned volume, think_ms;
    bool auto_listen, noise;
} bean_t;
extern const char *const bean_lines[BEAN_LINES];
extern const char *const bean_faces[BEAN_FACES];
void bean_init(bean_t *s, uint32_t seed, uint64_t now);
void bean_key(bean_t *s, int key, uint64_t now);
void bean_tick(bean_t *s, uint64_t now, bool sound, bool busy);
void bean_cancel(bean_t *s, uint64_t now);
unsigned bean_count(uint32_t mask);
/* Consecutive 20 ms blocks, adaptive background floor, DC-independent input. */
typedef struct { unsigned floor, voiced, quiet; bool active; } bean_vad_t;
bool bean_vad_tick(bean_vad_t *v, unsigned amplitude);

unsigned bean_ear_raise(uint64_t elapsed);
