#pragma once
#include <assert.h>
#include <string.h>
typedef struct { unsigned char data[8][32]; unsigned count, size, capacity; } StaticQueue_t;
typedef StaticQueue_t *QueueHandle_t;
static inline QueueHandle_t xQueueCreateStatic(unsigned n, unsigned size, void *storage, StaticQueue_t *q)
{
    (void)storage; assert(n <= 8 && size <= 32);
    q->capacity = n; q->size = size; q->count = 0; return q;
}
static inline int xQueueSend(QueueHandle_t q, const void *p, int timeout)
{
    assert(timeout == 0); if (q->count == q->capacity) return 0;
    memcpy(q->data[q->count++], p, q->size); return 1;
}
static inline int xQueueReceive(QueueHandle_t q, void *p, int timeout)
{
    assert(timeout == 0); if (!q->count) return 0;
    memcpy(p, q->data[0], q->size);
    memmove(q->data, q->data + 1, (--q->count) * 32); return 1;
}
static inline void xQueueReset(QueueHandle_t q) { q->count = 0; }
