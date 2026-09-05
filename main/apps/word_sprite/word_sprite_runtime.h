#pragma once
#include "word_sprite_state.h"

/* Start/load on app_main, before enter; all subsequent I/O is worker-owned. */
ws_progress_t ws_runtime_start(bool audio_available);
bool ws_runtime_audio_ready(void);
/* 0: session only, 1: saved, 2: saving, 3: save failed. */
int ws_runtime_save_status(void);
void ws_runtime_save(ws_progress_t progress);
void ws_runtime_speak(int first, int second);
/* Optional Mandarin cue, Mandarin meaning, then English pronunciation. */
void ws_runtime_explain(unsigned word, int cue);
void ws_runtime_stop_audio(void);
void ws_runtime_shutdown(void);
bool ws_runtime_stopped(void);
