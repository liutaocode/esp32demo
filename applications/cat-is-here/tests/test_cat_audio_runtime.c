#define _POSIX_C_SOURCE 200809L
#include "cat/cat_audio_runtime.h"
#include <pthread.h>
#include <stdatomic.h>
#include <assert.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
static pthread_t caller;
static void (*entry_fn)(void *);
static int failure;
static atomic_uint writes, nonzero;
static void delay_ms(unsigned ms) { struct timespec t={0,(long)ms*1000000}; nanosleep(&t,NULL); }
void vTaskDelay(unsigned ms) { assert(ms==10); delay_ms(1); }
static void *run(void *arg) { entry_fn(arg); return NULL; }
int xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle) {
    (void)name;(void)handle;assert(stack==4096 && priority==5);
    if(failure==4)return 0;
    entry_fn=entry;pthread_t t;assert(!pthread_create(&t,NULL,run,arg));pthread_detach(t);return 1;
}
static void worker_only(void) { assert(!pthread_equal(caller,pthread_self())); }
int bsp_audio_init(void) { worker_only();return failure==1 ? -1 : 0; }
int bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t channels) { worker_only();assert(hz==16000 && bits==16 && channels==1);return failure==2 ? -1 : 0; }
void bsp_audio_set_volume(uint8_t v) { worker_only();assert(v==70); }
int bsp_audio_write(const void *pcm,size_t bytes) {
    worker_only();assert(bytes && bytes<=640 && bytes%2==0);
    const int16_t *s=pcm;for(size_t i=0;i<bytes/2;i++) if(abs(s[i])>1000) atomic_fetch_add(&nonzero,1);
    atomic_fetch_add(&writes,1);delay_ms(1);return failure==3 ? -1 : 0;
}
int main(int argc,char **argv) {
    assert(argc==2);failure=atoi(argv[1]);caller=pthread_self();cat_audio_prepare();
    if(failure==1 || failure==2 || failure==4) {
        delay_ms(20);cat_audio_request(CAT_PURR);delay_ms(20);assert(!cat_audio_ready() && !atomic_load(&writes));
    } else {
        for(unsigned i=0;i<2000 && !cat_audio_ready();i++)delay_ms(1);
        assert(cat_audio_ready());cat_audio_request(CAT_PURR);
        for(unsigned i=0;i<2000 && atomic_load(&writes)<(failure==3 ? 1u : 15u);i++)delay_ms(1);
        assert(atomic_load(&writes));
        if(failure==3) { delay_ms(20);assert(!cat_audio_ready() && atomic_load(&writes)==1); }
        else {
            assert(atomic_load(&nonzero)>100);
            for(unsigned i=0;i<100;i++)cat_audio_request(CAT_PURR);
            cat_audio_request(CAT_SILENT);delay_ms(30);
            unsigned stopped=atomic_load(&writes);delay_ms(30);assert(atomic_load(&writes)==stopped);
            cat_audio_request(CAT_DANCE);
            for(unsigned i=0;i<2000 && atomic_load(&writes)==stopped;i++)delay_ms(1);
            assert(atomic_load(&writes)>stopped);cat_audio_request(CAT_SILENT);delay_ms(30);
        }
    }
    puts("Cat audio worker: independent I/O, mute, restart and failure handling PASS");
}
