idf_component_register(
    SRCS "main.c" "ui_pixel.c" "ui_pixel_math.c" "ebook/ebook.c" "ebook/ebook_state.c" "ebook/ebook_text.c" "ebook/ebook_name.c"
         "ebook/ebook_fs.c" "ebook/ebook_net.c" "ebook/ebook_audio.c" "ebook/ebook_audio_runtime.c" "ebook/ebook_speech.c"
         "tts_model.c" "tts_runtime.c" "tts_bank.c" "tts_tempo.c" "fap_screenshot.c"
         "../assets/fonts/ebook_zh_12.c" "../assets/fonts/ebook_zh_16.c" "../assets/fonts/ebook_zh_20.c"
    INCLUDE_DIRS "." "ebook"
    REQUIRES bsp driver esp_hw_support esp_tts speex mbedtls esp_timer nvs_flash esp_wifi esp_netif esp_event esp_http_server fatfs vfs wear_levelling
)
target_add_binary_data(${COMPONENT_LIB} "${PROJECT_DIR}/assets/music/chinese_tts/xiaole-compact.dat" BINARY)

foreach(book tang song)
    target_add_binary_data(${COMPONENT_LIB} "${PROJECT_DIR}/assets/books/${book}.txt" BINARY)
endforeach()
