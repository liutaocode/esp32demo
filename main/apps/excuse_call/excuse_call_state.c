#include "excuse_call_state.h"
void ec_init(ec_state_t *s) { *s=(ec_state_t){0}; }
static void finish(ec_state_t *s, bool timeout)
{
    s->ringing=false; s->timed_out=timeout;
    s->calls=(s->calls+1)%10000;
    s->caller=(s->caller+1)%EC_CALLERS;
}
void ec_tick(ec_state_t *s,uint64_t now)
{ if (s->ringing && now>=s->deadline) finish(s,true); }
void ec_confirm(ec_state_t *s,uint64_t now)
{
    if (s->ringing) finish(s,false);
    else { s->ringing=true; s->timed_out=false; s->deadline=now+EC_DURATION_MS; }
}
void ec_select(ec_state_t *s,int direction)
{ s->ring=(s->ring+EC_RINGS+(direction<0 ? -1 : 1))%EC_RINGS; }
unsigned ec_seconds(const ec_state_t *s,uint64_t now)
{ return s->ringing && now<s->deadline ? (unsigned)((s->deadline-now+999)/1000) : 0; }
const char *ec_ring_name(unsigned n)
{
    static const char *names[]={"标准来电","清亮木琴","温暖木琴","轻柔来电","明快来电"};
    return names[n%EC_RINGS];
}
const char *ec_caller_name(unsigned n)
{
    static const char *names[]={"家人","同事","朋友"};
    return names[n%EC_CALLERS];
}
