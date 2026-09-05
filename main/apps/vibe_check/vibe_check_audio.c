#include "vibe_check_audio.h"

bool vibe_check_audio_clip_valid(const vibe_check_audio_clip_t *clip,
                                 size_t data_size)
{
    return clip && clip->samples > 0 && clip->samples <= VC_AUDIO_RATE * 15U &&
           clip->step <= 88 && clip->offset <= data_size &&
           clip->size <= data_size - clip->offset &&
           clip->size == clip->samples / 2;
}
