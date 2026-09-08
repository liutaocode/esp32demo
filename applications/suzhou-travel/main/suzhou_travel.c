#include "suzhou_travel.h"

#include "suzhou_garden_header.h"
#include "suzhou_adpcm.h"
#include "suzhou_travel_state.h"
#include "suzhou_tts_audio.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdint.h>

LV_FONT_DECLARE(suzhou_zh_16);

#define COLOR_PAPER       0xF3E8CF
#define COLOR_PAPER_DARK  0xDDCBA7
#define COLOR_INK         0x282A25
#define COLOR_CINNABAR    0x9E3D2F
#define COLOR_CELADON     0x7B9684
#define COLOR_JADE_DARK   0x405C50
#define COLOR_GOLD        0xB98B48
#define AUDIO_REQUEST_STOP UINT32_MAX

static const char *TAG = "suzhou_ui";

typedef struct {
    const char *name;
    const char *kind;
    const char *tagline;
    const char *summary;
    const char *detail;
    uint32_t accent;
} attraction_t;

static const attraction_t ATTRACTIONS[] = {
    {
        "拙政园", "世界遗产", "水景开阔，移步换景",
        "苏州古典园林的代表。池水、曲桥、亭榭与花木相映，四季皆有不同风致。",
        "看点：远香堂、小飞虹、梧竹幽居。清晨入园，更适合慢看水面与借景。",
        0x718B72,
    },
    {
        "留园", "世界遗产", "厅堂精雅，奇石入画",
        "以建筑空间和庭院层次见长。长廊串联山水，转折之间常有意外景致。",
        "看点：冠云峰、五峰仙馆与曲廊花窗。可留心门洞如何框出一幅幅小景。",
        0x8A7255,
    },
    {
        "虎丘", "古迹名胜", "千年斜塔，吴中胜境",
        "山虽不高，却集古塔、剑池与石刻于一处，是认识苏州历史脉络的经典去处。",
        "看点：云岩寺塔、千人石、剑池。山路多石阶，建议穿轻便防滑的鞋。",
        0x697B68,
    },
    {
        "寒山寺", "古寺", "枫桥钟声，诗意江南",
        "因枫桥夜泊而广为人知。寺院、古钟与运河古桥共同构成悠远的人文意境。",
        "看点：钟楼、碑廊与邻近枫桥。适合与虎丘安排在同一天缓步游览。",
        0x9B5A43,
    },
    {
        "平江路", "历史街区", "河街相邻，小桥流水",
        "沿河老街保留水巷格局。白墙黛瓦、石桥、小店与评弹茶馆相互交织。",
        "看点：支巷、古桥与临水人家。离开主街走进小巷，更能感受日常苏州。",
        0x587C78,
    },
    {
        "山塘街", "历史街区", "七里水巷，灯影人家",
        "古街沿山塘河展开，桥、埠、民居相连。傍晚灯火映水，最有水乡气韵。",
        "看点：古石桥、河埠与临水街屋。夜景热闹，想安静拍照可选择上午。",
        0xA26949,
    },
    {
        "狮子林", "世界遗产", "假山迷宫，石峰如狮",
        "以层叠太湖石假山著称。洞壑、石峰与回廊交错，游园过程像穿行山水迷宫。",
        "看点：假山群、燕誉堂、真趣亭。岔路较多，慢走才能体会峰回路转。",
        0x77735F,
    },
    {
        "网师园", "世界遗产", "小园极致，夜色入戏",
        "面积不大却比例精巧。池水居中，建筑环绕，有限空间里营造出深远层次。",
        "看点：月到风来亭、濯缨水阁。观察窗、廊、池之间彼此借景的关系。",
        0x536E61,
    },
    {
        "沧浪亭", "世界遗产", "古园临水，清幽疏朗",
        "苏州现存历史悠久的园林之一。未入园先见水，复廊把园内山石与园外水景相连。",
        "看点：临水复廊、漏窗、翠玲珑。环境清雅，适合静看竹影与光线变化。",
        0x657D66,
    },
    {
        "金鸡湖", "城市湖景", "古今相映，湖畔新城",
        "开阔湖面与现代城市天际线相映，为古典园林之外的苏州提供另一种风景。",
        "看点：湖滨步道、日落与夜景。湖面风大，傍晚游览可多带一件外衣。",
        0x4D7180,
    },
};

