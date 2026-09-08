<p align="right"><strong>简体中文</strong> · <a href="README.md">English</a></p>

# 书架实现

操作、存储和构建说明见 [TTS 版指南](../../README.zh_CN.md)。

书架工作任务独占文件读取和阅读状态，音频任务独占 TTS，关闭翻页音效。
断句与朗读游标不依赖 ESP-IDF，由主机测试覆盖。
