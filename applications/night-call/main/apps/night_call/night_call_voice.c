#include "night_call_audio.h"
#include "minecraft_adpcm.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
extern const uint8_t nc_data_start[] __asm__("_binary_night_call_adpcm_bin_start");
extern const uint8_t nc_data_end[] __asm__("_binary_night_call_adpcm_bin_end");
typedef struct { int clip; unsigned volume, generation; } request_t;
static StaticQueue_t s_control;
static uint8_t s_buffer[sizeof(request_t)];
static QueueHandle_t s_queue;
static atomic_uint s_generation, s_completed;
static atomic_bool s_ready;
static bool s_started;
static void play(request_t r) {
    if (r.clip < 0 || r.clip >= (int)NC_AUDIO_COUNT || !r.volume || !atomic_load(&s_ready)) return;
    size_t size = (size_t)(nc_data_end - nc_data_start);
    const nc_clip_t *c = &nc_clips[r.clip];
    if (size != nc_audio_bytes || !nc_clip_valid(c, size)) { atomic_store(&s_ready, false); return; }
    static const uint8_t volumes[] = {0, 35, 55, 75};
    bsp_audio_set_volume(volumes[r.volume < 4 ? r.volume : 1]);
    minecraft_adpcm_state_t d; minecraft_adpcm_init(&d, c->predictor, c->step);
    uint32_t sample = 0;
    int16_t pcm[256];
    while (sample < c->samples && r.generation == atomic_load(&s_generation)) {
        unsigned n = 0;
        while (n < 256 && sample < c->samples) {
            if (!sample) pcm[n++] = (int16_t)d.predictor;
            else {
                uint32_t nibble = sample - 1;
                uint8_t b = nc_data_start[c->offset + nibble / 2];
                pcm[n++] = minecraft_adpcm_decode(&d, nibble & 1 ? b >> 4 : b & 15);
            }
            sample++;
        }
        if (r.generation != atomic_load(&s_generation)) break;
        if (bsp_audio_write(pcm, n * sizeof(int16_t)) != ESP_OK) { atomic_store(&s_ready, false); break; }
    }
}
static void worker(void *arg) {
    (void)arg;
    /* This service has application lifetime and never owns a screen pointer. */
    if (bsp_audio_set_format(NC_AUDIO_RATE, 16, 1) != ESP_OK) atomic_store(&s_ready, false);
    bsp_audio_set_volume(0);
    request_t r;
    for (;;) {
        if (xQueueReceive(s_queue, &r, portMAX_DELAY) != pdTRUE) continue;
        if (r.generation == atomic_load(&s_generation)) play(r);
        bsp_audio_set_volume(0);
        atomic_store(&s_completed, r.generation);
    }
}
void nc_audio_start(bool available) {
    if (s_started) return;
    s_started = true;
    if (!available) return;
    s_queue = xQueueCreateStatic(1, sizeof(request_t), s_buffer, &s_control);
    atomic_store(&s_ready, true);
    if (xTaskCreate(worker, "night_voice", 4096, NULL, 3, NULL) != pdPASS) {
        atomic_store(&s_ready, false); s_queue = NULL;
    }
}
void nc_audio_play(unsigned clip, unsigned volume) {
    if (!s_queue || !atomic_load(&s_ready) || clip >= NC_AUDIO_COUNT) return;
    request_t r = {(int)clip, volume, atomic_fetch_add(&s_generation, 1) + 1};
    (void)xQueueOverwrite(s_queue, &r);
}
void nc_audio_stop(void) {
    if (!s_queue) return;
    request_t r = {-1, 0, atomic_fetch_add(&s_generation, 1) + 1};
    (void)xQueueOverwrite(s_queue, &r);
}
bool nc_audio_ready(void) { return atomic_load(&s_ready); }
bool nc_audio_busy(void) { return atomic_load(&s_ready) && atomic_load(&s_completed) != atomic_load(&s_generation); }
