#include "down_100_audio.h"

typedef struct { uint16_t hz[4], note_ms; uint8_t count; } cue_t;
static const cue_t cues[D100_SOUND_COUNT] = {
    [D100_SOUND_START] = {{440, 660}, 65, 2},
    [D100_SOUND_GEM] = {{880, 1320}, 45, 2},
    [D100_SOUND_HEAL] = {{523, 659, 784}, 60, 3},
    [D100_SOUND_CRACK] = {{180, 120}, 30, 2},
    [D100_SOUND_HURT] = {{330, 165}, 65, 2},
    [D100_SOUND_LOSE] = {{440, 330, 220}, 90, 3},
    [D100_SOUND_CLEAR] = {{523, 659, 784, 1047}, 100, 4},
};
d100_sound_t d100_sound_event(const d100_state_t *before, const d100_state_t *after)
{
    if (before->page == D100_PLAY && after->page == D100_RESULT)
        return after->floor >= D100_GOAL ? D100_SOUND_CLEAR : D100_SOUND_LOSE;
    if (after->page != D100_PLAY) return D100_SOUND_NONE;
    if (before->page != D100_PLAY)
        return before->page == D100_PAUSE ? D100_SOUND_NONE : D100_SOUND_START;
    if (after->health < before->health) return D100_SOUND_HURT;
    if (after->supplies > before->supplies || after->health > before->health) return D100_SOUND_HEAL;
    if (after->gems > before->gems) return D100_SOUND_GEM;
    if (after->notice == D100_BREAKING &&
        (before->notice != D100_BREAKING || after->notice_ms > before->notice_ms)) return D100_SOUND_CRACK;
    return D100_SOUND_NONE;
}
size_t d100_sound_samples(d100_sound_t sound)
{
    if (sound <= D100_SOUND_NONE || sound >= D100_SOUND_COUNT) return 0;
    return cues[sound].note_ms * 16U * cues[sound].count;
}
size_t d100_sound_render(d100_sound_t sound, size_t offset, int16_t *pcm, size_t capacity)
{
    size_t total = d100_sound_samples(sound);
    if (!pcm || offset >= total) return 0;
    size_t count = total - offset < capacity ? total - offset : capacity;
    const cue_t *cue = &cues[sound];
    unsigned note_samples = cue->note_ms * 16U;
    for (size_t i = 0; i < count; i++) {
        size_t pos = offset + i;
        unsigned local = pos % note_samples;
        unsigned phase = local * cue->hz[pos / note_samples] * 256U / D100_AUDIO_RATE % 256U;
        int triangle = phase < 64 ? (int)phase : phase < 192 ? 128 - (int)phase : (int)phase - 256;
        /* Five ms attack/release avoids clicks; peak stays below 1/5 scale. */
        unsigned envelope = local < 80 ? local : 80;
        if (note_samples - 1 - local < envelope) envelope = note_samples - 1 - local;
        pcm[i] = (int16_t)(triangle * 90 * (int)envelope / 80);
    }
    return count;
}
