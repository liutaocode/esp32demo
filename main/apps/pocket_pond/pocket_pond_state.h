#pragma once
#include <stdbool.h>
#include <stdint.h>
#define PP_FISH 9
#define PP_CARDS 12
#define PP_SAVE_SIZE 24

typedef struct { uint16_t caught[PP_FISH], best, trips; } pp_progress_t;
typedef enum { PP_HOME, PP_PLAY, PP_RESULT } pp_page_t;
typedef struct {
    pp_progress_t progress;
    pp_page_t page;
    uint8_t deck[PP_CARDS], cursor, waves, score, last;
    uint16_t basket;
    bool lost, record;
} pp_state_t;
extern const char *const pp_names[PP_FISH];
extern const char *const pp_notes[PP_FISH];
extern const uint8_t pp_values[PP_FISH];
void pp_start(pp_state_t *s, uint32_t seed);
bool pp_draw(pp_state_t *s);
bool pp_bank(pp_state_t *s);
unsigned pp_species(const pp_progress_t *p);
unsigned pp_remaining_fish(const pp_state_t *s);
void pp_encode(const pp_progress_t *p, uint8_t bytes[PP_SAVE_SIZE]);
bool pp_decode(pp_progress_t *p, const uint8_t bytes[PP_SAVE_SIZE]);