_Static_assert(sizeof(ATTRACTIONS) / sizeof(ATTRACTIONS[0]) ==
                   SUZHOU_TRAVEL_ATTRACTION_COUNT,
               "attraction count mismatch");

static lv_obj_t *s_screen;
static lv_obj_t *s_number;
static lv_obj_t *s_name;
static lv_obj_t *s_kind;
static lv_obj_t *s_tagline;
static lv_obj_t *s_body_title;
static lv_obj_t *s_body;
static lv_obj_t *s_battery;
static lv_obj_t *s_battery_fill;
static lv_obj_t *s_accent_rule;
static lv_obj_t *s_audio_status;
static lv_timer_t *s_battery_timer;
static TaskHandle_t s_audio_task;
static bool s_buttons_available;
static bool s_battery_available;
static bool s_audio_available;
static int16_t s_pcm_buffer[512];
static suzhou_travel_state_t s_state;

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int width, int height,
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

static lv_obj_t *label(lv_obj_t *parent, const char *text, const lv_font_t *font,
                       uint32_t color)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    return object;
}

/* Four short bars evoke a traditional lattice window without a bitmap asset. */
static void add_lattice_corner(lv_obj_t *parent, int x, int y)
{
    block(parent, x, y, 24, 2, COLOR_GOLD);
    block(parent, x, y, 2, 24, COLOR_GOLD);
    block(parent, x + 6, y + 6, 18, 2, COLOR_GOLD);
    block(parent, x + 6, y + 6, 2, 18, COLOR_GOLD);
}

static void refresh_battery(lv_timer_t *timer)
{
    (void)timer;
    const int soc = s_battery_available ? bsp_battery_soc() : -1;
    if (soc < 0) {
        lv_label_set_text(s_battery, "--");
        lv_obj_set_style_bg_opa(s_battery_fill, LV_OPA_TRANSP, 0);
        return;
    }

    const int bounded_soc = soc > 100 ? 100 : soc;
    lv_label_set_text_fmt(s_battery, "%d%%", bounded_soc);
    lv_obj_set_width(s_battery_fill, LV_MAX(1, (31 * bounded_soc + 99) / 100));
    lv_obj_set_style_bg_color(
        s_battery_fill,
        lv_color_hex(bounded_soc <= 20 ? COLOR_CINNABAR : COLOR_CELADON), 0);
    lv_obj_set_style_bg_opa(s_battery_fill, LV_OPA_COVER, 0);
}

static void refresh(void)
{
    const attraction_t *entry = &ATTRACTIONS[s_state.index];
    lv_label_set_text_fmt(s_number, "%02u / %02u",
                          (unsigned)(s_state.index + 1),
                          SUZHOU_TRAVEL_ATTRACTION_COUNT);
    lv_label_set_text(s_name, entry->name);
    lv_label_set_text(s_kind, entry->kind);
    lv_label_set_text(s_tagline, entry->tagline);
    lv_label_set_text(s_body_title, s_state.detail_visible ? "游览看点" : "景点简介");
    lv_label_set_text(s_body,
                      s_state.detail_visible ? entry->detail : entry->summary);
    lv_obj_set_style_bg_color(s_accent_rule, lv_color_hex(entry->accent), 0);
}

static void set_audio_status(const char *text)
{
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    if (s_audio_status) {
        lv_label_set_text(s_audio_status, text);
    }
    bsp_lvgl_unlock();
}

