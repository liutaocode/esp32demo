#pragma once
#include <stdbool.h>
#include <stdint.h>
#define LI_TOPICS 8
#define LI_PER_TOPIC 48
#define LI_COUNT (LI_TOPICS * LI_PER_TOPIC)
#define LI_ROUND 8
#define LI_SAVE_VERSION 3
/* All times are monotonic milliseconds, with unsigned wrap-safe subtraction. */
typedef enum { LI_HOME, LI_TOPICS_PAGE, LI_LISTEN, LI_QUIZ, LI_FEEDBACK, LI_FINISH, LI_SETTINGS, LI_ALBUM } li_page_t;
/* Version 3 uses one byte of the old tail padding for screen_minutes.
 * Keep the blob size and all earlier field offsets for version 1/2 saves.
 * minutes=0 disables timed rest; screen_minutes is always 1, 3, 5, or 10. */
typedef struct { uint32_t version; uint8_t mistakes[LI_COUNT], heard[LI_COUNT], stamps[LI_TOPICS]; uint8_t volume, minutes, screen_minutes; } li_progress_t;
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

/* Backlight policy, owned by the LVGL task. Audio never resets this timer. */
#define LI_SCREEN_IDLE_MS (1u * 60u * 1000u)
typedef struct {
 uint32_t last_key, idle_ms;
 bool off;
 bool wake_only[3]; /* Ignore all follow-up events from a wake gesture. */
} li_screen_t;
void li_screen_init(li_screen_t *screen, uint32_t now);
void li_screen_set_minutes(li_screen_t *screen, unsigned minutes);
void li_screen_tick(li_screen_t *screen, uint32_t now);
/* press=true for physical press; false for click, double-click, or long press.
 * Returns true only when the event may perform its normal application action. */
bool li_screen_key(li_screen_t *screen, unsigned key, bool press, uint32_t now);
