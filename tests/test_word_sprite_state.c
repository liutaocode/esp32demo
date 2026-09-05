#include "word_sprite_state.h"
#include "word_sprite_catalog.h"
#include "word_sprite_audio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void select_correct(ws_state_t *s)
{
    for (unsigned i = 0; i < 3; i++) if (s->options[i] == ws_word(s)) s->selected = i;
}

static void deck_invariants(const ws_state_t *s, bool review)
{
    uint64_t seen = 0;
    for (unsigned i = 0; i < s->count; i++) {
        assert(s->deck[i] < WS_WORDS);
        uint64_t bit = UINT64_C(1) << s->deck[i];
        assert(!(seen & bit)); seen |= bit;
        if (!review) assert(s->deck[i] / 12 == s->world);
    }
}

int main(void)
{
    unsigned coverage[WS_WORDS] = {0}, slots[3] = {0};
    for (uint32_t seed = 0; seed < 1000; seed++) {
        for (unsigned world = 0; world < WS_WORLDS; world++) {
            ws_state_t s, same;
            ws_init(&s, seed, (ws_progress_t){0}); s.world = world;
            same = s;
            assert(ws_start(&s, false)); assert(ws_start(&same, false));
            assert(memcmp(s.deck, same.deck, WS_ROUND) == 0);
            assert(s.count == WS_ROUND); deck_invariants(&s, false);
            assert(!ws_start(&s, false));
            for (unsigned i = 0; i < WS_ROUND; i++) {
                assert(s.page == WS_QUESTION);
                uint8_t target = ws_word(&s);
                coverage[target]++;
                assert(s.options[0] != s.options[1] && s.options[1] != s.options[2] && s.options[0] != s.options[2]);
                for (unsigned j = 0; j < 3; j++) assert(s.options[j] / 12 == world);
                select_correct(&s); slots[s.selected]++;
                assert(ws_answer(&s)); assert(!ws_answer(&s));
                assert(s.page == WS_FEEDBACK && s.last_correct);
                ws_next(&s);
            }
            assert(s.page == WS_RESULT && s.correct == 6 && ws_count(s.progress.learned) == 6);
            assert(ws_stage(&s) == 1 && s.progress.review == 0);
            ws_next(&s); assert(s.page == WS_RESULT);
            assert(!ws_start(&s, true) && s.page == WS_RESULT);
            /* Next round prioritizes the six remaining unlit words. */
            uint64_t old = s.progress.learned;
            assert(ws_start(&s, false));
            for (unsigned i = 0; i < s.count; i++) assert(!(old & (UINT64_C(1) << s.deck[i])));
        }
    }
    for (unsigned i = 0; i < WS_WORDS; i++) assert(coverage[i] > 100);
    for (unsigned i = 0; i < 3; i++) assert(slots[i] > 4000);

    ws_state_t s;
    ws_init(&s, 0, (ws_progress_t){UINT64_MAX, UINT64_C(1) << 35});
    assert(ws_count(s.progress.learned) == 35 && s.progress.review == (UINT64_C(1) << 35));
    assert(ws_stage(&s) == 3);
    ws_move(&s, -1); assert(s.world == 2); ws_move(&s, 1); assert(s.world == 0);
    assert(ws_start(&s, true) && s.count == 1 && ws_word(&s) == 35);
    select_correct(&s); s.selected = (s.selected + 1) % 3;
    assert(ws_answer(&s) && !s.last_correct); ws_next(&s);
    assert(s.progress.review == (UINT64_C(1) << 35) && s.correct == 0);
    assert(ws_start(&s, true)); select_correct(&s); ws_answer(&s); ws_next(&s);
    assert(s.recovered == 1 && ws_count(s.progress.learned) == 36 && !s.progress.review);
    /* Reinitialization round-trips sanitized persistence, including high bits. */
    ws_state_t restored; ws_init(&restored, 9, s.progress);
    assert(restored.progress.learned == WS_VALID_MASK);
    for (unsigned round = 0; round < 50; round++) {
        ws_home(&s); assert(ws_start(&s, false));
        for (unsigned i = 0; i < WS_ROUND; i++) { select_correct(&s); ws_answer(&s); ws_next(&s); }
        assert(ws_count(s.progress.learned) == 36); /* Repeat words cannot inflate growth. */
    }
    ws_home(&s); assert(ws_start(&s, false));
    uint8_t missed = ws_word(&s);
    select_correct(&s); s.selected = (s.selected + 1) % 3; ws_answer(&s);
    ws_home(&s);
    assert(s.progress.review == (UINT64_C(1) << missed));
    ws_init(&restored, 8, s.progress);
    assert(restored.progress.review == s.progress.review);
    assert(ws_start(&restored, true) && ws_word(&restored) == missed);

    size_t end = 0;
    assert(WS_VOICE_MEANING_BASE + WS_WORDS == WS_AUDIO_COUNT);
    for (unsigned word = 0; word < WS_WORDS; word++) {
        int sequence[3];
        const int cues[] = {WS_VOICE_CORRECT, WS_VOICE_RETRY};
        for (unsigned i = 0; i < 2; i++) {
            assert(ws_explanation_clips(word, cues[i], sequence));
            assert(sequence[0] == cues[i]);
            assert(sequence[1] == WS_VOICE_MEANING_BASE + (int)word);
            assert(sequence[2] == (int)word);
        }
        assert(ws_explanation_clips(word, -1, sequence));
        assert(sequence[0] == WS_VOICE_MEANING_BASE + (int)word);
        assert(sequence[1] == (int)word && sequence[2] == -1);
    }
    int invalid_sequence[3];
    assert(!ws_explanation_clips(WS_WORDS, -1, invalid_sequence));
    assert(!ws_explanation_clips(0, WS_AUDIO_COUNT, invalid_sequence));
    assert(!ws_explanation_clips(0, -1, NULL));
    for (unsigned i = 0; i < WS_AUDIO_COUNT; i++) {
        assert(ws_clip_valid(&ws_clips[i], ws_audio_size));
        assert(ws_clips[i].offset == end);
        end += ws_clips[i].size;
    }
    assert(end == ws_audio_size);
    ws_clip_t bad = {UINT32_MAX, 4, 8, 0, 0};
    assert(!ws_clip_valid(&bad, 100));
    bad = (ws_clip_t){0, 1, 3, 0, 0}; assert(ws_clip_valid(&bad, 1));
    bad.samples = 0; assert(!ws_clip_valid(&bad, 1));
    bad.samples = 3; bad.step = 89; assert(!ws_clip_valid(&bad, 1));
    bad.step = 0; bad.size = 2; assert(!ws_clip_valid(&bad, 1));
    assert(!ws_clip_valid(NULL, 0));
    for (unsigned i = 0; i < WS_WORDS; i++) {
        assert(*ws_words[i].english && *ws_words[i].chinese);
        for (unsigned j = i + 1; j < WS_WORDS; j++) {
            assert(strcmp(ws_words[i].english, ws_words[j].english) != 0);
            assert(strcmp(ws_words[i].chinese, ws_words[j].chinese) != 0);
        }
    }
    puts("Word Sprite: 3000 decks, learning/review/persistence and 77 TTS clip bounds PASS");
}
