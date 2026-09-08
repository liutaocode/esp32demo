#include "cat_state.h"
#include <limits.h>
#include <string.h>
static uint32_t random_next(cat_state_t *c) {
    uint32_t x = c->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    c->rng = x ? x : 0x173b9u; return c->rng;
}
static uint32_t add_sat(uint32_t a, uint32_t b) { return UINT32_MAX-a < b ? UINT32_MAX : a+b; }
static uint32_t sub_sat(uint32_t a, uint32_t b) { return a>b ? a-b : 0; }
static void remember(cat_state_t *c, unsigned n) {
    uint8_t before=c->profile.memories; c->profile.memories |= (uint8_t)(1u<<n);
    if (before != c->profile.memories) c->dirty=true;
}
static void pose(cat_state_t *c, cat_pose_t p, uint32_t duration) {
    c->pose=p; c->pose_ms=0; c->pose_duration=duration;
    c->variant=(uint8_t)(random_next(c)%3);
}
void cat_init(cat_state_t *c, uint32_t seed, const cat_profile_t *saved) {
    memset(c,0,sizeof *c); c->profile.seed=seed ? seed : 713u; c->profile.sound=1;
    if (saved) c->profile=*saved;
    c->rng=c->profile.seed; c->gift_wait=90000+(random_next(c)%90000);
    pose(c,CAT_SIT,7000); c->dirty=!saved;
}
void cat_tick(cat_state_t *c, uint32_t dt) {
    c->idle_ms=add_sat(c->idle_ms,dt); c->pet_gap=add_sat(c->pet_gap,dt);
    c->interaction_cooldown=sub_sat(c->interaction_cooldown,dt);
    c->gift_wait=sub_sat(c->gift_wait,dt);
    uint64_t elapsed=(uint64_t)c->total_remainder+dt;
    if (elapsed>=60000) { c->profile.minutes=add_sat(c->profile.minutes,(uint32_t)(elapsed/60000)); c->dirty=true; }
    c->total_remainder=(uint32_t)(elapsed%60000);
    c->pose_ms=add_sat(c->pose_ms,dt);
    if (c->pose_ms<c->pose_duration) return;
    /* At most one transition after a long stall; never burst old animations. */
    switch(c->pose) {
    case CAT_DANCING: pose(c,CAT_STRETCH,6500); break;
    case CAT_APPROACH: pose(c,CAT_SNUGGLE,14000); break;
    case CAT_PAT: case CAT_ROLL: pose(c,CAT_SNUGGLE,18000); break;
    case CAT_PLAY: pose(c,CAT_GROOM,6500); break;
    case CAT_SNUGGLE: pose(c,CAT_SLEEP,24000); remember(c,4); break;
    case CAT_GIFT: pose(c,CAT_SNUGGLE,12000); break;
    default:
        if (!c->gift_wait) { pose(c,CAT_GIFT,9500); remember(c,5); c->gift_wait=180000+random_next(c)%180000; }
        else {
            static const cat_pose_t idle[]={CAT_SIT,CAT_LOOK,CAT_GROOM,CAT_STRETCH,CAT_SLEEP};
            unsigned n=random_next(c)%5;
            if (idle[n]==c->pose) n=(n+1)%5;
            pose(c,idle[n],n==4 ? 24000 : 6000+random_next(c)%5000);
            if (c->pose==CAT_SLEEP) remember(c,4);
        }
        break;
    }
}
cat_sound_t cat_act(cat_state_t *c, cat_action_t a) {
    if (a>CAT_TOY || c->interaction_cooldown) return CAT_SILENT;
    c->interaction_cooldown=220; c->idle_ms=0;
    if(a!=CAT_STROKE) c->pet_chain=0;
    cat_sound_t sound=CAT_SILENT;
    if (a==CAT_CALL) { pose(c,CAT_APPROACH,1800); remember(c,0); sound=CAT_MEW; }
    if (a==CAT_STROKE) {
        c->pet_chain=c->pet_gap<8000 ? (uint8_t)(c->pet_chain<3 ? c->pet_chain+1 : 3) : 1;
        c->pet_gap=0;
        pose(c,c->pet_chain>=3 ? CAT_ROLL : CAT_PAT, c->pet_chain>=3 ? 7000 : 4200);
        remember(c,c->pet_chain>=3 ? 2 : 1); sound=CAT_PURR;
    }
    if (a==CAT_TOY) {
        pose(c,CAT_PLAY,6500); remember(c,3); c->profile.last_toy=c->profile.toy;
        c->dirty=true; sound=CAT_CHIRP;
    }
    return c->profile.sound ? sound : CAT_SILENT;
}
cat_sound_t cat_dance(cat_state_t *c, bool start) {
    c->idle_ms=0; c->pet_chain=0;
    pose(c,start ? CAT_DANCING : CAT_STRETCH,start ? 32000 : 6500);
    return start && c->profile.sound ? CAT_DANCE : CAT_SILENT;
}
void cat_option(cat_state_t *c, unsigned row) {
    switch(row) {
    case 0: c->profile.name=(c->profile.name+1)%4; break;
    case 1: c->profile.coat=(c->profile.coat+1)%3; break;
    case 2: c->profile.toy=(c->profile.toy+1)%3; break;
    case 3: c->profile.sound=!c->profile.sound; break;
    case 4: c->profile.pocket=!c->profile.pocket; break;
    default: return;
    }
    c->dirty=true;
}
const char *cat_name(const cat_state_t *c) { static const char *n[]={"团团","糯米","豆包","年糕"}; return n[c->profile.name%4]; }
const char *cat_toy_name(unsigned i) { static const char *n[]={"纸团","毛线球","小羽毛"}; return n[i%3]; }
const char *cat_memory(unsigned i) {
    static const char *m[]={"第一次向你跑来","第一次眯眼呼噜","放心地露出肚皮","一起追过小玩具","在你身边睡着了","送给你一片叶子"}; return m[i%CAT_MEMORIES];
}
const char *cat_message(const cat_state_t *c) {
    static const char *m[CAT_POSES][3]={
        {"你忙你的，我在呢","陪你坐一小会儿","这里刚好有个位置"},
        {"窗外好像有只小鸟","发会儿呆也很好","尾巴发现了什么"},
        {"洗洗脸，慢慢来","今天也是干净小猫","耳朵也要擦一擦"},
        {"伸个懒腰，舒舒服服","爪爪再伸远一点","你也活动一下吧"},
        {"在你旁边，很安心","呼噜呼噜，睡着啦","做一个软软的梦"},
        {"听见啦，这就过来","你叫我呀","来了来了，小碎步"},
        {"就是这里，再摸一下","眼睛舒服得眯起来了","今天也很喜欢你"},
        {"肚皮也可以给你摸","翻过来啦，软乎乎","在你面前可以放心"},
        {"看我一爪抓住它","先看看，再扑过去","再陪我玩一小会儿"},
        {"再靠着你待一会儿","不用说话也很好","你在这里就很好"},
        {"捡到一片叶子，送你","这个小礼物，给你","看，我给你带了什么"},
        {"小猫上线，全场让开","爪爪一抬，烦恼拜拜","这段舞只跳给你看"}
    }; return m[c->pose][c->pose==CAT_DANCING ? (c->pose_ms/4000)%3 : c->variant%3];
}
static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++) p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t get32(const uint8_t *p) { return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24; }
static uint32_t checksum(const uint8_t *p,size_t n) { uint32_t h=2166136261u; for(size_t i=0;i<n;i++) h=(h^p[i])*16777619u; return h; }
void cat_encode(const cat_profile_t *p,uint8_t out[CAT_SAVE_BYTES]) {
    memset(out,0,CAT_SAVE_BYTES); memcpy(out,"CAT1",4);
    out[4]=p->name; out[5]=p->toy; out[6]=p->coat; out[7]=p->sound;
    out[8]=p->pocket; out[9]=p->memories; out[10]=p->last_toy;
    put32(out+12,p->seed); put32(out+16,p->minutes); put32(out+28,checksum(out,28));
}
bool cat_decode(cat_profile_t *p,const uint8_t *b,size_t n) {
    if(n!=CAT_SAVE_BYTES || memcmp(b,"CAT1",4) || get32(b+28)!=checksum(b,28) ||
       b[4]>=4 || b[5]>=3 || b[6]>=3 || b[7]>1 || b[8]>1 || b[9]>=64 || b[10]>=3 || !get32(b+12)) return false;
    *p=(cat_profile_t){.name=b[4],.toy=b[5],.coat=b[6],.sound=b[7],.pocket=b[8],.memories=b[9],.last_toy=b[10],.seed=get32(b+12),.minutes=get32(b+16)};
    return true;
}
