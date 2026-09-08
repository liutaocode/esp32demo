#include "night_call_state.h"
#include <string.h>
void nc_init(nc_state_t *s) { *s = (nc_state_t){.node=0, .volume=2}; }
static void enter(nc_state_t *s, unsigned node) {
    s->node = (uint16_t)node;
    int clue = nc_nodes[node].clue;
    if (clue >= 0 && clue < NC_CLUES) s->clues |= (uint8_t)(1u << clue);
}
void nc_begin(nc_state_t *s, unsigned chapter) {
    if (chapter >= NC_CHAPTERS) return;
    s->chapter = (uint8_t)chapter; s->active = true; enter(s, nc_starts[chapter]);
}
nc_result_t nc_choose(nc_state_t *s, unsigned choice, int *ending) {
    if (!s->active || !nc_state_valid(s) || choice > 1 || !ending) return NC_IGNORED;
    int target = nc_nodes[s->node].choices[choice].target;
    if (target == NC_TARGET_HOME) return NC_HOME;
    if (target == NC_TARGET_RESOLVE) {
        /* The true finale requires two completed rescues, not just seen text. */
        target = (s->endings & ((1u << 2) | (1u << 4))) == ((1u << 2) | (1u << 4)) ? -7 : (int)nc_missing_node;
    }
    if (target < 0) {
        *ending = -target - 1;
        if (*ending >= NC_ENDINGS) return NC_IGNORED;
        s->endings |= (uint8_t)(1u << *ending); s->active = false; return NC_END;
    }
    if ((unsigned)target >= nc_node_count) return NC_IGNORED;
    enter(s, (unsigned)target); return NC_STORY;
}
unsigned nc_count(unsigned bits) { unsigned n=0; while(bits) { n += bits & 1u; bits >>= 1; } return n; }
bool nc_state_valid(const nc_state_t *s) {
    return s && s->node < nc_node_count && s->chapter < NC_CHAPTERS &&
        s->volume <= 3 && s->clues < (1u << NC_CLUES) && s->endings < (1u << NC_ENDINGS) &&
        nc_nodes[s->node].chapter == s->chapter;
}
static uint32_t hash(const uint8_t *p, unsigned n) {
    uint32_t h=2166136261u; for (unsigned i=0;i<n;i++) h=(h^p[i])*16777619u; return h;
}
void nc_encode(const nc_state_t *s, uint8_t out[NC_SAVE_SIZE]) {
    memset(out,0,NC_SAVE_SIZE); memcpy(out,"NCAL",4); out[4]=1;
    out[5]=(uint8_t)s->node; out[6]=(uint8_t)(s->node>>8); out[7]=s->chapter;
    out[8]=s->clues; out[9]=s->endings; out[10]=s->volume; out[11]=s->active;
    uint32_t h=hash(out,20); for(unsigned i=0;i<4;i++) out[20+i]=(uint8_t)(h>>(8*i));
}
bool nc_decode(nc_state_t *s, const uint8_t *p, size_t length) {
    if (!p || length!=NC_SAVE_SIZE || memcmp(p,"NCAL",4) || p[4]!=1 || p[11]>1) return false;
    uint32_t h=0; for(unsigned i=0;i<4;i++) h|=(uint32_t)p[20+i]<<(8*i);
    if(h!=hash(p,20)) return false;
    for(unsigned i=12;i<20;i++) if(p[i]) return false;
    nc_state_t t={.node=(uint16_t)(p[5]|(p[6]<<8)),.chapter=p[7],.clues=p[8],.endings=p[9],.volume=p[10],.active=p[11]!=0};
    if(!nc_state_valid(&t)) return false;
    *s=t; return true;
}
