#include "down_100_audio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void)
{
    int16_t full[6400], chunks[6400];
    for (int cue = D100_SOUND_START; cue < D100_SOUND_COUNT; cue++) {
        size_t size = d100_sound_samples(cue);
        assert(size > 0 && size <= 6400);
        assert(d100_sound_render(cue, 0, full, 6400) == size);
        for (size_t off = 0; off < size;) {
            size_t count = d100_sound_render(cue, off, chunks + off, 137);
            assert(count && count <= 137); off += count;
        }
        assert(!memcmp(full, chunks, size * sizeof(*full)));
        assert(full[0] == 0 && full[size - 1] == 0);
        unsigned positive = 0, negative = 0;
        for (size_t i = 0; i < size; i++) {
            assert(full[i] >= -5760 && full[i] <= 5760);
            positive += full[i] > 0; negative += full[i] < 0;
        }
        assert(positive > size / 4 && negative > size / 4);
        assert(d100_sound_render(cue, size, full, 1) == 0);
    }
    assert(!d100_sound_samples(-1) && !d100_sound_samples(D100_SOUND_COUNT));
    assert(!d100_sound_render(D100_SOUND_GEM, 0, NULL, 1));
    d100_state_t before = {.page = D100_HOME}, after = {.page = D100_PLAY, .health = 3};
    assert(d100_sound_event(&before, &after) == D100_SOUND_START);
    before = after; after.gems++;
    assert(d100_sound_event(&before, &after) == D100_SOUND_GEM);
    before = after; assert(d100_sound_event(&before, &after) == D100_SOUND_NONE);
    after.health--; assert(d100_sound_event(&before, &after) == D100_SOUND_HURT);
    before = after; after.health++; assert(d100_sound_event(&before, &after) == D100_SOUND_HEAL);
    before = after; after.supplies++; assert(d100_sound_event(&before, &after) == D100_SOUND_HEAL);
    before = after; after.notice = D100_BREAKING; after.notice_ms = 1100;
    assert(d100_sound_event(&before, &after) == D100_SOUND_CRACK);
    before = after; after.notice_ms -= 20; assert(d100_sound_event(&before, &after) == D100_SOUND_NONE);
    after.page = D100_PAUSE; assert(d100_sound_event(&before, &after) == D100_SOUND_NONE);
    before = after; after.page = D100_PLAY; assert(d100_sound_event(&before, &after) == D100_SOUND_NONE);
    before = after; after.page = D100_RESULT; after.health = 0;
    assert(d100_sound_event(&before, &after) == D100_SOUND_LOSE);
    after.floor = 100; assert(d100_sound_event(&before, &after) == D100_SOUND_CLEAR);
    before = after; assert(d100_sound_event(&before, &after) == D100_SOUND_NONE);
    puts("Down 100 audio: seven bounded cues, sample continuity, event priority and no repeated cues PASS");
}
