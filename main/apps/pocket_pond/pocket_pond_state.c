#include "pocket_pond_state.h"
#include <string.h>
const char *const pp_names[PP_FISH] = {
    "豆豆鱼", "荷包鱼", "柠檬鱼", "西瓜鱼", "云朵鱼", "晚霞鱼", "月亮鱼", "星星鱼", "锦鲤王"
};
const char *const pp_notes[PP_FISH] = {
    "小小一条，也是收获", "把今天的好运装进口袋", "酸酸的日子也能发光", "摸鱼也要甜一点", "慢慢游，云会等你", "把落日捞进小鱼缸", "今晚的月亮被你捞到", "一闪一闪，全是好运", "今日份好运，稳稳接住"
};
const uint8_t pp_values[PP_FISH] = {1, 1, 2, 2, 3, 3, 4, 5, 8};
static uint32_t random_next(uint32_t *seed)
{
    *seed ^= *seed << 13; *seed ^= *seed >> 17; *seed ^= *seed << 5;
    return *seed;
}
void pp_start(pp_state_t *s, uint32_t seed)
{
    pp_progress_t p = s->progress;
    memset(s, 0, sizeof(*s)); s->progress = p;
    s->page = PP_PLAY; s->last = PP_CARDS;
    if (!seed) seed = 0x91E10DA5;
    for (unsigned i = 0; i < PP_CARDS; i++) s->deck[i] = i < PP_FISH ? i : PP_FISH;
    for (unsigned i = PP_CARDS - 1; i > 0; i--) {
        /* Rejection sampling avoids a modulo-biased shuffle. */
        uint32_t r, limit = UINT32_MAX - UINT32_MAX % (i + 1);
        do { r = random_next(&seed) - 1U; } while (r >= limit);
        uint8_t t = s->deck[i]; s->deck[i] = s->deck[r % (i + 1)]; s->deck[r % (i + 1)] = t;
    }
}
unsigned pp_species(const pp_progress_t *p)
{
    unsigned n = 0;
    for (unsigned i = 0; i < PP_FISH; i++) n += p->caught[i] != 0;
    return n;
}
unsigned pp_remaining_fish(const pp_state_t *s)
{
    unsigned n = 0;
    for (unsigned i = 0; i < PP_FISH; i++) n += !(s->basket & (1U << i));
    return n;
}
static void finish(pp_state_t *s)
{
    s->page = PP_RESULT;
    if (s->progress.trips < 9999) s->progress.trips++;
}
bool pp_bank(pp_state_t *s)
{
    if (s->page != PP_PLAY || !s->basket) return false;
    for (unsigned i = 0; i < PP_FISH; i++)
        if ((s->basket & (1U << i)) && s->progress.caught[i] < 999) s->progress.caught[i]++;
    s->record = s->score > s->progress.best;
    if (s->record) s->progress.best = s->score;
    finish(s); return true;
}
bool pp_draw(pp_state_t *s)
{
    if (s->page != PP_PLAY || s->cursor >= PP_CARDS) return false;
    s->last = s->deck[s->cursor++];
    if (s->last == PP_FISH) {
        if (++s->waves == 2) { s->lost = true; finish(s); }
    } else {
        s->basket |= 1U << s->last;
        s->score += pp_values[s->last];
        if (!pp_remaining_fish(s)) pp_bank(s);
    }
    return true;
}
void pp_encode(const pp_progress_t *p, uint8_t b[PP_SAVE_SIZE])
{
    b[0] = 1; b[1] = 0;
    for (unsigned i = 0; i < 11; i++) {
        unsigned v = i < PP_FISH ? p->caught[i] : i == 9 ? p->best : p->trips;
        b[2 + i * 2] = v & 255; b[3 + i * 2] = v >> 8;
    }
}
bool pp_decode(pp_progress_t *p, const uint8_t b[PP_SAVE_SIZE])
{
    if (b[0] != 1 || b[1] != 0) return false;
    pp_progress_t loaded = {0};
    for (unsigned i = 0; i < 11; i++) {
        unsigned v = b[2 + i * 2] | (unsigned)b[3 + i * 2] << 8;
        if (i < PP_FISH) { if (v > 999) return false; loaded.caught[i] = v; }
        else if (i == 9) { if (v > 29) return false; loaded.best = v; }
        else { if (v > 9999) return false; loaded.trips = v; }
    }
    *p = loaded; return true;
}
