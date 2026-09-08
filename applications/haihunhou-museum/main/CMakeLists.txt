idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "haihunhou_museum.c"
         "haihunhou_museum_state.c"
         "haihunhou_adpcm.c"
         "haihunhou_tts_audio.c"
         "fap_screenshot.c"
         "../assets/fonts/haihunhou_zh_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support
)