static bool play_clip(size_t clip_index, uint32_t *next_request)
{
    *next_request = 0;
    if (clip_index >= suzhou_tts_clip_count) {
        ESP_LOGE(TAG, "missing TTS clip %u", (unsigned)clip_index);
        set_audio_status("语音不可用");
        return true;
    }

    const suzhou_tts_clip_t *clip = &suzhou_tts_clips[clip_index];
    if (clip->sample_count == 0 ||
        clip->offset + clip->adpcm_size > suzhou_tts_audio_data_size ||
        (clip->sample_count / 2) > clip->adpcm_size) {
        ESP_LOGE(TAG, "invalid TTS clip %u", (unsigned)clip_index);
        set_audio_status("语音不可用");
        return true;
    }
    if (bsp_audio_set_format(16000, 16, 1) != ESP_OK) {
        set_audio_status("语音不可用");
        return true;
    }

    bsp_audio_set_volume(72);
    set_audio_status("正在播放介绍…");
    ESP_LOGI(TAG, "playing TTS clip %u", (unsigned)clip_index);

    suzhou_adpcm_state_t decoder;
    suzhou_adpcm_init(&decoder, clip->initial_predictor,
                      clip->initial_step_index);
    size_t sample = 0;
    while (sample < clip->sample_count) {
        if (xTaskNotifyWait(0, UINT32_MAX, next_request, 0) == pdTRUE) {
            return false;
        }

        size_t count = 0;
        while (count < 512 && sample < clip->sample_count) {
            if (sample == 0) {
                s_pcm_buffer[count++] = (int16_t)decoder.predictor;
            } else {
                const size_t nibble = sample - 1;
                const uint8_t packed =
                    suzhou_tts_audio_data[clip->offset + nibble / 2];
                const uint8_t code =
                    (nibble & 1) ? (packed >> 4) : (packed & 0x0f);
                s_pcm_buffer[count++] = suzhou_adpcm_decode(&decoder, code);
            }
            sample++;
        }
        if (bsp_audio_write(s_pcm_buffer,
                            count * sizeof(s_pcm_buffer[0])) != ESP_OK) {
            ESP_LOGE(TAG, "TTS playback failed at sample %u",
                     (unsigned)sample);
            set_audio_status("语音播放失败");
            return true;
        }
    }
    set_audio_status(s_buttons_available ? "确定键：简介 / 看点" : "按键不可用");
    return true;
}

static void audio_task(void *argument)
{
    (void)argument;
    for (;;) {
        uint32_t request = 0;
        xTaskNotifyWait(0, UINT32_MAX, &request, portMAX_DELAY);
        while (request > 0 && request <= suzhou_tts_clip_count) {
            uint32_t next_request = 0;
            play_clip(request - 1, &next_request);
            request = next_request;
        }
        if (request == AUDIO_REQUEST_STOP) {
            break;
        }
    }
    s_audio_task = NULL;
    vTaskDelete(NULL);
}

static void request_current_audio(void)
{
    if (!s_audio_task) {
        return;
    }
    const size_t clip_index =
        s_state.index * 2 + (s_state.detail_visible ? 1 : 0);
    xTaskNotify(s_audio_task, (uint32_t)clip_index + 1,
                eSetValueWithOverwrite);
}

static void create_header(void)
{
    lv_obj_t *image = lv_image_create(s_screen);
    lv_image_set_src(image, &suzhou_garden_header);
    lv_obj_set_pos(image, 0, 0);

    lv_obj_t *title_shadow = block(s_screen, 11, 10, 135, 35, COLOR_INK);
    (void)title_shadow;
    lv_obj_t *title_plate = block(s_screen, 7, 6, 135, 35, COLOR_PAPER);
    lv_obj_set_style_border_width(title_plate, 2, 0);
    lv_obj_set_style_border_color(title_plate, lv_color_hex(COLOR_INK), 0);
    block(title_plate, 5, 5, 4, 25, COLOR_CINNABAR);
    lv_obj_t *title = label(title_plate, "姑苏十景", &suzhou_zh_16, COLOR_INK);
    lv_obj_set_pos(title, 18, 7);
    lv_obj_set_style_text_letter_space(title, 2, 0);

    lv_obj_t *battery_panel = block(s_screen, 181, 8, 48, 24, COLOR_INK);
    lv_obj_set_style_border_width(battery_panel, 2, 0);
    lv_obj_set_style_border_color(battery_panel, lv_color_hex(COLOR_PAPER), 0);
    block(s_screen, 229, 15, 4, 10, COLOR_INK);
    s_battery_fill = block(battery_panel, 4, 4, 31, 12, COLOR_CELADON);
    s_battery = label(battery_panel, "--", &lv_font_montserrat_14, 0xFFFFFF);
    lv_obj_set_pos(s_battery, 0, 2);
    lv_obj_set_width(s_battery, 44);
    lv_label_set_long_mode(s_battery, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_CENTER, 0);
}

