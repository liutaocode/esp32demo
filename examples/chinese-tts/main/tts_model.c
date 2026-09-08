#include "tts_model.h"
const tts_sample_t tts_samples[] = {
    {"你好，中文", "你好，我是你的口袋朗读员。很高兴认识你！"},
    {"常用字 · 自然", "天，地，人，日，月，山，水，火，风，雨。"},
    {"常用字 · 生活", "你，我，他，吃，喝，走，跑，看，听，说。"},
    {"数字与金额", "一，二，三，四，五。今天收款十二元五角。"},
    {"数字解析", "现在是8点30分，温度26度，价格12.5元。"},
    {"多音字 · 行", "银行门口，有一个正在行走的人。"},
    {"多音字 · 乐", "听着音乐，我觉得很快乐。"},
    {"多音字 · 重", "请重新称一下，这个箱子很重。"},
    {"声调练习", "妈，麻，马，骂。妈妈骑马，爸爸喝茶。"},
    {"古诗朗读", "床前明月光，疑是地上霜。举头望明月，低头思故乡。"},
    {"生活提醒", "坐了很久，起来走一走。喝一杯水，看看远处。"},
    {"标点与停顿", "你好！今天过得怎么样？休息一下，我们慢慢说。"},
};
const size_t tts_sample_count = sizeof(tts_samples) / sizeof(tts_samples[0]);
void tts_model_move(tts_model_t *m, int direction) {
    if (direction < 0) m->selected = (m->selected + tts_sample_count - 1) % tts_sample_count;
    else if (direction > 0) m->selected = (m->selected + 1) % tts_sample_count;
}
void tts_model_speed(tts_model_t *m) { m->speed = (m->speed + 1) % TTS_SPEED_COUNT; }
bool tts_text_valid(const char *text) {
    if (!text) return false;
    size_t n = 0;
    while (n < TTS_TEXT_BYTES && text[n]) n++;
    if (!n || n == TTS_TEXT_BYTES) return false;
    bool content = false;
    for (size_t i = 0; i < n;) {
        uint32_t c = (unsigned char)text[i++];
        unsigned extra;
        uint32_t min;
        if (c < 0x80) {
            if (c < 0x20 || c == 0x7f) return false;
            if (c != ' ') content = true;
            continue;
        } else if (c >= 0xc2 && c <= 0xdf) { extra = 1; c &= 31; min = 0x80; }
        else if (c >= 0xe0 && c <= 0xef) { extra = 2; c &= 15; min = 0x800; }
        else if (c >= 0xf0 && c <= 0xf4) { extra = 3; c &= 7; min = 0x10000; }
        else return false;
        if (n - i < extra) return false;
        while (extra--) {
            unsigned char b = (unsigned char)text[i++];
            if ((b & 0xc0) != 0x80) return false;
            c = (c << 6) | (b & 63);
        }
        if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
        content = true;
    }
    return content;
}
