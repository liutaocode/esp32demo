#include "pb_game.h"
unsigned pb_sound_samples(pb_sound s){return s==PB_SILENT?0:s==PB_CLEAR?4800:s==PB_RELOAD?3200:1600;}
void pb_pcm(pb_sound s,unsigned off,int16_t *out,unsigned n)
{
    unsigned total=pb_sound_samples(s);
    for(unsigned i=0;i<n;i++){
        unsigned t=off+i;if(t>=total){out[i]=0;continue;}
        unsigned freq=s==PB_SHOT?120+(total-t)/5:s==PB_BREAK?660:s==PB_HIT?880:s==PB_HURT?150:s==PB_RELOAD?440:660+(t/1600)*220;
        int wave=((t*freq/8000)%2)?1:-1;
        int noise=(int)(((t*1103515245U+12345U)>>16)&65535)-32768;
        int value=(s==PB_SHOT||s==PB_BREAK)?noise/5:wave*4200;
        /* Integer envelopes and deterministic synthesis; no recordings or heap buffers. */
        unsigned attack=t<80?t:80;
        out[i]=(int16_t)(value*(int)(total-t)/(int)total*(int)attack/80);
    }
}
