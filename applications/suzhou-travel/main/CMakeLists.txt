idf_component_register(
    SRCS "app_main.c"
         "fap_screenshot.c"
         "fap_screenshot_protocol.c"
         "suzhou_travel.c"
         "suzhou_travel_state.c"
         "suzhou_adpcm.c"
         "suzhou_tts_audio.c"
         "suzhou_garden_header.c"
         "suzhou_zh_16.c"
    INCLUDE_DIRS "."
    REQUIRES bsp esp_hw_support esp_driver_usb_serial_jtag
)
