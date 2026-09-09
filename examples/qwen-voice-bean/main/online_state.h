#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
typedef enum { ONLINE_SETUP, ONLINE_CONNECTING, ONLINE_READY, ONLINE_LISTENING,
    ONLINE_THINKING, ONLINE_SPEAKING, ONLINE_ERROR } online_phase_t;
/* Configuration is stored as one versioned NVS blob, never in the image. */
typedef struct { uint32_t version; char ssid[33], password[65], url[256], token[193]; } online_config_t;
bool online_config_valid(const online_config_t *c);
bool online_fragment(size_t *used, size_t capacity, size_t offset, size_t length, size_t total);
const char *online_phase_text(online_phase_t phase);

bool online_endpoint_from_ip(const char *ip,char *out,size_t size);
void online_setup_password(uint32_t random_value,char out[9]);

typedef enum { ONLINE_HOME, ONLINE_SETTINGS, ONLINE_CONFIRM } online_page_t;
typedef enum { ONLINE_KEY_OK=1, ONLINE_KEY_UP, ONLINE_KEY_DOWN, ONLINE_KEY_LONG } online_key_t;
typedef enum { ONLINE_ACTION_NONE, ONLINE_ACTION_MIC, ONLINE_ACTION_VOLUME,
    ONLINE_ACTION_CANCEL, ONLINE_ACTION_SETUP } online_action_t;
typedef struct { online_page_t page; unsigned selected; } online_menu_t;
online_action_t online_menu_key(online_menu_t *menu,online_key_t key);

/* Start once roughly 300 ms is queued, or after a bounded short-answer wait. */
bool online_playback_prefill_wait(unsigned queued,unsigned elapsed_ms);
