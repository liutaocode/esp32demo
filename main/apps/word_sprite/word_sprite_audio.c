#include "word_sprite_audio.h"
#include "word_sprite_state.h"

bool ws_explanation_clips(unsigned word, int cue, int clips[3])
{
    if (!clips || word >= WS_WORDS ||
        (cue != -1 && cue != WS_VOICE_CORRECT && cue != WS_VOICE_RETRY)) return false;
    int meaning = WS_VOICE_MEANING_BASE + (int)word;
    clips[0] = cue >= 0 ? cue : meaning;
    clips[1] = cue >= 0 ? meaning : (int)word;
    clips[2] = cue >= 0 ? (int)word : -1;
    return true;
}

bool ws_clip_valid(const ws_clip_t *clip, size_t size)
{
    return clip && clip->samples > 0 && clip->step <= 88 &&
        clip->offset <= size && clip->size <= size - clip->offset &&
        clip->size == clip->samples / 2;
}
