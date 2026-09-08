/* This TTS edition deliberately disables page-turn effects. */
#include "ebook_audio.h"
#include "tts_runtime.h"
void eb_audio_prepare(void) { tts_runtime_start(); }
void eb_audio_active(bool value) { (void)value; }
void eb_audio_play(eb_sound_t sound) { (void)sound; }
