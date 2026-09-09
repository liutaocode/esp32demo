#!/usr/bin/env bash
set -euo pipefail
app_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
repo_root="$(cd -- "$app_root/../.." && pwd)"
mode="${1:---all}"
case "$mode" in --all|--static|--firmware) ;; *) exit 2;; esac
cd "$app_root"
if [[ "$mode" != --firmware ]]; then
  python3 tools/generate_online_font.py --check
  test_dir="$(mktemp -d)"
  "${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain tests/test_online_state.c main/online_state.c -o "$test_dir/test"
  "$test_dir/test"
  rm -rf -- "$test_dir"
  python3 -m py_compile backend/service.py
  echo 'Host tests: PASS'
fi
if [[ "$mode" != --static ]]; then
  command -v idf.py >/dev/null
  validation_build="$(mktemp -d /tmp/qwen-bean-build.XXXXXX)"
  trap 'rm -rf -- "$validation_build"' EXIT
  idf.py -B "$validation_build" -D "SDKCONFIG=$validation_build/sdkconfig" build
  idf.py -B "$validation_build" merge-bin -o "$validation_build/FoloToy-AI-Passport-full.bin"
  python3 "$repo_root/tools/verify_firmware.py" "$validation_build"
  mkdir -p build
  install -m 0644 "$validation_build/FoloToy-AI-Passport-full.bin" build/FoloToy-AI-Passport-full.bin
  echo 'Build: PASS'
fi
