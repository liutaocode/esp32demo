cmake_minimum_required(VERSION 3.16)
# Reuse this repository's board support and protected Recovery boot hook.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components/bsp")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
idf_build_set_property(BOOTLOADER_EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_LIST_DIR}/bootloader_components" APPEND)
project(FoloToy-AI-Passport)
