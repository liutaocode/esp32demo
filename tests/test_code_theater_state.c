#include "code_theater_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void ban(ct_state_t *s,uint32_t start)
{
    for (unsigned i=0;i<CT_BURST_LIMIT;i++)
        assert(ct_press(s,start+i*200)==(i+1==CT_BURST_LIMIT));
    assert(s->access==CT_BANNED && s->ban_left==10000);
}
int main(void)
{
    ct_state_t a,b; ct_init(&a,123); ct_init(&b,123);
    unsigned seen=0;
    for (unsigned i=0;i<360000;i++) {
        ct_tick(&a,100); ct_tick(&b,100);
        assert(!memcmp(&a,&b,sizeof a)); seen|=1u<<a.phase;
        assert(a.access==CT_ACCESS_OK && !a.has_press && a.burst==0);
        assert(a.count<=CT_ROWS && a.project<CT_TASKS && a.task<CT_TASKS);
    }
    assert(a.completed>500 && ct_card_count(&a)==12);
    assert((seen&((1u<<(CT_COMPACT+1))-1))==((1u<<(CT_COMPACT+1))-1));
    ct_init(&a,3);
    for (unsigned i=0;i<500;i++) {
        unsigned old=a.project; ct_next(&a); assert(a.project!=old);
        ct_interrupt(&a); assert(a.phase==CT_INTERRUPTED);
        ct_interrupt(&a); assert(a.phase==CT_READ || a.phase==CT_PLAN);
    }
    ct_interrupt(&a);
    for (unsigned i=0;i<6;i++) ct_tick(&a,1000);
    assert(a.phase!=CT_INTERRUPTED);
    a.phase=CT_WAIT; a.elapsed=0; ct_choose(&a,true); assert(a.choice==2 && a.phase==CT_REPAIR);
    a.phase=CT_WAIT; ct_choose(&a,false); assert(a.choice==1 && a.phase==CT_REPAIR);
    a.phase=CT_WAIT; a.elapsed=5999; a.choice=0; ct_tick(&a,1); assert(a.phase==CT_REPAIR && a.choice==0);
    ct_init(&a,5);
    for (unsigned i=0;i<6;i++) { assert(!ct_press(&a,i*300)); ct_next(&a); }
    assert(ct_press(&a,1800)); /* Changing project cannot evade a rapid streak. */
    ct_state_t banned=a; ct_boost(&a); ct_next(&a); ct_interrupt(&a); ct_choose(&a,true);
    assert(!memcmp(&a,&banned,sizeof a));
    assert(!ct_appeal(&a,1) && a.access==CT_APPEAL_FAILED);
    assert(!ct_appeal(&a,0) && a.access==CT_APPEAL_FAILED); /* One appeal only. */
    ct_tick(&a,9999); assert(a.access==CT_APPEAL_FAILED && a.ban_left==1);
    ct_tick(&a,1); assert(a.access==CT_ACCESS_OK && a.phase==CT_RECOVER && a.burst==0);
    assert(!ct_press(&a,20000) && a.burst==1);
    ct_init(&a,6);
    for (unsigned i=0;i<100;i++) assert(!ct_press(&a,i*(CT_BURST_GAP_MS+1)));
    ct_init(&a,6); ban(&a,UINT32_MAX-500); /* Timestamp wrap. */
    a.paused=true; ct_tick(&a,10000); assert(a.access==CT_ACCESS_OK && !a.paused);
    unsigned passed=0;
    for (unsigned coin=0;coin<10000;coin++) {
        ct_init(&a,coin+1); ban(&a,1000);
        ct_tick(&a,2000); passed+=ct_appeal(&a,coin);
        if (coin&1) assert(a.access==CT_APPEAL_FAILED && a.ban_left==8000);
        else assert(a.access==CT_ACCESS_OK && a.burst==0 && a.phase==CT_RECOVER);
    }
    assert(passed==5000);
    ct_init(&a,1); a.paused=true; b=a; ct_tick(&a,1000); assert(!memcmp(&a,&b,sizeof a));
    a.paused=false; a.completed=a.boosts=9999;
    for (unsigned i=0;i<1000;i++) { ct_tick(&a,1000); ct_boost(&a); }
    assert(a.completed==9999 && a.boosts==9999);
    puts("Code Theater: ten idle hours without ban, 17 states, interruptions, raw-click thresholds, 50/50 appeals, exact 10s recovery PASS");
}
