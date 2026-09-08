// main/apps/ebook/ebook_audio.h —— 翻页音效。
//
// 声音是程序实时合成的滤波噪声,不带任何音频素材:两下"沙沙"的纸页摩擦,
// 下一页是先重后轻(纸从右往左翻过去),上一页反过来。合成函数是纯 C,
// 无状态,任意偏移都能重现同一个采样,由主机测试覆盖。
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum { EB_AUDIO_RATE = 16000 };

typedef enum {
    EB_SOUND_NONE = 0,
    EB_SOUND_PAGE_NEXT,
    EB_SOUND_PAGE_PREV,
    EB_SOUND_COUNT,
} eb_sound_t;

// 一段音效的总采样数;非法值返回 0。
size_t eb_sound_samples(eb_sound_t sound);

// 渲染 [offset, offset+capacity) 区间的采样,返回实际写入数。
// 与偏移无关:分块渲染和整段渲染必须逐采样相同。
size_t eb_sound_render(eb_sound_t sound, size_t offset, int16_t *pcm, size_t capacity);

// 独立的常驻任务,不持有界面指针,也不碰 LVGL。下面三个调用只写原子状态和
// 一个单格队列,绝不在调用方线程里做 PCM 输出。
void eb_audio_prepare(void);
void eb_audio_active(bool active);
void eb_audio_play(eb_sound_t sound);
