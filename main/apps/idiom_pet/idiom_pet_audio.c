#include "idiom_pet_audio.h"

bool ip_audio_clip_valid(const ip_audio_clip_t *clip, size_t size)
{
    return clip && clip->samples > 0 && clip->samples <= IP_AUDIO_RATE * 30U &&
        clip->step <= 88 && clip->offset <= size &&
        clip->size <= size - clip->offset && clip->size == clip->samples / 2;
}

int ip_audio_transition(ip_page_t before, const ip_state_t *after)
{
    if (before == IP_FEEDBACK && after->page != IP_FEEDBACK) return IP_AUDIO_STOP;
    if (before != IP_FEEDBACK && after->page == IP_FEEDBACK && !after->last_correct &&
        after->question < IP_QUESTIONS) return after->question;
    return IP_AUDIO_NONE;
}
