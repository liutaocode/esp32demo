#pragma once
static inline void vTaskDelay(unsigned ticks){(void)ticks;}
static inline int xTaskCreate(void(*fn)(void*),const char *name,unsigned stack,void *arg,unsigned priority,void *handle)
{(void)fn;(void)name;(void)stack;(void)arg;(void)priority;(void)handle;return 1;}
