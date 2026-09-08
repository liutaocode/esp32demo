#include "six_arts_museum.h"
#include "six_arts_museum_state.h"
#include "six_arts_adpcm.h"
#include "six_arts_tts_audio.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "ui_pixel.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdint.h>

LV_FONT_DECLARE(six_arts_zh_16);

#define AUDIO_REQUEST_STOP UINT32_MAX

static const char *TAG = "six_arts_ui";

typedef enum {
    ART_MUSEUM,
    ART_QUETI,
    ART_BED,
    ART_DOORS,
    ART_WINDOW,
    ART_SEDAN,
    ART_PLAQUE,
    ART_BUDDHA,
    ART_FOUNDER,
    ART_CURATOR,
    ART_LOCATION,
} museum_art_t;

typedef struct {
    const char *title;
    const char *category;
    const char *context;
    const char *story;
    const char *detail;
    museum_art_t art;
    uint32_t accent;
} museum_page_t;

static const museum_page_t PAGES[] = {
    {
        "六悦博物馆", "云游序厅", "江苏苏州 · 黎里古镇",
        "四层展厅、六十余个展馆，陈列四万多件民间艺术。从日常老物件，看见无名匠人的心血。",
        "馆名取意眼、耳、鼻、舌、身、心皆悦。大量展品开放陈列，让传统艺术靠近今天的生活。",
        ART_MUSEUM, 0xD44B35,
    },
    {
        "狮子雀替", "建筑木雕", "柱与梁之间的承托构件",
        "雀替兼顾承重与装饰。狮子题材雕工繁密，把守护、吉祥和木匠技艺凝在屋檐下。",
        "它位于立柱与横梁的交接处，既加固建筑又装点空间，是理解中国古建筑细节的一把钥匙。",
        ART_QUETI, 0xC98A32,
    },
    {
        "多进拔步床", "民间家具", "明式床 · 床榻馆代表",
        "它像一间缩小的房屋：床前层层围合，过去兼顾睡眠、更衣、饮食与私密生活。",
        "报道记载床榻馆有四十六件藏品，并将这架多进明式拔步床称为镇馆之宝。",
        ART_BED, 0x9E3E32,
    },
    {
        "门神彩绘门", "宅门艺术", "五楼 · 门神展区",
        "成对门神以浓彩守护入口。古门不仅隔开内外，也把家族愿望、身份与礼俗画在门面上。",
        "五楼路线串起门神、药柜、书柜与古床。细看彩绘门，可观察服饰、兵器与人物神态。",
        ART_DOORS, 0xBC342E,
    },
    {
        "明清花窗", "建筑构件", "木雕与格栅窗",
        "花窗来自中国多地。几何格栅与雕花把光影变成装饰，是六悦最醒目的馆藏类别之一。",
        "一扇窗既通风采光，也借纹样表达祝愿。馆方称这里拥有规模可观的明清雕花窗收藏。",
        ART_WINDOW, 0x287B72,
    },
    {
        "神轿", "礼俗器物", "三楼 · 神轿展区",
        "神轿服务于迎神赛会等民间仪式。雕刻与彩绘把信仰变成可以移动的节庆舞台。",
        "三楼还陈列神龛与漆画。把神轿放回仪式场景，才能读懂它与社区共同记忆的联系。",
        ART_SEDAN, 0xD06231,
    },
    {
        "达尊有二匾", "清代匾额", "光绪三年 · 公元1877年",
        "这方寿匾祝一位八旬长者。“二”留出谦逊余地，让老匾记录下乡里礼俗与分寸。",
        "“达尊”指爵、齿、德三种尊荣。匾文只取其二，避免三者占尽，措辞颇有讲究。",
        ART_PLAQUE, 0x76512F,
    },
    {
        "万佛石窟", "石雕造像", "一楼 · 佛墙与石雕展",
        "古代石雕造像汇成佛墙，随六种色彩变换光影，是走进六悦最震撼的场景之一。",
        "馆方称它为对龙门石窟的艺术化想象，让散落的石雕在新空间里重新形成群像。",
        ART_BUDDHA, 0xA76A32,
    },
    {
        "创办人杜维明", "四十年收藏", "Mitch Dudek · 美国俄亥俄州",
        "1981年初到中国，从玉石狮子开启收藏。2018年，他让成千上万件老物件在黎里安家。",
        "他在城市与乡村变迁中保存民间器物，希望年轻一代仍能看见工匠心血与往日生活。",
        ART_FOUNDER, 0x356DA8,
    },
    {
        "馆长陈杰", "策展与守护", "让老物件鲜活起来",
        "陈杰负责藏品设计与陈列。鲜亮色彩衬托斑驳老物，让传统不再显得遥远和沉闷。",
        "修复团队遵循少改变、少修理，尽量保留岁月痕迹，开放式布展也拉近观众与器物的距离。",
        ART_CURATOR, 0x6D4A91,
    },
    {
        "前往六悦", "参观信息", "吴江区黎里镇人民东路1号",
        "每日9:00—18:00。地图搜索“六悦博物馆”，即可导航到黎里古镇。",
        "电话0512-63955388。出发前请向馆方确认开放、票务和交通安排。",
        ART_LOCATION, 0xC34236,
    },
};

