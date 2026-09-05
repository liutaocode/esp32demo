#include "pvz_audio.h"
#include "minecraft_adpcm.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>
extern const uint8_t pvz_blob_start[] asm("_binary_pvz_adpcm_bin_start");
extern const uint8_t pvz_blob_end[] asm("_binary_pvz_adpcm_bin_end");
typedef struct { int clip; unsigned epoch; } request_t;
static QueueHandle_t requests;
static SemaphoreHandle_t stopped;
static atomic_uint epoch, report;
static bool available;
static void playback(request_t r) {
    const pvz_clip_t *c = &pvz_clips[r.clip];
    if (!pvz_clip_valid(c, pvz_blob_end - pvz_blob_start) ||
        bsp_audio_set_format(16000, 16, 1) != ESP_OK) {
        atomic_store(&report, (r.epoch << 2) | 3U);
        return;
    }
    bsp_audio_set_volume(65);
    minecraft_adpcm_state_t decoder;
    minecraft_adpcm_init(&decoder, c->predictor, c->step);
    int16_t pcm[256];
    uint32_t sample = 0;
    while (sample < c->samples) {
        if (atomic_load(&epoch) != r.epoch) return;
        unsigned n = 0;
        while (n < 256 && sample < c->samples) {
            if (!sample) pcm[n++] = decoder.predictor;
            else {
                uint32_t nibble = sample - 1;
                uint8_t packed = pvz_blob_start[c->offset + nibble / 2];
                pcm[n++] = minecraft_adpcm_decode(&decoder,
                    (nibble & 1) ? packed >> 4 : packed & 15);
            }
            ++sample;
        }
        if (bsp_audio_write(pcm, n * sizeof(*pcm)) != ESP_OK) {
            atomic_store(&report, (r.epoch << 2) | 3U);
            return;
        }
    }
    atomic_store(&report, (r.epoch << 2) | 2U);
}
static void worker(void *arg) {
    (void)arg;
    request_t r;
    while (xQueueReceive(requests, &r, portMAX_DELAY) == pdTRUE) {
        if (r.clip == -2) break;
        if (r.clip == -1) atomic_store(&report, r.epoch << 2);
        if (r.clip >= 0 && r.clip < PVZ_AUDIO_COUNT && atomic_load(&epoch) == r.epoch) playback(r);
    }
    xSemaphoreGive(stopped);
    vTaskDelete(NULL);
}
bool pvz_audio_start(bool hardware_ok) {
    if (requests) return available;
    available = false;
    if (!hardware_ok) return false;
    requests = xQueueCreate(1, sizeof(request_t));
    stopped = xSemaphoreCreateBinary();
    if (!requests || !stopped || xTaskCreate(worker, "pvz_voice", 4096, NULL, 3, NULL) != pdPASS) {
        if (requests) vQueueDelete(requests);
        if (stopped) vSemaphoreDelete(stopped);
        requests = NULL; stopped = NULL;
        return false;
    }
    available = true;
    atomic_store(&report, atomic_load(&epoch) << 2);
    return true;
}
void pvz_audio_request(int clip) {
    if (!available || clip < -1 || clip >= PVZ_AUDIO_COUNT) return;
    unsigned e = atomic_fetch_add(&epoch, 1) + 1;
    atomic_store(&report, (e << 2) | (clip < 0 ? 0U : 1U));
    request_t r = {clip, e};
    xQueueOverwrite(requests, &r);
}
int pvz_audio_status(void) {
    if (!available) return -1;
    unsigned value = atomic_load(&report);
    if ((value >> 2) != (atomic_load(&epoch) & 0x3fffffffU)) return 1;
    return (value & 3U) == 3 ? -1 : (int)(value & 3U);
}
void pvz_audio_stop(void) {
    if (!requests) return;
    available = false;
    request_t r = {-2, atomic_fetch_add(&epoch, 1) + 1};
    xQueueOverwrite(requests, &r);
    xSemaphoreTake(stopped, portMAX_DELAY);
    vQueueDelete(requests); vSemaphoreDelete(stopped);
    requests = NULL; stopped = NULL;
}
