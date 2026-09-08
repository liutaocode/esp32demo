#include "pb_game.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static void tick(pb_game *g,unsigned ms){while(ms){unsigned d=ms>50?50:ms;pb_tick(g,d);ms-=d;}}
static void aim_at(pb_game *g,unsigned i){g->locked=-1;g->aim=atan2f(g->enemy[i].y-g->y,g->enemy[i].x-g->x)-g->base;while(g->aim < -PB_PI)g->aim+=2*PB_PI;while(g->aim>PB_PI)g->aim-=2*PB_PI;g->stable=300;}
int main(void)
{
    pb_game g={0};
    for(unsigned map=0;map<3;map++)for(unsigned mode=0;mode<2;mode++)for(unsigned seed=0;seed<80;seed++){
        g.map=map;g.mode=mode;pb_start(&g,seed);
        for(unsigned wave=1;wave<=12;wave++){
            assert(g.wave==wave&&g.page==PB_FIGHT);
            for(unsigned i=0;i<PB_ENEMIES;i++)if(g.enemy[i].hp){
                assert(!pb_wall(map,(int)g.enemy[i].x,(int)g.enemy[i].y));
                while(!pb_exposed(&g.enemy[i]))tick(&g,50);
                aim_at(&g,i);assert(fabsf(g.aim)<=.78f);assert(pb_target(&g)>=0);
                while(g.enemy[i].hp){if(pb_exposed(&g.enemy[i])){aim_at(&g,i);pb_fire(&g);}tick(&g,230);}
            }
            while(g.page!=PB_RESULT&&g.wave==wave)tick(&g,50);
        }
        assert(g.won&&g.health==100&&g.hits==g.shots&&g.kills==40);
        assert(g.score==g.best[mode]);
    }
    pb_start(&g,7);pb_game same=g;pb_start(&g,7);assert(memcmp(g.enemy,same.enemy,sizeof(g.enemy))==0);
    g.aim=1;pb_turn(&g,.2f);assert(g.aim>1.1f);
    g.aim=3.1f;pb_turn(&g,.2f);assert(g.aim< -2.9f);
    g.aim=-3.1f;pb_turn(&g,-.2f);assert(g.aim>2.9f);
    g.aim=2;g.locked=-1;
    for(int i=0;i<6;i++){pb_fire(&g);tick(&g,230);}assert(g.reload&&g.ammo==0);unsigned shots=g.shots;pb_fire(&g);assert(g.shots==shots);tick(&g,1200);assert(g.ammo==6);
    pb_pause(&g);same=g;tick(&g,60000);assert(memcmp(&same,&g,sizeof(g))==0);pb_pause(&g);assert(g.page==PB_FIGHT);
    pb_start(&g,1);while(g.page==PB_FIGHT)tick(&g,50);assert(g.page==PB_RESULT&&!g.won);
    pb_start(&g,1);unsigned attack=g.enemy[0].attack;pb_tick(&g,60000);assert(g.enemy[0].attack==attack-80);
    /* A wall must occlude an otherwise aligned target. */
    memset(g.enemy,0,sizeof(g.enemy));g.map=1;g.x=5.5f;g.y=3.5f;g.base=PB_PI/2;g.aim=0;g.enemy[0]=(pb_enemy){.x=5.5f,.y=8.5f,.hp=1,.attack=1000};assert(pb_target(&g)==-1);
    assert(fabsf(pb_ray(0,2.5f,2.5f,1,0,0,0)-8.5f)<.001f);
    /* Magnetic acquisition, held lock, manual release, no lock through cover. */
    pb_start(&g,13);g.aim=pb_bearing(&g,0)-.13f;g.locked=-1;pb_tick(&g,50);
    assert(g.locked==0&&fabsf(pb_bearing(&g,0))<.001f&&pb_target(&g)==0);
    pb_turn(&g,-.075f);assert(g.locked!=0);assert(g.skip_lock==0&&g.lock_grace==700);
    g.aim=PB_PI;g.locked=-1;tick(&g,800);assert(g.locked==-1);
    assert(pb_direction(.1f)==0&&pb_direction(-1)==1&&pb_direction(1)==2&&pb_direction(3)==3);
    assert(pb_direction(2*PB_PI+1)==2);
    pb_start(&g,17);g.aim=PB_PI;g.enemy[0].attack=1;pb_tick(&g,50);
    assert(g.last_attacker==0&&g.hurt_ms==1500&&pb_direction(g.hurt_bearing-g.base-g.aim)==3);
    g.aim=0;assert(pb_direction(g.hurt_bearing-g.base-g.aim)==0);
    memset(g.enemy,0,sizeof(g.enemy));g.map=1;g.x=5.5f;g.y=3.5f;g.base=PB_PI/2;g.aim=0;g.locked=-1;
    g.enemy[0]=(pb_enemy){.x=5.5f,.y=8.5f,.hp=1,.attack=1000};pb_tick(&g,50);assert(g.locked==-1&&pb_threat(&g)==-1);

    /* Reproduce the last-enemy ghost: flashes/death must finish BEFORE travel. */
    pb_start(&g,32);memset(g.enemy,0,sizeof(g.enemy));
    g.enemy[0]=(pb_enemy){.x=g.x+3.4f,.y=g.y,.hp=2,.attack=6000,.cover=true};
    aim_at(&g,0);pb_fire(&g);assert(g.enemy[0].hp==1&&!g.settle&&pb_alive(&g)==1);
    tick(&g,230);pb_fire(&g);assert(!pb_alive(&g)&&g.settle==900&&g.enemy[0].death_ms==320);
    tick(&g,180);assert(!g.enemy[0].flash&&g.enemy[0].death_ms&&g.page==PB_FIGHT);
    pb_pause(&g);unsigned death=g.enemy[0].death_ms;tick(&g,1000);assert(g.enemy[0].death_ms==death);pb_pause(&g);
    tick(&g,160);assert(!g.enemy[0].death_ms&&g.page==PB_FIGHT);
    static uint16_t actual[PB_W*PB_H],without_corpse[PB_W*PB_H];
    pb_render(&g,actual);pb_game absent=g;absent.enemy[0].flash=absent.enemy[0].death_ms=0;
    pb_render(&absent,without_corpse);assert(!memcmp(actual,without_corpse,sizeof(actual)));
    tick(&g,560);assert(g.page==PB_TRAVEL&&!g.enemy[0].flash&&!g.enemy[0].death_ms);
    /* No travel is allowed if any actual enemy survives. */
    pb_start(&g,32);g.settle=1;pb_tick(&g,50);assert(g.page==PB_FIGHT&&pb_alive(&g)>0);
    /* A hiding enemy cannot be shot or fire; it reappears after a bounded wait. */
    memset(g.enemy,0,sizeof(g.enemy));g.enemy[0]=(pb_enemy){.x=g.x+3.4f,.y=g.y,.hp=2,.attack=200,.hide_ms=900,.cover=true};g.locked=-1;g.aim=0;
    assert(!pb_exposed(&g.enemy[0])&&pb_target(&g)==-1);
    pb_fire(&g);assert(g.enemy[0].hp==2);unsigned health=g.health;tick(&g,900);
    assert(g.enemy[0].rise_ms==220&&g.enemy[0].attack==200&&g.health==health);
    tick(&g,220);assert(pb_exposed(&g.enemy[0]));tick(&g,200);
    assert(g.health<health&&g.enemy[0].hide_ms==900&&!pb_exposed(&g.enemy[0]));
    /* Guidance is off-screen only, including hit indicators and partial sprites. */
    g.hurt_ms=1500;g.last_attacker=0;g.aim=0;assert(pb_in_view(&g,0)&&pb_guidance(&g)==-1);
    g.aim=.66f;assert(pb_in_view(&g,0)&&pb_guidance(&g)==-1);
    g.aim=1.1f;assert(!pb_in_view(&g,0)&&pb_guidance(&g)==0);
    g.aim=PB_PI;assert(pb_guidance(&g)==0);
    g.aim=0;assert(pb_guidance(&g)==-1);g.enemy[0].hp=0;g.aim=PB_PI;assert(pb_guidance(&g)==-1);

    /* Guard the entire framebuffer; exercise all maps, angles and close sprites. */
    struct {uint32_t before;uint16_t pixels[PB_W*PB_H];uint32_t after;} buffer;
    buffer.before=0xfeed1234;buffer.after=0x4321beef;
    for(unsigned map=0;map<3;map++){
        g.map=map;pb_start(&g,4);
        for(int a=-314;a<=314;a+=2){g.aim=a/100.f;pb_render(&g,buffer.pixels);assert(buffer.before==0xfeed1234&&buffer.after==0x4321beef);}
        g.enemy[0].x=g.x+.16f;g.enemy[0].y=g.y;g.aim=0;pb_render(&g,buffer.pixels);
    }
    for(unsigned s=PB_SHOT;s<=PB_CLEAR;s++){
        int16_t pcm[160];bool audible=false;
        for(unsigned off=0;off<pb_sound_samples(s)+160;off+=160){pb_pcm(s,off,pcm,160);for(unsigned i=0;i<160;i++){assert(pcm[i]>=-7000&&pcm[i]<=7000);audible|=pcm[i]!=0;if(off+i>=pb_sound_samples(s))assert(!pcm[i]);}}
        assert(audible);
    }
    puts("Pocket Breach: 480 complete campaigns, occlusion, reload, pause, loss, replay, stall cap, framebuffer guards and PCM PASS");
}
