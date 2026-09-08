# Select exactly one published application.
set(selected_app "$ENV{FAP_APP}")
if(selected_app STREQUAL "")
    set(selected_app "vibe_check")
endif()
if(selected_app STREQUAL "clean_sweep")
    set(app_sources
        "apps/clean_sweep/clean_sweep.c"
        "apps/clean_sweep/clean_sweep_state.c"
        "../assets/fonts/clean_sweep_zh_12.c"
        "../assets/fonts/clean_sweep_zh_16.c"
        "../assets/fonts/clean_sweep_zh_24.c")
    set(app_include_dir "apps/clean_sweep")
    set(build_clean_sweep ON)
elseif(selected_app STREQUAL "cloud_hop")
    set(app_sources
        "apps/cloud_hop/cloud_hop.c"
        "apps/cloud_hop/cloud_hop_state.c"
        "../assets/fonts/cloud_hop_zh_16.c")
    set(app_include_dir "apps/cloud_hop")
    set(build_cloud_hop ON)
elseif(selected_app STREQUAL "code_theater")
    set(app_sources "apps/code_theater/code_theater.c" "apps/code_theater/code_theater_state.c"
        "../assets/fonts/code_theater_zh_14.c" "../assets/fonts/code_theater_zh_16.c")
    set(app_include_dir "apps/code_theater")
    set(build_code_theater ON)
elseif(selected_app STREQUAL "deadline_station")
    set(app_sources "apps/deadline_station/deadline_station.c"
        "apps/deadline_station/deadline_station_state.c"
        "apps/deadline_station/deadline_station_catalog.c"
        "apps/deadline_station/deadline_station_storage.c"
        "../assets/fonts/deadline_station_zh_12.c"
        "../assets/fonts/deadline_station_zh_16.c"
        "../assets/fonts/deadline_station_zh_24.c")
    set(app_include_dir "apps/deadline_station")
    set(build_deadline_station ON)
elseif(selected_app STREQUAL "down_100")
    set(app_sources "apps/down_100/down_100.c" "apps/down_100/down_100_state.c"
        "apps/down_100/down_100_audio.c" "apps/down_100/down_100_audio_runtime.c"
        "../assets/fonts/down_100_zh_16.c")
    set(app_include_dir "apps/down_100")
    set(build_down_100 ON)
elseif(selected_app STREQUAL "ebook")
    # 阅读器显示的是用户上传的任意文本,没法按文案抽取字体子集,
    # 因此带的是 GB2312 全集的三档点阵字库(1 bpp)。
    set(app_sources "apps/ebook/ebook.c" "apps/ebook/ebook_state.c"
        "apps/ebook/ebook_text.c" "apps/ebook/ebook_name.c"
        "apps/ebook/ebook_fs.c" "apps/ebook/ebook_net.c"
        "apps/ebook/ebook_audio.c" "apps/ebook/ebook_audio_runtime.c"
        "../assets/fonts/ebook_zh_12.c" "../assets/fonts/ebook_zh_16.c"
        "../assets/fonts/ebook_zh_20.c")
    set(app_include_dir "apps/ebook")
    set(build_ebook ON)
elseif(selected_app STREQUAL "excuse_call")
    set(app_sources "apps/excuse_call/excuse_call.c" "apps/excuse_call/excuse_call_state.c"
        "apps/excuse_call/excuse_call_audio.c" "apps/excuse_call/excuse_call_runtime.c"
        "../assets/music/excuse_call/excuse_call_pcm.c"
        "../assets/fonts/excuse_call_zh_16.c" "../assets/fonts/excuse_call_zh_24.c" "../assets/fonts/excuse_call_zh_32.c"
        "../assets/images/excuse_call/excuse_call_icons.c")
    set(app_include_dir "apps/excuse_call")
    set(build_excuse_call ON)
elseif(selected_app STREQUAL "focus_post")
    set(app_sources "apps/focus_post/focus_post.c" "apps/focus_post/focus_post_state.c"
        "../assets/fonts/focus_post_zh_16.c" "../assets/fonts/focus_post_zh_24.c")
    set(app_include_dir "apps/focus_post")
    set(build_focus_post ON)
