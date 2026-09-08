#!/usr/bin/env bash
set -euo pipefail

mode="${1:---all}"
project_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"

run_host_tests() {
    local test_dir
    test_dir="$(mktemp -d /tmp/suzhou-travel-tests.XXXXXX)"
    trap 'case "${test_dir}" in /tmp/suzhou-travel-tests.*) rm -rf -- "${test_dir}" ;; esac' RETURN
    python3 "${project_root}/tools/verify_ui_font.py"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"${project_root}/main" \
        "${project_root}/tests/test_suzhou_travel_state.c" \
        "${project_root}/main/suzhou_travel_state.c" \
        -o "${test_dir}/test_suzhou_travel_state"
    "${test_dir}/test_suzhou_travel_state"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"${project_root}/main" \
        "${project_root}/tests/test_suzhou_adpcm.c" \
        "${project_root}/main/suzhou_adpcm.c" \
        -o "${test_dir}/test_suzhou_adpcm"
    "${test_dir}/test_suzhou_adpcm"
    "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -I"${project_root}/main" \
        "${project_root}/tests/test_fap_screenshot_protocol.c" \
        "${project_root}/main/fap_screenshot_protocol.c" \
        -o "${test_dir}/test_fap_screenshot_protocol"
    "${test_dir}/test_fap_screenshot_protocol"
    echo "Host tests: PASS"
}

run_firmware() {
    if ! command -v idf.py >/dev/null 2>&1; then
        echo "ERROR: activate ESP-IDF v5.5.3 before firmware validation." >&2
        return 1
    fi
    if [[ "$(idf.py --version)" != "ESP-IDF v5.5.3" ]]; then
        echo "ERROR: ESP-IDF v5.5.3 is required." >&2
        return 1
    fi
    idf.py -B "${project_root}/build" build
    idf.py -B "${project_root}/build" merge-bin \
        -o "${project_root}/build/FoloToy-Suzhou-Travel-full.bin"
    python3 "${project_root}/tools/verify_firmware.py" "${project_root}/build"
    echo "Firmware build: PASS"
}

cd "${project_root}"
case "${mode}" in
    --all)
        run_host_tests
        run_firmware
        ;;
    --static)
        run_host_tests
        ;;
    --firmware)
        run_firmware
        ;;
    *)
        echo "Usage: $0 [--all|--static|--firmware]" >&2
        exit 2
        ;;
esac