_Static_assert(sizeof(PAGES) / sizeof(PAGES[0]) == SIX_ARTS_MUSEUM_PAGE_COUNT,
               "museum page count mismatch");

static lv_obj_t *s_screen;
static lv_obj_t *s_art_canvas;
static lv_obj_t *s_counter;
static lv_obj_t *s_title;
static lv_obj_t *s_category;
static lv_obj_t *s_context;
static lv_obj_t *s_body;
static lv_obj_t *s_mode;
static lv_obj_t *s_battery;
static lv_obj_t *s_battery_fill;
static lv_timer_t *s_battery_timer;
static TaskHandle_t s_audio_task;
static bool s_buttons_available;
static bool s_audio_available;
static six_arts_museum_state_t s_state;
static uint16_t s_art_buffer[80 * 80];
static int16_t s_pcm_buffer[512];

static lv_obj_t *museum_block(lv_obj_t *parent, int x, int y, int width, int height,
                              uint32_t color)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_radius(object, 0, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    return object;
}

static void canvas_rect(int x, int y, int width, int height, uint32_t color)
{
    if (x < 0) { width += x; x = 0; }
    if (y < 0) { height += y; y = 0; }
    if (x + width > 80) width = 80 - x;
    if (y + height > 80) height = 80 - y;
    if (width <= 0 || height <= 0) return;

    uint16_t pixel = lv_color_to_u16(lv_color_hex(color));
    for (int row = y; row < y + height; row++) {
        for (int column = x; column < x + width; column++) {
            s_art_buffer[row * 80 + column] = pixel;
        }
    }
}

static void draw_museum(void)
{
    canvas_rect(5, 22, 70, 52, 0xE8D9B8);
    canvas_rect(2, 18, 76, 7, 0x263D3A);
    canvas_rect(8, 13, 64, 6, 0x365953);
    canvas_rect(14, 8, 52, 6, 0x263D3A);
    for (int x = 10; x <= 64; x += 18) canvas_rect(x, 28, 6, 46, 0xB83C32);
    canvas_rect(18, 34, 44, 40, 0x345952);
    canvas_rect(23, 39, 14, 35, 0xD19A3F);
    canvas_rect(43, 39, 14, 35, 0xD19A3F);
    canvas_rect(0, 74, 80, 6, 0x6E9B72);
}

