#include "pocket_pond_state.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static void ordered(pp_state_t *s)
{
    pp_start(s, 1);
    for (unsigned i = 0; i < PP_CARDS; i++) s->deck[i] = i < PP_FISH ? i : PP_FISH;
}
int main(void)
{
    pp_state_t s = {0};
    ordered(&s); assert(!pp_bank(&s));
    for (unsigned i = 0; i < 9; i++) assert(pp_draw(&s));
    assert(s.page == PP_RESULT && s.score == 29 && s.record && !s.lost);
    assert(pp_species(&s.progress) == 9 && s.progress.best == 29 && s.progress.trips == 1);
    assert(!pp_draw(&s) && !pp_bank(&s));
    ordered(&s); pp_draw(&s); pp_draw(&s); assert(pp_bank(&s));
    assert(s.progress.caught[0] == 2 && s.progress.caught[2] == 1);
    pp_progress_t previous = s.progress;
    ordered(&s); s.deck[1] = s.deck[2] = PP_FISH;
    pp_draw(&s); pp_draw(&s); assert(s.waves == 1 && s.page == PP_PLAY);
    pp_draw(&s); assert(s.page == PP_RESULT && s.lost);
    assert(memcmp(previous.caught, s.progress.caught, sizeof(previous.caught)) == 0);
    assert(s.progress.best == previous.best && s.progress.trips == previous.trips + 1);
    for (unsigned seed = 0; seed < 20000; seed++) {
        pp_state_t a = {0}, b = {0}; pp_start(&a, seed); pp_start(&b, seed);
        assert(memcmp(a.deck, b.deck, PP_CARDS) == 0);
        unsigned n[10] = {0};
        for (unsigned j = 0; j < PP_CARDS; j++) { assert(a.deck[j] <= PP_FISH); n[a.deck[j]]++; }
        for (unsigned j = 0; j < PP_FISH; j++) assert(n[j] == 1);
        assert(n[9] == 3);
        while (a.page == PP_PLAY) {
            unsigned fish = 0, waves = 0;
            for (unsigned j = a.cursor; j < PP_CARDS; j++) { if (a.deck[j] == PP_FISH) waves++; else fish++; }
            assert(fish == pp_remaining_fish(&a) && waves == 3U - a.waves);
            assert(pp_draw(&a)); assert(a.score <= 29 && a.cursor <= 11);
        }
        assert(a.lost || a.score == 29);
    }
    s.progress = (pp_progress_t){.best=29,.trips=9999};
    for (unsigned i=0;i<PP_FISH;i++) s.progress.caught[i]=999;
    ordered(&s); pp_draw(&s); pp_bank(&s);
    assert(s.progress.caught[0]==999 && s.progress.trips==9999);
    uint8_t bytes[PP_SAVE_SIZE]; pp_encode(&s.progress,bytes);
    pp_progress_t decoded={0}; assert(pp_decode(&decoded,bytes));
    assert(memcmp(&decoded,&s.progress,sizeof(decoded))==0);
    for(unsigned i=0;i<PP_SAVE_SIZE;i++) {
        uint8_t bad[PP_SAVE_SIZE]; memcpy(bad,bytes,sizeof(bad)); bad[i]=255;
        pp_progress_t dst=decoded;
        if(!pp_decode(&dst,bad)) assert(memcmp(&dst,&decoded,sizeof(dst))==0);
    }
    bytes[0]=2; assert(!pp_decode(&decoded,bytes));
    puts("Pocket Pond: 20000 decks, bank/loss, odds, caps and save format PASS");
}
