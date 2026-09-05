#include "idiom_pet_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void answer(ip_state_t *s, bool correct)
{
    assert(s->page == IP_QUIZ);
    s->selected = (s->correct_slot + (correct ? 0 : 1)) % 3;
    ip_confirm(s);
    assert(s->page == IP_FEEDBACK);
}

static void round_perfect(ip_state_t *s)
{
    ip_confirm(s);
    for (unsigned i = 0; i < IP_ROUND; i++) {
        assert(s->index == i && !s->reviewing);
        answer(s, true);
        ip_confirm(s);
    }
    assert(s->page == IP_HATCH && s->first_correct == IP_ROUND);
}

static void test_reviews(void)
{
    ip_state_t s;
    ip_init(&s, 42, (ip_progress_t){0});
    ip_confirm(&s);
    uint8_t deck[IP_ROUND];
    memcpy(deck, s.deck, sizeof(deck));
    for (unsigned i = 0; i < IP_ROUND; i++) {
        answer(&s, false);
        assert(s.progress.pending & (1U << deck[i]));
        assert(!(s.progress.learned & (1U << deck[i])));
        ip_confirm(&s);
    }
    assert(s.reviewing && !s.progress.pets && s.first_correct == 0);
    /* An incorrect retry must not move forward or grant a pet. */
    answer(&s, false);
    ip_confirm(&s);
    assert(s.question == deck[0] && s.review_index == 0 && !s.progress.pets);
    for (unsigned i = 0; i < IP_ROUND; i++) {
        assert(s.question == deck[i]);
        answer(&s, true);
        ip_confirm(&s);
    }
    assert(s.page == IP_HATCH && s.first_correct == 0);
    assert(ip_count(s.progress.learned) == 4 && !s.progress.pending);
    assert(s.progress.pets == 1 && s.new_pet);
}

static void test_collection_and_coverage(void)
{
    ip_state_t s;
    ip_init(&s, 1, (ip_progress_t){0});
    for (unsigned land = 0; land < IP_LANDS; land++) {
        s.land = land;
        for (unsigned run = 0; run < 2; run++) {
            ip_home(&s);
            round_perfect(&s);
            assert(s.pet == land * 2 + run && s.new_pet);
        }
    }
    assert(s.progress.learned == IP_PROGRESS_MASK && s.progress.pets == 63);
    ip_home(&s);
    round_perfect(&s);
    assert(!s.new_pet && s.progress.pets == 63);
    ip_confirm(&s);
    assert(s.page == IP_ALBUM);
    s.cursor = 0; ip_move(&s, -1); assert(s.cursor == 5);
    ip_confirm(&s); assert(s.page == IP_HOME);
    s.land = 0; ip_move(&s, -1); assert(s.land == 2);
}

static void test_resume_and_validation(void)
{
    ip_state_t s;
    ip_init(&s, 0, (ip_progress_t){~0U, ~0U, 255});
    assert(!s.progress.learned && s.progress.pending == IP_PROGRESS_MASK);
    assert(s.progress.pets == 63 && s.rng && s.page == IP_HOME);
    ip_init(&s, 20, (ip_progress_t){0xFF, 1U << 3, 1});
    assert(!(s.progress.learned & (1U << 3)));
    ip_confirm(&s);
    assert(s.question == 3); /* Persistent mistakes have highest priority. */
    answer(&s, false);
    ip_home(&s); /* Abandoning never clears a mistake or grants a new pet. */
    assert(s.progress.pending == (1U << 3) && s.progress.pets == 1);
    ip_init(&s, 333, s.progress);
    ip_confirm(&s);
    assert(s.question == 3);
}

static void test_generation(void)
{
    unsigned positions = 0;
    for (unsigned seed = 0; seed < 1000; seed++) {
        for (unsigned land = 0; land < IP_LANDS; land++) {
            ip_state_t s;
            ip_init(&s, seed, (ip_progress_t){0});
            s.land = land;
            ip_confirm(&s);
            uint32_t seen = 0;
            for (unsigned i = 0; i < IP_ROUND; i++) {
                unsigned id = s.question;
                assert(id / IP_PER_LAND == land && !(seen & (1U << id)));
                seen |= 1U << id;
                positions |= 1U << s.correct_slot;
                unsigned matches = 0;
                for (unsigned j = 0; j < 3; j++) {
                    matches += strcmp(ip_option(&s, j), ip_questions[id].idiom) == 0;
                    for (unsigned k = j + 1; k < 3; k++)
                        assert(strcmp(ip_option(&s, j), ip_option(&s, k)) != 0);
                }
                assert(matches == 1);
                assert(strcmp(ip_questions[id].story, ip_questions[id].review) != 0);
                assert(ip_questions[id].meaning[0]);
                answer(&s, true); ip_confirm(&s);
            }
        }
    }
    assert(positions == 7);
}

int main(void)
{
    test_reviews(); test_collection_and_coverage();
    test_resume_and_validation(); test_generation();
    puts("Idiom Pet: review, collection, resume and 3000 seeded rounds PASS");
}
