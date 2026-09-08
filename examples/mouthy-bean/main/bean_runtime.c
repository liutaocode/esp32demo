#include "bean_runtime.h"
#include "minecraft_adpcm.h"
#include "bsp_audio.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
extern const uint8_t bean_data_start[] __asm__("_binary_mouthy_bean_adpcm_bin_start");
extern const uint8_t bean_data_end[] __asm__("_binary_mouthy_bean_adpcm_bin_end");
typedef struct { int line; unsigned volume, generation; } request_t;
static StaticQueue_t control;
static uint8_t buffer[sizeof(request_t)];
static QueueHandle_t queue;
static atomic_uint generation, completed, level, saved, requested_save;
static atomic_bool ready, listening, sound, storage_ok;
static nvs_handle_t store;
static uint64_t milliseconds(void) { return esp_timer_get_time()/1000; }
static void play(request_t r) {
    if (r.line < 0 || r.line >= BEAN_LINES || !r.volume) return;
    const bean_clip_t *c = &bean_clips[r.line];
    size_t total = bean_data_end - bean_data_start;
    if (total != bean_audio_bytes || c->offset > total || c->size > total-c->offset ||
        !c->samples || c->samples > 16000*8 || c->size != c->samples/2 || c->step > 88) {
        atomic_store(&ready, false); return;
    }
    static const uint8_t volumes[] = {0,55,75,90};
    bsp_audio_set_volume(volumes[r.volume < 4 ? r.volume : 1]);
    minecraft_adpcm_state_t d; minecraft_adpcm_init(&d,c->predictor,c->step);
    int16_t pcm[320]; unsigned pos=0;
    while (pos < c->samples && r.generation == atomic_load(&generation)) {
        unsigned n=0, sum=0;
        while (n < 320 && pos < c->samples) {
            int16_t value;
            if (!pos) value=(int16_t)d.predictor;
            else { unsigned nibble=pos-1; uint8_t b=bean_data_start[c->offset+nibble/2]; value=minecraft_adpcm_decode(&d,nibble&1 ? b>>4 : b&15); }
            pcm[n++]=value; sum += value < 0 ? -(int)value : value; pos++;
        }
        atomic_store(&level, n ? sum/n : 0);
        if (bsp_audio_write(pcm,n*sizeof(int16_t)) != ESP_OK) { atomic_store(&ready,false); break; }
    }
    if (r.generation != atomic_load(&generation)) bsp_audio_set_volume(0);
    for (unsigned i=0;i<320;i++) pcm[i]=0;
    for (unsigned i=0;i<5 && atomic_load(&ready);i++)
        if (bsp_audio_write(pcm,sizeof(pcm)) != ESP_OK) atomic_store(&ready,false);
    bsp_audio_set_volume(0); atomic_store(&level,0);
}
static void worker(void *arg) {
    (void)arg; bean_vad_t vad={0}; uint64_t suppress=milliseconds()+1000, last_save=0;
    int16_t pcm[320]; request_t r;
    for (;;) {
        if (xQueueReceive(queue,&r,0)==pdTRUE) {
            atomic_store(&sound,false); vad=(bean_vad_t){0};
            if (atomic_load(&ready) && r.generation==atomic_load(&generation)) play(r);
            atomic_store(&completed,r.generation); suppress=milliseconds()+600;
        }
        uint32_t pending=atomic_load(&requested_save);
        if (pending != atomic_load(&saved) && atomic_load(&storage_ok) && milliseconds()-last_save>=2000) {
            last_save=milliseconds();
            if (nvs_set_u32(store,"prefs",pending)==ESP_OK && nvs_commit(store)==ESP_OK) atomic_store(&saved,pending);
            else atomic_store(&storage_ok,false);
        }
        /* Drain microphone even while disabled: no stale speaker samples later. */
        if (atomic_load(&ready)) {
            if (bsp_audio_read(pcm,sizeof(pcm)) != ESP_OK) { atomic_store(&ready,false); atomic_store(&sound,false); continue; }
            int32_t mean=0; for (unsigned i=0;i<320;i++) mean+=pcm[i]; mean/=320;
            unsigned amplitude=0;
            for (unsigned i=0;i<320;i++) { int v=pcm[i]-mean; amplitude+=v<0?-v:v; }
            if (atomic_load(&listening) && milliseconds()>=suppress)
                atomic_store(&sound,bean_vad_tick(&vad,amplitude/320));
            else { vad=(bean_vad_t){0}; atomic_store(&sound,false); }
        } else vTaskDelay(pdMS_TO_TICKS(20));
    }
}
void bean_runtime_start(void) {
    if (queue) return;
    /* Called before acquiring LVGL. Never erase an existing NVS partition. */
    if (nvs_flash_init()==ESP_OK && nvs_open("mouthy_bean",NVS_READWRITE,&store)==ESP_OK) {
        uint32_t p=0; nvs_get_u32(store,"prefs",&p);
        atomic_store(&saved,p); atomic_store(&requested_save,p); atomic_store(&storage_ok,true);
    }
    atomic_store(&ready,bsp_audio_init()==ESP_OK && bsp_audio_set_format(16000,16,1)==ESP_OK);
    queue=xQueueCreateStatic(1,sizeof(request_t),buffer,&control);
    if (xTaskCreate(worker,"bean_audio",4096,NULL,5,NULL)!=pdPASS) {
        queue=NULL; atomic_store(&ready,false); atomic_store(&storage_ok,false);
    }
}
void bean_runtime_play(int line,unsigned volume) {
    if (!queue || !atomic_load(&ready)) return;
    request_t r={line,volume,atomic_fetch_add(&generation,1)+1}; xQueueOverwrite(queue,&r);
}
void bean_runtime_stop(void) { bean_runtime_play(-1,0); atomic_store(&sound,false); }
void bean_runtime_listen(bool enabled) { atomic_store(&listening,enabled); if (!enabled) atomic_store(&sound,false); }
bool bean_runtime_busy(void) { return atomic_load(&ready) && atomic_load(&generation)!=atomic_load(&completed); }
bool bean_runtime_sound(void) { return atomic_load(&sound); }
bool bean_runtime_ready(void) { return atomic_load(&ready); }
unsigned bean_runtime_level(void) { return atomic_load(&level); }
uint32_t bean_runtime_saved(void) { return atomic_load(&saved); }
void bean_runtime_save(uint32_t p) { atomic_store(&requested_save,p); }
bool bean_runtime_storage_ok(void) { return atomic_load(&storage_ok); }
