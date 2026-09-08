#pragma once
int xTaskCreate(void (*entry)(void *), const char *name, unsigned stack, void *arg, unsigned priority, void *handle);

void vTaskDelay(unsigned ticks);
