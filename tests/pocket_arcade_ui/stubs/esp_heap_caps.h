#pragma once
#include <stddef.h>
#define MALLOC_CAP_INTERNAL 0
static inline size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return 0; }
