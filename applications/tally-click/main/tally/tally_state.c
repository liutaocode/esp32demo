#include "tally_state.h"
#include <stddef.h>
#include <string.h>
_Static_assert(sizeof(tc_data) == 148, "Persistence format must stay stable");
static uint32_t checksum(const tc_data *d)
{
    const uint8_t *bytes = (const uint8_t *)d;
    uint32_t crc = 0xffffffffu;
    for (size_t i = 0; i < offsetof(tc_data, crc); ++i) {
        crc ^= bytes[i];
        for (unsigned j = 0; j < 8; ++j)
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}
void tc_seal(tc_data *d) { d->crc = checksum(d); }
bool tc_valid(const tc_data *d)
{
    if (d->magic != TC_MAGIC || (d->version != 1 && d->version != 2) || d->crc != checksum(d) ||
        d->count > TC_MAX || d->length > TC_HISTORY) return false;
    if (d->length != (d->serial < TC_HISTORY ? d->serial : TC_HISTORY)) return false;
    for (unsigned i = 0; i < d->length; ++i)
        if (d->records[i].count > TC_MAX ||
            d->records[i].serial != d->serial - i) return false;
    if(d->version==2) {
        if(d->reserved)return false;
        for(unsigned i=0;i<TC_HISTORY;i++)if(d->records[i].reserved)return false;
    }
    return true;
}
void tc_init(tc_state *s, const tc_data *saved)
{
    memset(s, 0, sizeof(*s));
    if (saved && tc_valid(saved)) s->data = *saved;
    else s->data.magic = TC_MAGIC;
    s->data.version = 2;
    s->data.reserved = 0;
    for (unsigned i=0;i<TC_HISTORY;i++) s->data.records[i].reserved=0;
    tc_seal(&s->data);
    s->page = TC_COUNT;
}
static int wrap(int v, int lo, int hi) { return v < lo ? hi : v > hi ? lo : v; }
tc_effect tc_input(tc_state *s, tc_button b, tc_event ev)
{
    if (b != TC_OK && ev != TC_PRESS) return TC_NONE;
    if (b == TC_OK && ev != TC_CLICK && ev != TC_DOUBLE && ev != TC_LONG) return TC_NONE;
    if (s->page == TC_HISTORY_PAGE) {
        if (b == TC_OK) s->page = TC_PAUSE;
        else if (s->data.length) s->selected = (unsigned)wrap((int)s->selected + (b == TC_DOWN ? 1 : -1),0,(int)s->data.length-1);
        return TC_REDRAW;
    }
    if (b == TC_OK) {
        if (ev == TC_LONG) return s->data.serial < UINT32_MAX ? TC_ARCHIVE : TC_NONE;
        s->page = s->page == TC_COUNT ? TC_PAUSE : TC_COUNT;
        return TC_REDRAW;
    }
    if (s->page == TC_PAUSE) {
        s->page = TC_HISTORY_PAGE; s->selected = 0;
        return TC_REDRAW;
    }
    if (b == TC_DOWN && s->data.count < TC_MAX) { ++s->data.count; return TC_DIRTY; }
    if (b == TC_UP && s->data.count > 0) { --s->data.count; return TC_DIRTY; }
    return TC_REDRAW;
}
void tc_candidate(const tc_state *s, tc_data *out)
{
    *out = s->data;
    memmove(&out->records[1], &out->records[0], (TC_HISTORY-1)*sizeof(tc_record));
    out->records[0] = (tc_record){s->data.count, s->data.serial + 1, 0};
    ++out->serial;
    if (out->length < TC_HISTORY) ++out->length;
    out->count = 0; out->reserved = 0; tc_seal(out);
}
