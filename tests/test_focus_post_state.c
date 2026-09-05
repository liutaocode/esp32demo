#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "focus_post_state.h"
static int64_t begin(fp_state_t *s, int64_t t) { fp_ok(s,t); fp_tick(s,t+850); return t+850; }
int main(void)
{
    fp_state_t s; fp_init(&s); assert(s.page == FP_HOME);
    for (unsigned pace=0; pace<3; pace++) for (unsigned seed=0; seed<1000; seed++) {
        s.pace=pace; fp_start(&s,seed,false);
        assert(s.target<3 && fp_duration(&s)==4000-pace*1000);
        unsigned go=0;
        for(unsigned i=0;i<12;i++) { assert(s.cards[i]<3); go+=s.cards[i]==s.target; }
        assert(go==6);
        uint8_t cards[12]; memcpy(cards,s.cards,12);
        int64_t t=begin(&s,0);
        for(unsigned r=0;r<12;r++) {
            assert(s.page==FP_VISITOR && s.round==r);
            fp_pause(&s,t+250); assert(s.page==FP_PAUSE);
            int64_t left=s.remaining; fp_tick(&s,t+100000); assert(s.page==FP_PAUSE);
            fp_resume(&s,t+100000); assert(s.page==FP_READY);
            fp_tick(&s,t+100850); assert(s.deadline==t+100850+left);
            t+=100850;
            if(fp_current(&s)==s.target) fp_ok(&s,t+100);
            else fp_tick(&s,s.deadline);
            assert(s.page==FP_FEEDBACK && s.correct);
            t=s.deadline+1000; fp_ok(&s,t); fp_tick(&s,t+850); t+=850;
        }
        assert(s.page==FP_RESULT && s.delivered==6 && s.waited==6 && !s.misses && !s.slips);
        fp_start(&s,seed,false); assert(!memcmp(cards,s.cards,12));
    }
    /* Tutorial target never expires, and premature delivery retries waiting. */
    fp_start(&s,42,true); int64_t t=begin(&s,0);
    fp_tick(&s,999999); assert(s.page==FP_PRACTICE);
    fp_ok(&s,1000000); assert(s.correct && !s.delivered);
    t=begin(&s,1001000); assert(s.practice==1);
    fp_ok(&s,t+100); assert(!s.correct);
    t=begin(&s,t+1000); assert(s.practice==1);
    fp_tick(&s,s.deadline); assert(s.correct && !s.waited);
    fp_ok(&s,s.deadline+1000); assert(s.page==FP_RULE && !s.tutorial);
    /* Exactly-deadline delivery is late, and may not advance feedback. */
    fp_start(&s,42,false); t=begin(&s,0); s.cards[0]=s.target;
    fp_ok(&s,s.deadline); assert(s.page==FP_FEEDBACK && s.misses==1);
    fp_ok(&s,s.opened-1); assert(s.page==FP_FEEDBACK);
    fp_start(&s,42,false); t=begin(&s,0); s.cards[0]=s.target;
    fp_ok(&s,s.deadline-1); assert(s.correct && s.delivered==1);
    fp_start(&s,42,false); t=begin(&s,0); s.cards[0]=(s.target+1)%3;
    fp_ok(&s,t+1); assert(s.slips==1 && !s.correct);
    /* Always waiting and always pressing cap the appropriate category at six. */
    for(unsigned strategy=0;strategy<2;strategy++) {
        fp_start(&s,321,false); t=begin(&s,0);
        for(unsigned i=0;i<12;i++) {
            if(strategy) fp_ok(&s,t+1); else fp_tick(&s,s.deadline);
            t=s.deadline+1000; fp_ok(&s,t); fp_tick(&s,t+850); t+=850;
        }
        assert(s.delivered+s.waited==6);
        assert(strategy ? s.slips==6 : s.misses==6);
    }
    /* Delayed render starts a fresh visible window; pause after expiry settles. */
    fp_start(&s,1,false); fp_ok(&s,0); fp_tick(&s,90000);
    assert(s.opened==90000 && s.deadline==90000+fp_duration(&s));
    fp_pause(&s,s.deadline+1); assert(s.resume_page==FP_FEEDBACK);
    /* Near-expiry resume provides a release cue without losing reaction time.
       Pausing the resume cue must preserve both cue time and response time. */
    fp_start(&s,10,false); t=begin(&s,0); s.cards[0]=s.target;
    fp_pause(&s,s.deadline-50); assert(s.remaining==50);
    fp_resume(&s,10000); assert(s.page==FP_READY && s.resume_window==50);
    fp_pause(&s,10200); assert(s.remaining==650);
    fp_resume(&s,20000); assert(s.page==FP_READY && s.deadline==20650);
    fp_tick(&s,20650); assert(s.deadline==20700);
    fp_ok(&s,20690); assert(s.correct && s.delivered==1);
    puts("Focus Post state: 3000 courses, balance, timing, tutorial, pause and scoring PASS");
}
