# Qwen 语音豆

[English](README.md)

黄色大眼睛「嘴硬小豆」的独立联网版，使用 **Qwen Audio Agent** 进行真实语音对话，并可把任务交给后台 Agent。原离线应用保留在原项目里。本工程开机直接进入联网小豆，应用代码独立放在 `main/`，不依赖之前的 `main/apps` 目录。

## 使用

1. 按[后端部署说明](backend/README.zh_CN.md)安装和启动后端。
2. 首次开机，设备先扫描附近 Wi-Fi，再启动热点。用手机连接屏幕显示的热点（随机 8 位数字密码），打开 `http://192.168.4.1`，从列表选择 2.4 GHz Wi-Fi，输入其密码和后端电脑 IPv4 地址。设备自动补齐端口 3101 和 `/api/realtime`：私有局域网地址使用 `ws://`，其他 IPv4 地址使用 `wss://`；默认无需填写令牌，需要时勾选后显示令牌输入框。隐藏网络可手动输入名称。
3. 保存后设备重启。显示准备好了后，按确定开启麦克风，自然说话并停顿，等待后端回答。
4. 首页右下角显示当前音量，0% 明确显示“静音”。确定关闭麦克风，上键循环音量，下键打断回答，长按确定进入设置，查看按键提示和当前配置；选择重新配置后需再次确认。

麦克风默认关闭；回答时暂停上传，可按下键手动打断。本版不包含自动语音打断或回声消除。保留原来的大眼睛、竖耳和张嘴动画，改为跟随真实对话状态。屏幕显示中文状态提示，不显示任意模型回答字幕。

## 后端与隐私

电脑或服务器需要持续运行，并能被设备访问。Gateway 已部署、语音服务可用、后台 Agent 配置完成，是三个不同条件。模型费用与 Agent 能力取决于用户配置。随附的局域网设备入口支持免令牌或强制令牌模式。免令牌时，同一局域网中的其他人也可访问语音和 Agent 服务，因此只适合可信私有网络。原生 Gateway 保持仅本机监听，并保留内部令牌。同一用户的后端可能被其他客户端占用，请先关闭它们；设备不会自动抢占连接，也不会自动批准 Agent 的授权请求。

只有开启麦克风后才流式上传语音，由配置的 Gateway 及其语音服务处理。对话留存和记忆行为取决于后端设置。设备在独立的 `bean_online` NVS 命名空间保存 Wi-Fi、后端地址和访问令牌，不保存录音或模型 API Key。当前 NVS 没有启用 Flash 加密。公网 IP 配置使用加密连接，需要适用于输入 IP 的可信证书和后端认证；TLS 校验需成功同步时间。

## 构建与检查

使用 ESP-IDF 5.5.3，不需要设置 `FAP_APP`。

```bash
python3 tools/generate_online_font.py --check
./tools/validate.sh --static
./tools/validate.sh
cmake -S tests/online_ui -B build_online_ui
cmake --build build_online_ui -j 8
mkdir -p build/preview
(cd build/preview && ../../build_online_ui/preview)
```

完整验证生成 `build/FoloToy-AI-Passport-full.bin`，保留 3 MB 应用限制、设备身份区和永久 Recovery 固定地址，以及上键五秒启动钩子，不会自动安装到设备。电脑预览使用真实 LVGL 渲染代码及模拟状态，**不是真机截图**。

## 验证状态

本独立应用已在社区上架。构建与主机测试通过不代表所有部署环境的硬件验收。本示例不包含自动语音打断。部署和配置说明见 [后端指南](backend/README.zh_CN.md)。

## 近期更新

首页右下角显示当前音量，0% 明确显示“静音”。近期修复了多轮测试发现的问题，涉及心跳处理、音频缓冲、传输恢复、界面内存占用及音量提示。诊断实机的恢复、暂停/续聊与界面检查通过；仍有偶发网络发送超时未完全解决。静音测试不等于实际音质或所有部署环境的验收。

客户端已开源至 [liutaocode/esp32demo](https://github.com/liutaocode/esp32demo)，目录为 `examples/qwen-voice-bean`。服务端仍需参考 [Qwen Audio Agent](https://github.com/QwenAudio/qwen-audio-agent) 自行部署。

输入公网 IPv4 地址会自动使用 `wss://IP:3101/api/realtime`，私有局域网地址使用 `ws://`。公网后端需要单独配置适用于该 IP 的受信任 TLS 证书、3101 端口入口与访问认证；随附入口本身提供本地 HTTP/WebSocket，不自动部署公网 TLS。TLS 连接等待时间同步成功。共享固件不得预置个人后端地址或凭据。

接入回归测试需要 Node.js 22 及已安装依赖的后端源码：

```bash
QWEN_AUDIO_RUNTIME=/path/to/qwen-audio-agent node tests/test_device_gateway.mjs
```
