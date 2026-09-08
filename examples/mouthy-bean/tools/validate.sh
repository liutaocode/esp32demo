#!/usr/bin/env bash
set -euo pipefail
export PYTHONDONTWRITEBYTECODE=1
app_root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
repo_root="$(cd -- "${app_root}/../.." && pwd)"
mode="${1:---all}"
case "$mode" in --all|--static|--firmware) ;; *) echo 'Usage: validate.sh [--all|--static|--firmware]' >&2; exit 2;; esac
cd "$app_root"
if [[ "$mode" != --static ]]; then
    command -v idf.py >/dev/null || { echo 'Activate ESP-IDF 5.5.3 first.' >&2; exit 1; }
    validation_build="$(mktemp -d /tmp/mouthy-bean-firmware.XXXXXX)"
    trap 'rm -rf -- "$validation_build"' EXIT
    idf.py -B "$validation_build" -D "SDKCONFIG=$validation_build/sdkconfig" reconfigure
fi
if [[ "$mode" != --firmware ]]; then
    python3 "$repo_root/tools/check_repo.py"
    python3 "$repo_root/tools/check_published_apps.py"
    python3 tools/test_mouthy_bean.py --ui
fi
if [[ "$mode" != --static ]]; then
    idf.py -B "$validation_build" build
    idf.py -B "$validation_build" merge-bin -o "$validation_build/FoloToy-AI-Passport-full.bin"
    python3 "$repo_root/tools/verify_firmware.py" "$validation_build"
    mkdir -p build
    install -m 0644 "$validation_build/FoloToy-AI-Passport-full.bin" build/FoloToy-AI-Passport-full.bin
    echo 'Build: PASS'
fi
