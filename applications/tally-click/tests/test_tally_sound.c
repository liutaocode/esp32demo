#include "tally_sound.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    unsigned hashes[TC_FX_ERROR+1]={0};
    for(int s=TC_FX_ADD;s<=TC_FX_ERROR;s++) {
        unsigned n=tc_sound_length((tc_feedback)s),hash=2166136261u,nonzero=0;
        assert(n>=480 && n<=3840);
        assert(tc_sound_sample((tc_feedback)s,0)==0);
        assert(tc_sound_sample((tc_feedback)s,n-1)==0);
        for(unsigned i=0;i<n;i++) {
            int sample=tc_sound_sample((tc_feedback)s,i); assert(abs(sample)<7000);
            if(sample)++nonzero;
            hash=(hash^(unsigned)(uint16_t)sample)*16777619u;
        }
        assert(nonzero>n/2); hashes[s]=hash;
        for(int j=TC_FX_ADD;j<s;j++)assert(hashes[j]!=hash);
        assert(tc_sound_sample((tc_feedback)s,n)==0);
        assert(tc_sound_sample((tc_feedback)s,n+1600)==0);
    }
    assert(tc_sound_length(TC_FX_NONE)==0 && tc_sound_length((tc_feedback)99)==0);
    tc_mixer mixer={0};int16_t out[TC_AUDIO_FRAMES];
    tc_mixer_request(&mixer,TC_FX_ADD);tc_mixer_render(&mixer,out,TC_AUDIO_FRAMES);
    int16_t expected=tc_sound_sample(TC_FX_ADD,TC_AUDIO_FRAMES);
    tc_mixer_request(&mixer,TC_FX_SUBTRACT);tc_mixer_render(&mixer,out,TC_AUDIO_FRAMES);
    assert(out[0]==expected); /* Switch begins with the old waveform, never an abrupt zero. */
    assert(out[TC_CROSSFADE_FRAMES]==tc_sound_sample(TC_FX_SUBTRACT,TC_CROSSFADE_FRAMES));
    for(int k=0;k<50;k++)tc_mixer_render(&mixer,out,TC_AUDIO_FRAMES);
    assert(!tc_mixer_busy(&mixer));
    for(unsigned i=0;i<TC_AUDIO_FRAMES;i++)assert(out[i]==0);
    puts("Tally sound: ten distinct bounded cues, click-free envelopes and silent tails PASS");
}
