/* Compile against the vendored fixed-point Speex decoder; decode every record. */
#include "tts_bank.h"
#include "speex/speex.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    FILE *f = fopen(argv[1], "rb"); assert(f);
    fseek(f, 0, SEEK_END); size_t size = (size_t)ftell(f); rewind(f);
    unsigned char *data = malloc(size); assert(data);
    assert(fread(data, 1, size, f) == size); fclose(f);
    tts_bank_t bank; assert(tts_bank_open(&bank, data, size));
    void *decoder = speex_decoder_init(&speex_wb_mode); assert(decoder);
    SpeexBits bits; speex_bits_init(&bits);
    unsigned frames = 0;
    for (unsigned i = 0; i < bank.count; i++) {
        tts_syllable_t syll; assert(tts_bank_syllable(&bank, i, &syll));
        assert(speex_decoder_ctl(decoder, SPEEX_RESET_STATE, NULL) == 0);
        for (unsigned j = 0; j < syll.frames; j++) {
            int16_t pcm[320];
            speex_bits_read_from(&bits, (char *)(syll.packets + j * bank.frame_bytes), bank.frame_bytes);
            assert(speex_decode_int(decoder, &bits, pcm) == 0);
            frames++;
        }
    }
    speex_bits_destroy(&bits); speex_decoder_destroy(decoder); free(data);
    printf("Fixed-point decoder: %u records / %u frames PASS\n", bank.count, frames);
}
