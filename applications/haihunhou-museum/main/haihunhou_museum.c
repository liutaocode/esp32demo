#include "haihunhou_museum.h"
#include "haihunhou_museum_state.h"
#include "haihunhou_adpcm.h"
#include "haihunhou_tts_audio.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "ui_pixel.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdint.h>

LV_FONT_DECLARE(haihunhou_zh_16);

#define AUDIO_REQUEST_STOP UINT32_MAX

static const char *TAG = "haihunhou_ui";

typedef enum {
    ART_SITE,
    ART_LIU_HE,
    ART_HOOF_GOLD,
    ART_GOLD_CAKE,
    ART_GOOSE_LAMP,
    ART_CONFUCIUS_SCREEN,
    ART_LUNYU,
    ART_BELLS,
    ART_COINS,
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
        "海昏侯国遗址博物馆", "云游序厅", "江西南昌 · 汉代海昏侯国",
        "一座侯国都城、墓园与陵墓，共同保存两千年前的生活。跟随出土器物，走进刘贺的时代。",
        "遗址自2011年开始考古发掘，出土文物一万余件套。博物馆与遗址现场共同讲述海昏侯国。",
        ART_SITE, 0x9E3329,
    },
    {
        "刘贺", "墓主人", "昌邑王 · 汉废帝 · 海昏侯",
        "刘贺的一生几经起落：先为昌邑王，短暂即位二十七天，后来被封为第一代海昏侯。",
        "墓中印章、奏牍与器物铭文等证据彼此印证，让考古学家确认墓主身份，并重建他的生活世界。",
        ART_LIU_HE, 0x7B2D26,
    },
    {
        "马蹄金与麟趾金", "汉代黄金", "以祥瑞动物命名的金器",
        "马蹄形与麟趾形金器光泽厚重，名字来自古人想象中的祥瑞动物，也显示汉代贵族的财富。",
        "它们与金饼、金板等一同出土。形制、重量与刻记，为研究西汉黄金制度提供了重要线索。",
        ART_HOOF_GOLD, 0xC8902F,
    },
    {
        "金饼", "汉代黄金", "成组出土的圆形金器",
        "一枚枚金饼像缩小的太阳，表面保留铸造、锤击和使用留下的痕迹，是海昏侯墓的醒目发现。",
        "考古记录的不只是金光，还包括重量、成色、刻划和出土位置；这些信息帮助理解财富如何被管理。",
        ART_GOLD_CAKE, 0xD4A52F,
    },
    {
        "雁鱼青铜灯", "生活器具", "会把烟尘引入水中的灯",
        "大雁回首衔鱼，鱼身托起灯盘。烟气可沿雁颈进入腹中水面，造型与实用巧妙结合。",
        "这类灯体现古人控制烟尘的构想。观看时可顺着灯罩、鱼身、雁颈和雁腹寻找烟气路径。",
        ART_GOOSE_LAMP, 0x477C6B,
    },
    {
        "孔子衣镜", "漆木器", "孔子画像与传记文字",
        "漆木构件上绘有孔子及弟子形象，并写下人物传记。图像、文字和日用器物在这里相遇。",
        "木胎漆器保存不易，出土后需持续保护。它也提醒我们，经典与先贤故事曾进入汉代贵族的日常空间。",
        ART_CONFUCIUS_SCREEN, 0x8D4A2F,
    },
    {
        "《论语》简", "简牍文献", "失传篇章“知道”",
        "写在竹简上的《论语》保存了今天通行本没有的“知道”篇，让失落两千年的文字重新进入视野。",
        "简牍往往残断、字迹漫漶，需要编号、清理、红外成像和缀合释读。每个判断都来自长期协作。",
        ART_LUNYU, 0x6D5132,
    },
    {
        "编钟", "礼乐重器", "成组悬挂的青铜乐器",
        "大小有序的钟组成乐列。它们既能发声，也是身份与礼制的象征，把侯国宴飨和祭祀带回耳边。",
        "观察钟体大小、悬挂次序和纹饰，再想象不同音高依次响起。考古发现让西汉礼乐有了具体形状。",
        ART_BELLS, 0x8A6A34,
    },
    {
        "五铢钱", "货币窖藏", "成堆出土的西汉铜钱",
        "密集的五铢钱曾以绳贯穿、成串存放。锈结的铜钱像时间切片，记录货币流通与财富储藏。",
        "考古人员通过数量、重量、版式和位置研究钱币。它们看似相同，细部却藏着铸造与年代信息。",
        ART_COINS, 0x557A5C,
    },
    {
        "馆长彭明瀚", "考古与公共叙事", "让遗址、文物与观众相连",
        "彭明瀚长期从事江西考古与博物馆工作，推动海昏侯国遗址从考古现场走向公众。",
        "他的策展思路把博物馆、遗址本体与周边环境连成整体，让观众在原址附近理解文物从何而来。",
        ART_CURATOR, 0x4B6580,
    },
    {
        "前往海昏侯", "参观信息", "南昌市新建区大塘坪乡",
        "地图搜索“南昌汉代海昏侯国遗址博物馆”，可导航到国家考古遗址公园。",
        "常规开放9:00—17:00，16:00停止入馆。预约、票务和临时闭馆信息请以出发前官方公告为准。",
        ART_LOCATION, 0xA33C2D,
    },
};

