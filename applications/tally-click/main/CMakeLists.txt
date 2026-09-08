idf_component_register(
    SRCS "main.c" "ui_pixel_math.c" "fap_screenshot.c"
        "tally/tally_audio.c" "tally/tally_sound.c" "tally/tally_state.c" "tally/tally_ui.c" "tally/tally_runtime.c"
        "../assets/fonts/tally_zh_12.c" "../assets/fonts/tally_zh_14.c"
        "../assets/fonts/tally_zh_20.c"
    INCLUDE_DIRS "." "tally"
    REQUIRES bsp driver esp_hw_support esp_timer nvs_flash)
