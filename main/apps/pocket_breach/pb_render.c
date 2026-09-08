#include "pb_game.h"
#include <math.h>
#include <stddef.h>
static uint16_t rgb(unsigned r,unsigned g,unsigned b){return (r>>3)<<11|(g>>2)<<5|(b>>3);}
static uint16_t shade(unsigned c,float light){return rgb(((c>>16)&255)*light,((c>>8)&255)*light,(c&255)*light);}
static void rect(uint16_t *p,int x,int y,int w,int h,uint16_t c)
{
    int x2=x+w,y2=y+h;if(x<0)x=0;if(y<0)y=0;if(x2>PB_W)x2=PB_W;if(y2>PB_H)y2=PB_H;
    for(int j=y;j<y2;j++)for(int i=x;i<x2;i++)p[j*PB_W+i]=c;
}
/* Intersect a real low box footprint. Walls and boxes share camera depth. */
static bool crate_ray(float x,float y,float dx,float dy,float cx,float cy,float hx,float hy,
                      float *near,float *far,int *side,float *u)
{
    float tx0=-100000,tx1=100000,ty0=-100000,ty1=100000;
    if(fabsf(dx)<.00001f){if(x<cx-hx||x>cx+hx)return false;}
    else {tx0=(cx-hx-x)/dx;tx1=(cx+hx-x)/dx;if(tx0>tx1){float t=tx0;tx0=tx1;tx1=t;}}
    if(fabsf(dy)<.00001f){if(y<cy-hy||y>cy+hy)return false;}
    else {ty0=(cy-hy-y)/dy;ty1=(cy+hy-y)/dy;if(ty0>ty1){float t=ty0;ty0=ty1;ty1=t;}}
    *near=fmaxf(tx0,ty0);*far=fminf(tx1,ty1);if(*far<*near||*near<.05f)return false;
    *side=ty0>tx0;
    *u=*side?(x+dx*(*near)-(cx-hx))/(2*hx):(y+dy*(*near)-(cy-hy))/(2*hy);
    return true;
}
void pb_render(const pb_game *g,uint16_t *p)
{
    const unsigned wall_color[]={0xCCA878,0x6DB8BE,0x8591D2};
    const unsigned sky_color[]={0x609DB5,0x254D69,0x262B53};
    const unsigned floor_color[]={0x6F6859,0x3E6970,0x474563};
    unsigned map=g->map%3;float angle=g->base+g->aim,dx=cosf(angle),dy=sinf(angle),zbuf[PB_W],cover_z[PB_W];
    int16_t cover_top[PB_W],cover_bottom[PB_W];
    for(int y=0;y<PB_H;y++){
        bool floor=y>=52;float light=floor?0.46f+(y-52)*0.009f:0.70f+y*0.004f;
        uint16_t c=shade(floor?floor_color[map]:sky_color[map],light);
        for(int x=0;x<PB_W;x++)p[y*PB_W+x]=c;
    }
    /* Perspective floor grid is computed from world coordinates, so it moves with the camera. */
    for(int y=55;y<PB_H;y++){
        float dist=36.0f/(y-52),sx=g->x+dist*(dx+dy*0.7f),sy=g->y+dist*(dy-dx*0.7f);
        float stepx=-dy*1.4f*dist/PB_W,stepy=dx*1.4f*dist/PB_W;
        for(int x=0;x<PB_W;x++,sx+=stepx,sy+=stepy){
            float fx=sx-floorf(sx),fy=sy-floorf(sy);
            if(fx<0.035f||fy<0.035f)p[y*PB_W+x]=shade(floor_color[map],0.4f);
        }
    }
    for(int x=0;x<PB_W;x++){
        float camera=2.0f*x/PB_W-1,rx=dx-dy*camera*0.7f,ry=dy+dx*camera*0.7f;
        int side;float tex;float d=pb_ray(map,g->x,g->y,rx,ry,&side,&tex);zbuf[x]=d;
        int h=(int)(80/d),top=52-h/2,bottom=52+h/2;
        float light=fmaxf(0.28f,1.0f-d*0.055f)*(side?0.72f:1);
        for(int y=top<0?0:top;y<=bottom&&y<PB_H;y++){
            float v=(float)(y-top)/(h?h:1);unsigned color=wall_color[map];
            if(map==0){int row=(int)(v*8);if(((int)(tex*32)+(row%2)*8)%16==0||((int)(v*64))%8==0)color=0x786755;}
            if(map==1){if(tex<0.08f||tex>0.92f)color=0x245963;if(v>0.65f&&v<0.73f)color=0xF2C463;}
            if(map==2){if(tex<0.035f||v<0.025f)color=0x75E1D5;if(v>0.66f&&v<0.73f)color=0xC092DF;}
            if(v>0.91f)color=0x283D47;
            p[y*PB_W+x]=shade(color,light);
        }
    }
    /* Crates remain in the arena after kills. Their top/side faces obey
     * perspective; their depth hides the sentry's legs and crouched body. */
    const unsigned crate_colors[]={0xBD8245,0x3A9BAB,0x7370AC};
    for(int x=0;x<PB_W;x++){
        float camera=2.0f*x/PB_W-1,rx=dx-dy*camera*.7f,ry=dy+dx*camera*.7f;
        float nearest=zbuf[x],far=0,tex=0;int side=0;bool found=false;
        for(int i=0;i<PB_ENEMIES;i++)if(g->enemy[i].cover){
            float cx,cy,hx,hy,n,f,u;int face;pb_crate(g,i,&cx,&cy,&hx,&hy);
            if(crate_ray(g->x,g->y,rx,ry,cx,cy,hx,hy,&n,&f,&face,&u)&&n<nearest){
                nearest=n;far=f;tex=u;side=face;found=true;
            }
        }
        cover_z[x]=found?nearest:100000;cover_top[x]=PB_H;cover_bottom[x]=-1;
        if(!found)continue;
        int top=52+(int)(6.08f/far),front=52+(int)(6.08f/nearest),bottom=52+(int)(38/nearest);
        cover_top[x]=top;cover_bottom[x]=bottom;
        for(int y=top<0?0:top;y<=bottom&&y<PB_H;y++){
            float v=(float)(y-front)/fmaxf(bottom-front,1);unsigned color=crate_colors[map];
            bool rim=tex<.09f||tex>.91f||v<.09f||v>.89f;
            if(rim)color=0xE9C188;
            else if(fabsf(tex-v)<.075f||fabsf(1-tex-v)<.075f)color=0xDAC297;
            if(y<front)color=0xE0B879;
            p[y*PB_W+x]=shade(color,side?.80f:1);
        }
    }
    /* Far-to-near billboards with per-column wall occlusion. */
    int order[PB_ENEMIES]={0,1,2,3};float depth[PB_ENEMIES],screen[PB_ENEMIES];
    for(int i=0;i<PB_ENEMIES;i++){
        float ex=g->enemy[i].x-g->x,ey=g->enemy[i].y-g->y;depth[i]=ex*dx+ey*dy;
        screen[i]=60+60*(ey*dx-ex*dy)/(0.7f*fmaxf(depth[i],0.01f));
    }
    for(int a=0;a<PB_ENEMIES;a++)for(int b=a+1;b<PB_ENEMIES;b++)if(depth[order[a]]<depth[order[b]]){int t=order[a];order[a]=order[b];order[b]=t;}
    for(int n=0;n<PB_ENEMIES;n++){
        int i=order[n];const pb_enemy *e=&g->enemy[i];if((!e->hp&&!e->death_ms)||e->hide_ms||depth[i]<0.15f)continue;
        int h=(int)(76/depth[i]),w=(int)(h*.72f),cx=screen[i],cy=52;
        if(e->rise_ms)cy+=(int)(h*.7f*e->rise_ms/220.f);
        if(!e->hp){
            float fall=1-e->death_ms/320.f;
            cy+=(int)(h*.45f*fall);h=(int)(h*(1-.8f*fall));
        }
        if(h<4||w<4)continue;
        uint16_t shell=shade(e->flash?0xFFFFFF:e->hp==2?0xB782F5:0xF36B57,0.94f);
        uint16_t edge=rgb(23,35,48),eye=rgb(255,73,64),metal=rgb(203,225,223);
        for(int x=cx-w/2;x<=cx+w/2;x++)if(x>=0&&x<PB_W&&depth[i]<zbuf[x]){
            float u=(float)(x-cx)/(w/2);
            for(int y=cy-h/2;y<=cy+h/2;y++)if(y>=0&&y<PB_H){
                if(cover_z[x]<depth[i]&&y>=cover_top[x]&&y<=cover_bottom[x])continue;
                float v=(float)(y-cy)/(h/2);uint16_t c=0;bool draw=false;
                /* Readable humanoid sentry: helmet, visor, red jacket, arms and boots. */
                if(fabsf(u)<.48f&&v<-.45f&&v>-.98f){c=metal;draw=true;}
                if(fabsf(u)<.38f&&v<-.56f&&v>-.75f){c=edge;draw=true;}
                if(fabsf(u)<.24f&&v<-.57f&&v>-.66f){c=eye;draw=true;}
                if(fabsf(u)<.62f&&v>=-.44f&&v<.43f){c=fabsf(u)>.5f?metal:shell;draw=true;}
                if(fabsf(u)>.61f&&fabsf(u)<.95f&&v>-.29f&&v<.34f){c=shell;draw=true;}
                if(fabsf(u)<.25f&&v>-.2f&&v<.18f){c=metal;draw=true;}
                if(fabsf(u)>.11f&&fabsf(u)<.53f&&v>=.43f&&v<.97f){c=v>.82f?edge:metal;draw=true;}
                if(u>.55f&&u<.96f&&v>.13f&&v<.61f){c=edge;draw=true;}
                if(u>.67f&&u<.9f&&v>.16f&&v<.26f){c=rgb(255,219,100);draw=true;}
                if(draw)p[y*PB_W+x]=c;
            }
            if(pb_exposed(e)&&cy-h/2-4>=0){p[(cy-h/2-4)*PB_W+x]=edge;
                if(x<cx-w/2+(int)(w*(1-fminf(e->attack/4500.f,1))))p[(cy-h/2-4)*PB_W+x]=eye;}
        }
        if(i==g->locked){
            uint16_t gold=rgb(255,231,108);int left=cx-w/2-3,top=cy-h/2-3;
            rect(p,left,top,6,1,gold);rect(p,left,top,1,6,gold);
            rect(p,cx+w/2-2,top,6,1,gold);rect(p,cx+w/2+3,top,1,6,gold);
            rect(p,left,cy+h/2+3,6,1,gold);rect(p,left,cy+h/2-2,1,6,gold);
            rect(p,cx+w/2-2,cy+h/2+3,6,1,gold);rect(p,cx+w/2+3,cy+h/2-2,1,6,gold);
        }
    }
    /* Original compact blaster, recoil and a brief muzzle spark. */
    int kick=g->recoil?3:0;
    rect(p,47,86+kick,30,18,rgb(22,32,43));rect(p,51,88+kick,24,16,rgb(60,80,94));
    rect(p,55,76+kick,13,26,rgb(25,39,50));rect(p,58,78+kick,7,23,rgb(138,171,182));
    rect(p,60,77+kick,3,5,rgb(121,238,203));rect(p,37,97+kick,14,9,rgb(154,127,96));
    rect(p,76,98+kick,13,8,rgb(154,127,96));
    if(g->recoil>65){rect(p,57,67,9,8,rgb(255,216,104));rect(p,60,63,3,16,rgb(255,244,199));}
    uint16_t cross=pb_target(g)>=0?rgb(126,255,190):rgb(242,245,229);int gap=g->stable>=180?3:5;
    rect(p,60-gap-5,52,5,1,cross);rect(p,61+gap,52,5,1,cross);rect(p,60,52-gap-5,1,5,cross);rect(p,60,53+gap,1,5,cross);
    rect(p,60,52,1,1,cross);
    /* Compact north-up minimap; no font glyphs required. */
    rect(p,3,3,26,26,rgb(18,33,44));
    for(int y=0;y<PB_MAP;y++)for(int x=0;x<PB_MAP;x++)if(pb_wall(map,x,y))rect(p,4+x*2,4+y*2,2,2,shade(wall_color[map],0.65f));
    for(int i=0;i<PB_ENEMIES;i++)if(g->enemy[i].hp)rect(p,4+g->enemy[i].x*2,4+g->enemy[i].y*2,2,2,rgb(255,91,92));
    rect(p,4+g->x*2,4+g->y*2,2,2,rgb(119,255,202));
    rect(p,4+(g->x+dx)*2,4+(g->y+dy)*2,1,1,rgb(255,255,255));
    int guide=pb_guidance(g);
    if(guide>=0){
        float a=pb_bearing(g,guide);bool left=a<0;
        uint16_t c=g->hurt_ms&&guide==g->last_attacker?rgb(255,70,70):rgb(255,222,102);
        /* Only off-screen enemies request a turn; the sign chooses the shortest turn. */
        for(int j=0;j<9;j++){
            if(left)rect(p,2+j,51-j,1,2*j+1,c);
            else rect(p,117-j,51-j,1,2*j+1,c);
        }
    }
    if(g->damage){uint16_t c=rgb(242,71,84);rect(p,0,0,PB_W,2,c);rect(p,0,PB_H-2,PB_W,2,c);rect(p,0,0,2,PB_H,c);rect(p,PB_W-2,0,2,PB_H,c);}
}
