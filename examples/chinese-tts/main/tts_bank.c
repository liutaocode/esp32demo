#include "tts_bank.h"
#include <string.h>
uint32_t tts_bank_u32(const uint8_t *p) {
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static uint16_t u16(const uint8_t *p) { return p[0] | (uint16_t)p[1] << 8; }
bool tts_bank_syllable(const tts_bank_t *b, uint32_t index, tts_syllable_t *s) {
    if (!b || !s || index >= b->count) return false;
    uint32_t a = tts_bank_u32(b->data + 40 + index * 4);
    uint32_t z = tts_bank_u32(b->data + 44 + index * 4);
    if (a > z || z > b->size - b->metadata_size || z - a < 8) return false;
    const uint8_t *p = b->data + b->metadata_size + a;
    uint32_t samples = tts_bank_u32(p);
    uint16_t skip = u16(p + 4), frames = u16(p + 6);
    if (!samples || samples > 32000 || !frames || skip >= 320 ||
        samples + skip > (uint32_t)frames * 320 ||
        z - a != 8u + (uint32_t)frames * b->frame_bytes) return false;
    *s = (tts_syllable_t){p + 8, samples, skip, frames};
    return true;
}
bool tts_bank_open(tts_bank_t *b, const uint8_t *data, size_t size) {
    if (!b || !data || size < 40) return false;
    memset(b, 0, sizeof(*b));
    uint32_t format = tts_bank_u32(data + 16), table = tts_bank_u32(data + 20);
    uint32_t metadata = tts_bank_u32(data + 36);
    if ((format & 0xffff0000) != 0x53020000 || (format & 255) < 6 ||
        (format & 255) > 40 || table != 7000 || metadata != 171600 || metadata > size - 40) return false;
    uint32_t last = table;
    for (unsigned offset = 24; offset <= 36; offset += 4) {
        uint32_t next = tts_bank_u32(data + offset);
        if (next < last || next > metadata || next % 2) return false;
        last = next;
    }
    b->data = data; b->size = size; b->metadata_size = metadata + 40;
    b->count = table / 4 - 1; b->frame_bytes = format & 255;
    if (tts_bank_u32(data + 40) != 0 ||
        tts_bank_u32(data + 40 + b->count * 4) != size - b->metadata_size) return false;
    for (uint32_t i = 0; i < b->count; i++) {
        tts_syllable_t s;
        if (!tts_bank_syllable(b, i, &s)) return false;
        if (s.samples > b->max_samples) b->max_samples = s.samples;
    }
    return true;
}
