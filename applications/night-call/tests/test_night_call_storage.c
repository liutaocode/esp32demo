#define _POSIX_C_SOURCE 200809L
#include "night_call_storage.h"
#include "nvs.h"
#include "freertos/queue.h"
#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

static int failure;
static unsigned creates;
static atomic_uint writes, commits;
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
    (void)name; (void)handle; assert(stack == 3072 && priority == 2); creates++;
    if (failure == 4) return 0;
    task_entry = entry; pthread_t thread; assert(!pthread_create(&thread, NULL, run, arg));
    pthread_detach(thread); return 1;
}
static uint8_t stored[NC_SAVE_SIZE];
static bool exists;
static void worker_only(void) { assert(!pthread_equal(owner,pthread_self())); }
int nvs_flash_init(void) { return failure==1?-1:0; }
int nvs_open(const char *name,int mode,nvs_handle_t *h) { assert(!strcmp(name,"nightcall") && mode==1);*h=1;return 0; }
int nvs_get_blob(nvs_handle_t h,const char *key,void *p,size_t *size) {
    assert(h==1 && !strcmp(key,"story") && *size==NC_SAVE_SIZE);
    if(!exists) return ESP_ERR_NVS_NOT_FOUND;
    memcpy(p,stored,NC_SAVE_SIZE); return 0;
}
int nvs_set_blob(nvs_handle_t h,const char *key,const void *p,size_t size) {
    worker_only(); assert(h==1 && !strcmp(key,"story") && size==NC_SAVE_SIZE);
    atomic_fetch_add(&writes,1); delay();
    if(failure==3) return -1;
    memcpy(stored,p,size); exists=true; return 0;
}
int nvs_commit(nvs_handle_t h) { worker_only();assert(h==1);atomic_fetch_add(&commits,1);return failure==5?-1:0; }
void nvs_close(nvs_handle_t h) { assert(h==1); }
static void wait_done(void) {
    for(unsigned i=0;i<3000;i++) { delay(); if(nc_storage_status()!=1) return; }
    assert(!"save timed out");
}
int main(int argc,char **argv) {
    assert(argc==2);failure=atoi(argv[1]);owner=pthread_self();
    nc_state_t source; nc_init(&source); nc_begin(&source,1); source.endings=4; nc_encode(&source,stored);exists=true;
    if(failure==2) stored[8]^=1;
    nc_state_t loaded;nc_storage_start(&loaded);
    if(failure==1 || failure==2) assert(!loaded.active && loaded.volume==2);
    else assert(loaded.active && loaded.chapter==1 && loaded.endings==4);
    if(failure==1 || failure==4) { assert(nc_storage_status()==-1);nc_storage_save(&source);assert(!atomic_load(&writes)); }
    else {
        for(unsigned i=0;i<100;i++) { source.volume=i%4;nc_storage_save(&source); }
        // The final request must win even when earlier writes are still finishing.
        source.volume=3;source.endings=20;nc_storage_save(&source);
        for(unsigned i=0;i<3000;i++) { delay(); if(atomic_load(&writes)) break; }
        wait_done();
        if(failure==3 || failure==5) { for(unsigned i=0;i<50;i++) delay();assert(nc_storage_status()==-1); }
        else {
            // A previous corrupt-load warning clears only after a successful commit.
            for(unsigned i=0;i<2000 && nc_storage_status()!=0;i++) delay();
            assert(nc_storage_status()==0 && atomic_load(&commits));
            nc_state_t result;assert(nc_decode(&result,stored,sizeof(stored)));assert(result.volume==3 && result.endings==20);
        }
    }
    puts("Night Call NVS worker: resume, corrupt data, last-write wins, flash/commit/task failures PASS");
}
