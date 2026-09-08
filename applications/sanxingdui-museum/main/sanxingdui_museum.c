#include "sanxingdui_museum.h"
#include "sanxingdui_museum_state.h"
#include "sanxingdui_adpcm.h"
#include "sanxingdui_tts_audio.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "ui_pixel.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdint.h>

LV_FONT_DECLARE(sanxingdui_zh_16);

#define AUDIO_REQUEST_STOP UINT32_MAX

static const char *TAG = "sanxingdui_ui";

typedef enum {
    ART_SITE,
    ART_STANDING_FIGURE,
    ART_SACRED_TREE,
    ART_PROTRUDING_EYES,
    ART_GOLD_STAFF,
    ART_GOLD_MASK,
    ART_KNEELING_FIGURE,
    ART_LATTICE_VESSEL,
    ART_JADE_ZHANG,
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
        "三星堆博物馆", "云游序厅", "四川广汉 · 古蜀文明",
        "神树、面具、金杖与人像从祭祀坑中醒来。十一站云游，走进想象奇绝的古蜀世界。",
        "新馆以“世纪逐梦、巍然王都、天地人神”展开叙事，展出一千五百余件套文物。",
        ART_SITE, 0x5E7650,
    },
    {
        "青铜大立人", "古蜀人像", "通高约2.62米",
        "高冠长袍的人像立在方座上，双手环握却留下空处。那件消失的物品，仍等待新的答案。",
        "人像本体高约1.72米，连座通高约2.62米。夸张双手、层叠礼服与纹饰共同显示庄严身份。",
        ART_STANDING_FIGURE, 0x4D7268,
    },
    {
        "一号青铜神树", "通天神树", "修复后通高约3.96米",
        "枝干分层向上，鸟立花果之间，神龙沿树身蜿蜒。它把古蜀人的宇宙想象铸成立体图景。",
        "现存树体由多段残件修复而成。树枝、果实、神鸟与龙彼此呼应，常被联系到通天与太阳崇拜。",
        ART_SACRED_TREE, 0x3F765E,
    },
    {
        "青铜纵目面具", "神秘面容", "眼柱向外突出",
        "巨耳展开，眼睛像望远镜般向前伸出。超越常人的五官，也许在表现能通达天地的神性。",
        "面具宽约1.38米。关于它代表神、祖先还是传说人物，学界仍有讨论，想象不等于定论。",
        ART_PROTRUDING_EYES, 0x547F70,
    },
    {
        "金杖", "权力象征", "金皮包卷木杖而成",
        "金杖表面刻有人头像、鱼、鸟与箭。连续图像像一段无声叙事，凝聚着古蜀王权与信仰。",
        "出土时内部木芯已朽，仅留卷成杖形的金皮。纹样含义尚有多种解释，细看可辨对称布局。",
        ART_GOLD_STAFF, 0xC69A2E,
    },
    {
        "金面具", "黄金礼器", "新祭祀坑的重要发现",
        "宽阔眉眼、挺直鼻梁与大耳被薄金塑出。它既延续三星堆面具传统，也把金色带入祭祀世界。",
        "面具出土时残缺，考古人员依据折痕与结构展开保护。它如何佩戴、代表谁，仍需更多证据。",
        ART_GOLD_MASK, 0xD3A62C,
    },
    {
        "青铜顶尊跪坐人像", "组合礼器", "人像跪坐并顶负尊",
        "人像双手扶住头顶大尊，身体、衣饰与容器连成复杂整体，像把一场仪式凝固在青铜中。",
        "它由多个部件组合，出土后经过细致拼对修复。人物动作提示承托与奉献，却不能简单还原仪式。",
        ART_KNEELING_FIGURE, 0x587B68,
    },
    {
        "龟背形网格状器", "未解之器", "铜框包裹玉石的独特结构",
        "椭圆网格像龟背，内部包着整块玉石。这件前所未见的器物，成为新一轮考古的代表发现。",
        "它的名称来自外形，并不等于用途结论。铜、玉如何组合，怎样使用，仍是研究中的开放问题。",
        ART_LATTICE_VESSEL, 0x4F7769,
    },
    {
        "祭山图玉璋", "玉石礼器", "线刻图像展开祭祀场景",
        "扁长玉璋上刻着人物、山形与礼仪图像。细线虽小，却像一幅浓缩的古蜀仪式长卷。",
        "“祭山图”是依据图像所作的命名。人物姿态和符号如何解读仍有争议，适合带着问题观看。",
        ART_JADE_ZHANG, 0x70805E,
    },
    {
        "馆长雷雨", "考古与守护", "三星堆研究院学术副院长",
        "雷雨长期参与三星堆考古与研究，从发掘现场到博物馆叙事，持续回答古蜀文明的新问题。",
        "他强调用考古材料理解中华文明多元一体。新发现不断出现，展览也会随着研究更新。",
        ART_CURATOR, 0x576A83,
    },
    {
        "前往三星堆", "参观信息", "四川省广汉市向新路133号",
        "地图搜索“三星堆博物馆”即可导航。常规开放8:30—18:00，17:00停止入馆。",
        "门票建议通过官方微信公众号或官网提前预约。节假日与暑期安排可能变化，请以最新公告为准。",
        ART_LOCATION, 0xA75B31,
    },
};

