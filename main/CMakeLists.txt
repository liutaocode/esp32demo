idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         "minecraft_guide.c"
         "minecraft_guide_state.c"
         "minecraft_adpcm.c"
         "minecraft_guide_audio.c"
         "fap_screenshot.c"
         "../assets/fonts/minecraft_zh_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp driver esp_hw_support
)
