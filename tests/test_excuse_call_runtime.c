#define _POSIX_C_SOURCE 200809L
#include "excuse_call_audio.h"
#include "esp_timer.h"
#include "freertos/queue.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static int failure;
static unsigned creates;
static atomic_uint writes, volume, failures;
static pthread_t owner;
static void (*task_entry)(void *);
static void delay(void) { struct timespec t = {0, 1000000}; nanosleep(&t, NULL); }
QueueHandle_t xQueueCreateStatic(unsigned count, unsigned size, void *storage, StaticQueue_t *q)
{
    (void)storage; assert(count == 1 && size <= sizeof(q->data));
    pthread_mutex_init(&q->lock, NULL); pthread_cond_init(&q->changed, NULL); q->size = size; return q;
}
int xQueueReceive(QueueHandle_t q, void *out, int timeout)
{
    assert(timeout == -1); pthread_mutex_lock(&q->lock);
    while (!q->pending) pthread_cond_wait(&q->changed, &q->lock);
    memcpy(out, q->data, q->size); q->pending = false; pthread_mutex_unlock(&q->lock); return 1;
}
void xQueueOverwrite(QueueHandle_t q, const void *in)
{
    pthread_mutex_lock(&q->lock); memcpy(q->data, in, q->size); q->pending = true;
    pthread_cond_signal(&q->changed); pthread_mutex_unlock(&q->lock);
}
static void *run(void *arg) { task_entry(arg); return NULL; }
int xTaskCreate(void (*entry)(void *), const char *name, unsigned stack, void *arg, unsigned priority, void *handle)
{
    (void)name; (void)handle; assert(stack == 4096 && priority == 5); creates++;
    if (failure == 4) return 0;
    task_entry = entry; pthread_t thread; assert(!pthread_create(&thread, NULL, run, arg));
    pthread_detach(thread); return 1;
}
static void worker_only(void) { assert(!pthread_equal(owner, pthread_self())); }
int bsp_audio_init(void) { worker_only(); return failure == 1 ? -1 : 0; }
int bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t channels)
{
    worker_only(); assert(hz == 16000 && bits == 16 && channels == 1); return failure == 2 ? -1 : 0;
}
void bsp_audio_set_volume(uint8_t value) { worker_only(); assert(value <= 85); atomic_store(&volume, value); }
int bsp_audio_write(const void *pcm, size_t bytes)
{
    worker_only(); assert(pcm && bytes > 0 && bytes <= 320);
    atomic_fetch_add(&writes, 1); delay();
    if (failure == 3) { atomic_fetch_add(&failures, 1); return -1; }
    return 0;
}
static atomic_llong time_ms;
int64_t esp_timer_get_time(void) { return atomic_load(&time_ms)*1000; }
static void wait_status(void)
{
    for(unsigned i=0;i<2000 && ec_audio_status()==0;i++) delay();
    assert(ec_audio_status()!=0);
}
static void wait_volume(unsigned desired)
{
    for(unsigned i=0;i<2000 && atomic_load(&volume)!=desired;i++) delay();
    assert(atomic_load(&volume)==desired);
}
int main(int argc,char **argv)
{
    assert(argc==2); failure=atoi(argv[1]); owner=pthread_self();
    ec_audio_prepare(); wait_status();
    if(failure==1 || failure==2 || failure==4) {
        assert(ec_audio_status()==-1); ec_audio_play(0,60000);
        for(unsigned i=0;i<30;i++) delay();
        assert(!atomic_load(&writes) && !atomic_load(&volume));
    } else if(failure==3) {
        ec_audio_play(0,60000);
        for(unsigned i=0;i<2000 && ec_audio_status()!=-1;i++) delay();
        assert(ec_audio_status()==-1 && atomic_load(&failures)==1 && !atomic_load(&volume));
    } else {
        assert(ec_audio_status()==1 && !atomic_load(&writes));
        ec_audio_play(0,60000); wait_volume(85);
        ec_audio_stop(); wait_volume(0);
        for(unsigned i=0;i<30;i++) delay();
        unsigned stopped=atomic_load(&writes);
        for(unsigned i=0;i<20;i++) delay();
        assert(atomic_load(&writes)==stopped);
        ec_audio_play(1,60000); wait_volume(85);
        ec_audio_play(4,60000);
        for(unsigned i=0;i<40;i++) delay();
        assert(atomic_load(&volume)==85);
        /* Worker enforces timeout even if UI timer never runs again. */
        atomic_store(&time_ms,60000); wait_volume(0);
        for(unsigned i=0;i<20;i++) delay();
        stopped=atomic_load(&writes);
        for(unsigned i=0;i<20;i++) delay();
        assert(atomic_load(&writes)==stopped);
        for(unsigned i=0;i<100;i++) { ec_audio_prepare(); ec_audio_play(i%5,120000); ec_audio_stop(); }
        for(unsigned i=0;i<40;i++) delay();
        assert(!atomic_load(&volume));
    }
    assert(creates==1); puts("Excuse Call audio worker: cancellation, UI-independent deadline, muted flush and failures PASS");
}
