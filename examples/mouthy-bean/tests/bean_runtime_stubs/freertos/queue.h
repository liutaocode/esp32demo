#pragma once
#include <pthread.h>
#include <stdbool.h>
typedef struct { pthread_mutex_t lock; pthread_cond_t changed; unsigned size; bool pending; unsigned char data[32]; } StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
QueueHandle_t xQueueCreateStatic(unsigned count, unsigned size, void *storage, StaticQueue_t *queue);
int xQueueReceive(QueueHandle_t queue, void *value, int timeout);
void xQueueOverwrite(QueueHandle_t queue, const void *value);
