#pragma once
#include <stdbool.h>
#include <pthread.h>
typedef struct {pthread_mutex_t lock;unsigned size;bool pending;unsigned char data[1024];} StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
QueueHandle_t xQueueCreateStatic(unsigned count,unsigned size,void *storage,StaticQueue_t *q);
int xQueueReceive(QueueHandle_t q,void *out,int ticks);
void xQueueOverwrite(QueueHandle_t q,const void *in);
