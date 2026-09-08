#include "bean_state.h"
#include <string.h>
const char *const bean_lines[BEAN_LINES] = {
    "嗯，你接着说。", "听懂了，但只懂一点。", "这事得问另一颗豆。", "我的脑袋刚路过鸭子。",
    "有道理，下次再想。", "要不先喝口水？", "我赞成先躺一会。", "你说，我负责点头。",
    "摸我？算你有眼光。", "再摸一下，就一下。", "我才没有偷偷开心。", "好吧，今天你最棒。",
    "别停，我还没装够。", "这颗豆被你摸熟了。", "我是豆，不是门铃。", "再戳我就变豆浆了。",
    "你是不是有点闲？", "哼，我记你三秒钟。", "我宣布，你赢了。", "别闹，我快绷不住。",
    "今天的云像个馒头。", "我在给空气放假。", "刚刚差点想通宇宙。", "发呆也是正经事。",
    "我先眯一小会。", "梦里也不加班。", "嘘，豆子正在充电。", "晚安，醒了再嘴硬。",
    "你一来，我就醒了。", "刚才那句，挺热闹。", "我听见啦，继续呀。", "这声音，有点意思。",
    "夸我还是逗我，选好。", "打一巴掌给颗糖？", "你把我整不会了。", "我决定原谅你一秒。",
    "好了好了，笑出声了。", "这局算你把我逗笑。", "我没笑，是脸打滑。", "忍住，豆设不能崩。"
};
const char *const bean_faces[BEAN_FACES] = {"发呆", "得意", "嘴硬", "偷笑", "困困", "认真", "迷糊", "笑崩"};
static uint32_t random_next(bean_t *s) {
    s->rng ^= s->rng << 13; s->rng ^= s->rng >> 17; s->rng ^= s->rng << 5;
    return s->rng;
}
static void choose(bean_t *s, unsigned first, unsigned count, unsigned face, uint64_t now) {
    int next = (int)(first + random_next(s) % count);
    if (next == s->line) next = (int)(first + ((unsigned)next - first + 1) % count);
    s->line = next; s->face = (int)face; s->discovered |= 1u << face;
    s->phase = BEAN_THINK; s->since = now; s->last_action = now;
    s->think_ms = 1200 + random_next(s) % 2001;
    s->next_idle = now + 45000 + random_next(s) % 30000;
}
void bean_init(bean_t *s, uint32_t seed, uint64_t now) {
    memset(s, 0, sizeof(*s)); s->rng = seed ? seed : 17; s->line = -1;
    s->last_key = -1; s->volume = 2; s->auto_listen = true;
    s->discovered = 1; s->since = s->last_action = now; s->next_idle = now + 60000;
}
void bean_cancel(bean_t *s, uint64_t now) {
    s->phase = BEAN_IDLE; s->since = s->last_action = now;
    s->next_idle = now + 60000; s->noise = false;
}
void bean_key(bean_t *s, int key, uint64_t now) {
    if (key < 0 || key > 2) return;
    bool alternate = s->last_key >= 0 && s->last_key != key && key != 2 && s->last_key != 2 && now - s->last_action < 7000;
    s->streak = s->last_key == key && now - s->last_action < 7000 ? s->streak + 1 : 1;
    if (s->streak > 9) s->streak = 9;
    s->last_key = key; s->noise = false;
    if (key == 2) choose(s, 0, 8, 0, now);
    else if (alternate) choose(s, 32, 4, 6, now);
    else if (key == 0) choose(s, s->streak >= 3 ? 12 : 8, s->streak >= 3 ? 2 : 4, s->streak >= 3 ? 3 : 1, now);
    else if (s->streak >= 5) choose(s, 36, 4, 7, now);
    else choose(s, 14, 6, 2, now);
}
void bean_tick(bean_t *s, uint64_t now, bool sound, bool busy) {
    if (s->phase != BEAN_TALK && s->auto_listen && sound) {
        s->last_sound = now;
        if (s->phase != BEAN_LISTEN) {
            s->phase = BEAN_LISTEN; s->since = now; s->face = 5;
            s->discovered |= 1u << 5;
        }
    }
    if (s->phase == BEAN_LISTEN) {
        if (now - s->since >= 15000) s->noise = true;
        if (now - s->last_sound >= 1000) {
            if (s->noise) bean_cancel(s, now);
            else choose(s, 28, 4, 5, now);
        }
    } else if (s->phase == BEAN_THINK && now - s->since >= s->think_ms) {
        s->phase = BEAN_TALK; s->since = now;
    } else if (s->phase == BEAN_TALK && !busy && now - s->since >= 3200) {
        s->phase = BEAN_IDLE; s->since = now;
    } else if (s->phase == BEAN_IDLE && now - s->last_action >= 120000) {
        s->phase = BEAN_REST; s->face = 4; s->discovered |= 1u << 4;
        s->line = 24 + (int)(random_next(s) % 4);
    } else if (s->phase == BEAN_IDLE && now >= s->next_idle) {
        /* An idle remark does not postpone rest indefinitely. */
        uint64_t activity = s->last_action;
        choose(s, 20, 4, 0, now); s->last_action = activity;
    }
}
unsigned bean_count(uint32_t mask) { unsigned n=0; for (unsigned i=0;i<BEAN_FACES;i++) n += (mask>>i)&1u; return n; }
bool bean_vad_tick(bean_vad_t *v, unsigned amplitude) {
    if (!v->floor) v->floor = 100;
    unsigned threshold = v->floor * 3 + 250;
    if (threshold < 650) threshold = 650;
    if (amplitude > threshold) {
        v->quiet = 0; if (v->voiced < 8) v->voiced++;
        if (v->voiced >= 8) v->active = true;
    } else {
        v->voiced = 0; if (v->quiet < 6) v->quiet++;
        if (v->quiet >= 6) v->active = false;
        v->floor = (v->floor * 63 + amplitude) / 64;
    }
    return v->active;
}

unsigned bean_ear_raise(uint64_t elapsed) {
    if (elapsed < 280) return (unsigned)(elapsed * 36 / 280);
    unsigned phase = (unsigned)((elapsed - 280) % 1400);
    return 36 + (phase < 700 ? phase : 1400 - phase) / 350;
}