elseif(selected_app STREQUAL "fruit_merge")
    set(app_sources "apps/fruit_merge/fruit_merge.c" "apps/fruit_merge/fruit_merge_state.c"
        "../assets/fonts/fruit_merge_zh_12.c" "../assets/fonts/fruit_merge_zh_16.c" "../assets/fonts/fruit_merge_zh_24.c")
    set(app_include_dir "apps/fruit_merge")
    set(build_fruit_merge ON)
elseif(selected_app STREQUAL "idiom_pet")
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
elseif(selected_app STREQUAL "jelly_squeeze")
    set(app_sources
        "apps/jelly_squeeze/jelly_squeeze.c"
        "apps/jelly_squeeze/jelly_squeeze_state.c"
        "apps/jelly_squeeze/jelly_squeeze_storage.c"
        "../assets/fonts/jelly_squeeze_zh_16.c"
        "../assets/fonts/jelly_squeeze_zh_24.c")
    set(app_include_dir "apps/jelly_squeeze")
    set(build_jelly_squeeze ON)
elseif(selected_app STREQUAL "just_seen")
    set(app_sources "apps/just_seen/just_seen.c" "apps/just_seen/just_seen_state.c"
        "../assets/fonts/just_seen_zh_16.c")
    set(app_include_dir "apps/just_seen")
    set(build_just_seen ON)
elseif(selected_app STREQUAL "lane_leap")
    set(app_sources "apps/lane_leap/lane_leap.c" "apps/lane_leap/lane_leap_state.c"
        "../assets/fonts/lane_leap_zh_12.c" "../assets/fonts/lane_leap_zh_16.c")
    set(app_include_dir "apps/lane_leap")
    set(build_lane_leap ON)
elseif(selected_app STREQUAL "laoluo_quotes")
    set(app_sources
        "apps/laoluo_quotes/laoluo_quotes.c"
        "apps/laoluo_quotes/laoluo_quotes_state.c"
        "apps/laoluo_quotes/laoluo_quotes_catalog.c"
        "apps/laoluo_quotes/laoluo_quotes_audio.c"
        "apps/laoluo_quotes/laoluo_adpcm.c"
        "../assets/fonts/laoluo_quotes_zh_18.c")
    set(app_include_dir "apps/laoluo_quotes")
    set(build_laoluo_quotes ON)
elseif(selected_app STREQUAL "math_rail")
    set(app_sources "apps/math_rail/math_rail.c" "apps/math_rail/math_rail_state.c"
        "../assets/fonts/math_rail_zh_16.c" "../assets/fonts/math_rail_zh_26.c")
    set(app_include_dir "apps/math_rail")
    set(build_math_rail ON)
elseif(selected_app STREQUAL "math_train")
    set(app_sources "apps/math_train/math_train.c" "apps/math_train/math_train_state.c"
        "../assets/fonts/math_train_zh_18.c" "../assets/fonts/math_train_math_28.c")
    set(app_include_dir "apps/math_train")
    set(build_math_train ON)
elseif(selected_app STREQUAL "memory_garden")
    set(app_sources
        "apps/memory_garden/memory_garden.c"
        "apps/memory_garden/memory_garden_state.c"
        "../assets/fonts/memory_garden_zh_18.c")
    set(app_include_dir "apps/memory_garden")
    set(build_memory_garden ON)
elseif(selected_app STREQUAL "minecraft_guide")
    set(app_sources "minecraft_guide.c" "minecraft_guide_state.c"
        "minecraft_adpcm.c" "minecraft_guide_audio.c"
        "../assets/fonts/minecraft_zh_16.c")
    set(app_include_dir ".")
    set(build_minecraft_guide ON)
elseif(selected_app STREQUAL "needle_rush")
    set(app_sources
        "apps/needle_rush/needle_rush.c"
        "apps/needle_rush/needle_rush_state.c"
        "../assets/fonts/needle_rush_zh_16.c")
    set(app_include_dir "apps/needle_rush")
    set(build_needle_rush ON)
elseif(selected_app STREQUAL "perfect_slice")
    set(app_sources
        "apps/perfect_slice/perfect_slice.c"
        "apps/perfect_slice/perfect_slice_state.c"
        "../assets/fonts/perfect_slice_zh_16.c")
    set(app_include_dir "apps/perfect_slice")
    set(build_perfect_slice ON)
