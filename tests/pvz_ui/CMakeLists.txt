cmake_minimum_required(VERSION 3.16)
project(pvz_ui_test C)
get_filename_component(REPO "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${REPO}/managed_components/lvgl__lvgl" lvgl)
add_executable(pvz_ui_test test_view.c
    "${REPO}/main/apps/pvz_almanac/pvz_view.c"
    "${REPO}/main/apps/pvz_almanac/pvz_art.c"
    "${REPO}/main/apps/pvz_almanac/pvz_state.c"
    "${REPO}/main/apps/pvz_almanac/pvz_catalog.c"
    "${REPO}/main/ui_pixel.c"
    "${REPO}/assets/fonts/pvz_zh_12.c"
    "${REPO}/assets/fonts/pvz_zh_16.c"
    "${REPO}/assets/fonts/pvz_zh_20.c")
target_include_directories(pvz_ui_test PRIVATE "${REPO}/main" "${REPO}/main/apps/pvz_almanac")
target_link_libraries(pvz_ui_test lvgl)
add_executable(pvz_input_test test_input.c
    "${REPO}/main/apps/pvz_almanac/pvz_view.c"
    "${REPO}/main/apps/pvz_almanac/pvz_art.c"
    "${REPO}/main/apps/pvz_almanac/pvz_state.c"
    "${REPO}/main/apps/pvz_almanac/pvz_catalog.c"
    "${REPO}/main/ui_pixel.c"
    "${REPO}/assets/fonts/pvz_zh_12.c"
    "${REPO}/assets/fonts/pvz_zh_16.c"
    "${REPO}/assets/fonts/pvz_zh_20.c")
target_include_directories(pvz_input_test PRIVATE stubs "${REPO}/main" "${REPO}/main/apps/pvz_almanac")
target_link_libraries(pvz_input_test lvgl)
