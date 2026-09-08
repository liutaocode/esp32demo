#include "minecraft_guide.h"
#include "minecraft_guide_state.h"
#include "minecraft_adpcm.h"
#include "minecraft_guide_audio.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_display.h"
#include "ui_pixel.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdint.h>

LV_FONT_DECLARE(minecraft_zh_16);

static const char *TAG = "minecraft_guide";

typedef struct {
    const char *name;
    const char *kind;
    const char *place;
    const char *description;
    uint32_t colors[4];
    uint8_t pixels[64];
} guide_entry_t;

/* 8 x 8 original block-art silhouettes. Zero is transparent. */
static const guide_entry_t ENTRIES[] = {
    {
        "苦力怕", "敌对生物", "主世界", "会悄悄靠近并爆炸。听到嘶声要立即拉开距离。",
        { 0, 0x56B947, 0x2F7D32, 0x15251A },
        { 0,1,1,1,1,1,1,0, 0,1,2,1,1,2,1,0, 0,1,3,3,3,3,1,0, 0,1,3,1,1,3,1,0,
          0,1,1,3,3,1,1,0, 0,0,1,3,3,1,0,0, 0,0,1,1,1,1,0,0, 0,1,1,0,0,1,1,0 }
    },
    {
        "末影人", "中立生物", "主世界、末地", "不要直视它。击败后可能掉落末影珍珠。",
        { 0, 0x1A171C, 0x7B35A8, 0xD13CFF },
        { 0,0,1,1,1,1,0,0, 0,1,1,1,1,1,1,0, 0,1,3,1,1,3,1,0, 0,1,1,1,1,1,1,0,
          0,0,0,1,1,0,0,0, 0,0,2,1,1,2,0,0, 0,0,1,0,0,1,0,0, 0,1,1,0,0,1,1,0 }
    },
    {
        "美西螈", "友好生物", "繁茂洞穴", "生活在水中，会帮助玩家攻击水生敌对生物。",
        { 0, 0xF59BC2, 0xD85A9D, 0x5C293F },
        { 2,0,0,0,0,0,0,2, 2,2,1,1,1,1,2,2, 0,1,1,1,1,1,1,0, 0,1,3,1,1,3,1,0,
          0,1,1,3,3,1,1,0, 2,2,1,1,1,1,2,2, 2,0,1,0,0,1,0,2, 0,0,1,0,0,1,0,0 }
    },
    {
        "蜜蜂", "中立生物", "花田、蜂巢", "会为作物授粉。攻击蜂群会让它们一起反击。",
        { 0, 0xF7CA3E, 0x4C3827, 0xDFF7FF },
        { 0,0,3,0,0,3,0,0, 0,3,3,0,0,3,3,0, 0,2,2,2,2,2,2,0, 1,1,1,1,1,1,1,1,
          2,2,2,2,2,2,2,2, 1,1,2,2,2,2,1,1, 0,1,1,1,1,1,1,0, 0,0,2,0,0,2,0,0 }
    },
    {
        "狼", "中立生物", "森林、针叶林", "用骨头驯服后，会跟随并帮助玩家战斗。",
        { 0, 0xBFC4C7, 0x6D7478, 0x2B3033 },
        { 2,2,0,0,0,0,2,2, 2,1,1,1,1,1,1,2, 0,1,1,1,1,1,1,0, 0,1,3,1,1,3,1,0,
          0,1,1,3,3,1,1,0, 0,0,1,1,1,1,0,0, 0,0,2,1,1,2,0,0, 0,2,2,0,0,2,2,0 }
    },
    {
        "钻石矿石", "稀有矿物", "深层地下", "用铁镐或更好的镐开采，是高级装备材料。",
        { 0, 0x787E83, 0x54E8E3, 0xC7FFFF },
        { 1,1,1,1,1,1,1,1, 1,2,2,1,1,3,2,1, 1,2,3,2,1,2,1,1, 1,1,2,1,2,3,2,1,
          1,2,1,1,3,2,1,1, 1,3,2,1,2,1,2,1, 1,2,1,2,3,2,3,1, 1,1,1,1,1,1,1,1 }
    },
    {
        "工作台", "工具方块", "任意地点", "把合成区域扩展为三乘三，是生存建造核心。",
        { 0, 0xB87437, 0x6B3D20, 0xE0A45C },
        { 2,2,2,2,2,2,2,2, 2,3,3,1,1,3,3,2, 2,3,1,1,1,1,3,2, 2,1,1,3,3,1,1,2,
          2,1,1,3,3,1,1,2, 2,3,1,1,1,1,3,2, 2,3,3,1,1,3,3,2, 2,2,2,2,2,2,2,2 }
    },
    {
        "下界传送门", "传送建筑", "黑曜石框架", "点燃黑曜石门框前往下界，建议准备防火装备。",
        { 0, 0x281835, 0x7836A8, 0xC563F0 },
        { 1,1,1,1,1,1,1,1, 1,2,3,2,2,3,2,1, 1,3,2,3,3,2,3,1, 1,2,3,2,2,3,2,1,
          1,3,2,3,3,2,3,1, 1,2,3,2,2,3,2,1, 1,3,2,3,3,2,3,1, 1,1,1,1,1,1,1,1 }
    },
    {
        "附魔台", "功能方块", "基地", "消耗经验和青金石强化装备，书架提高等级。",
        { 0, 0x8F202B, 0x2D252B, 0xF4E9C7 },
        { 0,0,3,3,3,3,0,0, 0,3,3,3,3,3,3,0, 0,0,2,3,3,2,0,0, 0,0,0,2,2,0,0,0,
          0,1,1,1,1,1,1,0, 1,1,2,2,2,2,1,1, 1,2,2,2,2,2,2,1, 2,2,2,2,2,2,2,2 }
    },
    {
        "末影龙", "首领生物", "末地", "先摧毁末地水晶，再寻找攻击最终首领的机会。",
        { 0, 0x17131B, 0x573060, 0xD147E8 },
        { 1,0,0,1,1,0,0,1, 0,1,0,1,1,0,1,0, 0,0,1,1,1,1,0,0, 2,2,1,3,3,1,2,2,
          0,0,1,1,1,1,0,0, 0,1,1,0,0,1,1,0, 1,1,0,0,0,0,1,1, 1,0,0,0,0,0,0,1 }
    },
    {
        "僵尸", "敌对生物", "夜晚、洞穴", "会追赶玩家，阳光下会燃烧；戴着头盔时可以在白天活动。",
        { 0, 0x5B8C3A, 0x315A9B, 0x202A28 },
        { 0,1,1,1,1,1,1,0, 0,1,3,1,1,3,1,0, 0,1,1,3,3,1,1,0, 0,0,0,1,1,0,0,0,
          0,2,2,2,2,2,2,0, 0,2,2,2,2,2,2,0, 0,0,2,2,2,2,0,0, 0,2,2,0,0,2,2,0 }
    },
    {
        "骷髅", "敌对生物", "夜晚、洞穴", "擅长远程射箭。用盾牌挡住箭矢，再靠近攻击会更安全。",
        { 0, 0xE5E1D2, 0x9B978B, 0x242424 },
        { 0,1,1,1,1,1,1,0, 0,1,3,1,1,3,1,0, 0,1,1,3,3,1,1,0, 0,0,1,1,1,1,0,0,
          0,0,0,1,1,0,0,0, 0,2,2,1,1,2,2,0, 0,0,0,1,1,0,0,0, 0,0,1,0,0,1,0,0 }
    },
    {
        "蜘蛛", "中立生物", "地表、洞穴", "白天通常保持中立，黑暗中会主动攻击，还能快速攀爬墙壁。",
        { 0, 0x241C27, 0x56305E, 0xD13B3B },
        { 0,0,1,1,1,1,0,0, 1,0,1,1,1,1,0,1, 0,1,1,3,3,1,1,0, 1,1,1,1,1,1,1,1,
          0,1,2,1,1,2,1,0, 1,0,1,0,0,1,0,1, 1,0,1,0,0,1,0,1, 1,0,0,0,0,0,0,1 }
    },
    {
        "史莱姆", "敌对生物", "沼泽、地下", "会跳跃撞击玩家。大型史莱姆被击败后会分裂成更小的个体。",
        { 0, 0x6BCB48, 0x3B8F36, 0x17381C },
        { 1,1,1,1,1,1,1,1, 1,2,2,2,2,2,2,1, 1,2,3,2,2,3,2,1, 1,2,2,2,2,2,2,1,
          1,2,2,3,3,2,2,1, 1,2,2,2,2,2,2,1, 1,2,2,2,2,2,2,1, 1,1,1,1,1,1,1,1 }
    },
    {
        "村民", "友好生物", "村庄", "可以用绿宝石交易。保护村民并升级职业，能获得稳定物资。",
        { 0, 0xA97850, 0x6B422F, 0xE0B08B },
        { 0,2,2,2,2,2,2,0, 0,2,1,1,1,1,2,0, 0,1,1,3,3,1,1,0, 0,1,1,1,1,1,1,0,
          0,0,1,3,3,1,0,0, 0,0,2,2,2,2,0,0, 0,2,2,2,2,2,2,0, 0,0,2,0,0,2,0,0 }
    },
    {
        "铁傀儡", "友好生物", "村庄", "会守护村民并重击敌人。主动伤害村民可能引来它的反击。",
        { 0, 0xD8D1BD, 0x8E9A82, 0xA65D46 },
        { 0,0,1,1,1,1,0,0, 0,1,1,3,3,1,1,0, 0,1,1,1,1,1,1,0, 2,2,1,1,1,1,2,2,
          0,0,1,1,1,1,0,0, 0,0,1,2,2,1,0,0, 0,0,1,0,0,1,0,0, 0,1,1,0,0,1,1,0 }
    },
    {
        "猪灵", "中立生物", "下界", "看到没穿金装备的玩家会攻击。手持金锭可与它交换物品。",
        { 0, 0xD79A82, 0x7E503E, 0xF1D36A },
        { 2,2,0,0,0,0,2,2, 2,1,1,1,1,1,1,2, 0,1,1,3,3,1,1,0, 0,1,2,1,1,2,1,0,
          0,0,1,1,1,1,0,0, 0,3,3,2,2,3,3,0, 0,0,2,2,2,2,0,0, 0,2,2,0,0,2,2,0 }
    },
    {
        "监守者", "强敌生物", "深暗之域", "依靠震动和气味追踪目标。尽量潜行绕开，不要正面对抗。",
        { 0, 0x173A42, 0x1E6970, 0x50D8C8 },
        { 3,0,0,1,1,0,0,3, 3,3,0,1,1,0,3,3, 0,3,1,1,1,1,3,0, 0,1,1,2,2,1,1,0,
          0,1,2,1,1,2,1,0, 0,0,1,1,1,1,0,0, 0,1,1,0,0,1,1,0, 1,1,0,0,0,0,1,1 }
    },
    {
        "凋灵", "首领生物", "玩家召唤", "会飞行并造成凋零效果。准备牛奶、护甲和远程武器再挑战。",
        { 0, 0x222225, 0x55555B, 0xA9A9AD },
        { 1,1,1,0,0,1,1,1, 1,3,1,1,1,1,3,1, 1,1,1,1,1,1,1,1, 0,0,0,1,1,0,0,0,
          0,0,0,2,2,0,0,0, 0,0,1,1,1,1,0,0, 0,1,1,0,0,1,1,0, 1,1,0,0,0,0,1,1 }
    },
    {
        "鞘翅", "稀有装备", "末地船", "在末地船中找到。跳跃时展开滑翔，配合烟花火箭可以远行。",
        { 0, 0x6D5A88, 0xB69BC8, 0x352D48 },
        { 1,1,0,0,0,0,1,1, 1,2,1,0,0,1,2,1, 0,1,2,1,1,2,1,0, 0,1,2,3,3,2,1,0,
          0,0,1,3,3,1,0,0, 0,0,1,0,0,1,0,0, 0,1,1,0,0,1,1,0, 1,1,0,0,0,0,1,1 }
    },
};

