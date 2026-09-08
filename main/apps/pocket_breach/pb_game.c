#include "pb_game.h"
#include <math.h>
#include <string.h>
static unsigned dec(unsigned value,unsigned ms) { return value>ms?value-ms:0; }
static uint32_t random_next(pb_game *g) { g->rng=g->rng*1664525U+1013904223U; return g->rng; }
/* Clear perimeter ring connects all four stations; interior cover differs by map. */
int pb_wall(unsigned map,int x,int y)
{
    if(x<=0||y<=0||x>=11||y>=11)return 1;
    if(x<4||x>7||y<4||y>7)return 0;
    if(map%3==0)return (x==4||x==7)&&(y==4||y==7)?2:0;
    if(map%3==1)return y==5||y==6?2:0;
    return x==5||x==6||y==5||y==6?2:0;
}
float pb_ray(unsigned map,float x,float y,float dx,float dy,int *side,float *tex)
{
    int mx=(int)floorf(x),my=(int)floorf(y),sx=dx<0?-1:1,sy=dy<0?-1:1,s=0;
    float ax=fabsf(dx)<0.00001f?100000:fabsf(1/dx),ay=fabsf(dy)<0.00001f?100000:fabsf(1/dy);
    float tx=(dx<0?x-mx:mx+1-x)*ax,ty=(dy<0?y-my:my+1-y)*ay,d=0;
    for(int i=0;i<32;i++) {
        if(tx<ty){d=tx;tx+=ax;mx+=sx;s=0;}else{d=ty;ty+=ay;my+=sy;s=1;}
        if(pb_wall(map,mx,my))break;
    }
    if(side)*side=s;
    if(tex){float q=s?x+d*dx:y+d*dy;*tex=q-floorf(q);}
    return fmaxf(d,0.05f);
}
static float wrap(float a)
{
    while(a>PB_PI)a-=2*PB_PI;
    while(a < -PB_PI)a+=2*PB_PI;
    return a;
}
float pb_bearing(const pb_game *g,int enemy)
{
    return wrap(atan2f(g->enemy[enemy].y-g->y,g->enemy[enemy].x-g->x)-g->base-g->aim);
}
int pb_direction(float a)
{
    a=wrap(a);
    if(fabsf(a)<.35f)return 0;
    if(fabsf(a)>2.35f)return 3;
    return a<0?1:2;
}
bool pb_exposed(const pb_enemy *e){return e->hp&&!e->hide_ms&&!e->rise_ms;}
unsigned pb_alive(const pb_game *g)
{
    unsigned count=0;for(int i=0;i<PB_ENEMIES;i++)count+=g->enemy[i].hp>0;return count;
}
void pb_crate(const pb_game *g,int i,float *x,float *y,float *hx,float *hy)
{
    float dx=cosf(g->base),dy=sinf(g->base);
    *x=g->enemy[i].x-dx*.48f;*y=g->enemy[i].y-dy*.48f;
    *hx=fabsf(dx)>.5f?.24f:.19f;*hy=fabsf(dx)>.5f?.19f:.24f;
}
static bool clear_sight(const pb_game *g,int i)
{
    if(i<0||i>=PB_ENEMIES||!g->enemy[i].hp)return false;
    float dx=g->enemy[i].x-g->x,dy=g->enemy[i].y-g->y,d=hypotf(dx,dy);
    return d>.01f&&pb_ray(g->map,g->x,g->y,dx/d,dy/d,0,0)>=d-.04f;
}
static bool visible_enemy(const pb_game *g,int i)
{
    return clear_sight(g,i)&&pb_exposed(&g->enemy[i]);
}
bool pb_in_view(const pb_game *g,int i)
{
    if(!clear_sight(g,i))return false;
    float a=g->base+g->aim,ex=g->enemy[i].x-g->x,ey=g->enemy[i].y-g->y;
    float z=ex*cosf(a)+ey*sinf(a);if(z<=.15f)return false;
    float center=60+60*(ey*cosf(a)-ex*sinf(a))/(.7f*z),half=27.36f/z;
    /* Include partly visible silhouettes; cover hiding does not request a turn. */
    return center+half>=0&&center-half<PB_W;
}
int pb_threat(const pb_game *g)
{
    int best=-1;
    for(int i=0;i<PB_ENEMIES;i++)if(clear_sight(g,i)&&!pb_in_view(g,i)&&
        (best<0||g->enemy[i].attack<g->enemy[best].attack))best=i;
    return best;
}
int pb_guidance(const pb_game *g)
{
    if(g->page!=PB_FIGHT||g->settle)return -1;
    if(g->hurt_ms&&clear_sight(g,g->last_attacker)&&!pb_in_view(g,g->last_attacker))return g->last_attacker;
    return pb_threat(g);
}
static void assist(pb_game *g)
{
    if(visible_enemy(g,g->locked)){g->aim=wrap(g->aim+pb_bearing(g,g->locked));return;}
    g->locked=-1;float angle=.16f;int best=-1;
    for(int i=0;i<PB_ENEMIES;i++)if(visible_enemy(g,i)){
        float a=fabsf(pb_bearing(g,i));
        if(i==g->skip_lock&&g->lock_grace)continue;
        if(a<angle){angle=a;best=i;}
    }
    if(best>=0){g->aim=wrap(g->aim+pb_bearing(g,best));g->locked=best;g->stable=300;}
}
static const float stations[4][2]={{2.0f,2.0f},{9.0f,2.0f},{9.0f,9.0f},{2.0f,9.0f}};
static void spawn(pb_game *g)
{
    unsigned corner=(g->wave-1)%4;
    g->x=stations[corner][0];g->y=stations[corner][1];g->base=corner*PB_PI/2;g->aim=0;
    memset(g->enemy,0,sizeof(g->enemy));
    unsigned count=g->wave<3?2:g->wave<7?3:4;
    for(unsigned i=0;i<count;i++){
        static const float lanes[4]={-.75f,.75f,-.40f,.40f};
        static const float distances[4]={3.3f,4.7f,4.2f,3.7f};
        float forward=distances[i]+(random_next(g)%35)*.01f;
        float side=lanes[i],dx=cosf(g->base),dy=sinf(g->base);
        pb_enemy *e=&g->enemy[i];
        e->x=g->x+dx*forward-dy*side;e->y=g->y+dy*forward+dx*side;
        e->cover=true;e->hide_ms=i%2?650:0;
        e->hp=g->wave>=5&&i==0?2:1;
        e->attack= (g->mode?4300:6400)+i*950-(g->wave-1)*110;
    }
    g->locked=g->last_attacker=g->skip_lock=-1;g->hurt_ms=g->lock_grace=0;
    g->page=PB_FIGHT;g->stable=400;g->settle=0;g->ammo=6;g->reload=0;
}
void pb_home(pb_game *g){g->page=PB_HOME;g->sound=PB_SILENT;}
void pb_start(pb_game *g,uint32_t seed)
{
    unsigned map=g->map%3,mode=g->mode%2,b0=g->best[0],b1=g->best[1];
    memset(g,0,sizeof(*g));g->map=map;g->mode=mode;g->best[0]=b0;g->best[1]=b1;
    g->seed=g->rng=seed;g->wave=1;g->health=100;spawn(g);
}
void pb_turn(pb_game *g,float amount)
{
    if(g->page!=PB_FIGHT)return;
    if(g->locked>=0){g->skip_lock=g->locked;g->lock_grace=700;}
    g->locked=-1;g->aim=wrap(g->aim+amount);g->stable=0;
    assist(g);
}
int pb_target(const pb_game *g)
{
    if(visible_enemy(g,g->locked)&&fabsf(pb_bearing(g,g->locked))<.18f)return g->locked;
    float angle=g->base+g->aim,dx=cosf(angle),dy=sinf(angle);
    float nearest=pb_ray(g->map,g->x,g->y,dx,dy,0,0);int result=-1;
    for(int i=0;i<PB_ENEMIES;i++)if(pb_exposed(&g->enemy[i])){
        float ex=g->enemy[i].x-g->x,ey=g->enemy[i].y-g->y;
        float z=ex*dx+ey*dy,lateral=fabsf(ey*dx-ex*dy);
        /* Stable aim provides a small, deterministic assist; no random misses. */
        float radius=g->stable>=180?0.24f:0.15f;
        if(z>0.1f&&z<nearest&&lateral<radius){nearest=z;result=i;}
    }
    return result;
}
static void finish(pb_game *g,bool won)
{
    g->page=PB_RESULT;g->won=won;
    if(won){g->score+=g->health*10;g->sound=PB_CLEAR;}
    if(g->score>g->best[g->mode])g->best[g->mode]=g->score;
}
void pb_fire(pb_game *g)
{
    if(g->page!=PB_FIGHT||g->reload||g->cooldown||g->settle)return;
    if(!g->ammo){g->reload=1100;g->sound=PB_RELOAD;return;}
    --g->ammo;++g->shots;g->cooldown=220;g->recoil=140;g->sound=PB_SHOT;
    int target=pb_target(g);
    if(target>=0){
        pb_enemy *e=&g->enemy[target];--e->hp;++g->hits;e->flash=170;
        g->sound=e->hp?PB_HIT:PB_BREAK;
        if(!e->hp){e->death_ms=320;g->locked=-1;++g->kills;++g->combo;g->score+=100+25*(g->combo>8?8:g->combo);
            if(g->combo>g->max_combo)g->max_combo=g->combo;}
    }else g->combo=0;
    if(!pb_alive(g)){g->settle=900;g->sound=PB_CLEAR;}
    else if(!g->ammo)g->reload=1100;
}
void pb_pause(pb_game *g)
{
    if(g->page==PB_PAUSE)g->page=g->resume;
    else if(g->page==PB_FIGHT||g->page==PB_TRAVEL){g->resume=g->page;g->page=PB_PAUSE;}
}
void pb_tick(pb_game *g,unsigned ms)
{
    if(g->page!=PB_FIGHT&&g->page!=PB_TRAVEL)return;
    if(ms>80)ms=80; /* Never punish the player for capture / render stalls. */
    g->elapsed+=ms;g->cooldown=dec(g->cooldown,ms);g->recoil=dec(g->recoil,ms);g->damage=dec(g->damage,ms);
    g->hurt_ms=dec(g->hurt_ms,ms);g->lock_grace=dec(g->lock_grace,ms);
    if(g->stable<1000)g->stable+=ms;
    if(g->reload){g->reload=dec(g->reload,ms);if(!g->reload){g->ammo=6;g->sound=PB_RELOAD;}}
    /* Presentation timers must advance during clear/travel too: otherwise the
     * final dead sentry stays visible until the next spawn. */
    for(int i=0;i<PB_ENEMIES;i++){
        g->enemy[i].flash=dec(g->enemy[i].flash,ms);
        g->enemy[i].death_ms=dec(g->enemy[i].death_ms,ms);
    }
    if(g->page==PB_TRAVEL){
        g->travel_ms+=ms;float t=fminf(g->travel_ms/1900.0f,1),ease=t*t*(3-2*t);
        unsigned from=(g->wave-1)%4,to=g->wave%4;
        g->x=stations[from][0]+(stations[to][0]-stations[from][0])*ease;
        g->y=stations[from][1]+(stations[to][1]-stations[from][1])*ease;
        g->aim*=0.90f;
        if(t>=1){++g->wave;spawn(g);}return;
    }
    if(g->settle&&pb_alive(g))g->settle=0;
    if(g->settle){g->settle=dec(g->settle,ms);if(!g->settle){
        if(g->wave==12)finish(g,true);else{g->page=PB_TRAVEL;g->travel_ms=0;}
    }return;}
    assist(g);
    for(unsigned i=0;i<PB_ENEMIES;i++){
        pb_enemy *e=&g->enemy[i];if(!e->hp)continue;
        if(e->hide_ms){
            e->hide_ms=dec(e->hide_ms,ms);if(!e->hide_ms)e->rise_ms=220;
            if(g->locked==(int)i)g->locked=-1;
            continue;
        }
        if(e->rise_ms){e->rise_ms=dec(e->rise_ms,ms);continue;}
        e->attack=dec(e->attack,ms);
        if(!e->attack){
            /* Walls block enemy attacks as well as the player's shots. */
            float dx=g->x-e->x,dy=g->y-e->y,d=hypotf(dx,dy);
            if(d>0.01f&&pb_ray(g->map,e->x,e->y,dx/d,dy/d,0,0)>=d-0.05f){
                unsigned hit=g->mode?22:14;g->health=dec(g->health,hit);g->damage=220;g->hurt_ms=1500;g->last_attacker=(int)i;
                g->hurt_bearing=atan2f(e->y-g->y,e->x-g->x);g->combo=0;g->sound=PB_HURT;
            }
            e->attack=g->mode?3200:4100;
            if(e->cover){e->hide_ms=900;g->locked=-1;}
            if(!g->health){finish(g,false);return;}
        }
    }
}
