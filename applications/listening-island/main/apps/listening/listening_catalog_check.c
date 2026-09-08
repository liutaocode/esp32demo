#include "listening_catalog.h"
bool li_clip_valid(const li_clip_t *c,size_t size) {
 return c && c->bank<2 && c->step<=88 && c->samples>0 && c->samples<=16000*15 &&
 c->bytes==c->samples/2 && c->offset<=size && c->bytes<=size-c->offset;
}
