#include "cat/cat_state.h"
#include "cat/cat_control.h"
#include "cat/cat_sound.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
static void tick(cat_control_t *c,unsigned ms) { cat_control_tick(c,ms); }
static void tap(cat_control_t *c,unsigned key) { cat_control_key(c,key,0); cat_control_key(c,key,1); }
int main(void) {
    cat_control_t c; cat_control_init(&c,73,NULL);
    assert(c.cat.pose==CAT_SIT && c.cat.profile.sound && cat_control_brightness(&c)==75);
    tap(&c,2); assert(c.cat.pose==CAT_APPROACH && c.cat.profile.memories==1);
    tick(&c,1800); assert(c.cat.pose==CAT_SNUGGLE);
    tick(&c,14000); assert(c.cat.pose==CAT_SLEEP);
    for(unsigned i=0;i<3;i++) { tick(&c,500); tap(&c,0); }
    assert(c.cat.pose==CAT_ROLL && (c.cat.profile.memories & 6)==6);
    tick(&c,7000); assert(c.cat.pose==CAT_SNUGGLE);
    tick(&c,8001); tap(&c,0); assert(c.cat.pose==CAT_PAT && c.cat.pet_chain==1);
    tick(&c,500); tap(&c,1); assert(c.cat.pose==CAT_PLAY);
    cat_pose_t before=c.cat.pose;
    cat_control_key(&c,2,0); cat_control_key(&c,2,3);
    assert(c.page==1 && c.cat.pose==before); /* Holding confirm does not call first. */
    for(unsigned row=0;row<6;row++) {
        c.selection=row; tap(&c,2);
        if(row==5) { assert(c.page==2); tap(&c,0); assert(c.page==1); }
    }
    assert(c.cat.profile.name==1 && c.cat.profile.coat==1 && c.cat.profile.toy==1 && c.cat.profile.sound==0 && c.cat.profile.pocket==1);
    cat_control_key(&c,2,3); assert(c.page==0);
    tick(&c,180000); assert(cat_control_brightness(&c)==0);
    before=c.cat.pose; tap(&c,0); assert(cat_control_brightness(&c)==75 && c.cat.pose==before);
    tick(&c,180000); before=c.cat.pose;
    cat_control_key(&c,2,0); cat_control_key(&c,2,3); assert(c.page==0 && c.cat.pose==before);
    tap(&c,2); assert(c.cat.pose==CAT_APPROACH);
    cat_control_init(&c,73,NULL);
    cat_control_key(&c,2,0); cat_control_key(&c,2,0);
    assert(cat_control_key(&c,2,2)==CAT_DANCE && c.cat.pose==CAT_DANCING);
    tick(&c,31999); assert(c.cat.pose==CAT_DANCING);
    tick(&c,1); assert(c.cat.pose==CAT_STRETCH);
    for(unsigned key=0;key<3;key++) {
        tick(&c,1000); cat_control_key(&c,2,0); cat_control_key(&c,2,2);
        assert(c.cat.pose==CAT_DANCING);
        cat_control_key(&c,key,0); assert(c.cat.pose==CAT_STRETCH);
        cat_control_key(&c,key,0); cat_control_key(&c,key,2); cat_control_key(&c,key,3);
        assert(c.cat.pose==CAT_STRETCH && c.page==0);
    }
    tick(&c,1000); c.cat.profile.sound=0;
    cat_control_key(&c,2,0); assert(cat_control_key(&c,2,2)==CAT_SILENT && c.cat.pose==CAT_DANCING);
    tick(&c,32000); assert(c.cat.pose==CAT_STRETCH);
    c.cat.profile.name=1; c.cat.profile.pocket=1;
    uint8_t data[CAT_SAVE_BYTES]; cat_profile_t restored;
    cat_encode(&c.cat.profile,data); assert(cat_decode(&restored,data,sizeof data));
    assert(restored.name==1 && restored.pocket==1 && restored.memories==c.cat.profile.memories);
    for(unsigned i=0;i<CAT_SAVE_BYTES;i++) { data[i]^=1; assert(!cat_decode(&restored,data,sizeof data)); data[i]^=1; }
    for(unsigned n=0;n<CAT_SAVE_BYTES;n++) assert(!cat_decode(&restored,data,n));
    assert(!cat_decode(&restored,data,CAT_SAVE_BYTES+1));
    cat_control_init(&c,7,&restored); assert(c.cat.profile.memories==restored.memories);
    unsigned poses=0;
    for(unsigned t=0;t<7200;t++) { tick(&c,1000); poses|=1u<<c.cat.pose; assert(c.cat.pose<CAT_POSES); }
    assert((poses&(1u<<CAT_GIFT)) && (poses&(1u<<CAT_SLEEP)) && (c.cat.profile.memories&32));
    assert(c.cat.profile.minutes>=120);
    for(unsigned seed=0;seed<100;seed++) {
        cat_control_init(&c,seed,NULL);
        for(unsigned n=0;n<20000;n++) {
            tick(&c,17+(n%100));
            if(n%13==0) tap(&c,(n/13)%3);
            if(n%131==0) cat_control_key(&c,2,3);
            assert(c.cat.pose<CAT_POSES && c.selection<6 && c.page<3);
            assert(c.cat.profile.name<4 && c.cat.profile.coat<3 && c.cat.profile.toy<3 && c.cat.profile.memories<64);
        }
    }
    c.cat.profile.minutes=UINT32_MAX; tick(&c,UINT32_MAX); assert(c.cat.profile.minutes==UINT32_MAX);
    for(cat_sound_t k=CAT_MEW;k<=CAT_CHIRP;k++) {
        cat_voice_t v={0}; cat_voice_start(&v,k); uint32_t expected=v.count;
        unsigned count=0,nonzero=0; int16_t samples[160],first=1,last=1;
        while(v.sample<v.count) {
            size_t n=cat_voice_render(&v,samples,160); assert(n>0 && n<=160);
            if(!count) first=samples[0]; last=samples[n-1]; count+=(unsigned)n;
            for(size_t i=0;i<n;i++) { assert(samples[i]>=-22000 && samples[i]<=22000); nonzero+=samples[i]!=0; }
        }
        assert(count==expected && nonzero>100 && first==0 && last==0);
        assert(cat_voice_render(&v,samples,160)==0);
    }
    cat_voice_t v={0}; int16_t chunk[160];
    cat_voice_start(&v,CAT_PURR); cat_voice_render(&v,chunk,160);
    uint32_t offset=v.sample; cat_voice_start(&v,CAT_PURR);
    assert(v.sample==offset && v.pending==CAT_PURR);
    for(unsigned i=0;i<100;i++) cat_voice_start(&v,CAT_PURR);
    uint32_t total=0; while(cat_voice_render(&v,chunk,160)) { total+=160; assert(total<200000); }
    assert(total>=100000 && v.pending==CAT_SILENT);
    cat_voice_start(&v,CAT_PURR); cat_voice_render(&v,chunk,1600/10);
    cat_voice_start(&v,CAT_MEW); cat_voice_stop(&v);
    assert(cat_voice_render(&v,chunk,160)==160);
    assert(cat_voice_render(&v,chunk,160)==160 && chunk[159]==0);
    assert(cat_voice_render(&v,chunk,160)==0 && v.pending==CAT_SILENT);
    for(cat_sound_t k=CAT_MEW;k<=CAT_CHIRP;k++) {
        cat_voice_t random_voice={.rng=97}; unsigned seen=0; int previous=-1;
        for(unsigned j=0;j<30;j++) {
            cat_voice_start(&random_voice,k);
            unsigned clip=random_voice.clip;
            assert(clip>=(unsigned)(k-1)*3 && clip<(unsigned)k*3 && (int)clip!=previous);
            seen|=1u<<(clip%3); previous=(int)clip;
            while(cat_voice_render(&random_voice,chunk,160)) {}
        }
        assert(seen==7);
    }
    cat_voice_start(&v,CAT_PURR); cat_voice_render(&v,chunk,160);
    cat_voice_start(&v,CAT_CHIRP);
    cat_voice_render(&v,chunk,160); cat_voice_render(&v,chunk,160); cat_voice_render(&v,chunk,160);
    assert(v.kind==CAT_CHIRP && v.sample==160);
    cat_voice_stop(&v); while(cat_voice_render(&v,chunk,160)) {}
    cat_voice_start(&v,CAT_DANCE); assert(v.loop && v.clip==9);
    for(unsigned i=0;i<3300;i++) assert(cat_voice_render(&v,chunk,160)==160);
    assert(v.loop && v.kind==CAT_DANCE);
    cat_voice_stop(&v); cat_voice_render(&v,chunk,160); cat_voice_render(&v,chunk,160);
    assert(cat_voice_render(&v,chunk,160)==0);
    puts("Cat: state, wake/long/double input, persistence corruption, two-hour idle, 2M mixed steps, sound bounds PASS");
}
