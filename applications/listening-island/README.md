[简体中文](README.zh_CN.md) | English

# English Listening Practice firmware

Chinese-interface offline English listening application. The [application guide](docs/listening-island.md) describes the complete behavior, build steps, storage design, and acceptance checks. The [baseline documentation index](docs/README.md) remains available for hardware references.

Activate ESP-IDF 5.5.3 and run `./tools/validate.sh` here. The gate produces `build/FoloToy-AI-Passport-full.bin`, verifies the BLE Recovery contract and resource payload, and exports segmented binaries for a later authorized installation.

This candidate was installed by verified segmented USB writing after creator approval; boot and a real home-framebuffer capture passed. Do not raw-flash its merged binary over a provisioned device: its resource payload is after the protected identity region.
