// main/apps/ebook/ebook_audio.c —— 见 ebook_audio.h 的合成部分。
#include "ebook_audio.h"

// 一次纸页摩擦:起点、时长、峰值。两下叠出"翻过去"的感觉。
typedef struct { uint16_t start_ms, length_ms; uint8_t peak; } burst_t;
typedef struct { burst_t burst[2]; uint16_t total_ms; } rustle_t;

static const rustle_t RUSTLE[EB_SOUND_COUNT] = {
    // 下一页:第一下重、第二下轻,像纸被推过去之后落下。
    [EB_SOUND_PAGE_NEXT] = { { { 0, 70, 24 }, { 52, 58, 13 } }, 110 },
    // 上一页:轻重顺序反过来,耳朵能听出方向,不必看屏幕确认翻对了没有。
    [EB_SOUND_PAGE_PREV] = { { { 0, 46, 12 }, { 30, 65, 20 } }, 95 },
};

// 位置决定的确定性噪声。用哈希而不是递推的伪随机数,是为了让 render()
// 无状态:任何偏移都能单独算出来,分块播放和整段播放完全一致。
static int noise_at(size_t index)
{
    uint32_t x = (uint32_t)index * 2654435761u + 0x9E3779B9u;
    x ^= x >> 15;
    x *= 0x85EBCA6Bu;
    x ^= x >> 13;
    return (int)(x & 0xFFu) - 128;
}

size_t eb_sound_samples(eb_sound_t sound)
{
    if (sound <= EB_SOUND_NONE || sound >= EB_SOUND_COUNT) return 0;
    return (size_t)RUSTLE[sound].total_ms * (EB_AUDIO_RATE / 1000);
}

// 单下摩擦在 pos 处的包络。起音 2 ms 拉满避免"咔"的一声,尾巴按平方衰减。
static int envelope_at(const burst_t *burst, size_t pos)
{
    size_t start = (size_t)burst->start_ms * (EB_AUDIO_RATE / 1000);
    size_t length = (size_t)burst->length_ms * (EB_AUDIO_RATE / 1000);
    if (pos < start || pos >= start + length || length == 0) return 0;

    size_t local = pos - start;
    const size_t attack = 2 * (EB_AUDIO_RATE / 1000);
    if (local < attack) return (int)((size_t)burst->peak * local / attack);

    size_t left = length - local;
    return (int)((size_t)burst->peak * left * left / (length * length));
}

size_t eb_sound_render(eb_sound_t sound, size_t offset, int16_t *pcm, size_t capacity)
{
    size_t total = eb_sound_samples(sound);
    if (!pcm || offset >= total) return 0;
    size_t count = total - offset < capacity ? total - offset : capacity;

    const rustle_t *rustle = &RUSTLE[sound];
    for (size_t i = 0; i < count; i++) {
        size_t pos = offset + i;
        // 相邻噪声相减是个一阶高通,把噪声推向"沙沙"的高频,而不是低频轰鸣。
        int hiss = noise_at(pos) - noise_at(pos - 1);
        int level = envelope_at(&rustle->burst[0], pos) + envelope_at(&rustle->burst[1], pos);
        pcm[i] = (int16_t)(hiss * level);
    }
    return count;
}
