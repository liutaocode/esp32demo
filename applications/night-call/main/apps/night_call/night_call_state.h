#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#define NC_ENDINGS 7
#define NC_CLUES 6
#define NC_CHAPTERS 3
#define NC_SAVE_SIZE 24
#define NC_TARGET_RESOLVE 1000
#define NC_TARGET_HOME 1001
typedef struct { const char *text; int target; } nc_choice_t;
typedef struct { const char *title, *body; nc_choice_t choices[2]; int clue; unsigned chapter; } nc_node_t;
typedef struct { const char *title, *body, *hint; } nc_ending_t;
typedef struct { const char *title, *body; } nc_clue_t;
extern const nc_node_t nc_nodes[];
extern const unsigned nc_node_count, nc_starts[NC_CHAPTERS], nc_missing_node;
extern const char *const nc_chapters[NC_CHAPTERS];
extern const nc_ending_t nc_endings[NC_ENDINGS];
extern const nc_clue_t nc_clues[NC_CLUES];
typedef struct { uint16_t node; uint8_t chapter, clues, endings, volume; bool active; } nc_state_t;
typedef enum { NC_STORY, NC_END, NC_HOME, NC_IGNORED } nc_result_t;
void nc_init(nc_state_t *s);
void nc_begin(nc_state_t *s, unsigned chapter);
nc_result_t nc_choose(nc_state_t *s, unsigned choice, int *ending);
unsigned nc_count(unsigned bits);
bool nc_state_valid(const nc_state_t *s);
void nc_encode(const nc_state_t *s, uint8_t out[NC_SAVE_SIZE]);
bool nc_decode(nc_state_t *s, const uint8_t *data, size_t length);
