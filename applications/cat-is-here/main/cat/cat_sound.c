#include "cat_sound.h"
#include "cat_clips.h"
static uint32_t next_random(cat_voice_t *v) {
    uint32_t x=v->rng ? v->rng : 714u;
    x^=x<<13; x^=x>>17; x^=x<<5; v->rng=x; return x;
}
static void begin(cat_voice_t *v,cat_sound_t kind) {
    v->kind=kind; v->sample=0; v->fade_left=0; v->loop=kind==CAT_DANCE;
    if(kind==CAT_DANCE) v->clip=9;
    else {
        unsigned group=(unsigned)kind-1, variant;
        if(v->used&(1u<<group)) variant=(v->last[group]+1+next_random(v)%2)%3;
        else variant=next_random(v)%3;
        v->last[group]=(uint8_t)variant; v->used|=(uint8_t)(1u<<group);
        v->clip=(uint8_t)(group*3+variant);
    }
    v->count=cat_clips[v->clip].samples;
    minecraft_adpcm_init(&v->decoder,0,0);
}
void cat_voice_start(cat_voice_t *v,cat_sound_t kind) {
    if(kind<CAT_MEW || kind>CAT_DANCE) return;
    /* One follow-up keeps repeated purring smooth. A different action switches
       after a short fade, instead of waiting for the entire old recording. */
    if(v->sample<v->count) {
        v->pending=kind;
        if(kind!=v->kind) { v->loop=false; if(!v->fade_left) v->fade_left=320; }
        return;
    }
    begin(v,kind);
}
void cat_voice_stop(cat_voice_t *v) {
    v->pending=CAT_SILENT; v->loop=false;
    if(v->sample<v->count && !v->fade_left) v->fade_left=320;
}
size_t cat_voice_render(cat_voice_t *v,int16_t *out,size_t cap) {
    size_t n=0;
    while(n<cap) {
        if(v->sample>=v->count) {
            if(v->pending!=CAT_SILENT) { cat_sound_t k=v->pending; v->pending=CAT_SILENT; begin(v,k); }
            else if(v->loop) begin(v,CAT_DANCE);
            else break;
        }
        uint32_t i=v->sample++;
        uint8_t byte=cat_clips[v->clip].data[i/2];
        int32_t sample=minecraft_adpcm_decode(&v->decoder,(i&1) ? byte&15 : byte>>4);
        if(sample>22000) sample=22000;
        if(sample< -22000) sample= -22000;
        uint32_t tail=v->count-v->sample;
        if(tail<64) sample=sample*(int32_t)tail/64;
        if(v->fade_left) {
            v->fade_left--; sample=sample*(int32_t)v->fade_left/320;
            if(!v->fade_left) v->sample=v->count;
        }
        out[n++]=(int16_t)sample;
    }
    return n;
}
