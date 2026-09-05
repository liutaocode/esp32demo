cmake_minimum_required(VERSION 3.16)
project(word_sprite_preview C)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${REPO_ROOT}/managed_components/lvgl__lvgl" lvgl)
add_executable(preview preview.c
    "${REPO_ROOT}/main/apps/word_sprite/word_sprite_state.c"
    "${REPO_ROOT}/main/apps/word_sprite/word_sprite_catalog.c"
    "${REPO_ROOT}/main/ui_pixel.c"
    "${REPO_ROOT}/assets/fonts/word_sprite_zh_16.c")
target_include_directories(preview PRIVATE stubs "${REPO_ROOT}/main")
target_compile_options(preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(preview lvgl)