elseif(selected_app STREQUAL "pocket_arcade")
    set(app_sources
        "apps/pocket_arcade/pocket_arcade.c"
        "apps/pocket_arcade/pa_scene.c"
        "apps/pocket_arcade/pa_hub.c"
        "apps/pocket_arcade/pa_storage.c"
        "apps/pocket_arcade/pa_g_snake.c"
        "apps/pocket_arcade/pa_g_tetris.c"
        "apps/pocket_arcade/pa_g_merge.c"
        "apps/pocket_arcade/pa_g_bird.c"
        "apps/pocket_arcade/pa_g_dino.c"
        "apps/pocket_arcade/pa_g_tiles.c"
        "apps/pocket_arcade/pa_g_mole.c"
        "apps/pocket_arcade/pa_g_invader.c"
        "apps/pocket_arcade/pa_g_jump.c"
        "apps/pocket_arcade/pa_g_sokoban.c"
        "apps/pocket_arcade/pa_g_cards.c"
        "apps/pocket_arcade/pa_g_pong.c"
        "apps/pocket_arcade/pa_g_four.c"
        "apps/pocket_arcade/pa_g_reversi.c"
        "apps/pocket_arcade/pa_g_mines.c"
        "apps/pocket_arcade/pa_g_slide.c"
        "apps/pocket_arcade/pa_g_hanoi.c"
        "apps/pocket_arcade/pa_g_frog.c"
        "apps/pocket_arcade/pa_g_bowl.c"
        "apps/pocket_arcade/pa_g_sling.c"
        "apps/pocket_arcade/pa_g_react.c"
        "apps/pocket_arcade/pa_g_pairs.c"
        "apps/pocket_arcade/pa_g_brick.c"
        "apps/pocket_arcade/pa_g_guess.c"
        "apps/pocket_arcade/pa_g_lights.c"
        "apps/pocket_arcade/pa_g_simon.c"
        "apps/pocket_arcade/pa_g_gomoku.c"
        "apps/pocket_arcade/pa_g_mathq.c"
        "apps/pocket_arcade/pa_g_nono.c"
        "apps/pocket_arcade/pa_g_peg.c"
        "apps/pocket_arcade/pa_g_poem.c"
        "apps/pocket_arcade/pa_g_quiz.c"
        "apps/pocket_arcade/pa_verse_data.c"
        "apps/pocket_arcade/pa_quiz_data.c"
        "apps/pocket_arcade/pa_picture_data.c"
        "../assets/fonts/pocket_arcade_zh_12.c"
        "../assets/fonts/pocket_arcade_zh_16.c")
    set(app_include_dir "apps/pocket_arcade")
    set(build_pocket_arcade ON)
elseif(selected_app STREQUAL "pocket_breach")
    set(app_sources "apps/pocket_breach/pocket_breach.c" "apps/pocket_breach/pb_game.c"
        "apps/pocket_breach/pb_render.c" "apps/pocket_breach/pb_audio.c" "apps/pocket_breach/pb_runtime.c"
        "../assets/fonts/pocket_breach_zh_16.c")
    set(app_include_dir "apps/pocket_breach")
    set(build_pocket_breach ON)
elseif(selected_app STREQUAL "pocket_hype")
    set(app_sources "apps/pocket_hype/pocket_hype.c" "apps/pocket_hype/pocket_hype_state.c"
        "apps/pocket_hype/pocket_hype_audio.c" "apps/pocket_hype/pocket_hype_audio_index.c"
        "apps/pocket_hype/pocket_hype_voice.c" "minecraft_adpcm.c"
        "../assets/fonts/pocket_hype_zh_12.c" "../assets/fonts/pocket_hype_zh_16.c"
        "../assets/fonts/pocket_hype_zh_24.c")
    set(app_include_dir "apps/pocket_hype")
    set(build_pocket_hype ON)
elseif(selected_app STREQUAL "pocket_pond")
    set(app_sources
        "apps/pocket_pond/pocket_pond.c"
        "apps/pocket_pond/pocket_pond_state.c"
        "apps/pocket_pond/pocket_pond_storage.c"
        "../assets/fonts/pocket_pond_zh_16.c")
    set(app_include_dir "apps/pocket_pond")
    set(build_pocket_pond ON)
