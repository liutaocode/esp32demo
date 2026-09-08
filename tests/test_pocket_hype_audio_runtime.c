#define _POSIX_C_SOURCE 200809L
#include "pocket_hype_audio.h"
#include "freertos/queue.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static int failure;
static unsigned creates;
static atomic_uint writes, volume, failures, trailing_zero_samples;
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
void bsp_audio_set_volume(uint8_t value) { worker_only(); assert(value <= 75); atomic_store(&volume, value); }
int bsp_audio_write(const void *pcm, size_t bytes)
{
    worker_only(); assert(pcm && bytes > 0 && bytes <= 1024);
    const int16_t *samples = pcm;
    for (unsigned i = 0; i < bytes / sizeof(int16_t); i++) {
        if (samples[i] == 0) atomic_fetch_add(&trailing_zero_samples, 1);
        else atomic_store(&trailing_zero_samples, 0);
    }
    atomic_fetch_add(&writes, 1); delay();
    if (failure == 3) { atomic_fetch_add(&failures, 1); return -1; }
    return 0;
}
static void wait_idle(void) {
    for (unsigned i = 0; i < 3000 && ph_audio_busy(); i++) delay();
    assert(!ph_audio_busy());
}
int main(int argc, char **argv) {
    assert(argc == 2); failure = atoi(argv[1]); owner = pthread_self();
    ph_audio_start(failure != 1); ph_audio_play(0, 2, true);
    if (!failure) {
        for (unsigned i = 0; i < 2000 && !atomic_load(&writes); i++) delay();
        assert(atomic_load(&writes)); ph_audio_stop(); wait_idle();
        unsigned stopped = atomic_load(&writes);
        for (int i = 0; i < 20; i++) delay();
        assert(atomic_load(&writes) == stopped && atomic_load(&volume) == 0);
        for (int i = 0; i < 100; i++) {
            ph_audio_start(true); ph_audio_play(i % 18, 3, i % 2); ph_audio_stop();
        }
        wait_idle(); stopped = atomic_load(&writes);
        ph_audio_play(17, 0, true); wait_idle(); assert(atomic_load(&writes) == stopped);
        ph_audio_play(17, 1, false); wait_idle(); assert(atomic_load(&writes) > stopped);
        assert(atomic_load(&volume) == 0);
        assert(atomic_load(&trailing_zero_samples) >= 1536);
        stopped = atomic_load(&writes); ph_audio_play(999, 3, true); wait_idle();
        assert(atomic_load(&writes) == stopped);
    } else {
        wait_idle();
        for (unsigned i = 0; i < 2000 && ph_audio_ready(); i++) delay();
        assert(!ph_audio_ready() && atomic_load(&volume) == 0);
        unsigned stopped = atomic_load(&writes); ph_audio_play(1, 2, true); wait_idle();
        assert(stopped == atomic_load(&writes));
        if (failure == 3) assert(atomic_load(&failures) == 1); else assert(stopped == 0);
    }
    assert(creates == (failure == 1 ? 0U : 1U)); ph_audio_stop(); wait_idle();
    puts("Pocket Hype audio worker: real ADPCM streaming, cancellation, rapid replacement, mute and failure fallback PASS");
}