_Static_assert(sizeof(ENTRIES) / sizeof(ENTRIES[0]) == MINECRAFT_GUIDE_ITEM_COUNT,
               "guide entry count mismatch");

static lv_obj_t *s_screen;
static lv_obj_t *s_sprite;
static lv_obj_t *s_number;
static lv_obj_t *s_name;
static lv_obj_t *s_kind;
static lv_obj_t *s_place;
static lv_obj_t *s_description;
static lv_obj_t *s_audio_status;
static lv_obj_t *s_battery;
static lv_obj_t *s_battery_fill;
static lv_timer_t *s_battery_timer;
static TaskHandle_t s_audio_task;
static bool s_audio_available;
static bool s_buttons_available;
static minecraft_guide_state_t s_state;
static int16_t s_pcm_buffer[512];
static uint16_t s_sprite_buffer[64 * 64];

static lv_obj_t *guide_block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, w, h);
    lv_obj_set_style_radius(object, 0, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    return object;
}

static void fill_sprite_rect(int x, int y, int width, int height, uint32_t color)
{
    uint16_t pixel = lv_color_to_u16(lv_color_hex(color));
    for (int row = y; row < y + height; row++) {
        for (int column = x; column < x + width; column++) {
            s_sprite_buffer[row * 64 + column] = pixel;
        }
    }
}

