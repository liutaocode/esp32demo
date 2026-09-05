#include "vibe_check_audio.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>

int main(void)
{
    size_t end = 0;
    for (unsigned i = 0; i < VC_AUDIO_COUNT; i++) {
        const vibe_check_audio_clip_t *clip = &vibe_check_audio_clips[i];
        assert(vibe_check_audio_clip_valid(clip, vibe_check_audio_size));
        assert(clip->offset == end);
        end += clip->size;
    }
    assert(end == vibe_check_audio_size);

    vibe_check_audio_clip_t bad = {UINT32_MAX, 4, 8, 0, 0};
    assert(!vibe_check_audio_clip_valid(&bad, 100));
    bad = (vibe_check_audio_clip_t){0, 1, 3, 0, 0};
    assert(vibe_check_audio_clip_valid(&bad, 1));
    bad.samples = 0;
    assert(!vibe_check_audio_clip_valid(&bad, 1));
    bad.samples = 3;
    bad.step = 89;
    assert(!vibe_check_audio_clip_valid(&bad, 1));
    bad.step = 0;
    bad.size = 2;
    assert(!vibe_check_audio_clip_valid(&bad, 1));
    assert(!vibe_check_audio_clip_valid(NULL, 0));

    puts("Vibe Check: 14 Mandarin TTS clip bounds PASS");
    return 0;
}
