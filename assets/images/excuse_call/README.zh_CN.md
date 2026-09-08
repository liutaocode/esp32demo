简体中文 | [English](README.md)

# 来电界面图标

`call.svg` 和 `person.svg` 来自 Google 的
[Material Design 图标仓库](https://github.com/google/material-design-icons)，
采用 Apache License 2.0 授权，请保留随附的 `LICENSE`。
源文件路径为 `src/communication/call/materialicons/24px.svg` 和
`src/social/person/materialicons/24px.svg`。

`tools/generate_excuse_call_icons.js` 使用 Node.js 和 sharp 重新着色并超采样，
生成带抗锯齿的 PNG 与 `excuse_call_icons.c`。
C 描述符按 LVGL 的 ARGB8888 格式保存 BGRA 像素。
听筒图标为 32 像素白色，联系人图标为 56 像素灰蓝色。
