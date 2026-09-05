#include "idiom_pet_state.h"

#include <string.h>

unsigned ip_count(uint32_t bits)
{
    unsigned n = 0;
    for (; bits; bits &= bits - 1U) n++;
    return n;
}

static uint32_t random_next(ip_state_t *s)
{
    uint32_t x = s->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return s->rng = x;
}

void ip_init(ip_state_t *s, uint32_t seed, ip_progress_t saved)
{
    memset(s, 0, sizeof(*s));
    s->rng = seed ? seed : 0x71A2U;
    saved.learned &= IP_PROGRESS_MASK;
    saved.pending &= IP_PROGRESS_MASK;
    saved.learned &= ~saved.pending;
    saved.pets &= (1U << IP_PETS) - 1U;
    s->progress = saved;
}

void ip_home(ip_state_t *s) { s->page = IP_HOME; }

void ip_album(ip_state_t *s)
{
    if (s->page == IP_HOME || s->page == IP_HATCH) {
        s->cursor = s->page == IP_HATCH ? s->pet : 0;
        s->page = IP_ALBUM;
    }
}

static void ask(ip_state_t *s)
{
    s->question = s->reviewing ? s->review_queue[s->review_index]
                               : s->deck[s->index];
    s->correct_slot = random_next(s) % 3;
    s->selected = 0;
    s->page = IP_QUIZ;
}

static void start(ip_state_t *s)
{
    uint8_t pool[IP_PER_LAND];
    for (unsigned i = 0; i < IP_PER_LAND; i++)
        pool[i] = s->land * IP_PER_LAND + i;
    for (unsigned i = IP_PER_LAND - 1; i > 0; i--) {
        unsigned j = random_next(s) % (i + 1);
        uint8_t tmp = pool[i]; pool[i] = pool[j]; pool[j] = tmp;
    }
    /* Previous mistakes first, then unseen content, then learned content. */
    unsigned count = 0;
    for (unsigned priority = 0; priority < 3; priority++) {
        for (unsigned i = 0; i < IP_PER_LAND && count < IP_ROUND; i++) {
            uint32_t bit = 1U << pool[i];
            unsigned rank = s->progress.pending & bit ? 0 :
                            s->progress.learned & bit ? 2 : 1;
            if (rank == priority) s->deck[count++] = pool[i];
        }
    }
    s->index = s->review_count = s->review_index = s->first_correct = 0;
    s->reviewing = false;
    ask(s);
}

void ip_move(ip_state_t *s, int direction)
{
    int delta = direction < 0 ? -1 : 1;
    if (s->page == IP_HOME) s->land = (s->land + IP_LANDS + delta) % IP_LANDS;
    else if (s->page == IP_QUIZ) s->selected = (s->selected + 3 + delta) % 3;
    else if (s->page == IP_ALBUM) s->cursor = (s->cursor + IP_PETS + delta) % IP_PETS;
}

const char *ip_option(const ip_state_t *s, unsigned slot)
{
    if (slot > 2) return "";
    const ip_question_t *q = &ip_questions[s->question];
    if (slot == s->correct_slot) return q->idiom;
    return q->distractors[slot < s->correct_slot ? slot : slot - 1];
}

static void hatch(ip_state_t *s)
{
    s->pet = s->land * 2;
    if (s->progress.pets & (1U << s->pet)) s->pet++;
    s->new_pet = !(s->progress.pets & (1U << s->pet));
    s->progress.pets |= 1U << s->pet;
    s->page = IP_HATCH;
}

void ip_confirm(ip_state_t *s)
{
    switch (s->page) {
    case IP_HOME: start(s); break;
    case IP_QUIZ: {
        uint32_t bit = 1U << s->question;
        s->last_correct = s->selected == s->correct_slot;
        if (s->last_correct) {
            s->progress.learned |= bit;
            s->progress.pending &= ~bit;
            if (!s->reviewing) s->first_correct++;
        } else {
            s->progress.pending |= bit;
            s->progress.learned &= ~bit;
            if (!s->reviewing) s->review_queue[s->review_count++] = s->question;
        }
        s->page = IP_FEEDBACK;
        break;
    }
    case IP_FEEDBACK:
        if (!s->reviewing) {
            if (++s->index < IP_ROUND) { ask(s); break; }
            if (!s->review_count) { hatch(s); break; }
            s->reviewing = true;
        } else if (s->last_correct) {
            if (++s->review_index == s->review_count) { hatch(s); break; }
        }
        /* A review mistake is retried with reshuffled options, never rewarded
           as mastery. The child can leave at any time via the home action. */
        ask(s);
        break;
    case IP_HATCH: ip_album(s); break;
    case IP_ALBUM: ip_home(s); break;
    }
}
