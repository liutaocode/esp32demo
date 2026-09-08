#pragma once
#include "listening_state.h"
li_progress_t li_runtime_start(bool audio);
void li_runtime_speak(int id,uint32_t serial,unsigned repeats,unsigned volume);
void li_runtime_save(const li_progress_t *p);
bool li_runtime_audio_ok(void);
uint32_t li_runtime_completed(void);
int li_runtime_save_status(void);
void li_runtime_shutdown(void);
bool li_runtime_stopped(void);
