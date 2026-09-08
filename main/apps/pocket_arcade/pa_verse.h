#pragma once
#include <stdint.h>

/* 一条名句:上句、下句,以及出处。len 是每句的字数。 */
typedef struct {
    const char *title;
    const char *author;
    const char *up;
    const char *down;
    uint8_t len;
} pa_verse_t;

extern const pa_verse_t PA_VERSE[];
extern const unsigned PA_VERSE_COUNT;
