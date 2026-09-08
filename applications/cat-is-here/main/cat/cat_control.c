#include "cat_control.h"
#include <string.h>
void cat_control_init(cat_control_t *c,uint32_t seed,const cat_profile_t *saved) { memset(c,0,sizeof *c); cat_init(&c->cat,seed,saved); }
unsigned cat_control_brightness(const cat_control_t *c) {
    if(c->cat.profile.pocket && c->cat.idle_ms>=180000) return 0;
    return c->cat.idle_ms>=60000 ? 32 : 75;
}
void cat_control_tick(cat_control_t *c,uint32_t ms) {
    c->dance_stop_guard=c->dance_stop_guard>ms ? c->dance_stop_guard-ms : 0;
    cat_tick(&c->cat,ms);
    if(c->cat.idle_ms>=30000) c->page=0;
}
cat_sound_t cat_control_key(cat_control_t *c,unsigned key,unsigned event) {
    if(key>2 || event>3) return CAT_SILENT;
    if(event==0) {
        if(c->cat.pose==CAT_DANCING || c->dance_stop_guard) {
            if(c->cat.pose==CAT_DANCING) cat_dance(&c->cat,false);
            c->dance_stop_guard=650; c->wake_key[key]=true;
            return CAT_SILENT;
        }
        c->wake_key[key]=cat_control_brightness(c)==0;
        c->cat.idle_ms=0;
        if(c->wake_key[key]) return CAT_SILENT;
    } else if(c->wake_key[key]) return CAT_SILENT;
    if(key==2 && event==3) { c->page=c->page ? 0 : 1; c->selection=0; c->cat.idle_ms=0; return CAT_SILENT; }
    bool activate=key==2 ? event==1 || event==2 : event==0;
    if(!activate) return CAT_SILENT;
    c->cat.idle_ms=0;
    if(c->page==2) { c->page=1; return CAT_SILENT; }
    if(c->page==1) {
        if(key==0) c->selection=(c->selection+5)%6;
        else if(key==1) c->selection=(c->selection+1)%6;
        else if(c->selection==5) c->page=2;
        else {
            cat_option(&c->cat,c->selection);
            if(c->selection==3 && c->cat.profile.sound) return CAT_PURR;
        }
        return CAT_SILENT;
    }
    if(key==2 && event==2) return cat_dance(&c->cat,true);
    return cat_act(&c->cat,key==0 ? CAT_STROKE : key==1 ? CAT_TOY : CAT_CALL);
}
