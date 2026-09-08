#define _POSIX_C_SOURCE 200809L
#include "bean_runtime.h"
#include "freertos/queue.h"
#include "nvs.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
static int failure;
static pthread_t owner;
static atomic_uint writes, amplitude, volume, stored, commits;
static atomic_uint_fast64_t clock_us;
static void (*entry_fn)(void *);
static void delay(void){struct timespec t={0,1000000};nanosleep(&t,NULL);}
static void worker_only(void){assert(!pthread_equal(owner,pthread_self()));}
int64_t esp_timer_get_time(void){return (int64_t)atomic_load(&clock_us);}
void vTaskDelay(unsigned ms){atomic_fetch_add(&clock_us,ms*1000);delay();}
QueueHandle_t xQueueCreateStatic(unsigned n,unsigned size,void *buffer,StaticQueue_t *q){
    (void)buffer;assert(n==1 && size<=sizeof(q->data));q->size=size;
    pthread_mutex_init(&q->lock,NULL);return q;
}
int xQueueReceive(QueueHandle_t q,void *out,int timeout){
    assert(!timeout);pthread_mutex_lock(&q->lock);int result=q->pending;
    if(result){memcpy(out,q->data,q->size);q->pending=false;}
    pthread_mutex_unlock(&q->lock);return result;
}
void xQueueOverwrite(QueueHandle_t q,const void *in){pthread_mutex_lock(&q->lock);memcpy(q->data,in,q->size);q->pending=true;pthread_mutex_unlock(&q->lock);}
static void *run(void *arg){entry_fn(arg);return NULL;}
int xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle){
    (void)name;(void)handle;assert(stack==4096 && priority==5);
    if(failure==4)return 0;
    entry_fn=fn;pthread_t thread;assert(!pthread_create(&thread,NULL,run,arg));pthread_detach(thread);return 1;
}
int bsp_audio_init(void){return failure==1?-1:0;}
int bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t ch){assert(hz==16000 && bits==16 && ch==1);return failure==2?-1:0;}
void bsp_audio_set_volume(uint8_t v){worker_only();assert(v<=90);atomic_store(&volume,v);}
int bsp_audio_write(const void *pcm,size_t bytes){worker_only();assert(pcm && bytes<=640 && bytes>0);atomic_fetch_add(&writes,1);atomic_fetch_add(&clock_us,20000);delay();return failure==3?-1:0;}
int bsp_audio_read(void *pcm,size_t bytes){worker_only();assert(bytes==640);int16_t *p=pcm;unsigned a=atomic_load(&amplitude);for(unsigned i=0;i<320;i++)p[i]=i%2?(int16_t)a:-(int16_t)a;atomic_fetch_add(&clock_us,20000);delay();return failure==5?-1:0;}
int nvs_flash_init(void){return failure==6?-1:0;}
int nvs_open(const char *n,int mode,nvs_handle_t *h){assert(!strcmp(n,"mouthy_bean") && mode==1);*h=1;return 0;}
int nvs_get_u32(nvs_handle_t h,const char *key,uint32_t *p){(void)h;(void)key;*p=0;return 0;}
int nvs_set_u32(nvs_handle_t h,const char *key,uint32_t p){worker_only();(void)h;(void)key;atomic_store(&stored,p);return 0;}
int nvs_commit(nvs_handle_t h){worker_only();(void)h;atomic_fetch_add(&commits,1);return 0;}
static void idle(void){for(unsigned i=0;i<3000 && bean_runtime_busy();i++)delay();assert(!bean_runtime_busy());}
int main(int argc,char **argv){
    assert(argc==2);failure=atoi(argv[1]);owner=pthread_self();bean_runtime_start();
    if(failure && failure!=6){
        if(failure==3)bean_runtime_play(0,1);
        for(unsigned i=0;i<3000 && bean_runtime_ready();i++)delay();
        assert(!bean_runtime_ready());idle();assert(!atomic_load(&volume));
    }else{
        bean_runtime_listen(true);atomic_store(&amplitude,4000);
        for(unsigned i=0;i<3000 && !bean_runtime_sound();i++)delay();assert(bean_runtime_sound());
        bean_runtime_play(0,2);
        for(unsigned i=0;i<3000 && !atomic_load(&writes);i++)delay();
        assert(atomic_load(&writes));assert(!bean_runtime_sound());
        bean_runtime_stop();idle();assert(!atomic_load(&volume));
        unsigned before=atomic_load(&writes);
        bean_runtime_play(1,0);idle();assert(atomic_load(&writes)==before);
        bean_runtime_play(999,3);idle();assert(atomic_load(&writes)==before);
        for(unsigned i=0;i<100;i++){bean_runtime_play(i%40,1);bean_runtime_stop();}
        idle();bean_runtime_listen(false);
        for(unsigned i=0;i<20;i++)delay();assert(!bean_runtime_sound());
        if(failure==6)assert(!bean_runtime_storage_ok());
        else{bean_runtime_save(0xB100070Fu);for(unsigned i=0;i<3000 && bean_runtime_saved()!=0xB100070Fu;i++)delay();assert(bean_runtime_saved()==0xB100070Fu && atomic_load(&stored)==0xB100070Fu && atomic_load(&commits)==1);}
    }
    puts("Mouthy Bean runtime: streaming, cancellation, mic suppression, mute, persistence and failure fallback PASS");
}