static void draw_queti(void)
{
    canvas_rect(7, 11, 66, 9, 0x593A2C);
    canvas_rect(35, 17, 10, 63, 0x76513A);
    canvas_rect(13, 20, 54, 8, 0xA86D39);
    canvas_rect(18, 28, 44, 7, 0xC98A32);
    canvas_rect(22, 35, 14, 10, 0xD9A74D);
    canvas_rect(44, 35, 14, 10, 0xD9A74D);
    canvas_rect(18, 43, 18, 8, 0xA86D39);
    canvas_rect(44, 43, 18, 8, 0xA86D39);
    canvas_rect(24, 37, 4, 4, 0x2B2925);
    canvas_rect(52, 37, 4, 4, 0x2B2925);
    canvas_rect(16, 51, 20, 5, 0x76513A);
    canvas_rect(44, 51, 20, 5, 0x76513A);
}

static void draw_bed(void)
{
    canvas_rect(9, 13, 62, 7, 0x5D3528);
    canvas_rect(11, 18, 6, 56, 0x76513A);
    canvas_rect(63, 18, 6, 56, 0x76513A);
    canvas_rect(20, 20, 40, 8, 0xB48A4D);
    canvas_rect(19, 29, 7, 36, 0x9E3E32);
    canvas_rect(54, 29, 7, 36, 0x9E3E32);
    canvas_rect(26, 31, 28, 30, 0xE2C992);
    canvas_rect(15, 61, 50, 12, 0x8E4D32);
    canvas_rect(22, 55, 36, 8, 0xC75D4A);
    canvas_rect(30, 34, 4, 21, 0xC75D4A);
    canvas_rect(46, 34, 4, 21, 0xC75D4A);
}

static void draw_doors(void)
{
    canvas_rect(10, 8, 60, 69, 0x563C2E);
    canvas_rect(14, 12, 24, 61, 0xB93631);
    canvas_rect(42, 12, 24, 61, 0xB93631);
    canvas_rect(38, 12, 4, 61, 0xE0AA47);
    for (int side = 0; side < 2; side++) {
        int x = side ? 47 : 19;
        canvas_rect(x, 22, 14, 18, 0xD9A340);
        canvas_rect(x + 3, 18, 8, 7, 0xE7C766);
        canvas_rect(x + 3, 27, 3, 3, 0x26313A);
        canvas_rect(x + 9, 27, 3, 3, 0x26313A);
        canvas_rect(x + 5, 34, 6, 3, 0x8E302B);
        canvas_rect(x + 2, 43, 16, 20, 0x315E73);
    }
}

static void draw_window(void)
{
    canvas_rect(7, 7, 66, 66, 0x5A3A29);
    canvas_rect(13, 13, 54, 54, 0xE8D9B8);
    for (int x = 18; x <= 58; x += 10) canvas_rect(x, 13, 4, 54, 0x76513A);
    for (int y = 18; y <= 58; y += 10) canvas_rect(13, y, 54, 4, 0x76513A);
    canvas_rect(28, 28, 24, 24, 0xE8D9B8);
    canvas_rect(31, 31, 18, 18, 0xB46C42);
    canvas_rect(36, 26, 8, 28, 0xE8D9B8);
    canvas_rect(26, 36, 28, 8, 0xE8D9B8);
}

static void draw_sedan(void)
{
    canvas_rect(2, 66, 76, 5, 0x6B452E);
    canvas_rect(15, 24, 50, 42, 0xB93631);
    canvas_rect(11, 20, 58, 7, 0xD59A36);
    canvas_rect(18, 15, 44, 6, 0x8F352C);
    canvas_rect(23, 30, 34, 31, 0xD2A348);
    canvas_rect(27, 34, 26, 27, 0x346778);
    canvas_rect(34, 38, 12, 23, 0xE9D4A2);
    canvas_rect(11, 29, 5, 32, 0xE2BD62);
    canvas_rect(64, 29, 5, 32, 0xE2BD62);
}

