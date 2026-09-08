#include "cat_audio_runtime.h"
#include "cat_sound.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
static atomic_int request=ATOMIC_VAR_INIT(-1);
static atomic_bool ready;
static bool created;
static void audio_worker(void *arg) {
    (void)arg;
    bool ok=bsp_audio_init()==ESP_OK && bsp_audio_set_format(16000,16,1)==ESP_OK;
    if(ok) bsp_audio_set_volume(70);
    atomic_store(&ready,ok);
    ESP_LOGI("cat_audio","ready=%d; output level=70",ok);
    cat_voice_t voice={0}; voice.rng=esp_random(); int16_t pcm[320];
    for(;;) {
        int next=atomic_exchange(&request,-1);
        if(next>=0 && ok) {
            if(next==CAT_SILENT) cat_voice_stop(&voice);
            else { cat_voice_start(&voice,(cat_sound_t)next); ESP_LOGI("cat_audio","cue=%d",next); }
        }
        size_t n=ok ? cat_voice_render(&voice,pcm,320) : 0;
        if(n) {
            /* This worker has no UI, battery or NVS work between PCM chunks.
               I2S backpressure paces playback; never insert a per-chunk sleep. */
            if(bsp_audio_write(pcm,n*sizeof(*pcm))!=ESP_OK) {
                ok=false; atomic_store(&ready,false); ESP_LOGW("cat_audio","PCM write failed");
            }
        } else vTaskDelay(pdMS_TO_TICKS(10));
    }
}
void cat_audio_prepare(void) {
    if(created) return;
    created=xTaskCreate(audio_worker,"cat_audio",4096,NULL,5,NULL)==pdPASS;
}
bool cat_audio_ready(void) { return atomic_load(&ready); }
void cat_audio_request(cat_sound_t sound) {
    if(sound>=CAT_SILENT && sound<=CAT_DANCE) atomic_store(&request,(int)sound);
}
