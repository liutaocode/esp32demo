#pragma once
#include <stdbool.h>
#include <stdint.h>
#define PH_SCENES 6
#define PH_ROUNDS 6
#define PH_HEAT_MS 4500U
#define PH_SHOW_MS 3500U
typedef enum { PH_LIVE, PH_MENU, PH_HELP, PH_CHALLENGE, PH_FEEDBACK, PH_RESULT } ph_page_t;
typedef struct {
    ph_page_t page;
    unsigned selected, played, tier, chain, menu, volume;
    bool voice, active, correct;
    uint32_t rng, since_hit, since_show, total;
    uint8_t visited, order[PH_ROUNDS], cues[PH_ROUNDS];
    unsigned round, score;
} ph_state_t;
extern const char *const ph_names[PH_SCENES + 1];
extern const char *const ph_lines[PH_SCENES][3];
extern const char *const ph_cues[PH_SCENES][3];
void ph_init(ph_state_t *s, uint32_t seed);
void ph_move(ph_state_t *s, int delta);
/* Returns a clip index 0..17, or -1 when no sound is requested. */
int ph_confirm(ph_state_t *s, unsigned taps);
void ph_back(ph_state_t *s);
bool ph_tick(ph_state_t *s, uint32_t ms);
void ph_challenge(ph_state_t *s);
unsigned ph_collected(const ph_state_t *s);