static void draw_plaque(void)
{
    canvas_rect(8, 20, 64, 42, 0x543728);
    canvas_rect(12, 24, 56, 34, 0x8C5A31);
    canvas_rect(17, 29, 46, 24, 0xB87B38);
    canvas_rect(23, 33, 4, 16, 0xE5C76B);
    canvas_rect(32, 33, 4, 16, 0xE5C76B);
    canvas_rect(41, 33, 4, 16, 0xE5C76B);
    canvas_rect(50, 33, 4, 16, 0xE5C76B);
    canvas_rect(18, 16, 8, 5, 0x6B452E);
    canvas_rect(54, 16, 8, 5, 0x6B452E);
}

static void draw_buddha(void)
{
    canvas_rect(5, 7, 70, 67, 0x6F5B49);
    for (int row = 0; row < 3; row++) {
        for (int column = 0; column < 5; column++) {
            int x = 10 + column * 13;
            int y = 12 + row * 20;
            uint32_t stone = (row + column) % 2 ? 0xC7AA78 : 0xA98D63;
            canvas_rect(x + 3, y, 6, 5, stone);
            canvas_rect(x + 1, y + 5, 10, 10, stone);
            canvas_rect(x + 4, y + 7, 2, 2, 0x594C42);
            canvas_rect(x + 7, y + 7, 2, 2, 0x594C42);
            canvas_rect(x + 3, y + 12, 6, 2, 0x806B50);
        }
    }
    canvas_rect(0, 74, 80, 6, 0xB43C34);
}

static void draw_founder(void)
{
    canvas_rect(9, 13, 45, 50, 0xE8D9B8);
    canvas_rect(13, 17, 37, 42, 0xB8D2D0);
    canvas_rect(18, 22, 7, 7, 0xC34236);
    canvas_rect(29, 30, 8, 8, 0xC98A32);
    canvas_rect(40, 20, 6, 12, 0x356DA8);
    canvas_rect(46, 55, 25, 20, 0x76513A);
    canvas_rect(50, 50, 17, 7, 0x9A6844);
    canvas_rect(53, 59, 11, 12, 0xD5B28B);
    canvas_rect(55, 61, 3, 3, 0x26313A);
    canvas_rect(61, 61, 3, 3, 0x26313A);
    canvas_rect(56, 67, 7, 3, 0x8E302B);
    canvas_rect(25, 65, 25, 5, 0x356DA8);
}

static void draw_curator(void)
{
    canvas_rect(8, 12, 45, 43, 0x6D4A91);
    canvas_rect(13, 17, 35, 33, 0xE7C766);
    canvas_rect(18, 22, 25, 23, 0xB93631);
    canvas_rect(50, 21, 17, 18, 0xD6B18B);
    canvas_rect(53, 17, 11, 8, 0x29363B);
    canvas_rect(53, 27, 4, 3, 0x26313A);
    canvas_rect(61, 27, 4, 3, 0x26313A);
    canvas_rect(47, 40, 25, 34, 0x356DA8);
    canvas_rect(39, 47, 10, 6, 0xD6B18B);
    canvas_rect(28, 43, 13, 5, 0xD6B18B);
}

static void draw_location(void)
{
    canvas_rect(8, 13, 64, 54, 0xE8D9B8);
    canvas_rect(14, 20, 45, 5, 0x6E9B72);
    canvas_rect(29, 21, 5, 39, 0x6E9B72);
    canvas_rect(14, 48, 50, 5, 0x9AB6C1);
    canvas_rect(48, 12, 19, 24, 0xC34236);
    canvas_rect(44, 18, 27, 12, 0xC34236);
    canvas_rect(52, 18, 11, 11, 0xF2D38A);
    canvas_rect(54, 35, 7, 9, 0xC34236);
    canvas_rect(56, 42, 3, 4, 0xC34236);
    canvas_rect(10, 68, 60, 5, 0x76513A);
}

