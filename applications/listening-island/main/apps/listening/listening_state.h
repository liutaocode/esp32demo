#pragma once
#include <stdbool.h>
#include <stdint.h>
#define LI_TOPICS 8
#define LI_PER_TOPIC 48
#define LI_COUNT (LI_TOPICS * LI_PER_TOPIC)
#define LI_ROUND 8
#define LI_SAVE_VERSION 2
/* All times are monotonic milliseconds, with unsigned wrap-safe subtraction. */
typedef enum { LI_HOME, LI_TOPICS_PAGE, LI_LISTEN, LI_QUIZ, LI_FEEDBACK, LI_FINISH, LI_SETTINGS, LI_ALBUM } li_page_t;
typedef struct { uint32_t version; uint8_t mistakes[LI_COUNT], heard[LI_COUNT], stamps[LI_TOPICS]; uint8_t volume, minutes; } li_progress_t;
typedef struct {
 li_page_t page; li_progress_t progress; uint32_t rng, serial, started, wait_since, now;
 uint16_t order[LI_COUNT], count, pos, options[2];
 uint8_t menu, topic, mode, selection, correct, setting;
 bool paused, waiting, right, dirty, voice_enabled, timed_out;
 int voice; /* -1 cancels; serial changes identify each request. */
} li_state_t;
void li_init(li_state_t *s, const li_progress_t *p, uint32_t seed);
void li_key(li_state_t *s, unsigned key, bool held, uint32_t now);
void li_tick(li_state_t *s, uint32_t now);
void li_audio_done(li_state_t *s, uint32_t serial, uint32_t now);
unsigned li_mistakes(const li_state_t *s);
unsigned li_heard(const li_state_t *s);
uint16_t li_current(const li_state_t *s);
