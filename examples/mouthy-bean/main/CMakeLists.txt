idf_component_register(
    SRCS "main.c" "mouthy_bean.c" "bean_state.c" "bean_runtime.c"
         "bean_audio_index.c" "minecraft_adpcm.c"
         "../../../main/fap_screenshot.c"
         "../assets/fonts/mouthy_bean_zh_12.c" "../assets/fonts/mouthy_bean_zh_16.c"
    INCLUDE_DIRS "." "../../../main"
    REQUIRES bsp driver esp_hw_support esp_timer nvs_flash)
target_add_binary_data(${COMPONENT_LIB}
    "${PROJECT_DIR}/assets/music/mouthy_bean/mouthy_bean_adpcm.bin" BINARY)
