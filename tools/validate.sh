#!/usr/bin/env bash
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1

mode="${1:---all}"
repo_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

usage() {
    echo "Usage: $0 [--all|--static|--firmware]" >&2
}

run_static_checks() {
    local actionlint_bin
    local test_dir

    python3 tools/check_repo.py
    python3 tools/check_published_apps.py
    python3 examples/mouthy-bean/tools/test_mouthy_bean.py
    python3 tools/generate_ricochet_rush_font.py --check
    local ricochet_test_dir
    ricochet_test_dir="$(mktemp -d /tmp/ricochet-rush-state.XXXXXX)"
    "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Imain/apps/ricochet_rush \
        tests/test_ricochet_rush_state.c main/apps/ricochet_rush/ricochet_rush_state.c \
        -lm -o "${ricochet_test_dir}/test_ricochet_rush_state"
    "${ricochet_test_dir}/test_ricochet_rush_state"
    rm -rf "${ricochet_test_dir}"
    python3 tools/generate_fruit_merge_font.py --check
    local fruit_test_dir
    fruit_test_dir="$(mktemp -d /tmp/fruit-merge-state.XXXXXX)"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/fruit_merge \
        tests/test_fruit_merge_state.c main/apps/fruit_merge/fruit_merge_state.c \
        -o "${fruit_test_dir}/test_fruit_merge_state"
    "${fruit_test_dir}/test_fruit_merge_state"
    rm -rf "${fruit_test_dir}"
    python3 tools/generate_down_100_font.py --check

    actionlint_bin="${ACTIONLINT_BIN:-}"
    if [[ -z "${actionlint_bin}" ]]; then
        actionlint_bin="$(command -v actionlint || true)"
    fi
    if [[ -z "${actionlint_bin}" || ! -x "${actionlint_bin}" ]]; then
        actionlint_bin="$(./tools/install-actionlint.sh)"
    fi
    "${actionlint_bin}" -color .github/workflows/*.yml

    test_dir="$(mktemp -d /tmp/ai-passport-host-tests.XXXXXX)"
    "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Imain/apps/down_100 \
        tests/test_down_100_state.c main/apps/down_100/down_100_state.c \
        -o "${test_dir}/test_down_100_state"
    "${test_dir}/test_down_100_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/down_100 \
        tests/test_down_100_audio.c main/apps/down_100/down_100_audio.c \
        -o "${test_dir}/test_down_100_audio"
    "${test_dir}/test_down_100_audio"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -pthread \
        -Itests/down_100_audio_stubs -Imain/apps/down_100 \
        tests/test_down_100_audio_runtime.c main/apps/down_100/down_100_audio.c \
        main/apps/down_100/down_100_audio_runtime.c -o "${test_dir}/test_down_100_audio_runtime"
    for audio_case in 0 1 2 3 4; do "${test_dir}/test_down_100_audio_runtime" "$audio_case"; done
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/math_train \
        tests/test_math_train_state.c main/apps/math_train/math_train_state.c \
        -o "${test_dir}/test_math_train_state"
    "${test_dir}/test_math_train_state"
    python3 tools/generate_math_train_fonts.py --check
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/math_rail \
        tests/test_math_rail_state.c main/apps/math_rail/math_rail_state.c \
        -o "${test_dir}/test_math_rail_state"
    "${test_dir}/test_math_rail_state"
    python3 tools/generate_math_rail_fonts.py --check
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_ui_pixel_math.c main/ui_pixel_math.c \
        -o "${test_dir}/test_ui_pixel_math"
    "${test_dir}/test_ui_pixel_math"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_minecraft_guide_state.c main/minecraft_guide_state.c \
        -o "${test_dir}/test_minecraft_guide_state"
    "${test_dir}/test_minecraft_guide_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
        tests/test_minecraft_adpcm.c main/minecraft_adpcm.c \
        -o "${test_dir}/test_minecraft_adpcm"
    "${test_dir}/test_minecraft_adpcm"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/vibe_check \
        tests/test_vibe_check_state.c \
        main/apps/vibe_check/vibe_check_state.c \
        -o "${test_dir}/test_vibe_check_state"
    "${test_dir}/test_vibe_check_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/vibe_check \
        tests/test_vibe_check_audio.c \
        main/apps/vibe_check/vibe_check_audio.c \
        main/apps/vibe_check/vibe_check_audio_index.c \
        -o "${test_dir}/test_vibe_check_audio"
    "${test_dir}/test_vibe_check_audio"
    python3 tools/generate_vibe_check_audio.py --check
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/memory_garden \
        tests/test_memory_garden_state.c \
        main/apps/memory_garden/memory_garden_state.c \
        -o "${test_dir}/test_memory_garden_state"
    "${test_dir}/test_memory_garden_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Imain/apps/tomato_bloom \
        tests/test_tomato_bloom_state.c \
        main/apps/tomato_bloom/tomato_bloom_state.c \
        -o "${test_dir}/test_tomato_bloom_state"
    "${test_dir}/test_tomato_bloom_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Imain/apps/laoluo_quotes \
        tests/test_laoluo_quotes_state.c \
        main/apps/laoluo_quotes/laoluo_quotes_state.c \
        main/apps/laoluo_quotes/laoluo_adpcm.c \
        -o "${test_dir}/test_laoluo_quotes_state"
    "${test_dir}/test_laoluo_quotes_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Imain/apps/stack_rush \
        tests/test_stack_rush_state.c main/apps/stack_rush/stack_rush_state.c \
        -o "${test_dir}/test_stack_rush_state"
    "${test_dir}/test_stack_rush_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -Imain/apps/social_battery \
        tests/test_social_battery_state.c \
        main/apps/social_battery/social_battery_state.c \
        -o "${test_dir}/test_social_battery_state"
    "${test_dir}/test_social_battery_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/idiom_pet \
        tests/test_idiom_pet_state.c main/apps/idiom_pet/idiom_pet_state.c \
        main/apps/idiom_pet/idiom_pet_catalog.c \
        -o "${test_dir}/test_idiom_pet_state"
    "${test_dir}/test_idiom_pet_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain -Imain/apps/idiom_pet \
        tests/test_idiom_pet_audio.c main/apps/idiom_pet/idiom_pet_audio.c \
        main/apps/idiom_pet/idiom_pet_audio_index.c main/minecraft_adpcm.c \
        -o "${test_dir}/test_idiom_pet_audio"
    "${test_dir}/test_idiom_pet_audio"
    python3 tools/generate_idiom_pet_audio.py --check
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/word_sprite \
        tests/test_word_sprite_state.c main/apps/word_sprite/word_sprite_state.c \
        main/apps/word_sprite/word_sprite_catalog.c \
        main/apps/word_sprite/word_sprite_audio.c \
        main/apps/word_sprite/word_sprite_audio_index.c \
        -o "${test_dir}/test_word_sprite_state"
    "${test_dir}/test_word_sprite_state"
    python3 tools/generate_word_sprite_assets.py --check
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/pvz_almanac \
        tests/test_pvz_state.c main/apps/pvz_almanac/pvz_state.c \
        main/apps/pvz_almanac/pvz_catalog.c main/apps/pvz_almanac/pvz_audio.c \
        main/apps/pvz_almanac/pvz_audio_index.c \
        -o "${test_dir}/test_pvz_state"
    "${test_dir}/test_pvz_state"
    python3 tools/generate_pvz_assets.py --check
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/perfect_slice \
        tests/test_perfect_slice_state.c main/apps/perfect_slice/perfect_slice_state.c \
        -o "${test_dir}/test_perfect_slice_state"
    "${test_dir}/test_perfect_slice_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/cloud_hop \
        tests/test_cloud_hop_state.c main/apps/cloud_hop/cloud_hop_state.c \
        -o "${test_dir}/test_cloud_hop_state"
    "${test_dir}/test_cloud_hop_state"
    python3 tools/generate_cloud_hop_font.py --check
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/pocket_pond \
        tests/test_pocket_pond_state.c main/apps/pocket_pond/pocket_pond_state.c \
        -o "${test_dir}/test_pocket_pond_state"
    "${test_dir}/test_pocket_pond_state"
    cc -std=c11 -Wall -Wextra -Werror -Imain/apps/focus_post \
        tests/test_focus_post_state.c main/apps/focus_post/focus_post_state.c \
        -o "${test_dir}/test_focus_post_state"
    "${test_dir}/test_focus_post_state"
    python3 tools/generate_focus_post_font.py --check
    python3 tests/test_verify_firmware.py
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/needle_rush \
        tests/test_needle_rush_state.c main/apps/needle_rush/needle_rush_state.c \
        -o "${test_dir}/test_needle_rush_state"
    "${test_dir}/test_needle_rush_state"
    rm -rf "${test_dir}"
    test_dir="$(mktemp -d /tmp/published-app-tests.XXXXXX)"
    python3 tools/test_excuse_call.py
    python3 tools/generate_pocket_breach_font.py --check
    python3 tools/test_pocket_breach.py
    python3 tools/test_lane_leap.py
    python3 tools/generate_code_theater_font.py --check
    python3 tools/generate_deadline_station_font.py --check
    python3 tools/test_pocket_hype.py
    python3 tools/generate_pocket_hype_font.py --check
    python3 tools/generate_pocket_hype_audio.py --check
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/deadline_station \
        tests/test_deadline_station_state.c main/apps/deadline_station/deadline_station_state.c \
        main/apps/deadline_station/deadline_station_catalog.c -o "${test_dir}/test_deadline_station"
    "${test_dir}/test_deadline_station"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/just_seen \
        tests/test_just_seen_state.c main/apps/just_seen/just_seen_state.c \
        -o "${test_dir}/test_just_seen_state"
    "${test_dir}/test_just_seen_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/code_theater \
        tests/test_code_theater_state.c main/apps/code_theater/code_theater_state.c \
        -o "${test_dir}/test_code_theater_state"
    "${test_dir}/test_code_theater_state"
    "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Imain/apps/pocket_arcade \
        tests/test_pocket_arcade_state.c main/apps/pocket_arcade/pa_scene.c \
        main/apps/pocket_arcade/pa_hub.c main/apps/pocket_arcade/pa_g_*.c \
        main/apps/pocket_arcade/pa_verse_data.c \
        main/apps/pocket_arcade/pa_quiz_data.c \
        main/apps/pocket_arcade/pa_picture_data.c \
        -o "${test_dir}/test_pocket_arcade_state"
    "${test_dir}/test_pocket_arcade_state"
    python3 tools/test_pocket_arcade_levels.py
    python3 tools/generate_pocket_arcade_fonts.py --check
    "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Imain/apps/clean_sweep \
        tests/test_clean_sweep_state.c main/apps/clean_sweep/clean_sweep_state.c \
        -o "${test_dir}/test_clean_sweep_state"
    "${test_dir}/test_clean_sweep_state"
    python3 tools/generate_clean_sweep_fonts.py --check
    "${CC:-cc}" -std=c11 -O2 -Wall -Wextra -Werror -Imain/apps/jelly_squeeze \
        tests/test_jelly_squeeze_state.c main/apps/jelly_squeeze/jelly_squeeze_state.c \
        -o "${test_dir}/test_jelly_squeeze_state"
    "${test_dir}/test_jelly_squeeze_state"
    python3 tools/generate_jelly_squeeze_fonts.py --check
    for ebook_case in text state name audio; do
        "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain/apps/ebook \
            "tests/test_ebook_${ebook_case}.c" "main/apps/ebook/ebook_${ebook_case}.c" \
            -o "${test_dir}/test_ebook_${ebook_case}"
        "${test_dir}/test_ebook_${ebook_case}"
    done
    python3 tools/generate_ebook_fonts.py --check
    rm -rf "${test_dir}"
    echo "Host tests: PASS"
}

run_firmware_checks() (
    local validation_build_dir

    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: idf.py is not available; activate ESP-IDF 5.5.3 first." >&2
        return 1
    fi

    validation_build_dir="$(mktemp -d /tmp/ai-passport-firmware.XXXXXX)"
    trap 'case "${validation_build_dir}" in /tmp/ai-passport-firmware.*) rm -rf -- "${validation_build_dir}" ;; esac' EXIT

    SDKCONFIG_DEFAULTS="${repo_root}/sdkconfig.defaults" \
        idf.py -B "${validation_build_dir}" \
        -D "SDKCONFIG=${validation_build_dir}/sdkconfig" build
    idf.py -B "${validation_build_dir}" merge-bin \
        -o "${validation_build_dir}/FoloToy-AI-Passport-full.bin"
    python3 tools/verify_firmware.py "${validation_build_dir}"
    mkdir -p "${repo_root}/build"
    install -m 0644 \
        "${validation_build_dir}/FoloToy-AI-Passport-full.bin" \
        "${repo_root}/build/FoloToy-AI-Passport-full.bin"
    echo "Firmware build: PASS"
)

cd "${repo_root}"
case "${mode}" in
    --all)
        run_static_checks
        run_firmware_checks
        ;;
    --static)
        run_static_checks
        ;;
    --firmware)
        run_firmware_checks
        ;;
    *)
        usage
        exit 2
        ;;
esac
