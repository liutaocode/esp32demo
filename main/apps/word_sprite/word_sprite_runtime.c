#include "word_sprite_runtime.h"
#include "word_sprite_audio.h"
#include "minecraft_adpcm.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"
#include <stdatomic.h>

extern const uint8_t ws_audio_start[] asm("_binary_word_sprite_adpcm_bin_start");
extern const uint8_t ws_audio_end[] asm("_binary_word_sprite_adpcm_bin_end");

typedef struct { int clips[3]; } voice_request_t;
static StaticQueue_t s_voice_control, s_save_control;
static uint8_t s_voice_buffer[sizeof(voice_request_t)];
static uint8_t s_save_buffer[sizeof(ws_progress_t)];
static QueueHandle_t s_voice_queue, s_save_queue;
static atomic_bool s_stop, s_stopped = true, s_audio_ok;
static atomic_int s_save_status;
static bool s_storage_ok;
static nvs_handle_t s_nvs;

static void save_pending(void)
{
    ws_progress_t progress;
    if (xQueueReceive(s_save_queue, &progress, 0) != pdTRUE) return;
    if (!s_storage_ok) { atomic_store(&s_save_status, 0); return; }
    esp_err_t err = nvs_set_blob(s_nvs, "words36_v1", &progress, sizeof(progress));
    if (err == ESP_OK) err = nvs_commit(s_nvs);
    atomic_store(&s_save_status, err == ESP_OK ? 1 : 3);
}

/* False means interrupted or failed. Caller must not play remaining clips. */
static bool play_clip(int id)
{
    if (id < 0 || id >= WS_AUDIO_COUNT || !atomic_load(&s_audio_ok)) return false;
    const ws_clip_t *clip = &ws_clips[id];
    size_t size = (size_t)(ws_audio_end - ws_audio_start);
    if (size != ws_audio_size || !ws_clip_valid(clip, size) ||
        bsp_audio_set_format(WS_SAMPLE_RATE, 16, 1) != ESP_OK) {
        atomic_store(&s_audio_ok, false);
        return false;
    }
    bsp_audio_set_volume(55);
    minecraft_adpcm_state_t decoder;
    minecraft_adpcm_init(&decoder, clip->predictor, clip->step);
    uint32_t sample = 0;
    int16_t pcm[512];
    while (sample < clip->samples) {
        if (atomic_load(&s_stop) || uxQueueMessagesWaiting(s_voice_queue)) return false;
        unsigned count = 0;
        while (count < 512 && sample < clip->samples) {
            if (sample == 0) pcm[count++] = (int16_t)decoder.predictor;
            else {
                uint32_t nibble = sample - 1;
                uint8_t packed = ws_audio_start[clip->offset + nibble / 2];
                pcm[count++] = minecraft_adpcm_decode(&decoder,
                    (nibble & 1) ? packed >> 4 : packed & 15);
            }
            sample++;
        }
        if (bsp_audio_write(pcm, count * sizeof(pcm[0])) != ESP_OK) {
            atomic_store(&s_audio_ok, false);
            return false;
        }
    }
    return true;
}

static void worker(void *arg)
{
    (void)arg;
    while (!atomic_load(&s_stop)) {
        save_pending();
        voice_request_t request;
        if (xQueueReceive(s_voice_queue, &request, pdMS_TO_TICKS(40)) == pdTRUE) {
            if (request.clips[0] < 0) { bsp_audio_set_volume(0); continue; }
            for (unsigned i = 0; i < 3; i++) {
                if (request.clips[i] < 0 || !play_clip(request.clips[i])) break;
            }
        }
    }
    save_pending();
    bsp_audio_set_volume(0);
    if (s_storage_ok) nvs_close(s_nvs);
    s_storage_ok = false;
    /* Worker never touches LVGL. After this handshake the UI may be deleted. */
    atomic_store(&s_stopped, true);
    vTaskDelete(NULL);
}

ws_progress_t ws_runtime_start(bool audio_available)
{
    ws_progress_t progress = {0};
    if (!atomic_load(&s_stopped)) return progress;
    atomic_store(&s_stop, false);
    atomic_store(&s_audio_ok, audio_available);
    atomic_store(&s_save_status, 0);
    s_storage_ok = false;
    s_voice_queue = xQueueCreateStatic(1, sizeof(voice_request_t), s_voice_buffer, &s_voice_control);
    s_save_queue = xQueueCreateStatic(1, sizeof(ws_progress_t), s_save_buffer, &s_save_control);
    /* Never erase NVS on init errors: other applications share this partition. */
    if (nvs_flash_init() == ESP_OK && nvs_open("word_sprite", NVS_READWRITE, &s_nvs) == ESP_OK) {
        size_t size = sizeof(progress);
        esp_err_t err = nvs_get_blob(s_nvs, "words36_v1", &progress, &size);
        if (err != ESP_OK || size != sizeof(progress)) progress = (ws_progress_t){0};
        s_storage_ok = true;
        atomic_store(&s_save_status, err == ESP_OK || err == ESP_ERR_NVS_NOT_FOUND ? 1 : 3);
    }
    atomic_store(&s_stopped, false);
    if (xTaskCreate(worker, "word_sprite_io", 4096, NULL, 3, NULL) != pdPASS) {
        if (s_storage_ok) nvs_close(s_nvs);
        s_storage_ok = false;
        atomic_store(&s_stopped, true);
        atomic_store(&s_audio_ok, false);
        atomic_store(&s_save_status, 0);
    }
    return progress;
}

bool ws_runtime_audio_ready(void) { return atomic_load(&s_audio_ok); }
int ws_runtime_save_status(void) { return atomic_load(&s_save_status); }

void ws_runtime_save(ws_progress_t progress)
{
    if (atomic_load(&s_stopped) || atomic_load(&s_stop)) return;
    atomic_store(&s_save_status, 2);
    xQueueOverwrite(s_save_queue, &progress);
}

void ws_runtime_speak(int first, int second)
{
    if (atomic_load(&s_stopped) || atomic_load(&s_stop)) return;
    voice_request_t request = {{first, second, -1}};
    xQueueOverwrite(s_voice_queue, &request);
}

void ws_runtime_explain(unsigned word, int cue)
{
    if (word >= WS_WORDS || atomic_load(&s_stopped) || atomic_load(&s_stop)) return;
    voice_request_t request;
    if (!ws_explanation_clips(word, cue, request.clips)) return;
    xQueueOverwrite(s_voice_queue, &request);
}

void ws_runtime_stop_audio(void) { ws_runtime_speak(-1, -1); }
void ws_runtime_shutdown(void) { atomic_store(&s_stop, true); }
bool ws_runtime_stopped(void) { return atomic_load(&s_stopped); }
