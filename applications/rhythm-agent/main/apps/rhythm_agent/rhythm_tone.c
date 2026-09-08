#include "rhythm_audio.h"
static const int16_t sine[256]={
#include "rhythm_sine.inc"
};
int16_t ra_tone(unsigned key,unsigned sample) {
    if(key>2 || sample>=2240) return 0;
    /* Fixed-point synthesis keeps the audio worker cheap on the C3. */
    static const unsigned step[]={1606,2700,4286};
    unsigned phase=(sample*step[key])&65535;
    int32_t wave=sine[phase>>8]+sine[((phase*2)&65535)>>8]*15/100;
    int32_t decay=(2240-(int32_t)sample)*1024/2240;
    int32_t env=decay*decay/1024;
    if(sample<80)env=env*(int32_t)sample/80;
    return (int16_t)(wave*env/1024*13/32);
}
