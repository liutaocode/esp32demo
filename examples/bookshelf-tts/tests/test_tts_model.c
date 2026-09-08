#include "tts_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    tts_model_t m = {0, 2};
    tts_model_move(&m, -1); assert(m.selected == tts_sample_count - 1);
    tts_model_move(&m, 1); assert(m.selected == 0);
    for (size_t i = 0; i < tts_sample_count; i++) {
        assert(tts_text_valid(tts_samples[i].text));
        tts_model_move(&m, 1);
    }
    assert(m.selected == 0);
    for (int i = 0; i < TTS_SPEED_COUNT; i++) tts_model_speed(&m);
    assert(m.speed == 2);
    assert(!tts_text_valid(NULL)); assert(!tts_text_valid(""));
    assert(!tts_text_valid("   ")); assert(!tts_text_valid("a\nb"));
    assert(!tts_text_valid("\xc0\xaf")); assert(!tts_text_valid("\xed\xa0\x80"));
    assert(!tts_text_valid("\xf4\x90\x80\x80")); assert(!tts_text_valid("\xe4\xb8"));
    assert(!tts_text_valid("\x80")); assert(!tts_text_valid("\xe4\x41\x80"));
    assert(tts_text_valid("你好，12.5元。"));
    char text[TTS_TEXT_BYTES + 1]; memset(text, 'a', sizeof(text));
    text[TTS_TEXT_BYTES] = 0; assert(!tts_text_valid(text));
    text[TTS_TEXT_BYTES - 1] = 0; assert(tts_text_valid(text));
    puts("TTS model and UTF-8 boundaries: PASS");
}
