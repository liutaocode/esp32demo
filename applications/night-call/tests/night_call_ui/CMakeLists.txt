cmake_minimum_required(VERSION 3.16)
project(night_call_preview C)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_LIST_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${REPO_ROOT}/managed_components/lvgl__lvgl" lvgl)
add_executable(preview preview.c "${REPO_ROOT}/main/apps/night_call/night_call_state.c"
    "${REPO_ROOT}/main/apps/night_call/night_call_story.c" "${REPO_ROOT}/main/ui_pixel.c"
    "${REPO_ROOT}/assets/fonts/night_call_zh_12.c" "${REPO_ROOT}/assets/fonts/night_call_zh_16.c"
    "${REPO_ROOT}/assets/fonts/night_call_zh_24.c")
target_include_directories(preview PRIVATE "${CMAKE_CURRENT_LIST_DIR}/stubs" "${REPO_ROOT}/main")
target_compile_options(preview PRIVATE -Wall -Wextra -Werror)
target_link_libraries(preview lvgl)
