idf_component_register(
    SRCS "main.c" "ui_pixel.c" "ui_pixel_math.c" "fap_screenshot.c"
         "cat/cat_state.c" "cat/cat_control.c" "cat/cat_ui.c" "cat/cat_sound.c" "cat/cat_audio_runtime.c"
         "../assets/music/cat/cat_clips.c" "minecraft_adpcm.c"
         "../assets/fonts/cat_zh_12.c" "../assets/fonts/cat_zh_14.c"
         "../assets/fonts/cat_zh_16.c" "../assets/fonts/cat_zh_20.c"
    INCLUDE_DIRS "." "cat"
    REQUIRES bsp driver esp_hw_support esp_timer nvs_flash
)
