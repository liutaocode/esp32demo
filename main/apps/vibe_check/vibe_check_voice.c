#include "vibe_check_voice.h"
#include "vibe_check_audio.h"
#include "minecraft_adpcm.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>

extern const uint8_t vibe_check_voice_start_data[]
    asm("_binary_vibe_check_adpcm_bin_start");
extern const uint8_t vibe_check_voice_end_data[]
    asm("_binary_vibe_check_adpcm_bin_end");

typedef struct {
    int clip;
    unsigned generation;
} vibe_check_voice_request_t;

static StaticQueue_t s_queue_control;
static uint8_t s_queue_buffer[sizeof(vibe_check_voice_request_t)];
static QueueHandle_t s_requests;
static atomic_uint s_generation;
static atomic_bool s_ready;
static bool s_started;

static void play(vibe_check_voice_request_t request)
{
    if (!atomic_load(&s_ready) || request.clip < 0 ||
        request.clip >= VC_AUDIO_COUNT) {
        return;
    }

    const vibe_check_audio_clip_t *clip =
        &vibe_check_audio_clips[request.clip];
    size_t data_size =
        (size_t)(vibe_check_voice_end_data - vibe_check_voice_start_data);
    if (data_size != vibe_check_audio_size ||
        !vibe_check_audio_clip_valid(clip, data_size) ||
        bsp_audio_set_format(VC_AUDIO_RATE, 16, 1) != ESP_OK) {
        atomic_store(&s_ready, false);
        return;
    }
    if (request.generation != atomic_load(&s_generation)) return;

    bsp_audio_set_volume(55);
    minecraft_adpcm_state_t decoder;
    minecraft_adpcm_init(&decoder, clip->predictor, clip->step);
    uint32_t sample = 0;
    int16_t pcm[512];
    while (sample < clip->samples &&
           request.generation == atomic_load(&s_generation)) {
        unsigned count = 0;
        while (count < 512 && sample < clip->samples) {
            if (sample == 0) {
                pcm[count++] = (int16_t)decoder.predictor;
            } else {
                uint32_t nibble = sample - 1;
                uint8_t packed =
                    vibe_check_voice_start_data[clip->offset + nibble / 2];
                pcm[count++] = minecraft_adpcm_decode(
                    &decoder, (nibble & 1) ? packed >> 4 : packed & 15);
            }
            sample++;
        }
        if (request.generation != atomic_load(&s_generation)) break;
        if (bsp_audio_write(pcm, count * sizeof(pcm[0])) != ESP_OK) {
            atomic_store(&s_ready, false);
            break;
        }
    }
}

static void worker(void *arg)
{
    (void)arg;
    vibe_check_voice_request_t request;
    for (;;) {
        if (xQueueReceive(s_requests, &request, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        if (request.generation == atomic_load(&s_generation)) play(request);
        /* Codec cancellation belongs to this worker, never the button callback. */
        bsp_audio_set_volume(0);
    }
}

void vibe_check_voice_start(bool audio_available)
{
    if (s_started) return;
    s_started = true;
    if (!audio_available) return;

    s_requests = xQueueCreateStatic(1, sizeof(vibe_check_voice_request_t),
                                    s_queue_buffer, &s_queue_control);
    atomic_store(&s_ready, true);
    if (xTaskCreate(worker, "vibe_voice", 4096, NULL, 3, NULL) != pdPASS) {
        atomic_store(&s_ready, false);
        s_requests = NULL;
    }
}

void vibe_check_voice_speak(unsigned clip)
{
    if (!s_requests || !atomic_load(&s_ready) || clip >= VC_AUDIO_COUNT) return;
    vibe_check_voice_request_t request = {
        .clip = (int)clip,
        .generation = atomic_fetch_add(&s_generation, 1) + 1,
    };
    (void)xQueueOverwrite(s_requests, &request);
}

void vibe_check_voice_stop(void)
{
    if (!s_requests) return;
    vibe_check_voice_request_t request = {
        .clip = -1,
        .generation = atomic_fetch_add(&s_generation, 1) + 1,
    };
    (void)xQueueOverwrite(s_requests, &request);
}

bool vibe_check_voice_ready(void)
{
    return atomic_load(&s_ready);
}
