#include "excuse_call_audio.h"
/* Recorded marimba and real smartphone vibration, held in memory-mapped Flash.
   See assets/music/excuse_call/source/SOURCES.md for attribution and changes. */
extern const int16_t ec_pcm_bank[5][EC_LOOP_SAMPLES];
void ec_pcm(unsigned ring,uint32_t offset,int16_t *pcm,size_t count)
{
    ring%=5;
    for(size_t i=0;i<count;i++) pcm[i]=ec_pcm_bank[ring][(offset+(uint32_t)i)%EC_LOOP_SAMPLES];
}
