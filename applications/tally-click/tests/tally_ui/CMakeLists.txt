cmake_minimum_required(VERSION 3.16)
project(tally_preview C)
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${ROOT}/managed_components/lvgl__lvgl" lvgl)
add_executable(preview preview.c "${ROOT}/main/tally/tally_state.c"
    "${ROOT}/assets/fonts/tally_zh_12.c"
    "${ROOT}/assets/fonts/tally_zh_14.c" "${ROOT}/assets/fonts/tally_zh_20.c")
target_include_directories(preview PRIVATE "${ROOT}/main" "${ROOT}/main/tally")
target_compile_options(preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(preview lvgl)
