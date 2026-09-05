#include "ricochet_rush_state.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static void invariant(const rr_state_t *s)
{
    assert(s->balls >= 4 && s->balls <= RR_BALLS);
    assert(s->round >= 1 && s->round <= RR_ROUNDS);
    assert(s->score <= 99999);
    assert(s->angle >= -65.01f && s->angle <= 65.01f);
    for (unsigned i=0; i<RR_CELLS; i++) assert(!(s->hp[i] && s->pickup[i]));
    for (unsigned i=0; i<RR_BALLS; i++) if(s->ball[i].active) {
        assert(isfinite(s->ball[i].x) && isfinite(s->ball[i].y));
        assert(s->ball[i].x >= 1.99f && s->ball[i].x <= 196.01f);
        assert(s->ball[i].y >= 1.99f && s->ball[i].y <= 166.01f);
    }
}
static rr_state_t collision(float x, float y, float vx, float vy)
{
    rr_state_t s={0}; rr_start(&s, 5); memset(s.hp,0,sizeof(s.hp)); memset(s.pickup,0,sizeof(s.pickup));
    s.page=RR_FLIGHT; s.balls=s.launched=1;
    s.ball[0]=(rr_ball_t){x,y,vx,vy,true};
    return s;
}
static void shot(rr_state_t *s, float angle)
{
    s->angle=angle; assert(rr_fire(s));
    for(unsigned i=0;i<4000 && s->page!=RR_AIM && s->page!=RR_RESULT;i++) {
        rr_tick(s,20); invariant(s);
    }
    assert(s->page==RR_AIM || s->page==RR_RESULT);
}
int main(void)
{
    rr_state_t s={0}, t={0}; rr_start(&s, 123); rr_start(&t,123);
    assert(!memcmp(s.hp,t.hp,sizeof(s.hp)) && s.seed==123 && s.balls==6);
    rr_pause(&s); t=s; rr_tick(&s,5000); assert(!memcmp(&s,&t,sizeof(s))); rr_pause(&s);
    int direction=s.direction; rr_reverse(&s); assert(s.direction==-direction);
    float preview[34]; unsigned n=rr_preview(&s,preview,17); assert(n>0 && n<=17);
    for(unsigned i=0;i<n;i++) assert(preview[2*i]>=2 && preview[2*i]<=196 && preview[2*i+1]>=2);
    /* Hit tile zero from each face, then move away without duplicate damage. */
    const float cases[][4]={{16,24.2f,0,-155},{16,1.8f,0,155},{.8f,13,155,0},{32.2f,13,-155,0}};
    for(unsigned i=0;i<4;i++) {
        s=collision(cases[i][0],cases[i][1],cases[i][2],cases[i][3]); s.hp[0]=9;
        rr_tick(&s,5); assert(s.hp[0]==8 && s.hits==1);
        /* Wall-adjacent faces can bounce back; check the unambiguous bottom face. */
        if(i==0) { rr_tick(&s,5); assert(s.hp[0]==8 && s.ball[0].vy>0); }
    }
    s=collision(32.2f,24.2f,-110,-110); s.hp[0]=1; rr_tick(&s,5);
    assert(s.hp[0]==0 && s.hits==1 && s.cleared==1 && s.score==11);
    s=collision(16,20,0,-155); s.pickup[0]=true; rr_tick(&s,5);
    assert(!s.pickup[0] && s.gained==1); rr_tick(&s,50); assert(s.gained==1);
    s=collision(16,20,0,-155); s.balls=s.launched=RR_BALLS; s.pickup[0]=true;
    rr_tick(&s,5); assert(!s.pickup[0] && s.gained==0);
    s=collision(2.2f,100,-155,0); rr_tick(&s,5); assert(s.ball[0].vx>0);
    s=collision(195.8f,100,155,0); rr_tick(&s,5); assert(s.ball[0].vx<0);
    s=collision(80,165.8f,0,155); rr_tick(&s,5);
    assert(s.page==RR_SETTLE && s.launch_x==80 && s.returned==1);
    s=collision(80,100,155,0); s.flight_ms=17995; rr_tick(&s,5);
    assert(s.page==RR_SETTLE && s.recalled && !s.ball[0].active);
    rr_start(&s,1); s.page=RR_SETTLE; s.settle_ms=695; s.hp[36]=1; rr_tick(&s,5);
    assert(s.page==RR_RESULT && !s.won);
    rr_start(&s,1); memset(s.hp,0,sizeof(s.hp)); s.round=30; s.score=345; s.page=RR_SETTLE; s.settle_ms=695;
    rr_tick(&s,5); assert(s.page==RR_RESULT && s.won && s.new_best && s.best[0]==345);
    rr_start(&s,1); assert(s.best[0]==345 && s.score==0);
    s=collision(80,165.8f,0,155); s.balls=s.launched=24; s.returned=23; s.gained=3; rr_tick(&s,5);
    assert(s.balls==24);
    rr_start(&s,888); rr_start(&t,888);
    s.angle=t.angle=45; rr_fire(&s); rr_fire(&t);
    for(unsigned i=0;i<100;i++) rr_tick(&s,20);
    for(unsigned i=0;i<400;i++) rr_tick(&t,5);
    assert(s.score==t.score && !memcmp(s.hp,t.hp,sizeof(s.hp)) && !memcmp(s.ball,t.ball,sizeof(s.ball)));
    /* Replay the same action sequence and verify the whole generated course. */
    for(unsigned mode=0;mode<2;mode++) for(unsigned seed=1;seed<=40;seed++) {
        s=(rr_state_t){.mode=mode}; t=s; rr_start(&s,seed); rr_start(&t,seed);
        for(unsigned round=0; round<30 && s.page!=RR_RESULT;round++) {
            float angle=(float)((seed*17+round*23)%121)-60;
            shot(&s,angle); shot(&t,angle);
            assert(s.rng==t.rng && s.score==t.score && s.page==t.page && s.round==t.round);
            assert(!memcmp(s.hp,t.hp,sizeof(s.hp)) && !memcmp(s.pickup,t.pickup,sizeof(s.pickup)));
        }
        assert(s.page==RR_RESULT);
    }
    puts("Ricochet Rush state: collisions, pickups, replay, timing, bounds, cap, recall, outcomes PASS (80 courses)");
}
