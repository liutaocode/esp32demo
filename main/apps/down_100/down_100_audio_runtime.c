#include "down_100_audio.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>

typedef struct { d100_sound_t sound; unsigned generation; } request_t;
static StaticQueue_t s_control;
static uint8_t s_storage[sizeof(request_t)];
static QueueHandle_t s_requests;
static bool s_worker_created;
static atomic_bool s_active;
static atomic_uint s_generation, s_completed_generation;

static bool current(const request_t *request)
{
    return atomic_load(&s_active) && request->generation == atomic_load(&s_generation);
}
static void worker(void *arg)
{
    (void)arg;
    bool ready = bsp_audio_init() == ESP_OK && bsp_audio_set_format(D100_AUDIO_RATE, 16, 1) == ESP_OK;
    if (ready) bsp_audio_set_volume(0);
    request_t request;
    for (;;) {
        if (xQueueReceive(s_requests, &request, portMAX_DELAY) != pdTRUE) continue;
        if (ready && current(&request) && request.sound != D100_SOUND_NONE) {
            bsp_audio_set_volume(55);
            size_t offset = 0, total = d100_sound_samples(request.sound);
            int16_t pcm[160];
            /* Ten ms chunks make newer cues, pause and exit cancellable.
               Flush 90 ms of silence so the final note can leave I2S DMA. */
            while (offset < total + 1440 && current(&request)) {
                size_t count = d100_sound_render(request.sound, offset, pcm, 160);
                if (!count) { for (unsigned i = 0; i < 160; i++) pcm[i] = 0; count = 160; }
                if (bsp_audio_write(pcm, count * sizeof(*pcm)) != ESP_OK) { ready = false; break; }
                offset += count;
            }
            bsp_audio_set_volume(0);
        }
        /* No UI pointer is retained. Exit may delete its screen immediately;
           this acknowledgement means all PCM writes and muting have ended. */
        atomic_store(&s_completed_generation, request.generation);
    }
}
void d100_audio_prepare(void)
{
    if (s_requests) return;
    s_requests = xQueueCreateStatic(1, sizeof(request_t), s_storage, &s_control);
    s_worker_created = xTaskCreate(worker, "down100_sound", 4096, NULL, 2, NULL) == pdPASS;
    if (!s_worker_created) {
        /* Retain the empty static queue; repeated enters must not retry or leak. */
        atomic_store(&s_completed_generation, atomic_load(&s_generation));
    }
}
void d100_audio_active(bool active)
{
    atomic_store(&s_active, active);
    if (!active) {
        request_t request = {D100_SOUND_NONE, atomic_fetch_add(&s_generation, 1) + 1};
        if (s_worker_created) xQueueOverwrite(s_requests, &request);
    }
}
void d100_audio_play(d100_sound_t sound)
{
    if (!s_worker_created || !atomic_load(&s_active) || !d100_sound_samples(sound)) return;
    request_t request = {sound, atomic_fetch_add(&s_generation, 1) + 1};
    xQueueOverwrite(s_requests, &request);
}
bool d100_audio_idle(void)
{
    return !s_worker_created || atomic_load(&s_completed_generation) == atomic_load(&s_generation);
}