static void draw_art(museum_art_t art)
{
    canvas_rect(0, 0, 80, 80, 0xF4E8CF);
    switch (art) {
        case ART_MUSEUM: draw_museum(); break;
        case ART_QUETI: draw_queti(); break;
        case ART_BED: draw_bed(); break;
        case ART_DOORS: draw_doors(); break;
        case ART_WINDOW: draw_window(); break;
        case ART_SEDAN: draw_sedan(); break;
        case ART_PLAQUE: draw_plaque(); break;
        case ART_BUDDHA: draw_buddha(); break;
        case ART_FOUNDER: draw_founder(); break;
        case ART_CURATOR: draw_curator(); break;
        case ART_LOCATION: draw_location(); break;
    }
    lv_obj_invalidate(s_art_canvas);
}

static void refresh_battery(lv_timer_t *timer)
{
    (void)timer;
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(s_battery, "--");
        lv_obj_set_style_bg_opa(s_battery_fill, LV_OPA_TRANSP, 0);
        return;
    }
    if (soc > 100) soc = 100;
    lv_label_set_text_fmt(s_battery, "%d%%", soc);
    lv_obj_set_width(s_battery_fill, LV_MAX(1, (36 * soc + 99) / 100));
    lv_obj_set_style_bg_color(s_battery_fill,
        lv_color_hex(soc <= 20 ? 0xC34236 : (soc <= 45 ? 0xD6A63A : 0x4F8B57)), 0);
    lv_obj_set_style_bg_opa(s_battery_fill, LV_OPA_COVER, 0);
}

static void refresh_page(void)
{
    const museum_page_t *page = &PAGES[s_state.page];
    lv_label_set_text_fmt(s_counter, "%02u / %02u", (unsigned)(s_state.page + 1),
                          SIX_ARTS_MUSEUM_PAGE_COUNT);
    lv_label_set_text(s_title, page->title);
    lv_label_set_text(s_category, page->category);
    lv_obj_set_style_bg_color(s_category, lv_color_hex(page->accent), 0);
    lv_label_set_text(s_context, page->context);
    lv_label_set_text(s_body, s_state.detail_view ? page->detail : page->story);
    if (!s_buttons_available) {
        lv_label_set_text(s_mode, "按键不可用");
    } else if (!s_audio_available) {
        lv_label_set_text(s_mode, "语音不可用 · 确定切换");
    } else {
        lv_label_set_text(s_mode, s_state.detail_view
            ? "细看 · 确定返回故事"
            : "故事 · 确定查看细节");
    }
    draw_art(page->art);
}

static void set_audio_status(const char *text)
{
    if (!bsp_lvgl_lock(500)) return;
    if (s_mode) lv_label_set_text(s_mode, text);
    bsp_lvgl_unlock();
}

static void set_idle_status(void)
{
    if (!bsp_lvgl_lock(500)) return;
    if (s_mode) {
        if (!s_buttons_available) {
            lv_label_set_text(s_mode, "按键不可用");
        } else {
            lv_label_set_text(s_mode, s_state.detail_view
                ? "细看 · 确定返回故事"
                : "故事 · 确定查看细节");
        }
    }
    bsp_lvgl_unlock();
}

