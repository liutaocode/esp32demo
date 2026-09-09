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
   computer IPv4 address. The device fills port 3101 and `/api/realtime`: private LAN addresses use `ws://`, other IPv4 addresses use `wss://`.
   The access-token field is optional and hidden unless enabled.
3. After saving and rebooting, wait for the ready state. Press OK to enable the
   microphone. Speak naturally and pause for the backend to answer.
4. The home screen shows volume at the bottom right, explicitly showing mute at 0%. Press OK to close the microphone, UP to cycle volume, DOWN to interrupt a
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
Public endpoints need a trusted TLS certificate valid for the entered IP, a reachable port 3101, and backend authentication. Deploy that HTTPS/WebSocket termination separately; the bundled device ingress itself serves local HTTP/WebSocket. TLS connections wait for successful clock synchronization. Never embed personal endpoints or credentials in shared firmware.

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

This independent application is published in the community. Recent updates fix issues found during repeated testing: heartbeat handling, audio buffering, transport recovery, UI memory pressure and volume visibility. Recovery, pause/resume and visual checks passed in diagnostic hardware testing; an intermittent network send timeout remains unresolved. Silent tests do not certify audible quality or every deployment.

The client is open source at [liutaocode/esp32demo](https://github.com/liutaocode/esp32demo), under `examples/qwen-voice-bean`. For backend deployment, follow [Qwen Audio Agent](https://github.com/QwenAudio/qwen-audio-agent).

Relay regression test (Node.js 22, a backend checkout with dependencies installed):

```bash
QWEN_AUDIO_RUNTIME=/path/to/qwen-audio-agent node tests/test_device_gateway.mjs
```
