#pragma once
#include <stddef.h>
#include <stdint.h>
#define PVZ_COUNT 24
#define PVZ_PLANTS 16
#define PVZ_ROUNDS 5
#define PVZ_OPTIONS 3
typedef struct {
    const char *id, *name, *role, *ability, *tip, *clue;
    int16_t cost;
    uint8_t kind;
    uint32_t color;
} pvz_entry_t;
extern const pvz_entry_t pvz_catalog[PVZ_COUNT];
