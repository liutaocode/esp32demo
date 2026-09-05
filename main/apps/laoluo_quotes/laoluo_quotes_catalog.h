#pragma once

#include <stddef.h>

typedef struct {
    const char *category;
    const char *text;
} laoluo_quote_t;

extern const laoluo_quote_t laoluo_quotes_catalog[];
extern const size_t laoluo_quotes_catalog_count;
