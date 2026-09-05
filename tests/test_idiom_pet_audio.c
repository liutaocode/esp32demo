#include "idiom_pet_audio.h"
#include "minecraft_adpcm.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static void transitions(void)
{
    for (unsigned i = 0; i < IP_QUESTIONS; i++) {
        ip_state_t s = {.page = IP_FEEDBACK, .question = i, .last_correct = false};
        assert(ip_audio_transition(IP_QUIZ, &s) == (int)i);
        s.reviewing = true;
        assert(ip_audio_transition(IP_QUIZ, &s) == (int)i);
        assert(ip_audio_transition(IP_FEEDBACK, &s) == IP_AUDIO_NONE);
        s.last_correct = true;
        assert(ip_audio_transition(IP_QUIZ, &s) == IP_AUDIO_NONE);
        for (unsigned page = IP_HOME; page <= IP_ALBUM; page++) {
            if (page == IP_FEEDBACK) continue;
            s.page = page;
            assert(ip_audio_transition(IP_FEEDBACK, &s) == IP_AUDIO_STOP);
        }
    }
    ip_audio_clip_t c = {0, 2, 4, 0, 0};
    assert(ip_audio_clip_valid(&c, 2));
    assert(!ip_audio_clip_valid(NULL, 2));
    assert(!ip_audio_clip_valid(&c, 1));
    c.offset = UINT32_MAX; assert(!ip_audio_clip_valid(&c, 20));
    c.offset = 0; c.step = 89; assert(!ip_audio_clip_valid(&c, 20));
    c.step = 0; c.samples = 0; assert(!ip_audio_clip_valid(&c, 20));
    c.samples = 5; assert(ip_audio_clip_valid(&c, 2));
    c.samples = 6; assert(!ip_audio_clip_valid(&c, 2));
}

int main(void)
{
    transitions();
    FILE *f = fopen("assets/music/idiom_pet/idiom_pet_adpcm.bin", "rb");
    assert(f);
    uint8_t *data = malloc(ip_audio_size); assert(data);
    assert(fread(data, 1, ip_audio_size, f) == ip_audio_size);
    assert(fgetc(f) == EOF); fclose(f);
    size_t end = 0;
    for (unsigned i = 0; i < IP_QUESTIONS; i++) {
        const ip_audio_clip_t *c = &ip_audio_clips[i];
        assert(ip_audio_clip_valid(c, ip_audio_size) && c->offset == end);
        end += c->size;
        minecraft_adpcm_state_t d;
        minecraft_adpcm_init(&d, c->predictor, c->step);
        int peak = 0;
        for (uint32_t sample = 1; sample < c->samples; sample++) {
            unsigned nibble = sample - 1;
            uint8_t packed = data[c->offset + nibble / 2];
            int pcm = minecraft_adpcm_decode(&d, nibble & 1 ? packed >> 4 : packed & 15);
            if (abs(pcm) > peak) peak = abs(pcm);
        }
        assert(peak > 100);
    }
    assert(end == ip_audio_size); free(data);
    puts("Idiom Pet audio: wrong-answer/review triggers, exit cancellation, clip bounds and 24 decodes PASS");
}