_Static_assert(sizeof(PAGES) / sizeof(PAGES[0]) == HAIHUNHOU_MUSEUM_PAGE_COUNT,
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
static haihunhou_museum_state_t s_state;
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

static void draw_site(void)
{
    canvas_rect(0, 58, 80, 22, 0x8CA26A);
    canvas_rect(5, 27, 70, 34, 0xD5C29A);
    canvas_rect(2, 23, 76, 6, 0x5C3228);
    canvas_rect(10, 18, 60, 6, 0x8C3B2F);
    canvas_rect(19, 33, 42, 28, 0x7A3028);
    canvas_rect(25, 38, 12, 23, 0xD5A84A);
    canvas_rect(43, 38, 12, 23, 0xD5A84A);
    canvas_rect(8, 64, 64, 4, 0xB79A69);
}

static void draw_liu_he(void)
{
    canvas_rect(23, 10, 34, 10, 0x282522);
    canvas_rect(29, 5, 22, 7, 0xA88945);
    canvas_rect(27, 18, 26, 29, 0xD7B08B);
    canvas_rect(31, 27, 4, 3, 0x282522);
    canvas_rect(45, 27, 4, 3, 0x282522);
    canvas_rect(36, 38, 9, 3, 0x89392D);
    canvas_rect(16, 47, 48, 30, 0x7B2D26);
    canvas_rect(24, 50, 32, 27, 0xB79045);
    canvas_rect(35, 50, 10, 27, 0xE4C169);
}

static void draw_hoof_gold(void)
{
    canvas_rect(9, 19, 29, 45, 0xD7A52F);
    canvas_rect(14, 13, 19, 10, 0xF0CE68);
    canvas_rect(14, 36, 8, 28, 0xF4E8CF);
    canvas_rect(25, 36, 8, 28, 0xF4E8CF);
    canvas_rect(43, 25, 28, 39, 0xD7A52F);
    canvas_rect(48, 19, 18, 10, 0xF0CE68);
    canvas_rect(49, 48, 16, 16, 0xF4E8CF);
    canvas_rect(5, 68, 70, 5, 0x8B613A);
}

static void draw_gold_cake(void)
{
    const int xs[] = {8, 30, 52, 19, 41};
    const int ys[] = {14, 10, 16, 39, 43};
    for (int i = 0; i < 5; i++) {
        canvas_rect(xs[i], ys[i] + 5, 20, 15, 0xB77A25);
        canvas_rect(xs[i] + 3, ys[i] + 2, 14, 18, 0xD9A52E);
        canvas_rect(xs[i] + 6, ys[i] + 5, 8, 7, 0xF0CE68);
    }
    canvas_rect(5, 67, 70, 6, 0x75442F);
}

static void draw_goose_lamp(void)
{
    canvas_rect(20, 34, 39, 29, 0x3D756A);
    canvas_rect(14, 40, 17, 17, 0x578F7E);
    canvas_rect(49, 22, 11, 34, 0x3D756A);
    canvas_rect(46, 13, 22, 16, 0x578F7E);
    canvas_rect(64, 17, 11, 5, 0x3D756A);
    canvas_rect(53, 18, 3, 3, 0x182D2A);
    canvas_rect(27, 60, 5, 11, 0x3D756A);
    canvas_rect(49, 60, 5, 11, 0x3D756A);
    canvas_rect(8, 70, 62, 5, 0x74503B);
    canvas_rect(5, 27, 31, 5, 0xC89A35);
    canvas_rect(7, 21, 27, 7, 0xE1B74E);
}

static void draw_confucius_screen(void)
{
    canvas_rect(8, 8, 64, 61, 0x552D25);
    canvas_rect(13, 13, 54, 51, 0xA13B2F);
    canvas_rect(18, 17, 25, 43, 0xD3B064);
    canvas_rect(25, 20, 11, 12, 0xD5B28B);
    canvas_rect(23, 17, 15, 6, 0x2D2925);
    canvas_rect(21, 33, 20, 25, 0x394E53);
    for (int y = 20; y < 56; y += 7) canvas_rect(48, y, 13, 3, 0xE4C97A);
    canvas_rect(4, 69, 72, 5, 0x6E432D);
}

static void draw_lunyu(void)
{
    for (int x = 9; x <= 65; x += 8) {
        canvas_rect(x, 9, 6, 61, 0xAA814F);
        canvas_rect(x + 1, 15, 2, 4, 0x443629);
        canvas_rect(x + 1, 25, 3, 3, 0x443629);
        canvas_rect(x + 2, 35, 2, 5, 0x443629);
        canvas_rect(x + 1, 48, 3, 3, 0x443629);
    }
    canvas_rect(5, 12, 70, 3, 0x6C4C30);
    canvas_rect(5, 65, 70, 3, 0x6C4C30);
}

static void draw_bells(void)
{
    canvas_rect(5, 10, 6, 62, 0x65432F);
    canvas_rect(69, 10, 6, 62, 0x65432F);
    canvas_rect(5, 10, 70, 6, 0x65432F);
    for (int i = 0; i < 5; i++) {
        int x = 12 + i * 12;
        int y = 21 + i * 4;
        canvas_rect(x + 4, 16, 3, y - 12, 0x7B5739);
        canvas_rect(x, y, 11, 25, 0x557767);
        canvas_rect(x + 2, y + 4, 7, 3, 0x8DAA82);
        canvas_rect(x - 2, y + 22, 15, 4, 0x3D5A50);
    }
}

static void draw_coins(void)
{
    for (int row = 0; row < 3; row++) {
        for (int column = 0; column < 4; column++) {
            int x = 7 + column * 18 + (row % 2) * 5;
            int y = 10 + row * 21;
            canvas_rect(x, y + 3, 15, 12, 0x486E56);
            canvas_rect(x + 3, y, 9, 18, 0x648B68);
            canvas_rect(x + 6, y + 6, 4, 5, 0x273B30);
        }
    }
    canvas_rect(4, 70, 72, 5, 0x79523A);
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
        case ART_SITE: draw_site(); break;
        case ART_LIU_HE: draw_liu_he(); break;
        case ART_HOOF_GOLD: draw_hoof_gold(); break;
        case ART_GOLD_CAKE: draw_gold_cake(); break;
        case ART_GOOSE_LAMP: draw_goose_lamp(); break;
        case ART_CONFUCIUS_SCREEN: draw_confucius_screen(); break;
        case ART_LUNYU: draw_lunyu(); break;
        case ART_BELLS: draw_bells(); break;
        case ART_COINS: draw_coins(); break;
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
                          HAIHUNHOU_MUSEUM_PAGE_COUNT);
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
    if (clip_index >= haihunhou_tts_clip_count) {
        ESP_LOGE(TAG, "missing TTS clip %u", (unsigned)clip_index);
        set_audio_status("语音不可用");
        return;
    }

    const haihunhou_tts_clip_t *clip = &haihunhou_tts_clips[clip_index];
    if (clip->sample_count == 0 ||
        clip->offset + clip->adpcm_size > haihunhou_tts_audio_data_size ||
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

    haihunhou_adpcm_state_t decoder;
    haihunhou_adpcm_init(&decoder, clip->initial_predictor,
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
                    haihunhou_tts_audio_data[clip->offset + nibble / 2];
                const uint8_t code =
                    (nibble & 1) ? (packed >> 4) : (packed & 0x0f);
                s_pcm_buffer[count++] = haihunhou_adpcm_decode(&decoder, code);
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
        while (request > 0 && request <= haihunhou_tts_clip_count) {
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
    lv_obj_t *label = ui_pixel_label(button, text, &haihunhou_zh_16, UI_INK);
    lv_obj_center(label);
}

void haihunhou_museum_enter(bool buttons_available, bool audio_available)
{
    s_buttons_available = buttons_available;
    s_audio_available = audio_available;
    if (haihunhou_tts_clip_count != HAIHUNHOU_MUSEUM_PAGE_COUNT * 2) {
        ESP_LOGE(TAG, "TTS clip count mismatch: %u", (unsigned)haihunhou_tts_clip_count);
        s_audio_available = false;
    }
    haihunhou_museum_state_init(&s_state);
    s_screen = ui_pixel_screen_create("HAIHUN");

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

    s_title = ui_pixel_label(card, "", &haihunhou_zh_16, UI_INK);
    lv_obj_set_pos(s_title, 101, 25);
    lv_obj_set_width(s_title, 105);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_WRAP);

    s_category = ui_pixel_label(card, "", &haihunhou_zh_16, 0xFFFFFF);
    lv_obj_set_pos(s_category, 101, 67);
    lv_obj_set_style_bg_opa(s_category, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(s_category, 5, 0);
    lv_obj_set_style_pad_right(s_category, 5, 0);
    lv_obj_set_style_pad_top(s_category, 1, 0);
    lv_obj_set_style_pad_bottom(s_category, 1, 0);

    s_context = ui_pixel_label(card, "", &haihunhou_zh_16, 0x563C2E);
    lv_obj_set_pos(s_context, 5, 100);
    lv_obj_set_width(s_context, 202);
    lv_label_set_long_mode(s_context, LV_LABEL_LONG_CLIP);

    lv_obj_t *body_panel = museum_block(card, 5, 122, 202, 69, 0xF7F0DF);
    lv_obj_set_style_border_width(body_panel, 2, 0);
    lv_obj_set_style_border_color(body_panel, lv_color_hex(0x76513A), 0);
    lv_obj_set_style_pad_all(body_panel, 4, 0);
    s_body = ui_pixel_label(body_panel, "", &haihunhou_zh_16, UI_INK);
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
        &haihunhou_zh_16, 0xFFFFFF);
    lv_obj_set_width(s_mode, 190);
    lv_obj_set_style_text_align(s_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_mode);

    refresh_page();
    refresh_battery(NULL);
    s_battery_timer = lv_timer_create(refresh_battery, 10000, NULL);
    lv_screen_load(s_screen);

    if (s_audio_available && !s_audio_task) {
        if (xTaskCreate(audio_task, "haihunhou_tts", 4096, NULL, 4,
                        &s_audio_task) != pdPASS) {
            s_audio_available = false;
            lv_label_set_text(s_mode, "语音不可用");
        } else {
            request_current_audio();
        }
    }
}

void haihunhou_museum_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_CLICK) return;
    if (button == BSP_BTN_UP) {
        haihunhou_museum_state_move(&s_state, -1);
        refresh_page();
    } else if (button == BSP_BTN_DOWN) {
        haihunhou_museum_state_move(&s_state, 1);
        refresh_page();
    } else if (button == BSP_BTN_OK) {
        haihunhou_museum_state_toggle_detail(&s_state);
        refresh_page();
    }
    request_current_audio();
}