_Static_assert(sizeof(PAGES) / sizeof(PAGES[0]) == SANXINGDUI_MUSEUM_PAGE_COUNT,
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
static sanxingdui_museum_state_t s_state;
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
    canvas_rect(0, 59, 80, 21, 0x76905E);
    canvas_rect(8, 27, 64, 35, 0xD7C793);
    canvas_rect(4, 23, 72, 6, 0x405E52);
    canvas_rect(14, 18, 52, 6, 0x64806B);
    canvas_rect(23, 34, 34, 28, 0x496E61);
    canvas_rect(29, 40, 9, 22, 0xC7A747);
    canvas_rect(43, 40, 9, 22, 0xC7A747);
    canvas_rect(7, 66, 66, 4, 0xA58A58);
}

static void draw_standing_figure(void)
{
    canvas_rect(29, 6, 22, 8, 0x365F55);
    canvas_rect(33, 13, 14, 14, 0x527D6B);
    canvas_rect(36, 18, 3, 3, 0x1E3832);
    canvas_rect(42, 18, 3, 3, 0x1E3832);
    canvas_rect(28, 27, 24, 35, 0x466E61);
    canvas_rect(19, 31, 11, 7, 0x466E61);
    canvas_rect(50, 31, 11, 7, 0x466E61);
    canvas_rect(13, 28, 9, 14, 0x5B8A77);
    canvas_rect(58, 28, 9, 14, 0x5B8A77);
    canvas_rect(24, 60, 32, 10, 0x35584E);
    canvas_rect(15, 70, 50, 6, 0x7B5C38);
}

static void draw_sacred_tree(void)
{
    canvas_rect(37, 8, 7, 62, 0x3E6E5C);
    for (int y = 17; y <= 52; y += 17) {
        canvas_rect(16, y, 48, 5, 0x4C7D67);
        canvas_rect(15, y - 6, 6, 12, 0x4C7D67);
        canvas_rect(59, y - 6, 6, 12, 0x4C7D67);
        canvas_rect(11, y - 10, 9, 7, 0xC59A36);
        canvas_rect(60, y - 10, 9, 7, 0xC59A36);
    }
    canvas_rect(28, 68, 25, 6, 0x31584C);
    canvas_rect(22, 74, 37, 4, 0x755839);
}

static void draw_protruding_eyes(void)
{
    canvas_rect(9, 18, 62, 43, 0x4E796A);
    canvas_rect(3, 25, 12, 28, 0x5D8B78);
    canvas_rect(65, 25, 12, 28, 0x5D8B78);
    canvas_rect(15, 26, 23, 10, 0x315A50);
    canvas_rect(42, 26, 23, 10, 0x315A50);
    canvas_rect(7, 28, 29, 5, 0x789B82);
    canvas_rect(44, 28, 29, 5, 0x789B82);
    canvas_rect(36, 34, 8, 14, 0x335B51);
    canvas_rect(28, 51, 24, 5, 0x294B43);
    canvas_rect(17, 12, 46, 8, 0x3F695D);
}

