// main/apps/ebook/ebook_audio_runtime.c —— 见 ebook_audio.h 的运行时部分。
// 独占音频设备的常驻任务。优先级低于 LVGL,写 PCM 时不会拖慢界面。
#include "ebook_audio.h"

#include <stdatomic.h>

#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define EB_AUDIO_CHUNK 160   // 10 ms:新的一次翻页能立刻打断上一次

typedef struct { eb_sound_t sound; unsigned generation; } request_t;

static StaticQueue_t  s_control;
static uint8_t        s_storage[sizeof(request_t)];
static QueueHandle_t  s_requests;
static bool           s_worker_created;
static atomic_bool    s_active;
static atomic_uint    s_generation;

// 请求还没被更新的一次翻页顶掉,并且页面还在前台。
static bool current(const request_t *request)
{
    return atomic_load(&s_active) && request->generation == atomic_load(&s_generation);
}

static void worker(void *arg)
{
    (void)arg;
    bool ready = bsp_audio_init() == ESP_OK
              && bsp_audio_set_format(EB_AUDIO_RATE, 16, 1) == ESP_OK;
    // 不播的时候静音,免得 codec 的底噪在安静的阅读页里一直响。
    if (ready) bsp_audio_set_volume(0);

    request_t request;
    for (;;) {
        if (xQueueReceive(s_requests, &request, portMAX_DELAY) != pdTRUE) continue;
        if (!ready || !current(&request) || request.sound == EB_SOUND_NONE) continue;

        bsp_audio_set_volume(45);
        size_t offset = 0, total = eb_sound_samples(request.sound);
        int16_t pcm[EB_AUDIO_CHUNK];
        // 末尾多送 60 ms 静音,让最后一块采样真正走完 I2S DMA 再静音。
        while (offset < total + 960 && current(&request)) {
            size_t count = eb_sound_render(request.sound, offset, pcm, EB_AUDIO_CHUNK);
            if (!count) {
                for (size_t i = 0; i < EB_AUDIO_CHUNK; i++) pcm[i] = 0;
                count = EB_AUDIO_CHUNK;
            }
            if (bsp_audio_write(pcm, count * sizeof(*pcm)) != ESP_OK) { ready = false; break; }
            offset += count;
        }
        bsp_audio_set_volume(0);
    }
}

void eb_audio_prepare(void)
{
    if (s_requests) return;
    s_requests = xQueueCreateStatic(1, sizeof(request_t), s_storage, &s_control);
    s_worker_created = xTaskCreate(worker, "eb_sound", 3072, NULL, 2, NULL) == pdPASS;
}

void eb_audio_active(bool active)
{
    atomic_store(&s_active, active);
}

void eb_audio_play(eb_sound_t sound)
{
    if (!s_worker_created || !atomic_load(&s_active) || !eb_sound_samples(sound)) return;
    request_t request = { sound, atomic_fetch_add(&s_generation, 1) + 1 };
    // 连续翻页时后一次直接盖掉前一次,队列只留最新的一格,调用方不会被阻塞。
    xQueueOverwrite(s_requests, &request);
}
