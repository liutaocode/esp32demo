#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define DS_MAX_ENTRIES 64
#define DS_SAVE_SIZE (12 + DS_MAX_ENTRIES * 3)
typedef struct { int year, month, day, hour, minute; } ds_date_t;
typedef struct { const char *name; int64_t utc; } ds_node_t;
typedef struct {
    uint16_t id, year;
    uint8_t field, count;
    const char *name, *track, *note;
    ds_node_t nodes[3];
} ds_entry_t;
extern const ds_entry_t ds_catalog[];
extern const unsigned ds_catalog_count;
extern const int64_t ds_checked_utc;
extern const char *const ds_fields[6];
typedef struct {
    uint8_t flags[DS_MAX_ENTRIES]; /* bit 0 favorite, bits 1..4 checklist */
    uint16_t sessions;
    ds_date_t seed; /* editor seed only; never an authoritative clock */
} ds_progress_t;
int ds_month_days(int year, int month);
bool ds_date_valid(ds_date_t date);
int64_t ds_date_utc(ds_date_t date);
ds_date_t ds_beijing(int64_t utc);
void ds_date_move(ds_date_t *date, unsigned field, int delta);
int ds_next_node(const ds_entry_t *entry, int64_t now);
unsigned ds_list(unsigned *out, unsigned field, unsigned year,
                 const ds_progress_t *progress, int64_t now);
unsigned ds_checks(uint8_t flags);
unsigned ds_urgency(int64_t deadline, int64_t now);
void ds_encode(const ds_progress_t *p, uint8_t bytes[DS_SAVE_SIZE]);
bool ds_decode(ds_progress_t *p, const uint8_t bytes[DS_SAVE_SIZE]);
