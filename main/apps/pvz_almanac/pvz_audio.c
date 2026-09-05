#include "pvz_audio.h"
bool pvz_clip_valid(const pvz_clip_t *clip, size_t size) {
    return clip && clip->samples > 0 && clip->step <= 88 &&
        clip->offset <= size && clip->size <= size - clip->offset &&
        clip->size == clip->samples / 2;
}
