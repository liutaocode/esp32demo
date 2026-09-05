#if defined(FAP_APP_MINECRAFT_GUIDE)
// main/main.c —— FoloToy AI Passport Minecraft 图鉴应用入口。
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "minecraft_guide.h"
#include "fap_screenshot.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "main";
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (!bsp_lvgl_lock(500)) return;
    minecraft_guide_key(btn, ev);
    bsp_lvgl_unlock();
}

void app_main(void) {
    ESP_LOGI(TAG, "FoloToy Minecraft guide starting");
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "wake-up cause: %d", wakeup);
    }

    bsp_i2c_init();
    bsp_i2c_scan();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed; check SPI wiring "
                      "(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    bool buttons_ok = (bsp_button_init(on_key, NULL) == ESP_OK);
    bool audio_ok = (bsp_audio_init() == ESP_OK);
    bool battery_ok = (bsp_battery_init() == ESP_OK);

    if (bsp_lvgl_lock(1000)) {
        minecraft_guide_enter(audio_ok, buttons_ok);
        bsp_lvgl_unlock();
    }

    fap_screenshot_start();

    ESP_LOGI(TAG, "ready: buttons=%d audio=%d battery=%d",
             buttons_ok, audio_ok, battery_ok);
}

#else
// main/main.c —— FoloToy AI Passport 可选应用入口。
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#if defined(FAP_APP_RICOCHET_RUSH)
#include "apps/ricochet_rush/ricochet_rush.h"
#elif defined(FAP_APP_FRUIT_MERGE)
#include "apps/fruit_merge/fruit_merge.h"
#elif defined(FAP_APP_FOCUS_POST)
#include "apps/focus_post/focus_post.h"
#elif defined(FAP_APP_DOWN_100)
#include "apps/down_100/down_100.h"
#elif defined(FAP_APP_MATH_RAIL)
#include "apps/math_rail/math_rail.h"
#elif defined(FAP_APP_MATH_TRAIN)
#include "apps/math_train/math_train.h"
#elif defined(FAP_APP_POCKET_POND)
#include "apps/pocket_pond/pocket_pond.h"
#elif defined(FAP_APP_BALLOON_RUSH)
#include "apps/balloon_rush/balloon_rush.h"
#elif defined(FAP_APP_PVZ_ALMANAC)
#include "apps/pvz_almanac/pvz_almanac.h"
#include "bsp_audio.h"
#elif defined(FAP_APP_WORD_SPRITE)
#include "apps/word_sprite/word_sprite.h"
#include "apps/word_sprite/word_sprite_runtime.h"
#include "bsp_audio.h"
#elif defined(FAP_APP_SOCIAL_BATTERY)
#include "apps/social_battery/social_battery.h"
#elif defined(FAP_APP_CLOUD_HOP)
#include "apps/cloud_hop/cloud_hop.h"
#elif defined(FAP_APP_PERFECT_SLICE)
#include "apps/perfect_slice/perfect_slice.h"
#elif defined(FAP_APP_NEEDLE_RUSH)
#include "apps/needle_rush/needle_rush.h"
#elif defined(FAP_APP_STACK_RUSH)
#include "apps/stack_rush/stack_rush.h"
#elif defined(FAP_APP_IDIOM_PET)
#include "apps/idiom_pet/idiom_pet.h"
#include "apps/idiom_pet/idiom_pet_voice.h"
#include "bsp_audio.h"
#elif defined(FAP_APP_LAOLUO_QUOTES)
#include "apps/laoluo_quotes/laoluo_quotes.h"
#include "bsp_audio.h"
#elif defined(FAP_APP_MEMORY_GARDEN)
#include "apps/memory_garden/memory_garden.h"
#elif defined(FAP_APP_TOMATO_BLOOM)
#include "apps/tomato_bloom/tomato_bloom.h"
#else
#include "apps/vibe_check/vibe_check.h"
#include "bsp_audio.h"
#endif
#include "fap_screenshot.h"
#include "esp_log.h"
#include "esp_sleep.h"

static const char *TAG = "main";
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
#if defined(FAP_APP_RICOCHET_RUSH)
    ricochet_rush_key(btn, ev);
#elif defined(FAP_APP_FRUIT_MERGE)
    fruit_merge_key(btn, ev);
#elif defined(FAP_APP_FOCUS_POST)
    focus_post_key(btn, ev);
#elif defined(FAP_APP_DOWN_100)
    down_100_key(btn, ev);
#elif defined(FAP_APP_MATH_RAIL)
    math_rail_key(btn, ev);
#elif defined(FAP_APP_MATH_TRAIN)
    math_train_key(btn, ev);
#elif defined(FAP_APP_POCKET_POND)
    pocket_pond_key(btn, ev);
