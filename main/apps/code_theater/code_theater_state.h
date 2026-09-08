#pragma once
#include <stdbool.h>
#include <stdint.h>
#define CT_ROWS 6
#define CT_TASKS 16
#define CT_CARDS 12
#define CT_LINE_BYTES 96
#define CT_BURST_GAP_MS 650
#define CT_BURST_LIMIT 7
#define CT_BAN_MS 10000
typedef enum {
    CT_THINK, CT_SEARCH, CT_READ, CT_PLAN, CT_EDIT, CT_DIFF, CT_TEST,
    CT_ERROR, CT_WAIT, CT_REPAIR, CT_VERIFY, CT_COMMIT, CT_DONE, CT_REST,
    CT_COMPACT, CT_INTERRUPTED, CT_RECOVER, CT_PHASES
} ct_phase_t;
typedef enum { CT_ACCESS_OK, CT_BANNED, CT_APPEAL_FAILED } ct_access_t;
typedef enum { CT_PET_THINK, CT_PET_READ, CT_PET_TYPE, CT_PET_WORRY,
               CT_PET_WAIT, CT_PET_HAPPY, CT_PET_WAVE, CT_PET_SLEEP, CT_PET_SAD } ct_pose_t;
typedef struct {
    uint32_t rng, elapsed, completed, boosts, work_ms, ban_left;
    uint32_t last_press, remark_left;
    uint16_t cards;
    uint8_t project, task, bug, phase, count, choice, last_card, remark, burst, access;
    bool paused, has_press;
    char lines[CT_ROWS][CT_LINE_BYTES];
    uint8_t colors[CT_ROWS];
} ct_state_t;
void ct_init(ct_state_t *s, uint32_t seed);
void ct_next(ct_state_t *s);
void ct_tick(ct_state_t *s, uint32_t ms);
void ct_boost(ct_state_t *s);
void ct_interrupt(ct_state_t *s);
void ct_choose(ct_state_t *s, bool bold);
/* Count physical PRESS events only, once each; event timestamps may wrap. */
bool ct_press(ct_state_t *s, uint32_t at_ms);
/* One appeal per suspension. An even random value succeeds; odd fails. */
bool ct_appeal(ct_state_t *s, uint32_t random_value);
const char *ct_project(unsigned index);
const char *ct_task(unsigned index);
const char *ct_card(unsigned index);
const char *ct_verb(const ct_state_t *s);
const char *ct_speech(const ct_state_t *s);
ct_pose_t ct_pose(const ct_state_t *s);
unsigned ct_card_count(const ct_state_t *s);
