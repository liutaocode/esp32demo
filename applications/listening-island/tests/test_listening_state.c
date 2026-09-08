#include "listening_state.h"
#include <assert.h>
#include <stdio.h>
static void init(li_state_t *s){li_init(s,NULL,123);s->voice_enabled=true;}
static void start(li_state_t *s,unsigned mode){s->menu=mode;li_key(s,2,false,0);if(mode!=2)li_key(s,2,false,0);}
int main(void){
 li_state_t s;init(&s);assert(s.progress.volume==60);start(&s,0);assert(s.page==LI_LISTEN&&s.count==48&&s.voice==0);
 uint32_t old=s.serial;li_key(&s,1,false,10);assert(s.voice==1);li_audio_done(&s,old,20);assert(!s.waiting&&!li_heard(&s));
 li_audio_done(&s,s.serial,30);assert(s.waiting&&li_heard(&s)==1);li_tick(&s,2529);assert(s.pos==1);li_tick(&s,2530);assert(s.pos==2);
 li_key(&s,2,false,2600);assert(s.paused&&s.voice==-1);li_tick(&s,20000);assert(s.pos==2);
 li_key(&s,2,false,20001);assert(!s.paused&&s.voice==2);li_tick(&s,600000);assert(s.page==LI_FINISH&&s.voice==-1&&s.timed_out);
 init(&s);start(&s,2);assert(s.page==LI_FINISH&&s.count==0);
 init(&s);s.progress.mistakes[383]=1;start(&s,2);assert(s.count==1&&s.voice==383);
 li_audio_done(&s,s.serial,10);li_tick(&s,2510);assert(s.page==LI_FINISH);assert(s.progress.mistakes[383]);
 for(unsigned seed=1;seed<200;seed++){
  li_init(&s,NULL,seed);s.voice_enabled=true;s.topic=seed%8;start(&s,1);assert(s.count==8);
  for(unsigned i=0;i<8;i++){
   unsigned id=li_current(&s);for(unsigned j=0;j<i;j++)assert(s.order[j]!=id);
   assert(s.options[0]!=s.options[1]);assert(s.options[0]==id||s.options[1]==id);
   li_key(&s,s.options[0]==id?0:1,false,100+i*10);li_key(&s,2,false,101+i*10);
   assert(s.page==LI_FEEDBACK&&s.right);li_key(&s,2,false,102+i*10);
  }
  assert(s.page==LI_FINISH&&s.correct==8&&s.progress.stamps[s.topic]==3);
 }
 init(&s);start(&s,1);unsigned id=li_current(&s);li_key(&s,s.options[0]!=id?0:1,false,1);li_key(&s,2,false,2);assert(s.progress.mistakes[id]&&s.correct==0);
 li_key(&s,2,true,3);assert(s.page==LI_HOME&&s.voice==-1);
 init(&s);start(&s,1);s.voice_enabled=false;li_key(&s,2,false,1);assert(s.page==LI_QUIZ&&!s.correct);
 li_progress_t corrupt={.version=1,.volume=255,.minutes=255,.stamps={255},.heard={255},.mistakes={255}};
 li_init(&s,&corrupt,0);assert(s.progress.volume==60&&s.progress.minutes==10&&s.progress.stamps[0]==0&&s.progress.heard[0]==1);
 li_progress_t previous={.version=1,.volume=45,.minutes=10,.heard={1},.mistakes={1},.stamps={2}};
 li_init(&s,&previous,1);assert(s.progress.version==2&&s.progress.volume==60&&s.dirty&&s.progress.stamps[0]==2&&s.progress.heard[0]&&s.progress.mistakes[0]);
 s.page=LI_SETTINGS;s.setting=0;for(unsigned i=0;i<5;i++)li_key(&s,2,false,i);assert(s.progress.volume==60);
 li_state_t again;li_init(&again,&s.progress,1);assert(again.progress.volume==60&&!again.dirty);
 init(&s);start(&s,0);s.started=UINT32_MAX-100;li_tick(&s,400);assert(s.page==LI_LISTEN);li_audio_done(&s,s.serial,UINT32_MAX-100);li_tick(&s,2500);assert(s.pos==1);
 puts("Listening state: PASS (shuffle, marks, pause, timer, cancellation, corrupt saves, wraparound)");
}
