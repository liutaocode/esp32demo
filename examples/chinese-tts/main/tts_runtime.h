#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef enum { TTS_LOADING, TTS_READY, TTS_PREPARING, TTS_SPEAKING, TTS_STOPPED, TTS_ERROR } tts_status_t;
typedef struct {
    tts_status_t status;
    esp_err_t error;
    uint32_t audio_ms;
    uint32_t synth_ms;
    uint32_t min_heap;
} tts_snapshot_t;
/* Call from one controlling task. Worker owns parser, codec and voice mapping.
 * Submission copies UTF-8 text; a newer request cancels and replaces the old one.
 * Normal speed streams 20 ms Speex frames directly to I2S.
 * Other speeds buffer one syllable for integer pitch-preserving time stretching.
 * No whole-sentence PCM cache or Flash writes. Split long text at sentence boundaries.
 * No LVGL access occurs here. stop is nonblocking; shutdown waits for worker exit. */
esp_err_t tts_runtime_start(void);
esp_err_t tts_runtime_say(const char *text, unsigned speed);
void tts_runtime_stop(void);
bool tts_runtime_shutdown(uint32_t timeout_ms);
tts_snapshot_t tts_runtime_snapshot(void);