static void create_content_card(void)
{
    block(s_screen, 11, 101, 222, 179, COLOR_INK);
    lv_obj_t *card = block(s_screen, 7, 97, 222, 179, COLOR_PAPER);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_INK), 0);
    add_lattice_corner(card, 5, 5);

    s_number = label(card, "", &lv_font_montserrat_14, COLOR_CINNABAR);
    lv_obj_set_pos(s_number, 142, 7);

    s_name = label(card, "", &suzhou_zh_16, COLOR_INK);
    lv_obj_set_pos(s_name, 17, 30);
    lv_obj_set_style_text_letter_space(s_name, 2, 0);

    s_kind = label(card, "", &suzhou_zh_16, 0xFFFFFF);
    lv_obj_set_pos(s_kind, 121, 28);
    lv_obj_set_style_bg_color(s_kind, lv_color_hex(COLOR_JADE_DARK), 0);
    lv_obj_set_style_bg_opa(s_kind, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(s_kind, 5, 0);
    lv_obj_set_style_pad_right(s_kind, 5, 0);
    lv_obj_set_style_pad_top(s_kind, 2, 0);
    lv_obj_set_style_pad_bottom(s_kind, 2, 0);

    s_tagline = label(card, "", &suzhou_zh_16, COLOR_JADE_DARK);
    lv_obj_set_pos(s_tagline, 17, 57);
    lv_obj_set_width(s_tagline, 190);

    s_accent_rule = block(card, 17, 79, 188, 3, COLOR_CELADON);

    s_body_title = label(card, "", &suzhou_zh_16, COLOR_CINNABAR);
    lv_obj_set_pos(s_body_title, 17, 87);

    s_body = label(card, "", &suzhou_zh_16, COLOR_INK);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_body, 188);
    lv_obj_set_pos(s_body, 17, 108);
    lv_obj_set_style_text_line_space(s_body, 1, 0);
}

static void create_footer(void)
{
    lv_obj_t *footer = block(s_screen, 0, 281, 240, 39, COLOR_INK);
    block(footer, 0, 0, 240, 3, COLOR_CINNABAR);

    lv_obj_t *up = label(footer, "上键  上一景", &suzhou_zh_16, COLOR_PAPER);
    lv_obj_set_pos(up, 8, 4);
    lv_obj_t *down = label(footer, "下键  下一景", &suzhou_zh_16, COLOR_PAPER);
    lv_obj_set_pos(down, 124, 4);

    const char *hint = !s_buttons_available ? "按键不可用" :
                       (!s_audio_available ? "语音不可用" :
                                             "确定键：简介 / 看点");
    s_audio_status = label(footer, hint, &suzhou_zh_16, COLOR_PAPER_DARK);
    lv_obj_set_width(s_audio_status, 220);
    lv_obj_set_style_text_align(s_audio_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_audio_status, 10, 21);
}

void suzhou_travel_enter(bool buttons_available, bool battery_available,
                         bool audio_available)
{
    s_buttons_available = buttons_available;
    s_battery_available = battery_available;
    s_audio_available = audio_available;
    suzhou_travel_state_init(&s_state);

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(COLOR_PAPER_DARK), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    create_header();
    create_content_card();
    create_footer();
    refresh();
    refresh_battery(NULL);
    s_battery_timer = lv_timer_create(refresh_battery, 10000, NULL);
    lv_screen_load(s_screen);

    if (s_audio_available && !s_audio_task) {
        if (xTaskCreate(audio_task, "suzhou_tts", 4096, NULL, 4,
                        &s_audio_task) != pdPASS) {
            s_audio_available = false;
            lv_label_set_text(s_audio_status, "语音不可用");
        } else {
            request_current_audio();
        }
    }
}

void suzhou_travel_exit(void)
{
    if (s_battery_timer) {
        lv_timer_delete(s_battery_timer);
        s_battery_timer = NULL;
    }
    s_audio_status = NULL;
    if (s_audio_task) {
        xTaskNotify(s_audio_task, AUDIO_REQUEST_STOP, eSetValueWithOverwrite);
    }
    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
    }
    s_number = NULL;
    s_name = NULL;
    s_kind = NULL;
    s_tagline = NULL;
    s_body_title = NULL;
    s_body = NULL;
    s_battery = NULL;
    s_battery_fill = NULL;
    s_accent_rule = NULL;
}

void suzhou_travel_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event != BSP_BTN_CLICK || !s_screen) {
        return;
    }

    if (button == BSP_BTN_UP) {
        suzhou_travel_state_move(&s_state, -1);
    } else if (button == BSP_BTN_DOWN) {
        suzhou_travel_state_move(&s_state, 1);
    } else if (button == BSP_BTN_OK) {
        suzhou_travel_state_toggle_detail(&s_state);
    } else {
        return;
    }
    refresh();
    request_current_audio();
}