static void draw_gold_staff(void)
{
    canvas_rect(8, 35, 64, 10, 0xD0A334);
    canvas_rect(8, 38, 64, 4, 0xF0D06A);
    canvas_rect(15, 27, 12, 8, 0xC58E29);
    canvas_rect(31, 27, 12, 8, 0xC58E29);
    canvas_rect(50, 27, 15, 8, 0xC58E29);
    canvas_rect(18, 29, 3, 3, 0x684A28);
    canvas_rect(34, 29, 3, 3, 0x684A28);
    canvas_rect(55, 28, 6, 4, 0x684A28);
    canvas_rect(6, 53, 68, 5, 0x7B5B38);
}

static void draw_gold_mask(void)
{
    canvas_rect(14, 16, 52, 43, 0xD4A62F);
    canvas_rect(8, 26, 10, 25, 0xC48F24);
    canvas_rect(62, 26, 10, 25, 0xC48F24);
    canvas_rect(21, 28, 15, 6, 0x443B2B);
    canvas_rect(44, 28, 15, 6, 0x443B2B);
    canvas_rect(36, 33, 8, 13, 0xBC8527);
    canvas_rect(29, 49, 22, 5, 0x8A6027);
    canvas_rect(19, 12, 42, 7, 0xE4BD4F);
    canvas_rect(23, 59, 34, 6, 0xB77C25);
}

static void draw_kneeling_figure(void)
{
    canvas_rect(20, 6, 40, 10, 0x477568);
    canvas_rect(25, 15, 30, 17, 0x5D8B78);
    canvas_rect(35, 32, 10, 10, 0x4B7568);
    canvas_rect(30, 41, 20, 24, 0x416A5E);
    canvas_rect(20, 42, 12, 6, 0x4B7568);
    canvas_rect(48, 42, 12, 6, 0x4B7568);
    canvas_rect(20, 61, 23, 8, 0x365B51);
    canvas_rect(37, 61, 23, 8, 0x365B51);
    canvas_rect(15, 69, 50, 5, 0x76583A);
}

static void draw_lattice_vessel(void)
{
    canvas_rect(14, 17, 52, 45, 0x426F62);
    canvas_rect(19, 12, 42, 55, 0x426F62);
    canvas_rect(23, 17, 34, 45, 0xD4C99B);
    for (int x = 25; x < 57; x += 8) canvas_rect(x, 17, 3, 45, 0x547E6D);
    for (int y = 21; y < 61; y += 8) canvas_rect(23, y, 34, 3, 0x547E6D);
    canvas_rect(7, 37, 66, 5, 0x355D52);
    canvas_rect(18, 67, 44, 5, 0x775A3B);
}

