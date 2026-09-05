set(app_sources
    "apps/vibe_check/vibe_check.c"
    "apps/vibe_check/vibe_check_audio.c"
    "apps/vibe_check/vibe_check_audio_index.c"
    "apps/vibe_check/vibe_check_state.c"
    "apps/vibe_check/vibe_check_voice.c"
    "minecraft_adpcm.c"
    "../assets/fonts/vibe_check_zh_16.c")
set(app_include_dir "apps/vibe_check")

# Keep product applications isolated in main/apps/. A fresh build can select
# one without rewriting the application sources or disturbing another app.
if("$ENV{FAP_APP}" STREQUAL "ricochet_rush")
    set(app_sources "apps/ricochet_rush/ricochet_rush.c" "apps/ricochet_rush/ricochet_rush_state.c"
        "../assets/fonts/ricochet_rush_zh_16.c")
    set(app_include_dir "apps/ricochet_rush")
    set(build_ricochet_rush ON)
elseif("$ENV{FAP_APP}" STREQUAL "fruit_merge")
    set(app_sources "apps/fruit_merge/fruit_merge.c" "apps/fruit_merge/fruit_merge_state.c"
        "../assets/fonts/fruit_merge_zh_12.c" "../assets/fonts/fruit_merge_zh_16.c" "../assets/fonts/fruit_merge_zh_24.c")
    set(app_include_dir "apps/fruit_merge")
    set(build_fruit_merge ON)
elseif("$ENV{FAP_APP}" STREQUAL "focus_post")
    set(app_sources "apps/focus_post/focus_post.c" "apps/focus_post/focus_post_state.c"
        "../assets/fonts/focus_post_zh_16.c" "../assets/fonts/focus_post_zh_24.c")
    set(app_include_dir "apps/focus_post")
    set(build_focus_post ON)
elseif("$ENV{FAP_APP}" STREQUAL "down_100")
    set(app_sources "apps/down_100/down_100.c" "apps/down_100/down_100_state.c"
        "apps/down_100/down_100_audio.c" "apps/down_100/down_100_audio_runtime.c"
        "../assets/fonts/down_100_zh_16.c")
    set(app_include_dir "apps/down_100")
    set(build_down_100 ON)
elseif("$ENV{FAP_APP}" STREQUAL "math_rail")
    set(app_sources "apps/math_rail/math_rail.c" "apps/math_rail/math_rail_state.c"
        "../assets/fonts/math_rail_zh_16.c" "../assets/fonts/math_rail_zh_26.c")
    set(app_include_dir "apps/math_rail")
    set(build_math_rail ON)
elseif("$ENV{FAP_APP}" STREQUAL "math_train")
    set(app_sources "apps/math_train/math_train.c" "apps/math_train/math_train_state.c"
        "../assets/fonts/math_train_zh_18.c" "../assets/fonts/math_train_math_28.c")
    set(app_include_dir "apps/math_train")
    set(build_math_train ON)
elseif("$ENV{FAP_APP}" STREQUAL "pocket_pond")
    set(app_sources
        "apps/pocket_pond/pocket_pond.c"
        "apps/pocket_pond/pocket_pond_state.c"
        "apps/pocket_pond/pocket_pond_storage.c"
        "../assets/fonts/pocket_pond_zh_16.c")
    set(app_include_dir "apps/pocket_pond")
    set(build_pocket_pond ON)
elseif("$ENV{FAP_APP}" STREQUAL "balloon_rush")
    set(app_sources
        "apps/balloon_rush/balloon_rush.c"
        "apps/balloon_rush/balloon_rush_state.c"
        "../assets/fonts/balloon_rush_zh_16.c")
    set(app_include_dir "apps/balloon_rush")
    set(build_balloon_rush ON)
elseif("$ENV{FAP_APP}" STREQUAL "word_sprite")
    set(app_sources
        "apps/word_sprite/word_sprite.c"
        "apps/word_sprite/word_sprite_state.c"
        "apps/word_sprite/word_sprite_catalog.c"
        "apps/word_sprite/word_sprite_audio.c"
        "apps/word_sprite/word_sprite_audio_index.c"
        "apps/word_sprite/word_sprite_runtime.c"
        "minecraft_adpcm.c"
        "../assets/fonts/word_sprite_zh_16.c")
    set(app_include_dir "apps/word_sprite")
    set(build_word_sprite ON)
