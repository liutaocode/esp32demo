#pragma once

#include <stdbool.h>

/* Start once after BSP audio initialization; the worker never touches LVGL. */
void vibe_check_voice_start(bool audio_available);
/* Queue only the newest narration request; playback remains worker-owned. */
void vibe_check_voice_speak(unsigned clip);
void vibe_check_voice_stop(void);
bool vibe_check_voice_ready(void);
