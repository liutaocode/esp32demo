#include "night_call_state.h"
#include "night_call_audio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool seen[64][64];
static unsigned reached;
static void walk(nc_state_t s,unsigned depth) {
    assert(nc_state_valid(&s)); assert(depth<100);
    if(seen[s.node][s.clues]) return;
    seen[s.node][s.clues]=true;
    for(unsigned c=0;c<2;c++) {
        nc_state_t t=s; int end=-1; nc_result_t r=nc_choose(&t,c,&end);
        assert(nc_state_valid(&t)); assert((t.clues&s.clues)==s.clues);
        if(r==NC_END) { assert(end>=0 && end<7 && !t.active); reached|=1u<<end; }
        else if(r==NC_STORY) walk(t,depth+1);
        else assert(r==NC_HOME);
    }
}
static int route(nc_state_t *s,unsigned chapter,const char *choices) {
    nc_begin(s,chapter); int ending=-1;
    while(*choices) nc_choose(s,(unsigned)(*choices++-'0'),&ending);
    return ending;
}
int main(void) {
    assert(nc_node_count<64 && nc_node_count+NC_ENDINGS==NC_AUDIO_COUNT);
    nc_state_t s; nc_init(&s); assert(nc_state_valid(&s) && !s.active && s.volume==2);
    int end=-1; assert(nc_choose(&s,0,&end)==NC_IGNORED);
    for(unsigned chapter=0;chapter<3;chapter++) {
        nc_init(&s); nc_begin(&s,chapter); memset(seen,0,sizeof(seen)); walk(s,0);
    }
    assert(reached==63); /* The seventh ending cannot be reached from an empty archive. */
    nc_init(&s); assert(route(&s,0,"000000")==2); assert(s.clues&1);
    assert(route(&s,1,"00000")==4); assert((s.clues&12)==12);
    assert(route(&s,2,"00000")==6); assert(s.endings&(1u<<6));
    assert(nc_count(s.clues)==6);
    assert(route(&s,0,"110")==-1); /* Detouring does not silently wipe collections. */
    uint8_t packed[NC_SAVE_SIZE]; nc_encode(&s,packed); nc_state_t restored; nc_init(&restored);
    assert(nc_decode(&restored,packed,sizeof(packed))); assert(restored.node==s.node && restored.endings==s.endings && restored.active);
    for(unsigned i=0;i<NC_SAVE_SIZE;i++) {
        packed[i]^=1; nc_state_t before=restored;
        assert(!nc_decode(&restored,packed,sizeof(packed))); assert(!memcmp(&before,&restored,sizeof(before))); packed[i]^=1;
    }
    assert(!nc_decode(&restored,packed,sizeof(packed)-1));
    s.chapter=2; nc_encode(&s,packed); assert(!nc_decode(&restored,packed,sizeof(packed)));
    nc_init(&s); nc_begin(&s,2); for(int i=0;i<5;i++) nc_choose(&s,0,&end);
    assert(s.active && s.node==nc_missing_node); assert(nc_choose(&s,0,&end)==NC_HOME && s.active);
    size_t offset=0;
    for(unsigned i=0;i<NC_AUDIO_COUNT;i++) { assert(nc_clip_valid(&nc_clips[i],nc_audio_bytes)); assert(nc_clips[i].offset==offset); offset+=nc_clips[i].size; }
    assert(offset==nc_audio_bytes); nc_clip_t bad=nc_clips[0]; bad.offset=(uint32_t)nc_audio_bytes; assert(!nc_clip_valid(&bad,nc_audio_bytes));
    puts("Night Call: all branches/endings, true-finale prerequisites, loop exploration, save corruption, resume and audio bounds PASS");
}
