# Qwen Audio Agent 后端

[English](README.md)

这里提供「Qwen 语音豆」可复现的安装与启动工具。后端独立于固件，在电脑或服务器上运行。服务脚本适用于 macOS/Linux，要求 Python 3.9+。

## 安装

准备 Node 22.22.2+、24.15.0+ 或 26+，npm 10+、Git，以及一个支持的后台 Agent。在固件项目目录运行 `./backend/install.sh`。脚本将 Qwen Audio Agent 提交 `4f27be30edd4d5a67059136008d13d4e5fc6cb5d` 和 Codex ACP 适配器 1.1.7 安装到 `~/.local/share/mouthy-bean-online/`。这份代码对应 Gateway 协议 7.0.0，不假定 npm 发布版与开发分支具有相同协议。

[上游快速开始](https://github.com/QwenAudio/qwen-audio-agent/blob/4f27be30edd4d5a67059136008d13d4e5fc6cb5d/docs/getting-started/quickstart.zh.md)及[Gateway 协议](https://github.com/QwenAudio/qwen-audio-agent/blob/4f27be30edd4d5a67059136008d13d4e5fc6cb5d/docs/gateway-protocol.zh.md)。

## 首次配置

创建权限为 700 的 `~/.config/mouthy-bean-online/`，将 `config.env.example` 复制为其中的 `config.env`，文件权限设为 600。在这个私有文件中：

- 填入你自己的有效 `DASHSCOPE_API_KEY`。它只用于语音服务，不能填到设备配网页。
- 将 `AGENT_PROTOCOL` 设为已经安装并登录的 Agent。示例使用 `codex`，沿用已有用户认证和模型，不修改用户的 Codex 配置；`native` 保留后台权限处理。`none` 是纯语音前台模式，不能完成后台任务。
- 用 `openssl rand -base64 32` 生成 `QWEN_AUDIO_GATEWAY_ACCESS_TOKEN` 并私密保存；免令牌模式下它仅在后端内部使用；强制令牌模式才需要填写到设备。
- 将 `QWAUDIO_WORKSPACE` 设为一个已创建的独立工作目录，将 `CODEX_PATH`、`CODEX_ACP_BIN` 设为已安装程序的绝对路径。
- 把 `ASSISTANT.zh_CN.md` 复制到私有配置目录，将 `QWEN_AUDIO_AGENT_ASSISTANT_PROFILE_PATH` 指向它。这份文件提供小豆人设，不替换 Gateway 的运行规则。

设置 `HOST=127.0.0.1`、`PORT=3102`，原生 Gateway 仅供本机访问。随附的 `device-gateway.mjs` 监听局域网端口 3101，只转发实时对话 WebSocket 和简化的健康检查，自动补上内部访问令牌。

`BEAN_DEVICE_REQUIRE_TOKEN=0` 为默认免令牌模式；设为 `1` 则要求设备填写同一个令牌。免令牌只适合可信私有局域网：能访问这个端口的人也能使用语音及 Agent 服务。设备入口拒绝带浏览器 Origin 的 WebSocket 和管理接口，不要把 3101 端口转发到公网。设备上只填写电脑 IPv4 地址，端口 3101 和 `/api/realtime` 自动补齐。

## 运行

```bash
python3 backend/service.py start
python3 backend/service.py status
python3 backend/service.py restart
python3 backend/service.py stop
```

默认 Node 过旧时，可设置 `ONLINE_NODE=/node的绝对路径`。配置、PID 和日志只保存在私有配置目录。脚本启动的是后台进程，不是登录启动服务；电脑重启后需再次运行 `start`。改动密钥或配置后运行 `restart`。API Key 为空会阻止启动；不为空但无效的 Key 仍可能在连接语音服务时失败。

启动后打开 `http://127.0.0.1:3102` 进入 WebUI。先在浏览器验证语音，再关闭浏览器对话，让设备接入；同一用户只允许一个活动客户端。HTTP 健康检查成功不代表模型调用、麦克风播放或后台 Agent 执行成功。

## 需要保持运行的条件

Gateway、语音服务和后台 Agent 均需可用。电脑要保持唤醒并连接可达网络。费用、额度和授权由相应服务控制。音频与转写可能由 Gateway 及服务商按配置处理或保存；这不是离线应用。私有配置和日志不要放入产品包或 Git。

安装器还会安装独立的 Node 22.22.2 运行时，后续服务启动优先使用它，不修改系统 Node。

## 设备流式传输稳定性

设备入口按有界小包及播放速度发送音频，同时持续读取上游心跳。队列上限 8 MiB；静音/取消会丢弃待播放音频。这些改进不保证所有网络都无中断。公网部署需提供适用于输入 IP 的可信 TLS 证书并要求设备令牌；不要将免令牌模式暴露到公网。