static void draw_sprite(const guide_entry_t *entry)
{
    fill_sprite_rect(0, 0, 64, 64, 0xCDEBFF);
    fill_sprite_rect(0, 49, 64, 15, 0x76502D);
    fill_sprite_rect(0, 49, 64, 4, 0x69A72C);
    for (int row = 0; row < 8; row++) {
        for (int column = 0; column < 8;) {
            uint8_t color = entry->pixels[row * 8 + column];
            int run = 1;
            while (column + run < 8 &&
                   entry->pixels[row * 8 + column + run] == color) {
                run++;
            }
            if (color != 0) {
                fill_sprite_rect(4 + column * 7, 4 + row * 7,
                                 7 * run, 7, entry->colors[color]);
            }
            column += run;
        }
    }
    lv_obj_invalidate(s_sprite);
}

static void refresh(void)
{
    const guide_entry_t *entry = &ENTRIES[s_state.index];
    lv_label_set_text_fmt(s_number, "%02u / %02u", (unsigned)(s_state.index + 1),
                          MINECRAFT_GUIDE_ITEM_COUNT);
    lv_label_set_text(s_name, entry->name);
    lv_label_set_text(s_kind, entry->kind);
    lv_label_set_text_fmt(s_place, "出没：%s", entry->place);
    lv_label_set_text(s_description, entry->description);
    draw_sprite(entry);
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
        lv_color_hex(soc <= 20 ? 0xD84935 : (soc <= 45 ? 0xE6B83F : 0x69B83A)), 0);
    lv_obj_set_style_bg_opa(s_battery_fill, LV_OPA_COVER, 0);
}

