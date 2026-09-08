#include "night_call_audio.h"
bool nc_clip_valid(const nc_clip_t *c, size_t size) {
    return c && c->samples > 0 && c->samples <= NC_AUDIO_RATE * 18 && c->step <= 88 &&
        c->offset <= size && c->size <= size-c->offset && c->size == c->samples/2;
}
