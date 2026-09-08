idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "sanxingdui_museum.c"
         "sanxingdui_museum_state.c"
         "sanxingdui_adpcm.c"
         "sanxingdui_tts_audio.c"
         "fap_screenshot.c"
         "../assets/fonts/sanxingdui_zh_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support
)
