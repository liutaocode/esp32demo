#include "listening_state.h"
#include <string.h>
static uint32_t random_next(li_state_t *s) { uint32_t x=s->rng; x^=x<<13; x^=x>>17; x^=x<<5; return s->rng=x; }
static void voice(li_state_t *s,int id) { s->voice=id; ++s->serial; s->waiting=false; }
uint16_t li_current(const li_state_t *s) { return s->count && s->pos<s->count ? s->order[s->pos] : 0; }
unsigned li_mistakes(const li_state_t *s) { unsigned n=0; for(unsigned i=0;i<LI_COUNT;i++) n+=s->progress.mistakes[i]>0; return n; }
unsigned li_heard(const li_state_t *s) { unsigned n=0; for(unsigned i=0;i<LI_COUNT;i++) n+=s->progress.heard[i]>0; return n; }
void li_init(li_state_t *s,const li_progress_t *p,uint32_t seed) {
 memset(s,0,sizeof(*s)); s->rng=seed?seed:20260906; s->voice=-1;
 s->progress=(li_progress_t){.version=LI_SAVE_VERSION,.volume=60,.minutes=10};
 if(p && (p->version==LI_SAVE_VERSION || p->version==1)) {
  s->progress=*p;
  if(p->version==1) {
   unsigned level=(p->volume>=15 && p->volume<=75 && p->volume%15==0)?p->volume/15:3;
   s->progress.volume=level*20;s->progress.version=LI_SAVE_VERSION;s->dirty=true;
  }
 }
 if(s->progress.volume<15 || s->progress.volume>75 || s->progress.volume%15) s->progress.volume=60;
 if(s->progress.minutes!=5 && s->progress.minutes!=10 && s->progress.minutes!=15) s->progress.minutes=10;
 for(unsigned i=0;i<LI_COUNT;i++) { s->progress.mistakes[i]=s->progress.mistakes[i]?1:0; s->progress.heard[i]=s->progress.heard[i]?1:0; }
 for(unsigned i=0;i<LI_TOPICS;i++) if(s->progress.stamps[i]>3) s->progress.stamps[i]=0;
}
static void ask(li_state_t *s) {
 unsigned id=li_current(s), base=(id/LI_PER_TOPIC)*LI_PER_TOPIC;
 unsigned other=base+random_next(s)%LI_PER_TOPIC;
 if(other==id) other=base+(other-base+1)%LI_PER_TOPIC;
 unsigned correct=random_next(s)%2;
 s->options[correct]=(uint16_t)id; s->options[1-correct]=(uint16_t)other;
 s->selection=0; s->page=LI_QUIZ; voice(s,(int)id);
}
static void begin(li_state_t *s) {
 s->pos=0;s->count=0;s->correct=0;s->paused=false;s->timed_out=false;s->started=s->now;
 if(s->mode==2) {
  for(unsigned i=0;i<LI_COUNT;i++) if(s->progress.mistakes[i]) s->order[s->count++]=(uint16_t)i;
 } else for(unsigned i=0;i<LI_PER_TOPIC;i++) s->order[s->count++]=(uint16_t)(s->topic*LI_PER_TOPIC+i);
 if(!s->count) {s->page=LI_FINISH;voice(s,-1);return;}
 if(s->mode==1) {
  for(unsigned i=s->count-1;i>0;i--) {unsigned j=random_next(s)%(i+1);uint16_t t=s->order[i];s->order[i]=s->order[j];s->order[j]=t;}
  /* Guarantee one previously missed sentence when this topic has one. */
  for(unsigned i=LI_ROUND;i<s->count;i++) if(s->progress.mistakes[s->order[i]]) {uint16_t t=s->order[0];s->order[0]=s->order[i];s->order[i]=t;break;}
  s->count=LI_ROUND;ask(s);
 } else {s->page=LI_LISTEN;voice(s,li_current(s));}
}
static void finish(li_state_t *s) {
 s->page=LI_FINISH;voice(s,-1);
 if(s->mode==1) {unsigned stars=s->correct>=6?s->correct-5:0;
  if(stars>s->progress.stamps[s->topic]) {s->progress.stamps[s->topic]=(uint8_t)stars;s->dirty=true;}}
}
void li_key(li_state_t *s,unsigned key,bool held,uint32_t now) {
 if(key>2) return;
 li_tick(s,now);
 if(held) {
  if(key==2) {s->page=LI_HOME;s->paused=false;voice(s,-1);}
  else if(key==0 && (s->page==LI_QUIZ || s->page==LI_FEEDBACK || s->page==LI_LISTEN)) {
   if(s->page==LI_LISTEN) s->paused=false;
   voice(s,li_current(s));
  }
  return;
 }
 switch(s->page) {
 case LI_HOME:
  if(key!=2) s->menu=(s->menu+(key==0?4:1))%5;
  else if(s->menu<3) {s->mode=s->menu;if(s->mode==2) begin(s);else s->page=LI_TOPICS_PAGE;}
  else s->page=s->menu==3?LI_ALBUM:LI_SETTINGS;
  break;
 case LI_TOPICS_PAGE:
  if(key==2) begin(s);else s->topic=(s->topic+(key==0?7:1))%LI_TOPICS;
  break;
 case LI_LISTEN:
  if(key==2) {s->paused=!s->paused;voice(s,s->paused?-1:li_current(s));}
  else {s->pos=(s->pos+(key==0?s->count-1:1))%s->count;s->paused=false;voice(s,li_current(s));}
  break;
 case LI_QUIZ:
  if(key!=2) s->selection=key==0?0:1;
  else if(s->voice_enabled) {
   unsigned id=li_current(s);s->right=s->options[s->selection]==id;
   s->progress.mistakes[id]=s->right?0:1;s->dirty=true;s->correct+=s->right;
   s->page=LI_FEEDBACK;voice(s,id);
  }
  break;
 case LI_FEEDBACK:
  if(key==2) {if(++s->pos==s->count) finish(s);else ask(s);}else voice(s,li_current(s));
  break;
 case LI_FINISH:
  if(key==2) {s->page=LI_HOME;voice(s,-1);}
  break;
 case LI_ALBUM:
  if(key==2) s->page=LI_HOME;else s->topic=(s->topic+(key==0?7:1))%LI_TOPICS;
  break;
 case LI_SETTINGS:
  if(key!=2) s->setting=key==0?0:1;
  else {if(s->setting==0)s->progress.volume=s->progress.volume==100?20:s->progress.volume+20;
   else s->progress.minutes=s->progress.minutes==15?5:s->progress.minutes+5;
   s->dirty=true;}
  break;
 }
}
void li_tick(li_state_t *s,uint32_t now) {
 s->now=now;
 if(s->page==LI_LISTEN) {
  if(now-s->started >= (uint32_t)s->progress.minutes*60000u) {s->timed_out=true;finish(s);return;}
  if(s->waiting && !s->paused && now-s->wait_since>=2500u) {
   if(++s->pos==s->count) finish(s);else voice(s,li_current(s));
  }
 }
}
void li_audio_done(li_state_t *s,uint32_t serial,uint32_t now) {
 if(serial!=s->serial || s->voice<0) return;
 if(s->page!=LI_LISTEN && s->page!=LI_QUIZ && s->page!=LI_FEEDBACK) return;
 unsigned id=li_current(s);if(!s->progress.heard[id]) {s->progress.heard[id]=1;s->dirty=true;}
 if(s->page==LI_LISTEN && !s->paused) {s->waiting=true;s->wait_since=now;}
}
