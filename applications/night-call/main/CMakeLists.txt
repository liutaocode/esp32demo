idf_component_register(
    SRCS "main.c" "ui_pixel.c" "ui_pixel_math.c" "fap_screenshot.c"
         "minecraft_adpcm.c"
         "apps/night_call/night_call.c" "apps/night_call/night_call_state.c"
         "apps/night_call/night_call_story.c" "apps/night_call/night_call_storage.c"
         "apps/night_call/night_call_audio.c" "apps/night_call/night_call_voice.c"
         "apps/night_call/night_call_audio_index.c"
         "../assets/fonts/night_call_zh_12.c" "../assets/fonts/night_call_zh_16.c"
         "../assets/fonts/night_call_zh_24.c"
    INCLUDE_DIRS "." "apps/night_call"
    REQUIRES bsp driver esp_hw_support esp_timer nvs_flash
)
target_add_binary_data(${COMPONENT_LIB}
    "${CMAKE_CURRENT_LIST_DIR}/../assets/music/night_call/night_call_adpcm.bin" BINARY)
