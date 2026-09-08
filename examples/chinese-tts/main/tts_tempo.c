#include "tts_tempo.h"
#include <string.h>
#define HOP 160
#define WINDOW 320
#define SEARCH 80
size_t tts_tempo_capacity(size_t n) { return n + n / 2 + 2 * WINDOW; }
size_t tts_tempo_process(const int16_t *in, size_t n, int16_t *out, size_t cap, unsigned speed) {
    static const unsigned rates[6] = {80, 90, 100, 110, 120, 130};
    if (!in || !out || speed >= 6 || cap < tts_tempo_capacity(n)) return 0;
    if (speed == 2 || n < WINDOW + SEARCH) { memcpy(out, in, n * sizeof(*out)); return n; }
    size_t used = HOP, last = 0;
    int16_t tail[HOP];
    memcpy(out, in, HOP * sizeof(*out));
    memcpy(tail, in + HOP, sizeof(tail));
    unsigned rate = rates[speed];
    for (size_t step = 1;; step++) {
        size_t target = step * HOP * rate / 100;
        if (target + WINDOW + SEARCH >= n) break;
        size_t low = target > SEARCH ? target - SEARCH : 0;
        size_t high = target + SEARCH;
        uint64_t best_cost = UINT64_MAX;
        size_t best = target;
        /* Search one sample at a time; subsampled error keeps the C3 workload small. */
        for (size_t pos = low; pos <= high; pos++) {
            uint64_t cost = 0;
            for (size_t i = 0; i < HOP; i += 4) {
                int32_t delta = (int32_t)tail[i] - in[pos + i];
                cost += (uint64_t)((int64_t)delta * delta);
            }
            if (cost < best_cost) { best_cost = cost; best = pos; }
        }
        if (used + HOP > cap) return 0;
        for (size_t i = 0; i < HOP; i++)
            out[used++] = (int16_t)(((int32_t)tail[i] * (HOP - (int)i) +
                                     (int32_t)in[best + i] * (int)i) / HOP);
        memcpy(tail, in + best + HOP, sizeof(tail));
        last = best;
    }
    size_t rest = n - (last + WINDOW);
    if (used + HOP + rest > cap) return 0;
    memcpy(out + used, tail, sizeof(tail)); used += HOP;
    memcpy(out + used, in + last + WINDOW, rest * sizeof(*out));
    return used + rest;
}
