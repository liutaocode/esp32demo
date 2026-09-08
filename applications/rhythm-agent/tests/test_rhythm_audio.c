#define _POSIX_C_SOURCE 200809L
#include "rhythm_audio.h"
#include "freertos/queue.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
static int failure;
static unsigned creates;
static atomic_uint writes,failures,nonzero;
static pthread_t owner;
static void (*task_entry)(void *);
static int16_t recorded[200000];
static unsigned recorded_n;
static void delay(void){struct timespec t={0,1000000};nanosleep(&t,NULL);}
QueueHandle_t xQueueCreateStatic(unsigned count,unsigned size,void *storage,StaticQueue_t *q) {
    (void)storage;assert(count==1 && size<=sizeof(q->data));
    pthread_mutex_init(&q->lock,NULL);pthread_cond_init(&q->changed,NULL);q->size=size;return q;
}
int xQueueReceive(QueueHandle_t q,void *out,int timeout) {
    assert(timeout==-1);pthread_mutex_lock(&q->lock);
    while(!q->pending)pthread_cond_wait(&q->changed,&q->lock);
    memcpy(out,q->data,q->size);q->pending=false;pthread_mutex_unlock(&q->lock);return 1;
}
void xQueueOverwrite(QueueHandle_t q,const void *in) {
    pthread_mutex_lock(&q->lock);memcpy(q->data,in,q->size);q->pending=true;
    pthread_cond_signal(&q->changed);pthread_mutex_unlock(&q->lock);
}
static void *run(void *arg){task_entry(arg);return NULL;}
int xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle) {
    (void)name;(void)handle;assert(stack==4096&&priority==4);creates++;
    if(failure==4)return 0;
    task_entry=entry;pthread_t thread;assert(!pthread_create(&thread,NULL,run,arg));pthread_detach(thread);return 1;
}
static void worker_only(void){assert(!pthread_equal(owner,pthread_self()));}
void vTaskDelay(unsigned ticks){worker_only();assert(ticks<=10);delay();}
int bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t channels) {worker_only();assert(hz==16000&&bits==16&&channels==1);return failure==2?-1:0;}
void bsp_audio_set_volume(uint8_t value){worker_only();assert(value==65);}
int bsp_audio_write(const void *pcm,size_t bytes) {
    worker_only();assert(pcm && bytes && bytes<=320 && bytes%2==0);unsigned n=bytes/2;
    const int16_t *p=pcm;for(unsigned i=0;i<n;i++) {if(p[i])atomic_fetch_add(&nonzero,1);if(recorded_n<200000)recorded[recorded_n++]=p[i];}
    atomic_fetch_add(&writes,1);delay();
    if(failure==3){atomic_fetch_add(&failures,1);return -1;}return 0;
}
static void idle(void){for(unsigned i=0;i<5000&&ra_audio_busy();i++)delay();assert(!ra_audio_busy());}
static bool audible(unsigned from,unsigned n) {for(unsigned i=from;i<from+n;i++)if(recorded[i])return true;return false;}
int main(int argc,char **argv) {
    assert(argc==2);failure=atoi(argv[1]);owner=pthread_self();ra_audio_start(failure!=1);
    ra_pattern_t p={.count=3,.keys={0,2,1},.at={0,500,1500}};
    ra_audio_demo(&p,3,false);idle();
    if(!failure) {
        /* Verify actual PCM contains exact 500/1000 ms gaps and a silence tail. */
        assert(recorded_n==(350+1500+420+120)*16);
        for(unsigned i=0;i<3;i++) {
            unsigned from=(350+p.at[i])*16;
            assert(audible(from,2240));assert(!audible(from+2240,160));
            for(unsigned j=0;j<2240;j++)assert(recorded[from+j]==ra_tone(p.keys[i],j));
        }
        assert(!audible(0,350*16));assert(!audible(recorded_n-120*16,120*16));
        recorded_n=0;unsigned n=atomic_load(&nonzero);ra_audio_demo(&p,0,true);idle();assert(atomic_load(&nonzero)==n);
        recorded_n=0;ra_audio_demo(&p,2,true);idle();assert(recorded_n>40000&&atomic_load(&nonzero)>n);
        ra_audio_demo(&p,2,true);for(unsigned i=0;i<5;i++)delay();ra_audio_stop();idle();
        unsigned stopped=atomic_load(&writes);for(unsigned i=0;i<20;i++)delay();assert(atomic_load(&writes)==stopped);
        for(unsigned i=0;i<100;i++){ra_audio_note(i%3,2);ra_audio_stop();}idle();
        ra_audio_feedback(true,2,true);idle();ra_audio_feedback(false,2,true);idle();
        stopped=atomic_load(&writes);ra_audio_note(5,3);idle();assert(stopped==atomic_load(&writes));
    } else {
        assert(!ra_audio_ready());
        if(failure==3)assert(atomic_load(&failures)==1);else assert(atomic_load(&writes)==0);
        ra_audio_stop();idle();
    }
    ra_audio_start(true);assert(creates==1);
    puts("Rhythm audio worker: actual packed speech, exact PCM gaps, cancellation, mute, invalid keys and failure fallback PASS");
}
