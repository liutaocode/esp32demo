#include "pvz_state.h"
#include "pvz_audio.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
static void navigation(void) {
    pvz_state_t s;pvz_init(&s);
    assert(s.page==PVZ_HOME && pvz_seen_count(&s)==0);
    pvz_move(&s,-1);assert(s.menu==2);
    pvz_move(&s,1);assert(s.menu==0);
    pvz_open(&s,0,1);assert(s.page==PVZ_BROWSE && s.index==0);
    pvz_move(&s,-1);assert(s.index==15);
    pvz_move(&s,1);assert(s.index==0);
    for(int i=0;i<16;i++) pvz_move(&s,1);
    assert(pvz_seen_count(&s)==16);
    pvz_open(&s,1,1);assert(s.index==16);
    pvz_move(&s,-1);assert(s.index==23);
    for(int i=0;i<8;i++) pvz_move(&s,1);
    assert(pvz_seen_count(&s)==24);
    pvz_home(&s);assert(s.page==PVZ_HOME && pvz_seen_count(&s)==24);
    pvz_open(&s,99,1);assert(s.page==PVZ_HOME);
    pvz_move(&s,0);assert(s.menu==1);
}
static void games(void) {
    for(unsigned seed=0;seed<2000;seed++) {
        for(unsigned goal=0;goal<=PVZ_ROUNDS;goal++) {
            pvz_state_t s, twin;pvz_init(&s);pvz_init(&twin);
            pvz_open(&s,2,seed);pvz_open(&twin,2,seed);
            uint8_t original[PVZ_ROUNDS];memcpy(original,s.deck,sizeof(original));
            unsigned used=0;
            for(unsigned round=0;round<PVZ_ROUNDS;round++) {
                unsigned answer=s.deck[round];
                assert(answer<PVZ_COUNT && !(used&(1U<<answer)));used|=1U<<answer;
                assert(s.page==PVZ_QUIZ && s.choice==0 && s.round==round);
                assert(!memcmp(s.options,twin.options,sizeof(s.options)));
                unsigned count=0;
                for(unsigned i=0;i<PVZ_OPTIONS;i++) {
                    unsigned v=s.options[i];assert(v<PVZ_COUNT);
                    assert((v<PVZ_PLANTS)==(answer<PVZ_PLANTS));
                    for(unsigned j=0;j<i;j++) assert(v!=s.options[j]);
                    if(v==answer) count++;
                }
                assert(count==1);
                pvz_move(&s,-1);assert(s.choice==2);
                pvz_move(&s,1);assert(s.choice==0);
                while((s.options[s.choice]==answer)!=(round<goal)) pvz_move(&s,1);
                twin.choice=s.choice;
                pvz_confirm(&s);pvz_confirm(&twin);
                assert(s.page==PVZ_REVEAL && s.correct==(round<goal));
                unsigned score=s.score;
                pvz_move(&s,1);assert(s.score==score);
                pvz_confirm(&s);pvz_confirm(&twin);
            }
            assert(s.page==PVZ_RESULT && s.score==goal && pvz_seen_count(&s)==5);
            pvz_confirm(&s);assert(s.page==PVZ_QUIZ && s.score==0);
            assert(!memcmp(original,s.deck,sizeof(original)));
        }
    }
    assert(strcmp(pvz_rank(5),pvz_rank(3)) && strcmp(pvz_rank(3),pvz_rank(0)));
}
static void catalog_and_audio(void) {
    assert(!pvz_clip_valid(NULL,10));
    pvz_clip_t c={0,2,4,0,0};assert(pvz_clip_valid(&c,2));
    c.samples=5;assert(pvz_clip_valid(&c,2));
    c.samples=6;assert(!pvz_clip_valid(&c,2));
    c.samples=0;assert(!pvz_clip_valid(&c,2));
    c.samples=4;c.offset=UINT_MAX;assert(!pvz_clip_valid(&c,20));
    c.offset=1;assert(!pvz_clip_valid(&c,2));
    c.offset=0;c.step=89;assert(!pvz_clip_valid(&c,2));
    for(unsigned i=0;i<PVZ_COUNT;i++) {
        const pvz_entry_t *e=&pvz_catalog[i];
        assert(e->kind==(i>=PVZ_PLANTS) && e->name[0] && e->clue[0] && e->tip[0]);
        assert(e->kind ? e->cost==-1 : e->cost>=0);
        assert(!strstr(e->clue,e->name));
        for(unsigned j=0;j<i;j++) assert(strcmp(e->id,pvz_catalog[j].id));
    }
    unsigned offset=0;
    for(unsigned i=0;i<PVZ_AUDIO_COUNT;i++) {
        assert(pvz_clip_valid(&pvz_clips[i],pvz_audio_size));
        assert(pvz_clips[i].offset==offset);
        offset+=pvz_clips[i].size;
    }
    assert(offset==pvz_audio_size);
}
int main(void) {navigation();games();catalog_and_audio();puts("PVZ: 12000 games, navigation, catalog and audio bounds PASS");}
