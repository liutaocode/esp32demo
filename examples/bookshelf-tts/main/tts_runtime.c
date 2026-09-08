#include "tts_runtime.h"
#include "tts_model.h"
#include "ebook_audio.h"
#include "tts_bank.h"
#include "tts_tempo.h"
#include "speex/speex.h"
#include "bsp_audio.h"
#include "esp_tts_parser.h"
#include "esp_tts_voice_template.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "mbedtls/sha256.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include <stdatomic.h>
#include <string.h>

#define EXITED BIT0
#define SHUTDOWN BIT1
static const char *TAG = "chinese_tts";
typedef struct { char text[TTS_TEXT_BYTES]; unsigned speed; unsigned generation; unsigned sound; } request_t;
static QueueHandle_t requests;
static EventGroupHandle_t events;
static atomic_uint generation;
static atomic_bool engine_ready;
static atomic_uint voice_volume = 65;
void tts_runtime_set_volume(unsigned value) { atomic_store(&voice_volume, value > 100 ? 100 : value); }
unsigned tts_runtime_volume(void) { return atomic_load(&voice_volume); }
static portMUX_TYPE guard = portMUX_INITIALIZER_UNLOCKED;
static tts_snapshot_t snapshot;
static void publish(unsigned token, tts_status_t status, esp_err_t error, uint32_t samples, int64_t synth_us) {
    uint32_t heap = (uint32_t)esp_get_minimum_free_heap_size();
    portENTER_CRITICAL(&guard);
    if (atomic_load(&generation) != token) { portEXIT_CRITICAL(&guard); return; }
    snapshot = (tts_snapshot_t){status, error, samples / 16, (uint32_t)(synth_us / 1000),
        heap};
    portEXIT_CRITICAL(&guard);
}
tts_snapshot_t tts_runtime_snapshot(void) {
    portENTER_CRITICAL(&guard);
    tts_snapshot_t result = snapshot;
    portEXIT_CRITICAL(&guard);
    return result;
}
static bool cancelled(unsigned token) {
    return atomic_load(&generation) != token || (xEventGroupGetBits(events) & SHUTDOWN);
}
extern const uint8_t voice_start[] asm("_binary_xiaole_compact_dat_start");
extern const uint8_t voice_end[] asm("_binary_xiaole_compact_dat_end");
static void worker(void *arg) {
    (void)arg;
    esp_log_level_set("tts_parser", ESP_LOG_WARN);
    esp_tts_voice_t voice = esp_tts_voice_template;
    tts_bank_t bank;
    void *decoder = NULL;
    int16_t *syllable_pcm = NULL, *tempo_pcm = NULL;
    SpeexBits bits;
    bool bits_ready = false;
    esp_err_t error = ESP_FAIL;
    size_t data_size = (size_t)(voice_end - voice_start);
    unsigned char digest[32];
    static const unsigned char wanted[32] = {0x08,0x9b,0xe5,0x78,0x1e,0xa0,0x8a,0xc9,0x4a,0x3f,0x2d,0x22,0x7c,0xa6,0x6c,0x52,0xfb,0x57,0xc6,0xb1,0x4a,0x90,0x6d,0xe1,0xc3,0x9c,0xea,0xf3,0x09,0x7f,0xcb,0x92};
    if (mbedtls_sha256(voice_start, data_size, digest, 0) != 0 || memcmp(digest, wanted, 32) ||
        !tts_bank_open(&bank, voice_start, data_size)) { error = ESP_ERR_INVALID_CRC; goto failed; }
    /* Preserve the upstream pronunciation tables and syllable numbering.
       The compact audio body is decoded only by Speex, never by the vendor AMR player. */
    voice.voice_name = (char *)voice_start;
    voice.format = "speex";
    voice.sample_rate = 16000;
    voice.syll_pos = (int *)(voice_start + 40);
    voice.pinyin_idx = (short *)(voice_start + 40 + tts_bank_u32(voice_start + 20));
    voice.phrase_dict = (short *)(voice_start + 40 + tts_bank_u32(voice_start + 24));
    voice.extern_idx = (short *)(voice_start + 40 + tts_bank_u32(voice_start + 28));
    voice.extern_dict = (short *)(voice_start + 40 + tts_bank_u32(voice_start + 32));
    voice.data = (unsigned char *)(voice_start + bank.metadata_size);
    decoder = speex_decoder_init(&speex_wb_mode);
    if (!decoder) { error = ESP_ERR_NO_MEM; goto failed; }
    speex_bits_init(&bits); bits_ready = true;
    int frame_size = 0;
    speex_decoder_ctl(decoder, SPEEX_GET_FRAME_SIZE, &frame_size);
    if (frame_size != 320) { error = ESP_ERR_NOT_SUPPORTED; goto failed; }
    error = bsp_audio_set_format(16000, 16, 1);
    if (error != ESP_OK) goto failed;
    bsp_audio_set_volume(0);
    atomic_store(&engine_ready, true);
    publish(0, TTS_READY, ESP_OK, 0, 0);
    while (!(xEventGroupGetBits(events) & SHUTDOWN)) {
        request_t req;
        if (xQueueReceive(requests, &req, pdMS_TO_TICKS(50)) != pdTRUE) continue;
        if (cancelled(req.generation)) continue;
        if (req.sound) {
            bsp_audio_set_volume(45);
            int16_t pcm[160];
            size_t total = eb_sound_samples((eb_sound_t)req.sound);
            for (size_t off = 0; off < total + 1600 && !cancelled(req.generation); off += 160) {
                if (!eb_sound_render((eb_sound_t)req.sound, off, pcm, 160)) memset(pcm, 0, sizeof(pcm));
                if (bsp_audio_write(pcm, sizeof(pcm)) != ESP_OK) break;
            }
            bsp_audio_set_volume(0);
            publish(req.generation, TTS_READY, ESP_OK, 0, 0);
            continue;
        }
        if (!req.text[0]) { publish(req.generation, TTS_STOPPED, ESP_OK, 0, 0); continue; }
        unsigned applied_volume = atomic_load(&voice_volume);
        bsp_audio_set_volume(applied_volume);
        publish(req.generation, TTS_PREPARING, ESP_OK, 0, 0);
        int64_t began = esp_timer_get_time(), parse_us, synth_us = 0, first_us = -1;
        uint32_t played = 0, max_decode_us = 0;
        error = ESP_OK;
        esp_tts_utt_t *utt = esp_tts_parser_chinese(req.text, &voice);
        parse_us = synth_us = esp_timer_get_time() - began;
        if (!utt || utt->syll_num <= 0) error = ESP_ERR_INVALID_ARG;
        if (req.speed != 2 && !syllable_pcm) {
            syllable_pcm = malloc(bank.max_samples * sizeof(int16_t));
            tempo_pcm = malloc(tts_tempo_capacity(bank.max_samples) * sizeof(int16_t));
            if (!syllable_pcm || !tempo_pcm) {
                free(syllable_pcm); free(tempo_pcm); syllable_pcm = tempo_pcm = NULL;
                error = ESP_ERR_NO_MEM;
            }
        }
        for (int word = 0; error == ESP_OK && word < utt->syll_num && !cancelled(req.generation); word++) {
            tts_syllable_t syll;
            if (utt->syll_idx[word] < 0 || !tts_bank_syllable(&bank, (uint32_t)utt->syll_idx[word], &syll)) {
                error = ESP_ERR_INVALID_RESPONSE; break;
            }
            speex_decoder_ctl(decoder, SPEEX_RESET_STATE, NULL);
            uint32_t decoded = 0, kept = 0;
            int16_t pcm[320];
            for (unsigned frame = 0; frame < syll.frames && !cancelled(req.generation); frame++) {
                unsigned wanted_volume = atomic_load(&voice_volume);
                if (wanted_volume != applied_volume) { bsp_audio_set_volume(wanted_volume); applied_volume = wanted_volume; }
                int64_t start = esp_timer_get_time();
                speex_bits_read_from(&bits, (char *)(syll.packets + frame * bank.frame_bytes), bank.frame_bytes);
                int result = speex_decode_int(decoder, &bits, pcm);
                uint32_t elapsed = (uint32_t)(esp_timer_get_time() - start);
                synth_us += elapsed;
                if (elapsed > max_decode_us) max_decode_us = elapsed;
                if (result != 0) { error = ESP_ERR_INVALID_RESPONSE; break; }
                unsigned offset = decoded < syll.skip ? syll.skip - decoded : 0;
                if (offset > 320) offset = 320;
                unsigned count = 320 - offset;
                if (count > syll.samples - kept) count = syll.samples - kept;
                decoded += 320;
                if (req.speed != 2) memcpy(syllable_pcm + kept, pcm + offset, count * sizeof(int16_t));
                else if (count) {
                    if (first_us < 0) first_us = esp_timer_get_time() - began;
                    error = bsp_audio_write(pcm + offset, count * sizeof(int16_t));
                    if (error != ESP_OK) break;
                    played += count;
                    publish(req.generation, TTS_SPEAKING, ESP_OK, played, synth_us);
                }
                kept += count;
            }
            if (error != ESP_OK || cancelled(req.generation)) break;
            if (req.speed != 2) {
                int64_t start = esp_timer_get_time();
                size_t n = tts_tempo_process(syllable_pcm, kept, tempo_pcm,
                                            tts_tempo_capacity(bank.max_samples), req.speed);
                synth_us += esp_timer_get_time() - start;
                if (!n) { error = ESP_ERR_INVALID_SIZE; break; }
                for (size_t off = 0; off < n && !cancelled(req.generation);) {
                    unsigned wanted_volume = atomic_load(&voice_volume);
                    if (wanted_volume != applied_volume) { bsp_audio_set_volume(wanted_volume); applied_volume = wanted_volume; }
                    size_t count = n - off > 320 ? 320 : n - off;
                    if (first_us < 0) first_us = esp_timer_get_time() - began;
                    error = bsp_audio_write(tempo_pcm + off, count * sizeof(int16_t));
                    if (error != ESP_OK) break;
                    off += count; played += count;
                    publish(req.generation, TTS_SPEAKING, ESP_OK, played, synth_us);
                }
            }
        }
        if (utt) esp_tts_utt_free(utt);
        /* Drain queued samples before reporting completion or muting. */
        int16_t silence[320] = {0};
        for (unsigned i = 0; i < 5 && !cancelled(req.generation); i++) bsp_audio_write(silence, sizeof(silence));
        bsp_audio_set_volume(0);

        publish(req.generation, error != ESP_OK ? TTS_ERROR : cancelled(req.generation) ? TTS_STOPPED : TTS_READY,
                error, played, synth_us);
        ESP_LOGI(TAG, "result=%s audio_ms=%lu synth_ms=%lu parse_ms=%lu first_ms=%ld max_frame_us=%lu min_heap=%lu stack_free=%u",
            esp_err_to_name(error), (unsigned long)(played / 16), (unsigned long)(synth_us / 1000),
            (unsigned long)(parse_us / 1000), (long)(first_us < 0 ? -1 : first_us / 1000),
            (unsigned long)max_decode_us, (unsigned long)esp_get_minimum_free_heap_size(),
            (unsigned)uxTaskGetStackHighWaterMark(NULL));
    }
    goto cleanup;
failed:
    publish(0, TTS_ERROR, error, 0, 0);
    ESP_LOGE(TAG, "TTS initialization failed: %s", esp_err_to_name(error));
cleanup:
    atomic_store(&engine_ready, false);
    free(syllable_pcm); free(tempo_pcm);
    if (bits_ready) speex_bits_destroy(&bits);
    if (decoder) speex_decoder_destroy(decoder);
    xEventGroupSetBits(events, EXITED);
    vTaskDelete(NULL);
}

