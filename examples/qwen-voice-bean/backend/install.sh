#!/usr/bin/env bash
set -euo pipefail
runtime="${ONLINE_RUNTIME:-$HOME/.local/share/mouthy-bean-online/qwen-audio-agent}"
revision=4f27be30edd4d5a67059136008d13d4e5fc6cb5d
node -e 'const [a,b,c]=process.versions.node.split(".").map(Number);if(!((a===22&&(b>22||(b===22&&c>=2)))||(a===24&&(b>15||(b===15&&c>=0)))||a>=26))throw Error("Use Node 22.22.2+, 24.15.0+, or 26+")'
if [[ ! -d "$runtime/.git" ]]; then
  git clone https://github.com/QwenAudio/qwen-audio-agent.git "$runtime"
  git -C "$runtime" checkout --detach "$revision"
fi
if [[ "$(git -C "$runtime" rev-parse HEAD)" != "$revision" ]]; then
  echo 'Existing runtime differs from the pinned revision; choose a fresh ONLINE_RUNTIME directory.' >&2
  exit 1
fi
(cd "$runtime" && ELECTRON_SKIP_BINARY_DOWNLOAD=1 npm ci --no-audit --no-fund)
npm install --prefix "$(dirname "$runtime")/adapter" --no-audit --no-fund @agentclientprotocol/codex-acp@1.1.7
npm install --prefix "$(dirname "$runtime")/runtime" --no-audit --no-fund node@22.22.2
echo 'Installed. Create the private configuration described in README, then run service.py start.'
