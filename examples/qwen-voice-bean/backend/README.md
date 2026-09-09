# Qwen Audio Agent backend

[简体中文](README.zh_CN.md)

This directory contains reproducible installation and service helpers for
Qwen Voice Bean. They are separate from the firmware and run on a
computer/server. The service helper targets macOS/Linux with Python 3.9+.

## Install

Install Node 22.22.2+, 24.15.0+, or 26+, npm 10+, Git, and a supported backend
Agent. Run `./backend/install.sh` from the firmware project. It installs Qwen
Audio Agent at commit `4f27be30edd4d5a67059136008d13d4e5fc6cb5d` and Codex ACP
adapter 1.1.7 under `~/.local/share/mouthy-bean-online/`. This pins the source
used for Gateway protocol 7.0.0; it does not assume the npm release has the
same protocol as the development branch.

[Upstream quickstart](https://github.com/QwenAudio/qwen-audio-agent/blob/4f27be30edd4d5a67059136008d13d4e5fc6cb5d/docs/getting-started/quickstart.md)
and [Gateway protocol](https://github.com/QwenAudio/qwen-audio-agent/blob/4f27be30edd4d5a67059136008d13d4e5fc6cb5d/docs/gateway-protocol.md).

## Configure once

Create `~/.config/mouthy-bean-online/` with mode 700. Copy
`config.env.example` to `config.env` there and set mode 600. In that private file:

- Fill `DASHSCOPE_API_KEY` with your own active DashScope key. It is only for
  the voice provider; never paste it into the device configuration.
- Set `AGENT_PROTOCOL` to an installed and authenticated Agent. The included
  example uses `codex` and its existing user authentication/model; it does not
  change the user's Codex configuration. `native` retains the backend's
  permission handling. `none` is voice-only and cannot fulfill backend tasks.
- Generate `QWEN_AUDIO_GATEWAY_ACCESS_TOKEN` using `openssl rand -base64 32`.
  Keep the result private. This stays inside the backend in token-free mode. Enter it on the device only when required-token mode is enabled.
- Set `QWAUDIO_WORKSPACE` to an existing, dedicated directory and set
  `CODEX_PATH` and `CODEX_ACP_BIN` to the installed executables' absolute paths.
- Copy `ASSISTANT.zh_CN.md` beside the private configuration and set
  `QWEN_AUDIO_AGENT_ASSISTANT_PROFILE_PATH` to that copy. It supplies the bean's
  personality without replacing the Gateway's operating instructions.

`HOST=127.0.0.1` and `PORT=3102` keep the original Gateway local. The included
`device-gateway.mjs` listens on LAN port 3101 and forwards only the realtime
WebSocket and a minimal health route, adding the private upstream token.
Set `BEAN_DEVICE_REQUIRE_TOKEN=0` for token-free device access (the default),
or `1` to require the same token on the device. Token-free access is intended
only for a trusted private LAN: anyone who can reach this port can use the
voice and Agent service. Browser-origin WebSockets and management routes are
rejected at the device ingress. Do not forward port 3101 to the Internet.
On the device, enter only the computer IPv4 address. The firmware supplies
port 3101 and `/api/realtime` automatically.

## Run

```bash
python3 backend/service.py start
python3 backend/service.py status
python3 backend/service.py restart
python3 backend/service.py stop
```

Set `ONLINE_NODE=/absolute/path/to/node` if the default Node is too old. Service
configuration, PID and logs remain in the private configuration directory.
The helper starts a background process, not a login service. After rebooting
the computer, run `start` again. After changing the key or settings, `restart`.
An empty API key prevents startup; a nonempty but invalid key can still fail
when connecting the voice provider.

Open `http://127.0.0.1:3102` for the WebUI after startup. Verify voice there, then
close its conversation before connecting the device: only one client per
owner can be active. A healthy HTTP endpoint is not proof of successful model
calls, microphone playback, or delegated Agent execution.

## What has to stay running

The Gateway, voice provider access and backend Agent must be available. The
computer must stay awake and on the reachable network. Service fees, account
quotas and permission requests are controlled by your providers. Audio and
transcripts may be processed/stored by the Gateway and providers according to
their settings; this is not an offline application. Keep all private
configuration and logs out of the product package and Git.

The installer also adds an isolated Node 22.22.2 runtime for subsequent service starts. The service helper prefers it without changing the system Node installation.
