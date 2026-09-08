#include "pocket_hype_state.h"
#include "pocket_hype_audio.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
int main(void) {
    ph_state_t s; ph_init(&s, 0);
    assert(s.page == PH_LIVE && s.voice && s.volume == 2);
    assert(ph_confirm(&s, 1) == 0 && s.tier == 0);
    ph_tick(&s, 500); assert(ph_confirm(&s, 1) == 1);
    assert(ph_confirm(&s, 2) == 2 && s.chain == 3);
    for (int i = 0; i < 40; i++) assert(ph_confirm(&s, 1) == 2);
    ph_tick(&s, PH_HEAT_MS); assert(ph_confirm(&s, 1) == 0);
    ph_tick(&s, UINT32_MAX); ph_tick(&s, 50); assert(!s.active && !s.chain);
    ph_move(&s, -1); assert(s.selected == PH_SCENES);
    unsigned previous = s.played;
    for (unsigned i = 0; i < 1000; i++) {
        assert(ph_confirm(&s, 1) >= 0 && s.played < PH_SCENES);
        assert(s.played != previous); previous = s.played;
    }
    assert(ph_collected(&s) == 6);
    ph_back(&s); assert(s.page == PH_MENU && !s.active);
    for (int i = 0; i < 4; i++) assert(ph_confirm(&s, 1) == -1);
    assert(s.volume == 2); ph_move(&s, 1); ph_confirm(&s, 1); assert(!s.voice);
    ph_back(&s); assert(s.page == PH_LIVE);
    for (uint32_t seed = 1; seed < 100; seed++) {
        ph_init(&s, seed); ph_challenge(&s);
        unsigned seen = 0;
        for (unsigned round = 0; round < PH_ROUNDS; round++) {
            unsigned answer = s.order[round]; assert(answer < 6 && !(seen & (1U << answer)));
            seen |= 1U << answer; assert(s.cues[round] < 3);
            s.selected = seed % 2 ? answer : (answer + 1) % 6;
            assert(ph_confirm(&s, 1) >= 0 && s.page == PH_FEEDBACK);
            assert(s.correct == (seed % 2 != 0));
            ph_tick(&s, 100000); assert(s.page == PH_FEEDBACK);
            ph_confirm(&s, 2); assert(s.round == round + 1);
        }
        assert(s.page == PH_RESULT && s.score == (seed % 2 ? 6U : 0U));
        ph_confirm(&s, 1); assert(s.page == PH_CHALLENGE && s.score == 0 && s.round == 0);
        ph_back(&s); assert(s.page == PH_LIVE);
    }
    for (unsigned i = 0; i < PH_AUDIO_COUNT; i++) assert(ph_clip_valid(&ph_clips[i], ph_audio_bytes));
    ph_clip_t bad = ph_clips[0]; bad.offset = UINT32_MAX; assert(!ph_clip_valid(&bad, ph_audio_bytes));
    bad = ph_clips[0]; bad.samples = 0; assert(!ph_clip_valid(&bad, ph_audio_bytes));
    bad = ph_clips[0]; bad.step = 89; assert(!ph_clip_valid(&bad, ph_audio_bytes));
    assert(!ph_clip_valid(NULL, ph_audio_bytes));
    puts("Pocket Hype: heat expiry, saturation, no-repeat surprise, all challenge permutations and clip bounds PASS");
}