static void create_nav_button(int x, const char *text, uint32_t color)
{
    guide_block(s_screen, x + 3, 273, 68, 26, UI_INK);
    lv_obj_t *button = guide_block(s_screen, x, 270, 68, 26, color);
    lv_obj_set_style_border_width(button, 2, 0);
    lv_obj_set_style_border_color(button, lv_color_hex(UI_INK), 0);
    guide_block(button, 2, 2, 62, 3, 0xE7E7E7);
    lv_obj_t *label = ui_pixel_label(button, text, &minecraft_zh_16, UI_INK);
    lv_obj_center(label);
}

static void set_audio_status(const char *text)
{
    if (!bsp_lvgl_lock(500)) return;
    if (s_audio_status) lv_label_set_text(s_audio_status, text);
    bsp_lvgl_unlock();
}

static bool play_entry_audio(size_t entry_index, uint32_t *next_request)
{
    *next_request = 0;
    if (entry_index >= minecraft_guide_audio_clip_count) {
        ESP_LOGE(TAG, "missing TTS clip for entry %u", (unsigned)entry_index);
        set_audio_status("语音不可用");
        return true;
    }
    const minecraft_guide_audio_clip_t *clip = &minecraft_guide_audio_clips[entry_index];
    if (clip->sample_count == 0 || clip->offset + clip->adpcm_size > minecraft_guide_audio_data_size ||
        (clip->sample_count / 2) > clip->adpcm_size) {
        ESP_LOGE(TAG, "invalid TTS clip for entry %u", (unsigned)entry_index);
        set_audio_status("语音不可用");
        return true;
    }

    if (bsp_audio_set_format(16000, 16, 1) != ESP_OK) {
        set_audio_status("语音不可用");
        return true;
    }
    bsp_audio_set_volume(75);
    set_audio_status("正在播放介绍…");
    ESP_LOGI(TAG, "playing entry audio %u: %s", (unsigned)entry_index, ENTRIES[entry_index].name);

    minecraft_adpcm_state_t decoder;
    minecraft_adpcm_init(&decoder, clip->initial_predictor, clip->initial_step_index);
    size_t sample = 0;
    while (sample < clip->sample_count) {
        if (xTaskNotifyWait(0, UINT32_MAX, next_request, 0) == pdTRUE) return false;

        size_t count = 0;
        while (count < 512 && sample < clip->sample_count) {
            if (sample == 0) {
                s_pcm_buffer[count++] = (int16_t)decoder.predictor;
            } else {
                size_t nibble = sample - 1;
                uint8_t packed = minecraft_guide_audio_data[clip->offset + nibble / 2];
                uint8_t code = (nibble & 1) ? (packed >> 4) : (packed & 0x0f);
                s_pcm_buffer[count++] = minecraft_adpcm_decode(&decoder, code);
            }
            sample++;
        }
        if (bsp_audio_write(s_pcm_buffer, count * sizeof(s_pcm_buffer[0])) != ESP_OK) {
            ESP_LOGE(TAG, "TTS playback failed at sample %u", (unsigned)sample);
            set_audio_status("语音播放失败");
            return true;
        }
    }
    set_audio_status(s_buttons_available ? "确定：重播介绍" : "按键不可用");
    return true;
}

