#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ebook_audio.h"

static long long rms_of(eb_sound_t sound, size_t from, size_t to) {
    int16_t buf[64];
    long long sum = 0;
    for (size_t pos = from; pos < to; pos += 64) {
        size_t want = to - pos < 64 ? to - pos : 64;
        size_t got = eb_sound_render(sound, pos, buf, want);
        for (size_t i = 0; i < got; i++) sum += (long long)buf[i] * buf[i];
    }
    return sum / (long long)(to - from);
}

int main(void) {
    assert(eb_sound_samples(EB_SOUND_NONE) == 0);
    assert(eb_sound_samples(EB_SOUND_COUNT) == 0);
    assert(eb_sound_samples((eb_sound_t)99) == 0);
    assert(eb_sound_samples(EB_SOUND_PAGE_NEXT) == 110 * 16);
    assert(eb_sound_samples(EB_SOUND_PAGE_PREV) == 95 * 16);

    for (int s = EB_SOUND_PAGE_NEXT; s < EB_SOUND_COUNT; s++) {
        eb_sound_t sound = (eb_sound_t)s;
        size_t total = eb_sound_samples(sound);

        // 整段渲染一次作为基准。
        int16_t *whole = malloc(total * sizeof(int16_t));
        assert(whole);
        assert(eb_sound_render(sound, 0, whole, total) == total);

        // 分块渲染必须逐采样等于整段渲染:运行时按 10 ms 一块送 I2S,
        // 一旦 render 带了隐式状态,真机上就会在块边界"咔"一声。
        for (size_t chunk = 1; chunk <= 160; chunk = chunk * 3 + 1) {
            int16_t piece[161];
            for (size_t off = 0; off < total; off += chunk) {
                size_t got = eb_sound_render(sound, off, piece, chunk);
                assert(got == (total - off < chunk ? total - off : chunk));
                assert(memcmp(piece, whole + off, got * sizeof(int16_t)) == 0);
            }
        }

        // 不削波,而且留足余量:峰值不超过满量程的四成。
        for (size_t i = 0; i < total; i++) assert(abs(whole[i]) <= 13000);
        // 起音处从零开始,不会有"咔"的一声;结尾同样收干净。
        assert(abs(whole[0]) < 400);
        assert(abs(whole[total - 1]) < 400);

        // 越界与非法参数。
        assert(eb_sound_render(sound, total, whole, 16) == 0);
        assert(eb_sound_render(sound, total + 999, whole, 16) == 0);
        assert(eb_sound_render(sound, 0, NULL, 16) == 0);
        assert(eb_sound_render(EB_SOUND_NONE, 0, whole, 16) == 0);
        free(whole);
    }

    // 方向可听:下一页先重后轻,上一页反过来。
    size_t next_total = eb_sound_samples(EB_SOUND_PAGE_NEXT);
    assert(rms_of(EB_SOUND_PAGE_NEXT, 0, next_total / 3) >
           rms_of(EB_SOUND_PAGE_NEXT, next_total * 2 / 3, next_total));
    size_t prev_total = eb_sound_samples(EB_SOUND_PAGE_PREV);
    assert(rms_of(EB_SOUND_PAGE_PREV, 0, prev_total / 3) <
           rms_of(EB_SOUND_PAGE_PREV, prev_total / 3, prev_total * 2 / 3));

    printf("ebook page-turn audio: ok\n");
    return 0;
}
