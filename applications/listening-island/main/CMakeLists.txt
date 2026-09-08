idf_component_register(
 SRCS "main.c" "ui_pixel.c" "ui_pixel_math.c" "minecraft_adpcm.c" "fap_screenshot.c"
 "apps/listening/listening_state.c" "apps/listening/listening_runtime.c"
 "apps/listening/listening_ui.c" "apps/listening/listening_catalog.c" "apps/listening/listening_catalog_check.c"
 "../assets/fonts/listening_zh_16.c" "../assets/fonts/listening_zh_22.c"
 INCLUDE_DIRS "." "apps/listening"
 REQUIRES bsp driver esp_hw_support esp_timer nvs_flash esp_partition)
target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../assets/music/listening/listening_a.bin" BINARY)
esptool_py_flash_to_partition(flash "listening" "${CMAKE_CURRENT_LIST_DIR}/../assets/music/listening/listening_b.bin")
