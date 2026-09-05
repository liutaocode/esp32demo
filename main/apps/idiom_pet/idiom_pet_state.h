#pragma once

#include <stdbool.h>
#include <stdint.h>

#define IP_LANDS 3
#define IP_PER_LAND 8
#define IP_QUESTIONS (IP_LANDS * IP_PER_LAND)
#define IP_ROUND 4
#define IP_PETS 6
#define IP_PROGRESS_MASK ((1U << IP_QUESTIONS) - 1U)

typedef struct {
    const char *idiom;
    const char *story;
    const char *meaning;
    const char *review;
    const char *distractors[2];
} ip_question_t;

extern const ip_question_t ip_questions[IP_QUESTIONS];
extern const char *const ip_land_names[IP_LANDS];
extern const char *const ip_pet_names[IP_PETS];

typedef enum { IP_HOME, IP_QUIZ, IP_FEEDBACK, IP_HATCH, IP_ALBUM } ip_page_t;
typedef struct {
    uint32_t learned; /* Successful first answers or corrected review answers. */
    uint32_t pending; /* Wrong answers awaiting review, including after reboot. */
    uint8_t pets;     /* Collection bitmask; no identifiers or personal data. */
} ip_progress_t;

typedef struct {
    ip_page_t page;
    ip_progress_t progress;
    uint32_t rng;
    uint8_t land, selected, cursor, correct_slot, question;
    uint8_t deck[IP_ROUND], review_queue[IP_ROUND];
    uint8_t index, review_count, review_index, first_correct, pet;
    bool reviewing, last_correct, new_pet;
} ip_state_t;

void ip_init(ip_state_t *s, uint32_t seed, ip_progress_t saved);
void ip_home(ip_state_t *s);
void ip_move(ip_state_t *s, int direction);
void ip_confirm(ip_state_t *s);
void ip_album(ip_state_t *s);
const char *ip_option(const ip_state_t *s, unsigned slot);
unsigned ip_count(uint32_t bits);
