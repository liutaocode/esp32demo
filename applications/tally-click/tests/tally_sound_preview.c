/* Export the production synthesis for review listening, without a device. */
#include "tally_sound.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv)
{
    if(argc!=2)return 1;
    tc_feedback f=(tc_feedback)atoi(argv[1]);
    for(unsigned i=0;i<tc_sound_length(f);i++) {
        uint16_t v=(uint16_t)tc_sound_sample(f,i);
        unsigned char b[]={(unsigned char)v,(unsigned char)(v>>8)};
        if(fwrite(b,1,2,stdout)!=2)return 1;
    }
    return 0;
}
