#include "excuse_call_audio.h"
#include "bsp_audio.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdbool.h>
typedef struct { unsigned ring, generation; int64_t deadline; } request_t;
static StaticQueue_t s_control;
static uint8_t s_storage[sizeof(request_t)];
static QueueHandle_t s_requests;
static bool s_created;
static atomic_uint s_generation;
static atomic_int s_status;
static bool current(request_t r)
{ return r.generation==atomic_load(&s_generation) && esp_timer_get_time()/1000<r.deadline; }
static void worker(void *unused)
{
    (void)unused;
    bool ready=bsp_audio_init()==ESP_OK && bsp_audio_set_format(EC_AUDIO_RATE,16,1)==ESP_OK;
    if (ready) bsp_audio_set_volume(0);
    atomic_store(&s_status,ready ? 1 : -1);
    ESP_LOGI("excuse_audio", "ready=%d", ready);
    request_t r;
    for (;;) {
        if (xQueueReceive(s_requests,&r,portMAX_DELAY)!=pdTRUE) continue;
        if (!ready) continue;
        bsp_audio_set_volume(0);
        int16_t pcm[160]={0};
        /* Clear queued audio while muted, including after cancellation. */
        for (unsigned i=0;i<12;i++) {
            if (bsp_audio_write(pcm,sizeof pcm)!=ESP_OK) { ready=false; break; }
        }
        if (ready && current(r)) {
            bsp_audio_set_volume(85);
            ESP_LOGI("excuse_audio", "playing ring=%u, remaining_ms=%lld", r.ring+1, (long long)(r.deadline-esp_timer_get_time()/1000));
            uint32_t offset=0;
            int64_t started=esp_timer_get_time();
            while (current(r)) {
                ec_pcm(r.ring,offset,pcm,160);
                if (bsp_audio_write(pcm,sizeof pcm)!=ESP_OK) { ready=false; break; }
                offset+=160;
                if (offset==16000) ESP_LOGI("excuse_audio", "16000 PCM samples sent in %lld ms", (long long)((esp_timer_get_time()-started)/1000));
            }
        }
        bsp_audio_set_volume(0);
        if (!ready) atomic_store(&s_status,-1);
    }
}
void ec_audio_prepare(void)
{
    if (s_requests) return;
    s_requests=xQueueCreateStatic(1,sizeof(request_t),s_storage,&s_control);
    /* Feed DMA ahead of LVGL (priority 4); writes block at the sample clock. */
    s_created=xTaskCreate(worker,"excuse_audio",4096,NULL,5,NULL)==pdPASS;
    if (!s_created) atomic_store(&s_status,-1);
}
void ec_audio_play(unsigned ring,int64_t deadline_ms)
{
    request_t r={ring,atomic_fetch_add(&s_generation,1)+1,deadline_ms};
    if (s_created) xQueueOverwrite(s_requests,&r);
}
void ec_audio_stop(void) { ec_audio_play(0,0); }
int ec_audio_status(void) { return atomic_load(&s_status); }
