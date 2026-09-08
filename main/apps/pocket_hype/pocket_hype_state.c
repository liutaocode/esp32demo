#include "pocket_hype_state.h"
#include <string.h>
#include <limits.h>
const char *const ph_names[] = {"掌声送上", "主角登场", "悬念拉满", "冷场救援", "胜利时刻", "抱抱安慰", "惊喜盲盒"};
const char *const ph_lines[PH_SCENES][3] = {
    {"可以啊，有点东西", "这波必须给掌声", "全体起立！太强了"},
    {"让一让，主角来了", "灯光就位！看这里", "全场注意！闪亮登场"},
    {"等一下，先别揭晓", "答案马上就来", "见证奇迹的时候到了"},
    {"刚刚是谁按了静音", "没关系，我先笑一个", "冷场不存在！我来捧"},
    {"拿下！漂亮", "这就是实力", "冠军诞生！撒花"},
    {"没事，慢慢来", "已经很棒啦", "给你一个大大的拥抱"}
};
const char *const ph_cues[PH_SCENES][3] = {
    {"朋友讲完一段精彩故事", "同学终于讲完了演讲", "有人秀了一手绝活"},
    {"寿星走进了房间", "朋友要上台表演了", "今天的主角终于到了"},
    {"礼盒马上要打开了", "答案还差一秒揭晓", "大家正在等待抽签"},
    {"笑话讲完，大家安静了", "刚才的梗没人接住", "全场突然安静三秒"},
    {"朋友终于打通了关", "比赛终于赢下来了", "刚刚刷新了最高纪录"},
    {"朋友说今天有点累", "努力了却差一点成功", "伙伴失落地低下头"}
};
static uint32_t random_next(ph_state_t *s) {
    uint32_t x = s->rng; x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    return s->rng = x;
}
static uint32_t add_ms(uint32_t a, uint32_t b) { return b > UINT32_MAX - a ? UINT32_MAX : a + b; }
void ph_init(ph_state_t *s, uint32_t seed) {
    memset(s, 0, sizeof(*s)); s->rng = seed ? seed : 937U;
    s->volume = 2; s->voice = true; s->since_hit = PH_HEAT_MS;
}
unsigned ph_collected(const ph_state_t *s) {
    unsigned n = 0; for (unsigned i = 0; i < PH_SCENES; i++) n += !!(s->visited & (1U << i)); return n;
}
void ph_move(ph_state_t *s, int delta) {
    if (s->page == PH_LIVE || s->page == PH_CHALLENGE) {
        unsigned count = s->page == PH_LIVE ? PH_SCENES + 1 : PH_SCENES;
        s->selected = (s->selected + count + (delta < 0 ? -1 : 1)) % count;
        s->chain = 0; s->active = false;
    } else if (s->page == PH_MENU) s->menu = (s->menu + 5 + (delta < 0 ? -1 : 1)) % 5;
}
void ph_challenge(ph_state_t *s) {
    for (unsigned i = 0; i < PH_ROUNDS; i++) s->order[i] = i;
    for (unsigned i = PH_ROUNDS - 1; i > 0; i--) {
        unsigned j = random_next(s) % (i + 1); uint8_t t = s->order[i]; s->order[i] = s->order[j]; s->order[j] = t;
    }
    for (unsigned i = 0; i < PH_ROUNDS; i++) s->cues[i] = random_next(s) % 3;
    s->page = PH_CHALLENGE; s->round = s->score = s->selected = s->chain = 0; s->active = false;
}
static int hit(ph_state_t *s, unsigned scene, unsigned taps) {
    if (!s->chain || s->played != scene || s->since_hit >= PH_HEAT_MS) s->chain = 0;
    s->chain += taps > 1 ? 2 : 1; if (s->chain > 3) s->chain = 3;
    s->tier = s->chain - 1; s->played = scene;
    s->since_hit = s->since_show = 0; s->active = true;
    if (s->total < 9999) s->total++;
    s->visited |= 1U << scene;
    return (int)(scene * 3 + s->tier);
}
int ph_confirm(ph_state_t *s, unsigned taps) {
    if (s->page == PH_LIVE) {
        unsigned scene = s->selected;
        if (scene == PH_SCENES) {
            /* No immediately repeated scene, including after a manual cue. */
            scene = random_next(s) % (PH_SCENES - 1);
            if (scene >= s->played) scene++;
        }
        return hit(s, scene, taps);
    }
    if (s->page == PH_MENU) {
        switch (s->menu) {
            case 0: s->volume = (s->volume + 1) % 4; break;
            case 1: s->voice = !s->voice; break;
            case 2: ph_challenge(s); break;
            case 3: s->page = PH_HELP; break;
            default: s->page = PH_LIVE; break;
        }
    } else if (s->page == PH_HELP) s->page = PH_MENU;
    else if (s->page == PH_CHALLENGE) {
        unsigned answer = s->order[s->round];
        s->correct = s->selected == answer; s->score += s->correct;
        s->chain = 0; s->page = PH_FEEDBACK;
        return hit(s, answer, s->correct ? 2 : 1);
    } else if (s->page == PH_FEEDBACK) {
        s->active = false; s->selected = 0;
        if (++s->round == PH_ROUNDS) s->page = PH_RESULT; else s->page = PH_CHALLENGE;
    } else if (s->page == PH_RESULT) ph_challenge(s);
    return -1;
}
void ph_back(ph_state_t *s) {
    s->active = false; s->chain = 0;
    if (s->page == PH_LIVE) { s->page = PH_MENU; s->menu = 0; }
    else { s->page = PH_LIVE; s->selected = 0; }
}
bool ph_tick(ph_state_t *s, uint32_t ms) {
    s->since_hit = add_ms(s->since_hit, ms); s->since_show = add_ms(s->since_show, ms);
    if (s->since_hit >= PH_HEAT_MS) s->chain = 0;
    if (s->active && s->since_show >= PH_SHOW_MS) { s->active = false; return true; }
    return false;
}
