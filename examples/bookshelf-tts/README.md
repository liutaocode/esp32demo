<p align="right"><a href="README.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Pocket Bookshelf (TTS Edition)

A separate copy of Pocket Bookshelf with offline Chinese narration. Keep the
original Wi-Fi text upload, bookmarks, reading progress, statistics, three font
sizes and five themes, and listen from the current page. The 929,972-byte wideband
voice bank is embedded in the app; the books partition remains unchanged.

## Controls

- Shelf: Up/Down selects a book, OK opens it, and hold OK opens Wi-Fi transfer.
- Reading: Up/Down turns pages when narration is stopped. During narration or
  pause, Up increases volume and Down decreases it. OK opens the menu; hold OK
  returns to the shelf and stops speech.
- Menu: Start/Pause/Resume narration, Stop narration, Speech speed, Volume, Add bookmark,
  Bookmarks, Statistics, Font size, Theme, Return to shelf. Up/Down scrolls the
  list; OK selects; hold OK returns to reading.
- Pause/Resume repeats the current sentence so that an interrupted phrase is
  not lost. Reading continues across pages automatically. Speed changes pause
  speech; select Resume to continue at the new speed.
- Header and footer automatically hide during narration too. Volume changes
  briefly show the current volume before the bars hide again.
- Page-turn sound effects are disabled in this edition.
- Hold UP during boot for five seconds to enter permanent Recovery.

Custom UTF-8 `.txt` books use the same phone upload page as the original reader.
Speech is generated from book text, not prerecorded sentences. Long content is
split at punctuation or UTF-8 boundaries. Unsupported pronunciations are possible;
this is not a guarantee of arbitrary-language or rare-character coverage.
Initial setup adds three test books: a reading introduction, Tang poems and Song lyrics.
Upgrades add missing poetry samples while preserving existing content and reading progress.

## Build and storage

Use ESP-IDF 5.5.3 on the ESP32-C3 board with 8 MB Flash and no PSRAM:

```bash
source /path/to/esp-idf-v5.5.3/export.sh
./tools/validate.sh
idf.py -p PORT flash
```

The application remains below the 3 MiB installer limit. `books` stays at
`0x360000` with size `0x3A0000`; identity and Recovery addresses are unchanged.
Segmented USB flashing does not overwrite book data. As in the original reader,
a missing/unmountable filesystem is formatted on first mount. Back up existing
storage before migration. Builds and device backups are excluded from Git.

One task owns the speech codec. The book worker handles file reads and queues
sentences; LVGL only draws snapshots. Speech resources are released before Wi-Fi
transfer starts and recreated after transfer ends to preserve internal RAM.
The application runs as the sole foreground screen until restart.

## Source and licenses

Book functions were copied from the existing Pocket Bookshelf source. The TTS
port is derived from the tested Chinese TTS demo. Application glue is under the
repository license; retain the bundled ESP-TTS, Speex and font notices. Some
upstream speech-engine components are precompiled libraries.

Run host tests for pagination, filenames, menu transitions, sentence boundaries,
pause cursor and the voice decoder. Validate firmware size and protected regions
separately from device playback, page following, transfer and memory behavior.

## Volume controls

Select Volume in the reading menu and press OK to edit it. Up/Down adjusts
volume; OK or hold OK finishes editing. The menu shows the current percentage.
During narration, Up/Down displays a volume bar over the text for about 2.5
seconds; the bar and reading chrome then disappear automatically.
