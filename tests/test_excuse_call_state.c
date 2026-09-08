#include "excuse_call_state.h"
#include "excuse_call_audio.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
int main(void)
{
    ec_state_t s; ec_init(&s); assert(!s.ringing && !s.calls);
    ec_select(&s,-1); assert(s.ring==4); ec_select(&s,1); assert(s.ring==0);
    ec_confirm(&s,100); assert(s.ringing && s.deadline==60100 && ec_seconds(&s,100)==60);
    ec_tick(&s,60099); assert(s.ringing && ec_seconds(&s,60099)==1);
    ec_select(&s,1); assert(s.deadline==60100);
    ec_tick(&s,60100); assert(!s.ringing && s.timed_out && s.calls==1 && s.caller==1);
    ec_tick(&s,999999); assert(s.calls==1);
    ec_confirm(&s,1000000); ec_confirm(&s,1000001); assert(!s.ringing && !s.timed_out && s.calls==2);
    ec_confirm(&s,UINT64_C(4294967000)); ec_tick(&s,UINT64_C(4295026999)); assert(s.ringing);
    ec_tick(&s,UINT64_C(4295027000)); assert(!s.ringing && s.caller==0);
    for (unsigned n=0;n<10000;n++) { ec_confirm(&s,n); ec_confirm(&s,n); }
    assert(s.calls==3);
    static int16_t pcm[EC_LOOP_SAMPLES];
    uint64_t hashes[5]={0};
    for (unsigned ring=0;ring<5;ring++) {
        ec_pcm(ring,0,pcm,EC_LOOP_SAMPLES); uint64_t ring_energy=0,buzz_energy=0;
        for (unsigned i=0;i<EC_LOOP_SAMPLES;i++) {
            assert(abs(pcm[i])<=26500);
            if(i<32000) ring_energy+=(int64_t)pcm[i]*pcm[i];
            if(i>=44800 && i<60800) buzz_energy+=(int64_t)pcm[i]*pcm[i];
            hashes[ring]=hashes[ring]*31+(uint16_t)pcm[i];
        }
        assert(ring_energy>UINT64_C(800)*800*32000);
        assert(buzz_energy>UINT64_C(800)*800*16000);
        assert(!pcm[0] && !pcm[EC_LOOP_SAMPLES-1]);
        int16_t chunk[160]; ec_pcm(ring,EC_LOOP_SAMPLES-80,chunk,160);
        for(unsigned i=0;i<160;i++) assert(chunk[i]==pcm[(EC_LOOP_SAMPLES-80+i)%EC_LOOP_SAMPLES]);
        for(unsigned j=0;j<ring;j++) assert(hashes[ring]!=hashes[j]);
    }
    puts("Excuse Call: fixed deadline, cancellation, recorded PCM, buzz energy, loop seam and five distinct variants PASS");
}