static void play_clip(size_t clip_index, uint32_t *next_request)
{
    *next_request = 0;
    if (clip_index >= six_arts_tts_clip_count) {
        ESP_LOGE(TAG, "missing TTS clip %u", (unsigned)clip_index);
        set_audio_status("语音不可用");
        return;
    }

    const six_arts_tts_clip_t *clip = &six_arts_tts_clips[clip_index];
    if (clip->sample_count == 0 ||
        clip->offset + clip->adpcm_size > six_arts_tts_audio_data_size ||
        (clip->sample_count / 2) > clip->adpcm_size) {
        ESP_LOGE(TAG, "invalid TTS clip %u", (unsigned)clip_index);
        set_audio_status("语音不可用");
        return;
    }
    if (bsp_audio_set_format(16000, 16, 1) != ESP_OK) {
        set_audio_status("语音不可用");
        return;
    }

    bsp_audio_set_volume(72);
    set_audio_status("云导游讲解中…");
    ESP_LOGI(TAG, "playing TTS clip %u", (unsigned)clip_index);

    six_arts_adpcm_state_t decoder;
    six_arts_adpcm_init(&decoder, clip->initial_predictor,
                        clip->initial_step_index);
    size_t sample = 0;
    while (sample < clip->sample_count) {
        if (xTaskNotifyWait(0, UINT32_MAX, next_request, 0) == pdTRUE) return;

        size_t count = 0;
        while (count < 512 && sample < clip->sample_count) {
            if (sample == 0) {
                s_pcm_buffer[count++] = (int16_t)decoder.predictor;
            } else {
                const size_t nibble = sample - 1;
                const uint8_t packed =
                    six_arts_tts_audio_data[clip->offset + nibble / 2];
                const uint8_t code =
                    (nibble & 1) ? (packed >> 4) : (packed & 0x0f);
                s_pcm_buffer[count++] = six_arts_adpcm_decode(&decoder, code);
            }
            sample++;
        }
        if (bsp_audio_write(s_pcm_buffer,
                            count * sizeof(s_pcm_buffer[0])) != ESP_OK) {
            ESP_LOGE(TAG, "TTS playback failed at sample %u",
                     (unsigned)sample);
            set_audio_status("语音播放失败");
            return;
        }
    }
    set_idle_status();
}

static void audio_task(void *argument)
{
    (void)argument;
    for (;;) {
        uint32_t request = 0;
        xTaskNotifyWait(0, UINT32_MAX, &request, portMAX_DELAY);
        while (request > 0 && request <= six_arts_tts_clip_count) {
            uint32_t next_request = 0;
            play_clip(request - 1, &next_request);
            request = next_request;
        }
        if (request == AUDIO_REQUEST_STOP) break;
    }
    s_audio_task = NULL;
    vTaskDelete(NULL);
}

static void request_current_audio(void)
{
    if (!s_audio_task) return;
    const size_t clip_index =
        s_state.page * 2 + (s_state.detail_view ? 1 : 0);
    xTaskNotify(s_audio_task, (uint32_t)clip_index + 1,
                eSetValueWithOverwrite);
}

static void create_nav_button(int x, const char *text)
{
    museum_block(s_screen, x + 3, 273, 68, 24, UI_INK);
    lv_obj_t *button = museum_block(s_screen, x, 270, 68, 24, 0xE7D7B5);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(UI_INK), 0);
    lv_obj_t *label = ui_pixel_label(button, text, &six_arts_zh_16, UI_INK);
    lv_obj_center(label);
}

