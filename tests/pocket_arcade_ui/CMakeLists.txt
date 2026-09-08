cmake_minimum_required(VERSION 3.16)
project(pocket_arcade_preview C)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${REPO_ROOT}/tests/word_sprite_ui/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${REPO_ROOT}/managed_components/lvgl__lvgl" lvgl)
# CONFIGURE_DEPENDS so a newly added game file is picked up without a manual
# reconfigure; the firmware build lists the same files explicitly.
file(GLOB ARCADE_GAMES CONFIGURE_DEPENDS "${REPO_ROOT}/main/apps/pocket_arcade/pa_g_*.c")
add_executable(preview preview.c
    ${ARCADE_GAMES}
    "${REPO_ROOT}/main/apps/pocket_arcade/pa_scene.c"
    "${REPO_ROOT}/main/apps/pocket_arcade/pa_hub.c"
    "${REPO_ROOT}/main/apps/pocket_arcade/pa_verse_data.c"
    "${REPO_ROOT}/main/apps/pocket_arcade/pa_quiz_data.c"
    "${REPO_ROOT}/main/apps/pocket_arcade/pa_picture_data.c"
    "${REPO_ROOT}/main/ui_pixel.c"
    "${REPO_ROOT}/assets/fonts/pocket_arcade_zh_16.c"
    "${REPO_ROOT}/assets/fonts/pocket_arcade_zh_12.c")
target_include_directories(preview PRIVATE stubs
    "${REPO_ROOT}/main" "${REPO_ROOT}/main/apps/pocket_arcade")
target_compile_options(preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(preview lvgl)
