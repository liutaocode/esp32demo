#define _POSIX_C_SOURCE 200809L
#include "rhythm_service.h"
#include "freertos/queue.h"
#include "nvs.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
static int failure;
static atomic_uint writes,stored;
static atomic_bool battery_seen;
static pthread_t owner;
static void (*entry_fn)(void *);
static void delay(void) {struct timespec t={0,1000000};nanosleep(&t,NULL);}
static void worker_only(void) {assert(!pthread_equal(owner,pthread_self()));}
QueueHandle_t xQueueCreateStatic(unsigned count,unsigned size,void *storage,StaticQueue_t *q) {
    (void)storage;assert(count==1&&size==sizeof(unsigned));
    pthread_mutex_init(&q->lock,NULL);pthread_cond_init(&q->changed,NULL);q->size=size;return q;
}
int xQueueReceive(QueueHandle_t q,void *out,int timeout) {
    assert(timeout==10000);pthread_mutex_lock(&q->lock);while(!q->pending)pthread_cond_wait(&q->changed,&q->lock);
    memcpy(out,q->data,q->size);q->pending=false;pthread_mutex_unlock(&q->lock);return 1;
}
void xQueueOverwrite(QueueHandle_t q,const void *in) {
    pthread_mutex_lock(&q->lock);memcpy(q->data,in,q->size);q->pending=true;pthread_cond_signal(&q->changed);pthread_mutex_unlock(&q->lock);
}
static void *run(void *arg) {entry_fn(arg);return NULL;}
int xTaskCreate(void (*entry)(void *),const char *name,unsigned stack,void *arg,unsigned priority,void *handle) {
    (void)name;(void)handle;assert(stack==3072&&priority==1);if(failure==3)return 0;
    entry_fn=entry;pthread_t thread;assert(!pthread_create(&thread,NULL,run,arg));pthread_detach(thread);return 1;
}
int bsp_battery_soc(void) {worker_only();atomic_store(&battery_seen,true);return failure==2?-1:87;}
int nvs_flash_init(void) {worker_only();return failure==1?-1:0;}
int nvs_open(const char *name,int mode,nvs_handle_t *h) {worker_only();assert(!strcmp(name,"rhythm_agent")&&mode==1);*h=7;return 0;}
int nvs_get_u32(nvs_handle_t h,const char *key,uint32_t *v) {assert(h==7&&!strcmp(key,"best_v1"));*v=620;return 0;}
int nvs_set_u32(nvs_handle_t h,const char *key,uint32_t v) {worker_only();assert(h==7&&!strcmp(key,"best_v1")&&v<=800);atomic_store(&stored,v);atomic_fetch_add(&writes,1);return failure==2?-1:0;}
int nvs_commit(nvs_handle_t h) {worker_only();assert(h==7);return 0;}
static void wait_best(unsigned n) {for(unsigned i=0;i<2000&&ra_service_best()!=n;i++)delay();assert(ra_service_best()==n);}
int main(int argc,char **argv) {
    assert(argc==2);failure=atoi(argv[1]);owner=pthread_self();ra_service_start();
    if(failure==3) {ra_service_save(800);assert(!ra_service_saved()&&ra_service_best()==0);return 0;}
    for(unsigned i=0;i<2000&&!atomic_load(&battery_seen);i++)delay();assert(atomic_load(&battery_seen));
    assert(ra_service_soc()==(failure==2?-1:87));assert(ra_service_best()==(failure==1?0:620));
    ra_service_save(740);ra_service_save(680);wait_best(740);for(int i=0;i<20;i++)delay();
    if(failure==0) {assert(ra_service_saved()&&atomic_load(&stored)==740);}
    else assert(!ra_service_saved());
    unsigned old=atomic_load(&writes);ra_service_save(120);ra_service_save(900);for(int i=0;i<20;i++)delay();assert(ra_service_best()==740&&atomic_load(&writes)==old);
    ra_service_save(800);wait_best(800);assert(failure!=1 || atomic_load(&writes)==0);
    puts("Rhythm storage: isolated namespace, monotonic high score, no lower overwrite, cached battery and RAM fallback PASS");
}