elseif("$ENV{FAP_APP}" STREQUAL "pvz_almanac")
    set(app_sources
        "apps/pvz_almanac/pvz_almanac.c"
        "apps/pvz_almanac/pvz_state.c"
        "apps/pvz_almanac/pvz_view.c"
        "apps/pvz_almanac/pvz_art.c"
        "apps/pvz_almanac/pvz_catalog.c"
        "apps/pvz_almanac/pvz_audio.c"
        "apps/pvz_almanac/pvz_audio_runtime.c"
        "apps/pvz_almanac/pvz_audio_index.c"
        "minecraft_adpcm.c"
        "../assets/fonts/pvz_zh_12.c"
        "../assets/fonts/pvz_zh_16.c"
        "../assets/fonts/pvz_zh_20.c")
    set(app_include_dir "apps/pvz_almanac")
    set(build_pvz_almanac ON)
elseif("$ENV{FAP_APP}" STREQUAL "social_battery")
    set(app_sources
        "apps/social_battery/social_battery.c"
        "apps/social_battery/social_battery_state.c"
        "../assets/fonts/social_battery_zh_16.c"
        "../assets/fonts/social_battery_zh_28.c")
    set(app_include_dir "apps/social_battery")
    set(build_social_battery ON)
elseif("$ENV{FAP_APP}" STREQUAL "cloud_hop")
    set(app_sources
        "apps/cloud_hop/cloud_hop.c"
        "apps/cloud_hop/cloud_hop_state.c"
        "../assets/fonts/cloud_hop_zh_16.c")
    set(app_include_dir "apps/cloud_hop")
    set(build_cloud_hop ON)
elseif("$ENV{FAP_APP}" STREQUAL "perfect_slice")
    set(app_sources
        "apps/perfect_slice/perfect_slice.c"
        "apps/perfect_slice/perfect_slice_state.c"
        "../assets/fonts/perfect_slice_zh_16.c")
    set(app_include_dir "apps/perfect_slice")
    set(build_perfect_slice ON)
elseif("$ENV{FAP_APP}" STREQUAL "needle_rush")
    set(app_sources
        "apps/needle_rush/needle_rush.c"
        "apps/needle_rush/needle_rush_state.c"
        "../assets/fonts/needle_rush_zh_16.c")
    set(app_include_dir "apps/needle_rush")
    set(build_needle_rush ON)
elseif("$ENV{FAP_APP}" STREQUAL "stack_rush")
    set(app_sources
        "apps/stack_rush/stack_rush.c"
        "apps/stack_rush/stack_rush_state.c"
        "../assets/fonts/stack_rush_zh_16.c")
    set(app_include_dir "apps/stack_rush")
    set(build_stack_rush ON)
elseif("$ENV{FAP_APP}" STREQUAL "idiom_pet")
    set(app_sources
        "apps/idiom_pet/idiom_pet.c"
        "apps/idiom_pet/idiom_pet_state.c"
        "apps/idiom_pet/idiom_pet_catalog.c"
        "apps/idiom_pet/idiom_pet_storage.c"
        "apps/idiom_pet/idiom_pet_audio.c"
        "apps/idiom_pet/idiom_pet_audio_index.c"
        "apps/idiom_pet/idiom_pet_voice.c"
        "minecraft_adpcm.c"
        "../assets/fonts/idiom_pet_zh_16.c")
    set(app_include_dir "apps/idiom_pet")
    set(build_idiom_pet ON)
elseif("$ENV{FAP_APP}" STREQUAL "memory_garden")
    set(app_sources
        "apps/memory_garden/memory_garden.c"
        "apps/memory_garden/memory_garden_state.c"
        "../assets/fonts/memory_garden_zh_18.c")
    set(app_include_dir "apps/memory_garden")
    set(build_memory_garden ON)
