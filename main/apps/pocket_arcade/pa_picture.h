#pragma once
#include <stdint.h>

/* 一张七乘七的小图:name 是画的是什么,row[y] 的低七位是那一行涂了哪些格。 */
typedef struct {
    const char *name;
    uint8_t row[7];
} pa_picture_t;

extern const pa_picture_t PA_PICTURE[];
extern const unsigned PA_PICTURE_COUNT;