static void draw_jade_zhang(void)
{
    canvas_rect(7, 30, 66, 21, 0x789579);
    canvas_rect(11, 25, 58, 31, 0x91AA89);
    canvas_rect(15, 29, 50, 23, 0xB5C29E);
    canvas_rect(19, 34, 8, 13, 0x627C66);
    canvas_rect(31, 33, 8, 14, 0x627C66);
    canvas_rect(43, 34, 8, 13, 0x627C66);
    canvas_rect(55, 33, 6, 14, 0x627C66);
    canvas_rect(5, 60, 70, 5, 0x775A3B);
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
        case ART_STANDING_FIGURE: draw_standing_figure(); break;
        case ART_SACRED_TREE: draw_sacred_tree(); break;
        case ART_PROTRUDING_EYES: draw_protruding_eyes(); break;
        case ART_GOLD_STAFF: draw_gold_staff(); break;
        case ART_GOLD_MASK: draw_gold_mask(); break;
        case ART_KNEELING_FIGURE: draw_kneeling_figure(); break;
        case ART_LATTICE_VESSEL: draw_lattice_vessel(); break;
        case ART_JADE_ZHANG: draw_jade_zhang(); break;
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
                          SANXINGDUI_MUSEUM_PAGE_COUNT);
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
    if (clip_index >= sanxingdui_tts_clip_count) {
        ESP_LOGE(TAG, "missing TTS clip %u", (unsigned)clip_index);
        set_audio_status("语音不可用");
        return;
    }

    const sanxingdui_tts_clip_t *clip = &sanxingdui_tts_clips[clip_index];
    if (clip->sample_count == 0 ||
        clip->offset + clip->adpcm_size > sanxingdui_tts_audio_data_size ||
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

    sanxingdui_adpcm_state_t decoder;
    sanxingdui_adpcm_init(&decoder, clip->initial_predictor,
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
                    sanxingdui_tts_audio_data[clip->offset + nibble / 2];
                const uint8_t code =
                    (nibble & 1) ? (packed >> 4) : (packed & 0x0f);
                s_pcm_buffer[count++] = sanxingdui_adpcm_decode(&decoder, code);
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
        while (request > 0 && request <= sanxingdui_tts_clip_count) {
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
    lv_obj_t *label = ui_pixel_label(button, text, &sanxingdui_zh_16, UI_INK);
    lv_obj_center(label);
}

void sanxingdui_museum_enter(bool buttons_available, bool audio_available)
{
    s_buttons_available = buttons_available;
    s_audio_available = audio_available;
    if (sanxingdui_tts_clip_count != SANXINGDUI_MUSEUM_PAGE_COUNT * 2) {
        ESP_LOGE(TAG, "TTS clip count mismatch: %u", (unsigned)sanxingdui_tts_clip_count);
        s_audio_available = false;
    }
    sanxingdui_museum_state_init(&s_state);
    s_screen = ui_pixel_screen_create("SANXINGDUI");

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

    s_title = ui_pixel_label(card, "", &sanxingdui_zh_16, UI_INK);
    lv_obj_set_pos(s_title, 101, 25);
    lv_obj_set_width(s_title, 105);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_WRAP);

    s_category = ui_pixel_label(card, "", &sanxingdui_zh_16, 0xFFFFFF);
    lv_obj_set_pos(s_category, 101, 67);
    lv_obj_set_style_bg_opa(s_category, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(s_category, 5, 0);
    lv_obj_set_style_pad_right(s_category, 5, 0);
    lv_obj_set_style_pad_top(s_category, 1, 0);
    lv_obj_set_style_pad_bottom(s_category, 1, 0);

    s_context = ui_pixel_label(card, "", &sanxingdui_zh_16, 0x563C2E);
    lv_obj_set_pos(s_context, 5, 100);
    lv_obj_set_width(s_context, 202);
    lv_label_set_long_mode(s_context, LV_LABEL_LONG_CLIP);

    lv_obj_t *body_panel = museum_block(card, 5, 122, 202, 69, 0xF7F0DF);
    lv_obj_set_style_border_width(body_panel, 2, 0);
    lv_obj_set_style_border_color(body_panel, lv_color_hex(0x76513A), 0);
    lv_obj_set_style_pad_all(body_panel, 4, 0);
    s_body = ui_pixel_label(body_panel, "", &sanxingdui_zh_16, UI_INK);
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
        &sanxingdui_zh_16, 0xFFFFFF);
    lv_obj_set_width(s_mode, 190);
    lv_obj_set_style_text_align(s_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_mode);

    refresh_page();
    refresh_battery(NULL);
    s_battery_timer = lv_timer_create(refresh_battery, 10000, NULL);
    lv_screen_load(s_screen);

    if (s_audio_available && !s_audio_task) {
        if (xTaskCreate(audio_task, "sanxingdui_tts", 4096, NULL, 4,
                        &s_audio_task) != pdPASS) {
            s_audio_available = false;
            lv_label_set_text(s_mode, "语音不可用");
        } else {
            request_current_audio();
        }
    }
}

void sanxingdui_museum_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_CLICK) return;
    if (button == BSP_BTN_UP) {
        sanxingdui_museum_state_move(&s_state, -1);
        refresh_page();
    } else if (button == BSP_BTN_DOWN) {
        sanxingdui_museum_state_move(&s_state, 1);
        refresh_page();
    } else if (button == BSP_BTN_OK) {
        sanxingdui_museum_state_toggle_detail(&s_state);
        refresh_page();
    }
    request_current_audio();
}
