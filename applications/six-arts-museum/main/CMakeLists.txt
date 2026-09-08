idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "six_arts_museum.c"
         "six_arts_museum_state.c"
         "six_arts_adpcm.c"
         "six_arts_tts_audio.c"
         "fap_screenshot.c"
         "../assets/fonts/six_arts_zh_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support
)