elseif("$ENV{FAP_APP}" STREQUAL "tomato_bloom")
    set(app_sources
        "apps/tomato_bloom/tomato_bloom.c"
        "apps/tomato_bloom/tomato_bloom_state.c"
        "../assets/fonts/tomato_bloom_zh_16.c")
    set(app_include_dir "apps/tomato_bloom")
    set(build_tomato_bloom ON)
elseif("$ENV{FAP_APP}" STREQUAL "laoluo_quotes")
    set(app_sources
        "apps/laoluo_quotes/laoluo_quotes.c"
        "apps/laoluo_quotes/laoluo_quotes_state.c"
        "apps/laoluo_quotes/laoluo_quotes_catalog.c"
        "apps/laoluo_quotes/laoluo_quotes_audio.c"
        "apps/laoluo_quotes/laoluo_adpcm.c"
        "../assets/fonts/laoluo_quotes_zh_18.c")
    set(app_include_dir "apps/laoluo_quotes")
    set(build_laoluo_quotes ON)
elseif("$ENV{FAP_APP}" STREQUAL "minecraft_guide")
    set(app_sources "minecraft_guide.c" "minecraft_guide_state.c"
        "minecraft_adpcm.c" "minecraft_guide_audio.c"
        "../assets/fonts/minecraft_zh_16.c")
    set(app_include_dir ".")
    set(build_minecraft_guide ON)
elseif(NOT "$ENV{FAP_APP}" STREQUAL "" AND NOT "$ENV{FAP_APP}" STREQUAL "vibe_check")
    message(FATAL_ERROR "Unknown FAP_APP: $ENV{FAP_APP}. See README.md for supported applications.")
endif()

idf_component_register(
    SRCS "main.c"
         "ui_pixel.c"
         "ui_pixel_math.c"
         ${app_sources}
         "fap_screenshot.c"
    INCLUDE_DIRS "." ${app_include_dir}
    REQUIRES bsp driver esp_hw_support esp_timer nvs_flash
)

if(build_ricochet_rush)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_RICOCHET_RUSH=1)
elseif(build_fruit_merge)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_FRUIT_MERGE=1)
elseif(build_focus_post)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_FOCUS_POST=1)
elseif(build_down_100)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_DOWN_100=1)
elseif(build_math_rail)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_MATH_RAIL=1)
elseif(build_math_train)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_MATH_TRAIN=1)
elseif(build_pocket_pond)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_POCKET_POND=1)
elseif(build_balloon_rush)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_BALLOON_RUSH=1)
elseif(build_word_sprite)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_WORD_SPRITE=1)
    target_add_binary_data(${COMPONENT_LIB}
        "${CMAKE_CURRENT_LIST_DIR}/../assets/music/word_sprite/word_sprite_adpcm.bin" BINARY)
elseif(build_pvz_almanac)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_PVZ_ALMANAC=1)
    target_add_binary_data(${COMPONENT_LIB}
        "${CMAKE_CURRENT_LIST_DIR}/../assets/music/pvz_almanac/pvz_adpcm.bin" BINARY)
elseif(build_social_battery)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_SOCIAL_BATTERY=1)
elseif(build_cloud_hop)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_CLOUD_HOP=1)
elseif(build_perfect_slice)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_PERFECT_SLICE=1)
elseif(build_needle_rush)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_NEEDLE_RUSH=1)
elseif(build_stack_rush)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_STACK_RUSH=1)
elseif(build_idiom_pet)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_IDIOM_PET=1)
    target_add_binary_data(${COMPONENT_LIB}
        "${CMAKE_CURRENT_LIST_DIR}/../assets/music/idiom_pet/idiom_pet_adpcm.bin" BINARY)
elseif(build_memory_garden)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_MEMORY_GARDEN=1)
elseif(build_tomato_bloom)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_TOMATO_BLOOM=1)
elseif(build_laoluo_quotes)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_LAOLUO_QUOTES=1)
elseif(build_minecraft_guide)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_MINECRAFT_GUIDE=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE FAP_APP_VIBE_CHECK=1)
    target_add_binary_data(${COMPONENT_LIB}
        "${CMAKE_CURRENT_LIST_DIR}/../assets/music/vibe_check/vibe_check_adpcm.bin" BINARY)
endif()