void six_arts_museum_enter(bool buttons_available, bool audio_available)
{
    s_buttons_available = buttons_available;
    s_audio_available = audio_available;
    if (six_arts_tts_clip_count != SIX_ARTS_MUSEUM_PAGE_COUNT * 2) {
        ESP_LOGE(TAG, "TTS clip count mismatch: %u", (unsigned)six_arts_tts_clip_count);
        s_audio_available = false;
    }
    six_arts_museum_state_init(&s_state);
    s_screen = ui_pixel_screen_create("SIX ARTS");

    museum_block(s_screen, 181, 36, 48, 20, UI_INK);
    lv_obj_t *battery_panel = museum_block(s_screen, 178, 33, 50, 20, 0x45413A);
    lv_obj_set_style_border_width(battery_panel, 2, 0);
    lv_obj_set_style_border_color(battery_panel, lv_color_hex(UI_INK), 0);
    museum_block(s_screen, 228, 38, 4, 9, UI_INK);
    s_battery_fill = museum_block(battery_panel, 4, 3, 36, 10, 0x4F8B57);
    s_battery = lv_label_create(battery_panel);
    lv_obj_set_style_text_font(s_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_battery, lv_color_white(), 0);
    lv_obj_set_pos(s_battery, 0, 0);
    lv_obj_set_width(s_battery, 46);
    lv_label_set_long_mode(s_battery, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *card = ui_pixel_panel_create(s_screen, 8, 57, 224, 205, 0xE8D9B8);
    lv_obj_t *art_frame = museum_block(card, 5, 7, 88, 88, 0xF4E8CF);
    lv_obj_set_style_border_width(art_frame, 3, 0);
    lv_obj_set_style_border_color(art_frame, lv_color_hex(UI_INK), 0);
    s_art_canvas = lv_canvas_create(art_frame);
    lv_canvas_set_buffer(s_art_canvas, s_art_buffer, 80, 80, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_art_canvas, 4, 4);

    s_counter = ui_pixel_label(card, "", &lv_font_montserrat_14, 0x5D6A65);
    lv_obj_set_pos(s_counter, 101, 3);

    s_title = ui_pixel_label(card, "", &six_arts_zh_16, UI_INK);
    lv_obj_set_pos(s_title, 101, 25);
    lv_obj_set_width(s_title, 105);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_WRAP);

    s_category = ui_pixel_label(card, "", &six_arts_zh_16, 0xFFFFFF);
    lv_obj_set_pos(s_category, 101, 67);
    lv_obj_set_style_bg_opa(s_category, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(s_category, 5, 0);
    lv_obj_set_style_pad_right(s_category, 5, 0);
    lv_obj_set_style_pad_top(s_category, 1, 0);
    lv_obj_set_style_pad_bottom(s_category, 1, 0);

    s_context = ui_pixel_label(card, "", &six_arts_zh_16, 0x563C2E);
    lv_obj_set_pos(s_context, 5, 100);
    lv_obj_set_width(s_context, 202);
    lv_label_set_long_mode(s_context, LV_LABEL_LONG_CLIP);

    lv_obj_t *body_panel = museum_block(card, 5, 122, 202, 69, 0xF7F0DF);
    lv_obj_set_style_border_width(body_panel, 2, 0);
    lv_obj_set_style_border_color(body_panel, lv_color_hex(0x76513A), 0);
    lv_obj_set_style_pad_all(body_panel, 4, 0);
    s_body = ui_pixel_label(body_panel, "", &six_arts_zh_16, UI_INK);
    lv_obj_set_pos(s_body, 0, -2);
    lv_obj_set_width(s_body, 190);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_line_space(s_body, -3, 0);

    create_nav_button(8, "上一页");
    create_nav_button(86, "确定");
    create_nav_button(164, "下一页");

    lv_obj_t *mode_panel = museum_block(s_screen, 20, 300, 200, 20, 0x493A31);
    lv_obj_set_style_border_width(mode_panel, 1, 0);
    lv_obj_set_style_border_color(mode_panel, lv_color_hex(UI_INK), 0);
    s_mode = ui_pixel_label(mode_panel,
        !buttons_available ? "按键不可用" :
        (!s_audio_available ? "语音不可用 · 确定切换" :
                              "故事 · 确定查看细节"),
        &six_arts_zh_16, 0xFFFFFF);
    lv_obj_set_width(s_mode, 190);
    lv_obj_set_style_text_align(s_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_mode);

    refresh_page();
    refresh_battery(NULL);
    s_battery_timer = lv_timer_create(refresh_battery, 10000, NULL);
    lv_screen_load(s_screen);

    if (s_audio_available && !s_audio_task) {
        if (xTaskCreate(audio_task, "six_arts_tts", 4096, NULL, 4,
                        &s_audio_task) != pdPASS) {
            s_audio_available = false;
            lv_label_set_text(s_mode, "语音不可用");
        } else {
            request_current_audio();
        }
    }
}

void six_arts_museum_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_CLICK) return;
    if (button == BSP_BTN_UP) {
        six_arts_museum_state_move(&s_state, -1);
        refresh_page();
    } else if (button == BSP_BTN_DOWN) {
        six_arts_museum_state_move(&s_state, 1);
        refresh_page();
    } else if (button == BSP_BTN_OK) {
        six_arts_museum_state_toggle_detail(&s_state);
        refresh_page();
    }
    request_current_audio();
}