static void audio_task(void *argument)
{
    (void)argument;
    for (;;) {
        uint32_t request = 0;
        xTaskNotifyWait(0, UINT32_MAX, &request, portMAX_DELAY);
        while (request > 0 && request <= minecraft_guide_audio_clip_count) {
            uint32_t next_request = 0;
            play_entry_audio(request - 1, &next_request);
            request = next_request;
        }
    }
}

static void request_current_audio(void)
{
    if (s_audio_task) {
        xTaskNotify(s_audio_task, (uint32_t)s_state.index + 1, eSetValueWithOverwrite);
    }
}

void minecraft_guide_enter(bool audio_available, bool buttons_available)
{
    s_audio_available = audio_available;
    s_buttons_available = buttons_available;
    minecraft_guide_state_init(&s_state);
    s_screen = ui_pixel_screen_create("MINECRAFT");

    guide_block(s_screen, 181, 12, 48, 24, UI_INK);
    lv_obj_t *battery_panel = guide_block(s_screen, 178, 9, 50, 24, 0x454545);
    lv_obj_set_style_border_width(battery_panel, 2, 0);
    lv_obj_set_style_border_color(battery_panel, lv_color_hex(UI_INK), 0);
    guide_block(s_screen, 228, 16, 4, 10, UI_INK);
    s_battery_fill = guide_block(battery_panel, 4, 4, 36, 12, 0x69B83A);
    s_battery = lv_label_create(battery_panel);
    lv_obj_set_style_text_font(s_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_battery, lv_color_white(), 0);
    lv_obj_set_pos(s_battery, 0, 2);
    lv_obj_set_width(s_battery, 46);
    lv_label_set_long_mode(s_battery, LV_LABEL_LONG_CLIP);
    lv_obj_set_style_text_align(s_battery, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *card = ui_pixel_panel_create(s_screen, 8, 49, 224, 213, 0xC6C6C6);

    lv_obj_t *sprite_frame = guide_block(card, 5, 8, 72, 72, 0xCDEBFF);
    lv_obj_set_style_border_width(sprite_frame, 3, 0);
    lv_obj_set_style_border_color(sprite_frame, lv_color_hex(UI_INK), 0);
    s_sprite = lv_canvas_create(sprite_frame);
    lv_canvas_set_buffer(s_sprite, s_sprite_buffer, 64, 64, LV_COLOR_FORMAT_RGB565);
    lv_obj_set_pos(s_sprite, 4, 4);

    s_number = ui_pixel_label(card, "", &lv_font_montserrat_14, UI_SKY_DARK);
    lv_obj_set_pos(s_number, 88, 4);

    s_name = ui_pixel_label(card, "", &minecraft_zh_16, UI_INK);
    lv_obj_set_pos(s_name, 88, 27);

    s_kind = ui_pixel_label(card, "", &minecraft_zh_16, 0xFFFFFF);
    lv_obj_set_pos(s_kind, 88, 51);
    lv_obj_set_style_bg_color(s_kind, lv_color_hex(UI_SKY_DARK), 0);
    lv_obj_set_style_bg_opa(s_kind, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_left(s_kind, 5, 0);
    lv_obj_set_style_pad_right(s_kind, 5, 0);
    lv_obj_set_style_pad_top(s_kind, 2, 0);
    lv_obj_set_style_pad_bottom(s_kind, 2, 0);

    s_place = ui_pixel_label(card, "", &minecraft_zh_16, UI_INK);
    lv_obj_set_pos(s_place, 5, 86);
    lv_obj_set_width(s_place, 202);

    lv_obj_t *description_panel = guide_block(card, 5, 106, 202, 78, 0xE8E3D4);
    lv_obj_set_style_border_width(description_panel, 3, 0);
    lv_obj_set_style_border_color(description_panel, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_pad_all(description_panel, 3, 0);
    s_description = ui_pixel_label(description_panel, "", &minecraft_zh_16, UI_INK);
    lv_label_set_long_mode(s_description, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_description, 188);
    lv_obj_set_style_text_line_space(s_description, -2, 0);
    lv_obj_set_pos(s_description, 0, -1);

    create_nav_button(8, "上一个", 0x9B9B9B);
    create_nav_button(86, "确定", 0xB98552);
    create_nav_button(164, "下一个", 0x9B9B9B);

    lv_obj_t *status_panel = guide_block(s_screen, 20, 300, 200, 20, 0x403225);
    lv_obj_set_style_border_width(status_panel, 1, 0);
    lv_obj_set_style_border_color(status_panel, lv_color_hex(UI_INK), 0);
    s_audio_status = ui_pixel_label(status_panel,
        audio_available ? "切换后自动介绍" : "语音不可用",
        &minecraft_zh_16, 0xFFFFFF);
    lv_obj_set_width(s_audio_status, 190);
    lv_obj_set_style_text_align(s_audio_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_audio_status);
    if (!buttons_available) lv_label_set_text(s_audio_status, "按键不可用");

    refresh();
    refresh_battery(NULL);
    s_battery_timer = lv_timer_create(refresh_battery, 10000, NULL);
    lv_screen_load(s_screen);

    if (audio_available && !s_audio_task) {
        if (xTaskCreate(audio_task, "guide_tts", 6144, NULL, 4,
                        &s_audio_task) != pdPASS) {
            s_audio_available = false;
            lv_label_set_text(s_audio_status, "语音不可用");
        }
    }
}

void minecraft_guide_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_UP) {
        minecraft_guide_state_move(&s_state, -1);
        refresh();
        request_current_audio();
    } else if (btn == BSP_BTN_DOWN) {
        minecraft_guide_state_move(&s_state, 1);
        refresh();
        request_current_audio();
    } else if (btn == BSP_BTN_OK && s_audio_available) {
        request_current_audio();
    }
}
