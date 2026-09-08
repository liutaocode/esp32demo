#include "tts_bank.h"
#include "tts_tempo.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    fseek(f, 0, SEEK_END); size_t size = (size_t)ftell(f); rewind(f);
    uint8_t *data = malloc(size); assert(data);
    assert(fread(data, 1, size, f) == size); fclose(f);
    tts_bank_t bank; tts_syllable_t syll;
    assert(tts_bank_open(&bank, data, size));
    assert(bank.count == 1749 && bank.frame_bytes == 20);
    for (uint32_t i = 0; i < bank.count; i++) assert(tts_bank_syllable(&bank, i, &syll));
    assert(!tts_bank_syllable(&bank, bank.count, &syll));
    assert(!tts_bank_open(&bank, data, size - 1));
    for (size_t n = 0; n < 7040; n += 17) assert(!tts_bank_open(&bank, data, n));
    uint8_t saved = data[36]; data[36] = 0;
    assert(!tts_bank_open(&bank, data, size)); data[36] = saved;
    saved = data[171643]; data[171643] = 255;
    assert(!tts_bank_open(&bank, data, size)); data[171643] = saved;
    free(data);
    enum { N = 8000, CAP = 12640 };
    int16_t in[N], out[CAP];
    for (int i = 0; i < N; i++) in[i] = (int16_t)(10000 * sin(2 * 3.141592653589793 * 200 * i / 16000));
    const double rates[] = {.8, .9, 1, 1.1, 1.2, 1.3};
    for (unsigned speed = 0; speed < 6; speed++) {
        size_t n = tts_tempo_process(in, N, out, CAP, speed);
        assert(n && fabs((double)n - N / rates[speed]) < 600);
        if (speed == 2) assert(n == N && memcmp(in, out, sizeof(in)) == 0);
        unsigned crosses = 0;
        for (size_t i = 1; i < n; i++) if (out[i-1] <= 0 && out[i] > 0) crosses++;
        assert(fabs(crosses * 16000.0 / n - 200) < 5);
    }
    assert(!tts_tempo_process(in, N, out, 1, 0));
    assert(!tts_tempo_process(in, N, out, CAP, 6));
    for (size_t n = 1; n < 400; n++)
        for (unsigned speed = 0; speed < 6; speed++)
            assert(tts_tempo_process(in, n, out, CAP, speed));
    puts("Compact voice bounds / tempo duration and pitch: PASS");
}
