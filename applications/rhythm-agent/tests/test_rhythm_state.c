#include "rhythm_state.h"
#include "rhythm_audio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void perfect(ra_game_t *g,int64_t origin) {
    ra_ready(g);
    for(unsigned i=0;i<g->pattern.count;i++) assert(ra_press(g,g->pattern.keys[i],origin+g->pattern.at[i]));
    assert(g->phase==RA_FEEDBACK && g->passed && g->accuracy==100);
}
int main(void) {
    ra_game_t g;
    for(unsigned mode=0;mode<2;mode++) for(unsigned seed=0;seed<2000;seed++) {
        ra_start(&g,seed,mode);
        for(unsigned door=0;door<RA_DOORS;door++) {
            assert(g.door==door && g.pattern.count>=3 && g.pattern.count<=8);
            ra_pattern_t same;ra_pattern(&same,seed,door,mode);assert(memcmp(&same,&g.pattern,sizeof(same))==0);
            for(unsigned i=0;i<g.pattern.count;i++) {
                assert(g.pattern.keys[i]<3);
                if(i) assert(g.pattern.at[i]-g.pattern.at[i-1]>=500);
            }
            perfect(&g,1000000000000LL);ra_next(&g);
        }
        assert(g.phase==RA_FINISHED && g.completed && g.score==800 && g.lives==3);
        assert(!ra_press(&g,0,0));ra_next(&g);assert(g.score==800);
    }
    ra_start(&g,42,RA_CHALLENGE);
    for(unsigned life=3;life>0;life--) {
        ra_ready(&g);assert(ra_press(&g,(g.pattern.keys[0]+1)%3,1000));
        ra_tick(&g,100000);assert(g.phase==RA_FEEDBACK && !g.passed && g.lives==life-1);
        assert(g.score==0);ra_tick(&g,200000);assert(g.lives==life-1);ra_next(&g);
    }
    assert(g.phase==RA_FINISHED && !g.completed);
    ra_start(&g,4,RA_TRAIN);ra_ready(&g);ra_tick(&g,9999999);assert(g.phase==RA_READY);
    assert(!ra_press(&g,3,100));assert(!ra_press(&g,0,-1));
    assert(ra_press(&g,g.pattern.keys[0],1000));assert(!ra_press(&g,0,1050));assert(g.index==1);
    assert(!ra_press(&g,0,999));ra_tick(&g,999999);assert(g.phase==RA_INPUT && g.index==1);
    for(unsigned i=1;i<g.pattern.count;i++) assert(ra_press(&g,(g.pattern.keys[i]+1)%3,1000000+i*1000));
    assert(!g.passed && g.lives==3);
    ra_next(&g);assert(g.door==0 && g.attempts==1);
    perfect(&g,1000);ra_next(&g);assert(g.score==100 && g.door==1);
    ra_ready(&g);assert(ra_press(&g,g.pattern.keys[0],1000));ra_pattern_t saved=g.pattern;ra_replay(&g);
    assert(g.index==0 && g.phase==RA_DEMO && memcmp(&saved,&g.pattern,sizeof(saved))==0 && g.score==100);
    ra_ready(&g);assert(ra_press(&g,g.pattern.keys[0],2000));
    assert(ra_press(&g,g.pattern.keys[1],2000+g.pattern.at[1]+111));assert(g.last_mark==RA_GOOD);
    assert(ra_press(&g,g.pattern.keys[2],2000+g.pattern.at[2]-221));assert(g.last_mark==RA_EARLY);
    assert(g.accuracy==66 && g.passed);
    ra_start(&g,42,RA_CHALLENGE);ra_ready(&g);
    for(unsigned i=0;i<g.pattern.count;i++) ra_press(&g,g.pattern.keys[i],1000+g.pattern.at[i]+i*161);
    assert(!g.passed && g.lives==2);
    ra_start(&g,42,RA_CHALLENGE);ra_ready(&g);
    for(unsigned i=0;i<g.pattern.count;i++) ra_press(&g,g.pattern.keys[i],1000+g.pattern.at[i]+i*160);
    assert(g.passed && g.lives==3);
    /* A correct but slow four-note training attempt used to get stuck at 43%. */
    ra_start(&g,4198,RA_TRAIN);g.door=2;ra_pattern(&g.pattern,g.seed,g.door,g.mode);ra_ready(&g);
    for(unsigned i=0;i<g.pattern.count;i++) {
        ra_tick(&g,1000+(int64_t)i*5000);
        assert(ra_press(&g,g.pattern.keys[i],1000+(int64_t)i*5000));
    }
    assert(g.all_keys && g.accuracy==43 && g.passed && g.lives==3);
    ra_next(&g);assert(g.door==3 && g.phase==RA_DEMO);
    /* Consistent slight tempo drift must not accumulate into a failed lock. */
    ra_start(&g,42,RA_CHALLENGE);ra_ready(&g);
    for(unsigned i=0;i<g.pattern.count;i++) {
        int64_t ms=1000+g.pattern.at[i]+i*120;
        ra_tick(&g,ms);assert(ra_press(&g,g.pattern.keys[i],ms));
    }
    assert(g.passed && g.accuracy==81 && g.lives==3);
    ra_start(&g,42,RA_CHALLENGE);ra_ready(&g);ra_press(&g,g.pattern.keys[0],1000);
    assert(!ra_press(&g,g.pattern.keys[1],999999));assert(g.phase==RA_FEEDBACK && g.lives==2);
    ra_replay(&g);assert(g.phase==RA_FEEDBACK && g.lives==2);
    for(unsigned key=0;key<3;key++) {
        int peak=0;long sum=0;
        for(unsigned i=0;i<2240;i++) {int v=ra_tone(key,i);if(v<0)v=-v;if(v>peak)peak=v;sum+=v;}
        assert(peak>5000 && peak<16000 && sum>1000000);
        assert(ra_tone(key,0)==0 && ra_tone(key,2240)==0 && ra_tone(key,10000)==0);
    }
    assert(ra_tone(5,100)==0);
    puts("Rhythm state/tone: 32,000 seeded locks, scoring boundaries, retries, lives, timeout, timestamps and waveform bounds PASS");
}
