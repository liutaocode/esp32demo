#include "ebook_speech.h"
#include "ebook_text.h"
#include <string.h>
void eb_speech_begin(eb_speech_t *s, uint32_t offset) {
    s->cursor = offset; s->next = offset; s->active = true; s->paused = false; s->waiting = false;
}
void eb_speech_pause(eb_speech_t *s) { s->active = false; s->paused = true; s->waiting = false; }
void eb_speech_stop(eb_speech_t *s) { s->active = s->paused = s->waiting = false; }
void eb_speech_completed(eb_speech_t *s) { if (s->waiting) { s->cursor = s->next; s->waiting = false; } }
size_t eb_speech_chunk(const char *src, size_t n, char *out, size_t cap) {
    if (!src || !out || cap < 5) return 0;
    size_t pos = 0, used = 0;
    out[0] = 0;
    while (pos < n) {
        size_t start = pos; uint32_t cp;
        if (!eb_utf8_next(src, n, &pos, &cp)) break;
        if (cp == 0xfffd || cp == 0xfeff) continue;
        if (cp <= 32 || cp == 127 || cp == 0x3000) {
            if (used && out[used-1] != ' ') {
                if (used + 1 >= cap) return start;
                out[used++] = ' '; out[used] = 0;
            }
            if ((cp == '\n' || cp == '\r') && used) return pos;
            continue;
        }
        size_t len = pos - start;
        if (used + len >= cap) return start;
        memcpy(out + used, src + start, len); used += len; out[used] = 0;
        if (cp == 0x3002 || cp == 0xff01 || cp == 0xff1f || cp == '!' || cp == '?' || cp == 0xff1b || cp == ';') return pos;
    }
    return pos;
}

unsigned eb_speech_volume_step(unsigned volume, int direction) {
    if (volume > 100) volume = 100;
    if (direction > 0) return volume > 90 ? 100 : volume + 10;
    if (direction < 0) return volume < 10 ? 0 : volume - 10;
    return volume;
}
