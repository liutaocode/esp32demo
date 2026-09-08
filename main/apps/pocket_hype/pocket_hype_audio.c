#include "pocket_hype_audio.h"
bool ph_clip_valid(const ph_clip_t *c, size_t size) {
    return c && c->samples > 0 && c->samples <= PH_AUDIO_RATE * 6 && c->step <= 88 &&
        c->offset <= size && c->size <= size - c->offset && c->size == c->samples / 2;
}
