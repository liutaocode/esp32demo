# Qwen Voice Bean

[简体中文](README.zh_CN.md)

An independent online edition of the yellow, big-eyed Mouthy Bean. It uses
**Qwen Audio Agent** for real voice conversations and delegated Agent tasks.
The original offline application is preserved in its original project. This
firmware boots directly into the online companion; its application source is
in `main/`, with no dependency on the original `main/apps` directory.

## Use

1. Deploy the backend using the [backend guide](backend/README.md).
2. On first boot, connect a phone to the password-protected hotspot shown on the
   device (a random eight-digit password). Nearby networks are scanned before
   the hotspot starts. Open `http://192.168.4.1` and choose a nearby 2.4 GHz Wi-Fi network, enter its password and the backend
   computer IPv4 address. The device builds `ws://IP:3101/api/realtime`.
   The access-token field is optional and hidden unless enabled.
3. After saving and rebooting, wait for the ready state. Press OK to enable the
   microphone. Speak naturally and pause for the backend to answer.
4. Press OK to close the microphone, UP to cycle volume, DOWN to interrupt a
   reply, or hold OK to open settings, review the saved connection, and confirm any reconfiguration.

The microphone starts closed. Upload pauses during playback; press DOWN to
interrupt a reply manually. This release does not include automatic voice
interruption or acoustic echo cancellation. The original eyes, raised ears and animated mouth now
follow real conversation states. The screen shows Chinese status messages;
it does not display arbitrary model-response transcripts.

## Backend and privacy

The computer/server must remain running and reachable. A deployed Gateway,
a working voice provider, and a configured backend Agent are separate
requirements. Model fees and Agent capabilities depend on that configuration.
The included LAN device gateway supports optional or required device tokens.
In token-free mode, other users on that LAN can use the voice/Agent service;
use it only on a trusted private network. The original Gateway stays on
loopback with its own token. A second client for the same owner may occupy the Gateway; close it before
connecting this device. The application does not silently take over another
client. It does not automatically approve Agent permission requests.

Microphone PCM is streamed only after the user enables it. The configured
Gateway and its voice provider process that audio. Conversation retention and
memory follow the backend settings. The device stores Wi-Fi credentials,
endpoint and access token in its independent `bean_online` NVS namespace;
it does not store recordings or model API keys. NVS is not flash-encrypted.
The simplified IP setup uses private-LAN `ws://` on port 3101. It does not
configure external WSS endpoints. Previously stored WSS endpoints remain
supported by the transport and need successful clock synchronization.

## Build and checks

Use ESP-IDF 5.5.3. No `FAP_APP` selector is needed.

```bash
python3 tools/generate_online_font.py --check
./tools/validate.sh --static
./tools/validate.sh
cmake -S tests/online_ui -B build_online_ui
cmake --build build_online_ui -j 8
mkdir -p build/preview
(cd build/preview && ../../build_online_ui/preview)
```

The complete gate produces `build/FoloToy-AI-Passport-full.bin`. It preserves
the 3 MB application limit, protected identity/Recovery offsets and five-second
UP bootloader hook. It does not flash the device. Host UI images use the real
LVGL renderer with synthetic status and are **not device captures**.

## Delivery status

This is an independent application. Continuous conversation and repeated microphone pause/resume still require device acceptance.
