[简体中文](cat-is-here.zh_CN.md)

# Cat Is Here: design and engineering

## Product scope

Version 0.1.2 is a quiet, single-room Chinese pet companion. Its central sequence
is call, approach, cuddle, linger, and nap. Twelve poses combine with breathing,
blinking, tail movement, alternate captions, three coats and three toys. Idle
behavior avoids consecutive identical choices. An occasional leaf gift happens
after roughly 1.5–3 minutes, then at 3–6-minute intervals, plus the remaining
animation time. Gifts and idle behavior never play unsolicited sound.

All customization is available immediately. Six memory flags mark the first call,
stroke, belly roll, play, nap, and leaf gift. They are a gentle record rather than
a score, quest list, bond meter, streak, or punishment system. No behavior depends
on a real-time clock, calendar, microphone, touch sensor or internet service.

Double Confirm in the room starts a 32-second dance; any button press stops it.
Single Confirm calls the cat and long Confirm opens the nest. Stop consumes the
whole gesture, including a second tap within 650 ms, to prevent a restart.
Three meows, three purr excerpts and three original toy cues form separate random
banks without consecutive repeats. The original 120 BPM groove loops every eight
seconds until the dance ends; mute applies to all sounds.

## Implementation

- `main/cat/cat_state.*`: hardware-independent poses, timing, seeded variation,
  captions, memory flags, versioned 32-byte profile and integrity validation.
- `main/cat/cat_control.*`: pure button routing, long/short/double distinction,
  menu timeout, dimming and consume-first-press wake behavior.
- `main/cat/cat_ui.*`: production LVGL renderer, shared pixel-theme frame,
  original procedural cat and room, fixed Chinese label boxes and menus.
- `main/cat/cat_sound.*`: bounded 16 kHz mono IMA ADPCM banks, CC0 cat recordings and original toy/dance cues, with fades, disjoint random groups and one pending repeat. Different groups switch after a 20 ms fade. The independent audio worker keeps PCM writes free of UI and storage delays.
- `main/main.c`: one permanent owner loop drains a bounded button queue. State
  mutation and rendering hold the LVGL lock; battery, audio, NVS and backlight
  operations occur outside it. Button callbacks only enqueue without waiting.
- `main/fap_screenshot.c`: the inherited read-only screenshot protocol now freezes
  its mirror while transmitting, so animations cannot tear a serial capture.

The screen exists for the entire application lifetime. UI host lifecycle tests
restore the previous screen before deleting a test screen. There are no app-owned
LVGL animation objects or timers accessing a deleted page.

## Storage and resource boundaries

The application uses only the `cat_here` namespace in ordinary NVS. It never
formats NVS on an initialization error and never writes identity or Recovery.
A profile contains appearance, choices, memory flags, last toy, a random seed,
and powered-on minutes. It does not contain recordings or personal identifiers.
A damaged or unknown profile is rejected. Settings and memories coalesce for
1.5 seconds, with a five-second maximum pending interval. Powered-on minutes
cause at most one additional save per minute; recent unsaved changes can be lost
on sudden power removal. On failure the current pet stays usable without saving.

The room is drawn with primitives, avoiding full-size animation bitmaps. Fonts
contain exactly the Chinese UI characters plus ASCII numerals and punctuation.
Audio uses a 640-byte streaming buffer in a separate worker. The screenshot mirror inherited from the
baseline consumes 153,600 bytes, so real minimum heap and rendering latency remain
mandatory device checks. The application binary stays inside the mandatory 3 MB
partition; identity at `0x356000`, Recovery at `0x700000` and the five-second UP
boot hook remain unchanged.

## Validation

`./tools/validate.sh` runs repository checks, native state and sound tests under
AddressSanitizer/UndefinedBehaviorSanitizer, font coverage, the production LVGL
host renderer, baseline host tests, and the ESP-IDF merged-image gate.

The UI check enumerates coats, poses, caption variants, animation times, all
names/toys/menu choices, all 64 memory masks, missing peripherals, and repeated
screen creation/destruction. It rejects Latin UI text, missing glyphs, clipped
labels, intersecting sibling labels, off-screen labels, and an occluded cat.
Host images deliberately carry a non-device-preview label.

Device acceptance and review sequencing are in the review package.
