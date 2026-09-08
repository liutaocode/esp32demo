idf_component_register(
    SRCS "main.c" "ui_pixel.c" "ui_pixel_math.c" "minecraft_adpcm.c" "fap_screenshot.c"
         "apps/rhythm_agent/rhythm_agent.c" "apps/rhythm_agent/rhythm_state.c"
         "apps/rhythm_agent/rhythm_audio.c" "apps/rhythm_agent/rhythm_tone.c"
         "apps/rhythm_agent/rhythm_service.c" "apps/rhythm_agent/rhythm_voice_index.c"
         "../assets/fonts/rhythm_zh_12.c" "../assets/fonts/rhythm_zh_16.c" "../assets/fonts/rhythm_zh_24.c"
    INCLUDE_DIRS "." "apps/rhythm_agent"
    REQUIRES bsp driver esp_hw_support esp_timer nvs_flash
)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../assets/music/rhythm_agent/rhythm_voice.bin" BINARY)
