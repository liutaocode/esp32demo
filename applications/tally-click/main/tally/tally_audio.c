#include "tally_sound.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdbool.h>
static QueueHandle_t sounds;
static StaticQueue_t sound_control;
static uint8_t sound_storage[sizeof(tc_feedback)];
static atomic_bool ready, quiet;
static void audio_worker(void *arg)
{
    (void)arg;
    if(bsp_audio_init()!=ESP_OK || bsp_audio_set_format(TC_SOUND_RATE,16,1)!=ESP_OK) {
        ESP_LOGW("tally_audio","Audio unavailable; counter remains usable");
        vTaskDelete(NULL); return;
    }
    bsp_audio_set_volume(66); atomic_store(&ready,true);
    tc_mixer mixer={0};
    unsigned silent_frames=0;
    int16_t pcm[TC_AUDIO_FRAMES];
    for(;;) {
        tc_feedback next;
        if(xQueueReceive(sounds,&next,0)==pdTRUE) {
            tc_mixer_request(&mixer,next); silent_frames=0;
        }
        bool active=tc_mixer_busy(&mixer);
        tc_mixer_render(&mixer,pcm,TC_AUDIO_FRAMES);
        /* Keep DMA supplied even between cues. Writes pace this task at the
           hardware sample rate; no delay or trigonometry can starve playback. */
        if(bsp_audio_write(pcm,sizeof(pcm))!=ESP_OK) {
            atomic_store(&ready,false); atomic_store(&quiet,true);
            ESP_LOGW("tally_audio","Audio write failed; disabling sound");
            vTaskDelete(NULL); return;
        }
        if(active)silent_frames=0;
        else if(silent_frames<1600)silent_frames+=TC_AUDIO_FRAMES;
        atomic_store(&quiet,silent_frames>=1600);
    }
}

void tc_audio_start(void)
{
    if(sounds)return;
    sounds=xQueueCreateStatic(1,sizeof(tc_feedback),sound_storage,&sound_control);
    if(xTaskCreate(audio_worker,"tally_audio",4096,NULL,6,NULL)!=pdPASS)
        ESP_LOGW("tally_audio","Could not create audio worker");
}
void tc_audio_request(tc_feedback sound)
{
    if(atomic_load(&ready) && sound>TC_FX_NONE && sound<=TC_FX_ERROR) {
        atomic_store(&quiet,false);
        xQueueOverwrite(sounds,&sound);
        atomic_store(&quiet,false);
    }
}

bool tc_audio_quiet(void) { return !atomic_load(&ready) || atomic_load(&quiet); }
