<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Book reader implementation

See the [TTS edition guide](../../README.md) for controls, storage and build instructions.

The book worker owns file reads and reading state. The audio worker owns TTS;
page-turn sounds are disabled. Sentence splitting and cursor handling are
independent of ESP-IDF and covered by host tests.
