cmake_minimum_required(VERSION 3.16)
project(cat_preview C)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LVGL_SOURCE "${REPO_ROOT}/managed_components/lvgl__lvgl" CACHE PATH "LVGL checkout")
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${LVGL_SOURCE}" lvgl)
add_executable(preview preview.c "${REPO_ROOT}/main/cat/cat_state.c" "${REPO_ROOT}/main/cat/cat_control.c"
    "${REPO_ROOT}/main/cat/cat_ui.c" "${REPO_ROOT}/main/ui_pixel.c"
    "${REPO_ROOT}/assets/fonts/cat_zh_12.c" "${REPO_ROOT}/assets/fonts/cat_zh_14.c"
    "${REPO_ROOT}/assets/fonts/cat_zh_16.c" "${REPO_ROOT}/assets/fonts/cat_zh_20.c")
target_include_directories(preview PRIVATE "${REPO_ROOT}/main")
target_compile_options(preview PRIVATE -Wall -Wextra -Werror -UNDEBUG)
target_link_libraries(preview lvgl)