#elif defined(FAP_APP_BALLOON_RUSH)
    balloon_rush_key(btn, ev);
#elif defined(FAP_APP_PVZ_ALMANAC)
    pvz_almanac_key(btn, ev);
#elif defined(FAP_APP_WORD_SPRITE)
    word_sprite_key(btn, ev);
#elif defined(FAP_APP_SOCIAL_BATTERY)
    social_battery_key(btn, ev);
#elif defined(FAP_APP_CLOUD_HOP)
    cloud_hop_key(btn, ev);
#elif defined(FAP_APP_PERFECT_SLICE)
    perfect_slice_key(btn, ev);
#elif defined(FAP_APP_NEEDLE_RUSH)
    needle_rush_key(btn, ev);
#elif defined(FAP_APP_STACK_RUSH)
    stack_rush_key(btn, ev);
#elif defined(FAP_APP_IDIOM_PET)
    idiom_pet_key(btn, ev);
#else
    if (!bsp_lvgl_lock(500)) return;
#if defined(FAP_APP_LAOLUO_QUOTES)
    laoluo_quotes_key(btn, ev);
#elif defined(FAP_APP_MEMORY_GARDEN)
    memory_garden_key(btn, ev);
#elif defined(FAP_APP_TOMATO_BLOOM)
    tomato_bloom_key(btn, ev);
#else
    vibe_check_key(btn, ev);
#endif
    bsp_lvgl_unlock();
#endif
}

void app_main(void) {
#if defined(FAP_APP_RICOCHET_RUSH)
    ESP_LOGI(TAG, "FoloToy Ricochet Rush starting");
#elif defined(FAP_APP_FRUIT_MERGE)
    ESP_LOGI(TAG, "FoloToy Fruit Merge starting");
#elif defined(FAP_APP_FOCUS_POST)
    ESP_LOGI(TAG, "FoloToy Focus Post starting");
#elif defined(FAP_APP_DOWN_100)
    ESP_LOGI(TAG, "FoloToy Down 100 starting");
#elif defined(FAP_APP_MATH_RAIL)
    ESP_LOGI(TAG, "FoloToy Math Rail starting");
#elif defined(FAP_APP_MATH_TRAIN)
    ESP_LOGI(TAG, "FoloToy Math Train starting");
#elif defined(FAP_APP_POCKET_POND)
    ESP_LOGI(TAG, "FoloToy Pocket Pond starting");
#elif defined(FAP_APP_BALLOON_RUSH)
    ESP_LOGI(TAG, "FoloToy Balloon Rush starting");
#elif defined(FAP_APP_PVZ_ALMANAC)
    ESP_LOGI(TAG, "FoloToy Grassland Lab starting");
#elif defined(FAP_APP_WORD_SPRITE)
    ESP_LOGI(TAG, "FoloToy Word Sprite starting");
#elif defined(FAP_APP_SOCIAL_BATTERY)
    ESP_LOGI(TAG, "FoloToy Social Battery starting");
#elif defined(FAP_APP_CLOUD_HOP)
    ESP_LOGI(TAG, "FoloToy Cloud Hop starting");
#elif defined(FAP_APP_PERFECT_SLICE)
    ESP_LOGI(TAG, "FoloToy Perfect Slice starting");
#elif defined(FAP_APP_NEEDLE_RUSH)
    ESP_LOGI(TAG, "FoloToy Needle Rush starting");
#elif defined(FAP_APP_STACK_RUSH)
    ESP_LOGI(TAG, "FoloToy Stack Rush starting");
#elif defined(FAP_APP_IDIOM_PET)
    ESP_LOGI(TAG, "FoloToy Idiom Pet starting");
#elif defined(FAP_APP_LAOLUO_QUOTES)
    ESP_LOGI(TAG, "FoloToy Lao Luo Quotes starting");
#elif defined(FAP_APP_MEMORY_GARDEN)
    ESP_LOGI(TAG, "FoloToy Daily Memory starting");
#elif defined(FAP_APP_TOMATO_BLOOM)
    ESP_LOGI(TAG, "FoloToy Tomato Bloom starting");
#else
    ESP_LOGI(TAG, "FoloToy Vibe Check starting");
#endif
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "wake-up cause: %d", wakeup);
    }

    bsp_i2c_init();
    bsp_i2c_scan();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed; check SPI wiring "
                      "(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

#if defined(FAP_APP_RICOCHET_RUSH)
    ricochet_rush_prepare();
#elif defined(FAP_APP_FRUIT_MERGE)
    fruit_merge_prepare();
#elif defined(FAP_APP_FOCUS_POST)
    focus_post_prepare();