elseif(selected_app STREQUAL "pvz_almanac")
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
elseif(selected_app STREQUAL "ricochet_rush")
    set(app_sources "apps/ricochet_rush/ricochet_rush.c" "apps/ricochet_rush/ricochet_rush_state.c"
        "../assets/fonts/ricochet_rush_zh_16.c")
    set(app_include_dir "apps/ricochet_rush")
    set(build_ricochet_rush ON)
elseif(selected_app STREQUAL "social_battery")
    set(app_sources
        "apps/social_battery/social_battery.c"
        "apps/social_battery/social_battery_state.c"
        "../assets/fonts/social_battery_zh_16.c"
        "../assets/fonts/social_battery_zh_28.c")
    set(app_include_dir "apps/social_battery")
    set(build_social_battery ON)
elseif(selected_app STREQUAL "stack_rush")
    set(app_sources
        "apps/stack_rush/stack_rush.c"
        "apps/stack_rush/stack_rush_state.c"
        "../assets/fonts/stack_rush_zh_16.c")
    set(app_include_dir "apps/stack_rush")
    set(build_stack_rush ON)
elseif(selected_app STREQUAL "tomato_bloom")
    set(app_sources
        "apps/tomato_bloom/tomato_bloom.c"
        "apps/tomato_bloom/tomato_bloom_state.c"
        "../assets/fonts/tomato_bloom_zh_16.c")
    set(app_include_dir "apps/tomato_bloom")
    set(build_tomato_bloom ON)
elseif(selected_app STREQUAL "vibe_check")
    set(app_sources
    "apps/vibe_check/vibe_check.c"
    "apps/vibe_check/vibe_check_audio.c"
    "apps/vibe_check/vibe_check_audio_index.c"
    "apps/vibe_check/vibe_check_state.c"
    "apps/vibe_check/vibe_check_voice.c"
    "minecraft_adpcm.c"
    "../assets/fonts/vibe_check_zh_16.c")
    set(app_include_dir "apps/vibe_check")
elseif(selected_app STREQUAL "word_sprite")
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
else()
    message(FATAL_ERROR "Unknown or unpublished FAP_APP: ${selected_app}")
endif()
set(app_requires "")
if(build_ebook)
    list(APPEND app_requires esp_wifi esp_netif esp_event esp_http_server fatfs vfs wear_levelling)
endif()
idf_component_register(SRCS "main.c" "ui_pixel.c" "ui_pixel_math.c" "fap_screenshot.c" ${app_sources}
    INCLUDE_DIRS "." ${app_include_dir}
    REQUIRES bsp driver esp_hw_support esp_timer nvs_flash ${app_requires})
string(TOUPPER "${selected_app}" app_define)
target_compile_definitions(${COMPONENT_LIB} PRIVATE "FAP_APP_${app_define}=1")
if(selected_app STREQUAL "pocket_hype")
    target_add_binary_data(${COMPONENT_LIB}
        "${CMAKE_CURRENT_LIST_DIR}/../assets/music/pocket_hype/pocket_hype_adpcm.bin" BINARY)
endif()
if(selected_app STREQUAL "pvz_almanac")
    target_add_binary_data(${COMPONENT_LIB}
        "${CMAKE_CURRENT_LIST_DIR}/../assets/music/pvz_almanac/pvz_adpcm.bin" BINARY)
endif()
if(selected_app STREQUAL "word_sprite")
    target_add_binary_data(${COMPONENT_LIB}
        "${CMAKE_CURRENT_LIST_DIR}/../assets/music/word_sprite/word_sprite_adpcm.bin" BINARY)
endif()
if(selected_app STREQUAL "idiom_pet")
    target_add_binary_data(${COMPONENT_LIB}
        "${CMAKE_CURRENT_LIST_DIR}/../assets/music/idiom_pet/idiom_pet_adpcm.bin" BINARY)
endif()
if(selected_app STREQUAL "vibe_check")
    target_add_binary_data(${COMPONENT_LIB} "${CMAKE_CURRENT_LIST_DIR}/../assets/music/vibe_check/vibe_check_adpcm.bin" BINARY)
endif()
