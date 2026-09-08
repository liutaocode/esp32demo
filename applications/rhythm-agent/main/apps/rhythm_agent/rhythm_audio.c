#include "rhythm_audio.h"
#include "minecraft_adpcm.h"
#include "bsp_audio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include <stdatomic.h>
extern const uint8_t voice_start[] __asm__("_binary_rhythm_voice_bin_start");
extern const uint8_t voice_end[] __asm__("_binary_rhythm_voice_bin_end");
typedef struct { int kind; unsigned key, volume, generation; bool voice; ra_pattern_t pattern; } command_t;
static StaticQueue_t control;
static uint8_t storage[sizeof(command_t)];
static QueueHandle_t queue;
static atomic_uint generation, completed;
static atomic_bool ready;
static atomic_int demo_note = -1;
static bool started;
static bool current(const command_t *c) { return c->generation == atomic_load(&generation); }
static bool output(const command_t *c, int16_t *pcm, unsigned n) {
    if (!current(c)) return false;
    static const int32_t gain[] = {0, 35, 65, 100};
    for (unsigned i=0; i<n; i++) pcm[i] = (int16_t)((int32_t)pcm[i] * gain[c->volume < 4 ? c->volume : 1] / 100);
    if (atomic_load(&ready)) {
        if (bsp_audio_write(pcm, n*sizeof(*pcm)) != ESP_OK) atomic_store(&ready, false);
    } else {
        /* Visual-only fallback stays at the same tempo when audio is unavailable. */
        vTaskDelay(pdMS_TO_TICKS(n * 1000 / RA_RATE));
    }
    return current(c);
}
static bool silence(const command_t *c, unsigned ms) {
    for (unsigned t=0; t<ms; t+=10) { int16_t pcm[160]={0}; if (!output(c,pcm,160)) return false; }
    return current(c);
}
static bool speech(const command_t *c, unsigned id) {
    if (!c->voice || !c->volume || !atomic_load(&ready)) return current(c);
    const ra_clip_t *clip=&ra_clips[id];
    size_t size=(size_t)(voice_end-voice_start);
    if (size!=ra_voice_bytes || clip->offset>size || clip->bytes>size-clip->offset ||
        clip->samples==0 || clip->samples>RA_RATE*4 || clip->bytes!=clip->samples/2 || clip->step>88) return false;
    minecraft_adpcm_state_t d; minecraft_adpcm_init(&d,clip->predictor,clip->step);
    unsigned sample=0;
    while (sample<clip->samples) {
        int16_t pcm[160]; unsigned n=0;
        while (n<160 && sample<clip->samples) {
            if (!sample) pcm[n++]=(int16_t)d.predictor;
            else { unsigned ni=sample-1; uint8_t b=voice_start[clip->offset+ni/2]; pcm[n++]=minecraft_adpcm_decode(&d,ni&1 ? b>>4 : b&15); }
            sample++;
        }
        if (!output(c,pcm,n)) return false;
    }
    return silence(c,150);
}
static void play(const command_t *c) {
    if (c->kind==0) { (void)silence(c,120); return; }
    if (c->kind==3) { (void)speech(c,c->key ? 2 : 3); (void)silence(c,120); return; }
    if (c->kind==1 && !speech(c,0)) return;
    if (c->kind==1 && !silence(c,350)) return;
    unsigned length=c->kind==1 ? c->pattern.at[c->pattern.count-1]+420 : 140;
    unsigned index=0;
    for (unsigned t=0;t<length;t+=10) {
        if (!current(c)) break;
        if (c->kind==1) {
            while (index+1<c->pattern.count && t>=c->pattern.at[index+1]) index++;
            atomic_store(&demo_note,t-c->pattern.at[index]<170 ? (int)index : -1);
        }
        unsigned offset=(c->kind==1 ? t-c->pattern.at[index] : t)*16;
        unsigned key=c->kind==1 ? c->pattern.keys[index] : c->key;
        int16_t pcm[160];
        for(unsigned n=0;n<160;n++) pcm[n]=ra_tone(key,offset+n);
        if (!output(c,pcm,160)) break;
    }
    atomic_store(&demo_note,-1);
    if (c->kind==1 && current(c)) (void)speech(c,1);
    /* Drain queued DMA audio without cutting off the final syllable or note. */
    (void)silence(c,120);
}
static void worker(void *unused) {
    (void)unused;
    if (atomic_load(&ready) && bsp_audio_set_format(RA_RATE,16,1)!=ESP_OK) atomic_store(&ready,false);
    if (atomic_load(&ready)) bsp_audio_set_volume(65);
    command_t c;
    for (;;) {
        if (xQueueReceive(queue,&c,portMAX_DELAY)!=pdTRUE) continue;
        if (current(&c)) play(&c);
        atomic_store(&completed,c.generation);
    }
}
void ra_audio_start(bool available) {
    if (started) return;
    started=true;
    queue=xQueueCreateStatic(1,sizeof(command_t),storage,&control);
    atomic_store(&ready,available);
    if (xTaskCreate(worker,"rhythm_sound",4096,NULL,4,NULL)!=pdPASS) { queue=NULL; atomic_store(&ready,false); }
}
static void send(command_t c) {
    if (!queue) return;
    c.generation=atomic_fetch_add(&generation,1)+1;
    (void)xQueueOverwrite(queue,&c);
}
void ra_audio_demo(const ra_pattern_t *p,unsigned volume,bool voice) {
    if(!p || p->count<1 || p->count>RA_MAX_NOTES || p->at[0]!=0 || p->at[p->count-1]>10000) return;
    for(unsigned i=0;i<p->count;i++) if(p->keys[i]>2 || (i && p->at[i]<p->at[i-1]+140)) return;
    send((command_t){.kind=1,.pattern=*p,.volume=volume,.voice=voice});
}
void ra_audio_note(unsigned key,unsigned volume) { if(key<3) send((command_t){.kind=2,.key=key,.volume=volume}); }
void ra_audio_feedback(bool passed,unsigned volume,bool voice) { send((command_t){.kind=3,.key=passed,.volume=volume,.voice=voice}); }
void ra_audio_stop(void) { send((command_t){0}); }
bool ra_audio_busy(void) { return atomic_load(&completed)!=atomic_load(&generation); }
bool ra_audio_ready(void) { return atomic_load(&ready); }
int ra_audio_note_index(void) { return atomic_load(&demo_note); }
