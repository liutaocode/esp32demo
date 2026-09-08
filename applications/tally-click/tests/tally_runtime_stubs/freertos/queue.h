#pragma once
typedef struct { unsigned char data[64][32]; unsigned count,size,capacity; } StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
QueueHandle_t xQueueCreateStatic(unsigned n,unsigned size,void *storage,StaticQueue_t *q);
int xQueueSend(QueueHandle_t q,const void *p,unsigned timeout);
int xQueueReceive(QueueHandle_t q,void *p,unsigned timeout);
void xQueueReset(QueueHandle_t q);
int xQueueOverwrite(QueueHandle_t q,const void *p);
