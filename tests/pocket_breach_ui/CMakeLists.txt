cmake_minimum_required(VERSION 3.16)
project(pocket_breach_preview C)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${REPO_ROOT}/tests/word_sprite_ui/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${REPO_ROOT}/managed_components/lvgl__lvgl" lvgl)
add_executable(preview preview.c
    "${REPO_ROOT}/main/apps/pocket_breach/pb_game.c"
    "${REPO_ROOT}/main/apps/pocket_breach/pb_render.c"
    "${REPO_ROOT}/main/ui_pixel.c"
    "${REPO_ROOT}/assets/fonts/pocket_breach_zh_16.c")
target_include_directories(preview PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" "${REPO_ROOT}/tests/word_sprite_ui/stubs" "${REPO_ROOT}/main")
target_compile_options(preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(preview lvgl m)
add_executable(capture_test capture_test.c)
target_include_directories(capture_test PRIVATE "${CMAKE_CURRENT_LIST_DIR}/capture_stubs" "${REPO_ROOT}/main")
target_compile_options(capture_test PRIVATE -Wall -Wextra -Werror)
target_link_libraries(capture_test lvgl m)
