#include "bean_state.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    bean_t s; bean_init(&s,1,0);
    bean_tick(&s,200,true,false); assert(s.phase==BEAN_LISTEN);
    bean_tick(&s,1000,true,false); bean_tick(&s,1999,false,false);assert(s.phase==BEAN_LISTEN);
    bean_tick(&s,2000,false,false);assert(s.phase==BEAN_THINK);
    unsigned wait=s.think_ms;assert(wait>=1200 && wait<=3200);
    bean_tick(&s,2000+wait-1,false,false);assert(s.phase==BEAN_THINK);
    bean_tick(&s,2000+wait,false,false);assert(s.phase==BEAN_TALK);
    bean_tick(&s,10000,true,true);assert(s.phase==BEAN_TALK); /* Speaker cannot trigger itself. */
    bean_tick(&s,10001,false,false);assert(s.phase==BEAN_IDLE);
    unsigned waits_seen=0, last_wait=0;
    int previous=-1;
    for(unsigned i=0;i<500;i++){bean_key(&s,2,11000+i*500);assert(s.line!=previous);previous=s.line;assert(s.think_ms>=1200 && s.think_ms<=3200);waits_seen+=last_wait!=s.think_ms;last_wait=s.think_ms;}
    assert(waits_seen>400);
    for(unsigned t=0;t<5000;t++)assert(bean_ear_raise(t)<=38);
    assert(bean_ear_raise(0)==0 && bean_ear_raise(280)==36);
    bean_init(&s,99,0);
    for(unsigned i=0;i<3;i++)bean_key(&s,0,1000+i*1000);
    assert(s.face==3 && (s.discovered&8));
    for(unsigned i=0;i<5;i++)bean_key(&s,1,5000+i*1000);
    assert(s.face==7);
    bean_key(&s,0,10000);assert(s.face==6);
    bean_init(&s,1,0);
    for(unsigned t=0;t<=16000;t+=100)bean_tick(&s,t,true,false);
    assert(s.phase==BEAN_LISTEN && s.noise);
    bean_tick(&s,17000,false,false);assert(s.phase==BEAN_IDLE); /* Sustained noise gets no random reply. */
    bean_init(&s,3,0);s.auto_listen=false;
    bean_tick(&s,1000,true,false);assert(s.phase==BEAN_IDLE);
    bean_tick(&s,60000,false,false);assert(s.phase==BEAN_THINK);
    bean_tick(&s,60000+s.think_ms,false,false);assert(s.phase==BEAN_TALK);
    bean_tick(&s,67000,false,false);assert(s.phase==BEAN_IDLE);
    bean_tick(&s,120000,false,false);assert(s.phase==BEAN_REST && s.face==4);
    bean_key(&s,0,121000);assert(s.phase==BEAN_THINK && s.face==1);
    bean_init(&s,2,0);bean_key(&s,2,10);bean_tick(&s,200,true,false);assert(s.phase==BEAN_LISTEN);
    bean_vad_t v={0};
    for(unsigned i=0;i<100;i++)assert(!bean_vad_tick(&v,120));
    assert(!bean_vad_tick(&v,4000)); /* Single impulse is not a voice. */
    for(unsigned i=0;i<10;i++)assert(!bean_vad_tick(&v,100));
    for(unsigned i=0;i<7;i++)assert(!bean_vad_tick(&v,4000));
    assert(bean_vad_tick(&v,4000));
    for(unsigned i=0;i<5;i++)assert(bean_vad_tick(&v,100));
    assert(!bean_vad_tick(&v,100));
    assert(bean_count(255)==8 && bean_count(0)==0);
    puts("Mouthy Bean state: silence timing, noise suppression, no-repeat, combos, rest and VAD PASS");
}