#elif defined(FAP_APP_DOWN_100)
    down_100_prepare();
#elif defined(FAP_APP_MATH_RAIL)
    math_rail_prepare();
#elif defined(FAP_APP_MATH_TRAIN)
    math_train_prepare();
#elif defined(FAP_APP_POCKET_POND)
    pocket_pond_prepare();
#elif defined(FAP_APP_BALLOON_RUSH)
    balloon_rush_prepare();
#elif defined(FAP_APP_PVZ_ALMANAC)
    pvz_almanac_prepare();
#elif defined(FAP_APP_WORD_SPRITE)
    word_sprite_prepare();
#elif defined(FAP_APP_CLOUD_HOP)
    cloud_hop_prepare();
#elif defined(FAP_APP_PERFECT_SLICE)
    perfect_slice_prepare();
#elif defined(FAP_APP_NEEDLE_RUSH)
    needle_rush_prepare();
#elif defined(FAP_APP_STACK_RUSH)
    stack_rush_prepare_input();
#elif defined(FAP_APP_IDIOM_PET)
    idiom_pet_prepare();
#endif
    bool buttons_ok = (bsp_button_init(on_key, NULL) == ESP_OK);
    bool battery_ok = (bsp_battery_init() == ESP_OK);
#if defined(FAP_APP_LAOLUO_QUOTES) || defined(FAP_APP_WORD_SPRITE) || defined(FAP_APP_IDIOM_PET) || defined(FAP_APP_PVZ_ALMANAC) || defined(FAP_APP_VIBE_CHECK)
    bool audio_ok = (bsp_audio_init() == ESP_OK);
#endif
#if defined(FAP_APP_WORD_SPRITE)
    ws_progress_t word_progress = ws_runtime_start(audio_ok);
#elif defined(FAP_APP_IDIOM_PET)
    ip_voice_start(audio_ok);
#endif

    if (bsp_lvgl_lock(1000)) {
#if defined(FAP_APP_RICOCHET_RUSH)
        ricochet_rush_enter(buttons_ok);
#elif defined(FAP_APP_FRUIT_MERGE)
        fruit_merge_enter(buttons_ok);
#elif defined(FAP_APP_FOCUS_POST)
        focus_post_enter(buttons_ok);
#elif defined(FAP_APP_DOWN_100)
        down_100_enter(buttons_ok);
#elif defined(FAP_APP_MATH_RAIL)
        math_rail_enter(buttons_ok);
#elif defined(FAP_APP_MATH_TRAIN)
        math_train_enter(buttons_ok);
#elif defined(FAP_APP_POCKET_POND)
        pocket_pond_enter(buttons_ok);
#elif defined(FAP_APP_BALLOON_RUSH)
        balloon_rush_enter(buttons_ok);
#elif defined(FAP_APP_PVZ_ALMANAC)
        pvz_almanac_enter(audio_ok, buttons_ok);
#elif defined(FAP_APP_WORD_SPRITE)
        word_sprite_enter(buttons_ok, word_progress);
#elif defined(FAP_APP_SOCIAL_BATTERY)
        social_battery_enter(buttons_ok);
#elif defined(FAP_APP_CLOUD_HOP)
        cloud_hop_enter(buttons_ok);
#elif defined(FAP_APP_PERFECT_SLICE)
        perfect_slice_enter(buttons_ok);
#elif defined(FAP_APP_NEEDLE_RUSH)
        needle_rush_enter(buttons_ok);
#elif defined(FAP_APP_STACK_RUSH)
        stack_rush_enter(buttons_ok);
#elif defined(FAP_APP_IDIOM_PET)
        idiom_pet_enter(buttons_ok);
#elif defined(FAP_APP_LAOLUO_QUOTES)
        laoluo_quotes_enter(audio_ok, buttons_ok);
#elif defined(FAP_APP_MEMORY_GARDEN)
        memory_garden_enter(buttons_ok);
#elif defined(FAP_APP_TOMATO_BLOOM)
        tomato_bloom_enter(buttons_ok);
#else
        vibe_check_enter(buttons_ok, audio_ok);
#endif
        bsp_lvgl_unlock();
    }

    fap_screenshot_start();

#if defined(FAP_APP_LAOLUO_QUOTES) || defined(FAP_APP_WORD_SPRITE) || defined(FAP_APP_IDIOM_PET) || defined(FAP_APP_PVZ_ALMANAC) || defined(FAP_APP_VIBE_CHECK)
    ESP_LOGI(TAG, "ready: buttons=%d battery=%d audio=%d",
             buttons_ok, battery_ok, audio_ok);
#else
    ESP_LOGI(TAG, "ready: buttons=%d battery=%d", buttons_ok, battery_ok);
#endif
}

#endif
