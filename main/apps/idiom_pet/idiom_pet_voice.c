#include "idiom_pet_voice.h"
#include "idiom_pet_audio.h"
#include "minecraft_adpcm.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>

extern const uint8_t ip_voice_start_data[] asm("_binary_idiom_pet_adpcm_bin_start");
extern const uint8_t ip_voice_end_data[] asm("_binary_idiom_pet_adpcm_bin_end");
typedef struct { int question; unsigned generation; } request_t;
static StaticQueue_t s_control;
static uint8_t s_buffer[sizeof(request_t)];
static QueueHandle_t s_requests;
static atomic_uint s_generation;
static atomic_bool s_ready;
static bool s_started;

static void play(request_t request)
{
    if (!atomic_load(&s_ready) || request.question < 0 || request.question >= IP_QUESTIONS) return;
    const ip_audio_clip_t *clip = &ip_audio_clips[request.question];
    size_t size = (size_t)(ip_voice_end_data - ip_voice_start_data);
    if (size != ip_audio_size || !ip_audio_clip_valid(clip, size) ||
        bsp_audio_set_format(IP_AUDIO_RATE, 16, 1) != ESP_OK) {
        atomic_store(&s_ready, false);
        return;
    }
    if (request.generation != atomic_load(&s_generation)) return;
    bsp_audio_set_volume(55);
    minecraft_adpcm_state_t decoder;
    minecraft_adpcm_init(&decoder, clip->predictor, clip->step);
    uint32_t sample = 0;
    int16_t pcm[512];
    while (sample < clip->samples && request.generation == atomic_load(&s_generation)) {
        unsigned count = 0;
        while (count < 512 && sample < clip->samples) {
            if (!sample) pcm[count++] = (int16_t)decoder.predictor;
            else {
                uint32_t nibble = sample - 1;
                uint8_t packed = ip_voice_start_data[clip->offset + nibble / 2];
                pcm[count++] = minecraft_adpcm_decode(&decoder,
                    (nibble & 1) ? packed >> 4 : packed & 15);
            }
            sample++;
        }
        if (request.generation != atomic_load(&s_generation)) break;
        if (bsp_audio_write(pcm, count * sizeof(pcm[0])) != ESP_OK) {
            atomic_store(&s_ready, false);
            break;
        }
    }
    /* Drain preceding speech through the I2S DMA queue before muting. Cancel
       requests bypass this drain and are handled at the next chunk boundary. */
    if (sample == clip->samples && atomic_load(&s_ready)) {
        for (unsigned i = 0; i < 512; i++) pcm[i] = 0;
        for (unsigned i = 0; i < 3 && request.generation == atomic_load(&s_generation); i++) {
            if (bsp_audio_write(pcm, sizeof(pcm)) != ESP_OK) {
                atomic_store(&s_ready, false);
                break;
            }
        }
    }
}

static void worker(void *arg)
{
    (void)arg;
    request_t request;
    for (;;) {
        if (xQueueReceive(s_requests, &request, portMAX_DELAY) != pdTRUE) continue;
        if (request.generation != atomic_load(&s_generation)) continue;
        play(request);
        /* Codec access belongs only to this worker, including cancellation. */
        bsp_audio_set_volume(0);
    }
}

void ip_voice_start(bool audio_available)
{
    if (s_started) return;
    s_started = true;
    if (!audio_available) return;
    s_requests = xQueueCreateStatic(1, sizeof(request_t), s_buffer, &s_control);
    atomic_store(&s_ready, true);
    if (xTaskCreate(worker, "idiom_voice", 4096, NULL, 3, NULL) != pdPASS) {
        atomic_store(&s_ready, false);
        s_requests = NULL;
    }
}

void ip_voice_speak(unsigned question)
{
    if (!s_requests || !atomic_load(&s_ready) || question >= IP_QUESTIONS) return;
    request_t request = {(int)question, atomic_fetch_add(&s_generation, 1) + 1};
    (void)xQueueOverwrite(s_requests, &request);
}

void ip_voice_stop(void)
{
    if (!s_requests) return;
    request_t request = {IP_AUDIO_STOP, atomic_fetch_add(&s_generation, 1) + 1};
    (void)xQueueOverwrite(s_requests, &request);
}

bool ip_voice_ready(void) { return atomic_load(&s_ready); }
