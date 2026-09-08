#include "ebook_speech.h"
#include "tts_model.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const char *text = "你好，世界。下一句！\n最后一段";
    char out[TTS_TEXT_BYTES];
    size_t n = eb_speech_chunk(text, strlen(text), out, sizeof(out));
    assert(!strcmp(out, "你好，世界。") && n == strlen(out));
    size_t k = eb_speech_chunk(text+n, strlen(text)-n, out, sizeof(out));
    assert(!strcmp(out, "下一句！") && k == strlen(out));
    n += k;
    assert(eb_speech_chunk(text+n, strlen(text)-n, out, sizeof(out)) == strlen(text)-n);
    assert(!strcmp(out, "最后一段"));
    char large[901]; for (unsigned i=0;i<300;i++) memcpy(large+i*3,"书",3); large[900]=0;
    size_t pos=0;
    while (pos<900) {
        n=eb_speech_chunk(large+pos,900-pos,out,sizeof(out));
        assert(n && n%3==0 && n<=240 && tts_text_valid(out)); pos+=n;
    }
    assert(eb_speech_chunk("\xef\xbb\xbf\t你好\r\n",11,out,sizeof(out))>0);
    assert(!strcmp(out,"你好 "));
    eb_speech_t s={.speed=2}; eb_speech_begin(&s,100);
    s.waiting=true; s.next=240; eb_speech_pause(&s);
    assert(s.paused && !s.active && !s.waiting && s.cursor==100);
    s.active=true;s.paused=false;s.waiting=true;eb_speech_completed(&s);
    assert(s.cursor==240 && !s.waiting);
    eb_speech_stop(&s);assert(!s.active && !s.paused && s.speed==2);
    assert(eb_speech_volume_step(65,1)==75);
    assert(eb_speech_volume_step(95,1)==100);
    assert(eb_speech_volume_step(5,-1)==0);
    assert(eb_speech_volume_step(0,-1)==0);
    assert(eb_speech_volume_step(100,1)==100);
    puts("Speech sentence boundaries and pause cursor: PASS");
}
