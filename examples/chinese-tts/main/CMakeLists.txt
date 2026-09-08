idf_component_register(
    SRCS "main.c" "ui_pixel.c" "ui_pixel_math.c" "demo_chinese_tts.c"
         "tts_model.c" "tts_runtime.c" "tts_bank.c" "tts_tempo.c" "fap_screenshot.c"
         "../assets/fonts/chinese_tts_zh_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp driver esp_hw_support esp_tts speex mbedtls esp_timer
)
target_add_binary_data(${COMPONENT_LIB} "${PROJECT_DIR}/assets/music/chinese_tts/xiaole-compact.dat" BINARY)