esp_err_t tts_runtime_start(void) {
    if (requests) return ESP_ERR_INVALID_STATE;
    requests = xQueueCreate(1, sizeof(request_t));
    events = xEventGroupCreate();
    if (!requests || !events) {
        if (requests) vQueueDelete(requests);
        if (events) vEventGroupDelete(events);
        requests = NULL; events = NULL;
        return ESP_ERR_NO_MEM;
    }
    atomic_store(&generation, 0);
    atomic_store(&engine_ready, false);
    publish(0, TTS_LOADING, ESP_OK, 0, 0);
    if (xTaskCreate(worker, "tts_worker", 8192, NULL, 4, NULL) != pdPASS) {
        vQueueDelete(requests); vEventGroupDelete(events);
        requests = NULL; events = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
esp_err_t tts_runtime_say(const char *text, unsigned speed) {
    if (!requests || (xEventGroupGetBits(events) & (EXITED | SHUTDOWN))) return ESP_ERR_INVALID_STATE;
    if (!atomic_load(&engine_ready)) return ESP_ERR_INVALID_STATE;
    if (!tts_text_valid(text) || speed >= TTS_SPEED_COUNT) return ESP_ERR_INVALID_ARG;
    request_t req = {.speed = speed, .generation = atomic_fetch_add(&generation, 1) + 1};
    memcpy(req.text, text, strlen(text) + 1);
    publish(req.generation, TTS_PREPARING, ESP_OK, 0, 0);
    return xQueueOverwrite(requests, &req) == pdPASS ? ESP_OK : ESP_FAIL;
}
void tts_runtime_stop(void) {
    if (!requests || !atomic_load(&engine_ready) ||
        (xEventGroupGetBits(events) & (EXITED | SHUTDOWN))) return;
    request_t req = {.generation = atomic_fetch_add(&generation, 1) + 1};
    xQueueOverwrite(requests, &req);
    publish(req.generation, TTS_STOPPED, ESP_OK, 0, 0);
}
bool tts_runtime_shutdown(uint32_t timeout_ms) {
    if (!requests) return true;
    xEventGroupSetBits(events, SHUTDOWN);
    atomic_fetch_add(&generation, 1);
    if (!(xEventGroupWaitBits(events, EXITED, pdFALSE, pdTRUE, pdMS_TO_TICKS(timeout_ms)) & EXITED)) return false;
    vQueueDelete(requests); vEventGroupDelete(events);
    requests = NULL; events = NULL;
    return true;
}

esp_err_t tts_runtime_sound(unsigned sound) {
    if (!requests || !atomic_load(&engine_ready) || !eb_sound_samples((eb_sound_t)sound)) return ESP_ERR_INVALID_STATE;
    request_t req = {.sound = sound, .generation = atomic_fetch_add(&generation, 1) + 1};
    publish(req.generation, TTS_PREPARING, ESP_OK, 0, 0);
    return xQueueOverwrite(requests, &req) == pdPASS ? ESP_OK : ESP_FAIL;
}
