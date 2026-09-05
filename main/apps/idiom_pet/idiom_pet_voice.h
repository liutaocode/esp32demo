#pragma once
#include <stdbool.h>

/* Start once after BSP audio initialization, outside the LVGL lock. */
void ip_voice_start(bool audio_available);
/* Nonblocking latest-request delivery. Worker never holds any UI pointers. */
void ip_voice_speak(unsigned question);
void ip_voice_stop(void);
bool ip_voice_ready(void);
