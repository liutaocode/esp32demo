[English](README.md) | 简体中文

# 英语磨耳朵固件

全中文界面的离线英语磨耳朵应用。[应用说明](docs/listening-island.zh_CN.md)包含玩法、构建步骤、存储设计和验收项目。[基线文档索引](docs/README.zh_CN.md)保留硬件参考。

在本目录激活 ESP-IDF 5.5.3 后执行 `./tools/validate.sh`。完整校验生成 `build/FoloToy-AI-Passport-full.bin`，验证 BLE Recovery 兼容性及音频资源，并导出供后续授权安装的分段固件。

此候选版本已获授权并通过 USB 分段安装，启动及真实首页画面获取成功。合并固件的资源位于设备身份区域之后，不能在已有身份的设备上直接从零地址整包写入。
