#pragma once
#include <stdbool.h>
#include <stdint.h>
#define TC_MAX 9999u
#define TC_HISTORY 10u
#define TC_MAGIC 0x54434c31u
/* Reserved words retain the v1 blob layout for count/history migration.
   Version 2 never stores wall-clock information; every reserved word is zero. */
typedef struct { uint32_t count, serial, reserved; } tc_record;
typedef struct {
    uint32_t magic, version, count, serial, length, reserved;
    tc_record records[TC_HISTORY];
    uint32_t crc;
} tc_data;
typedef enum { TC_COUNT, TC_PAUSE, TC_HISTORY_PAGE } tc_page;
typedef enum { TC_UP, TC_DOWN, TC_OK } tc_button;
typedef enum { TC_PRESS, TC_CLICK, TC_DOUBLE, TC_LONG } tc_event;
typedef enum { TC_NONE, TC_REDRAW, TC_DIRTY, TC_ARCHIVE } tc_effect;
typedef struct { tc_data data; tc_page page; unsigned selected; } tc_state;
void tc_init(tc_state *s, const tc_data *saved);
tc_effect tc_input(tc_state *s, tc_button b, tc_event ev);
void tc_candidate(const tc_state *s, tc_data *out);
void tc_seal(tc_data *data);
bool tc_valid(const tc_data *data);
