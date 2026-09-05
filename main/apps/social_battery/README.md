<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Social Battery



Public project introduction and publication draft.
The cover is a promotional illustration, not a device capture.


A pocket sign that says the quiet part for you. Wear it at a meetup or stand it
on your desk: choose Open to Chat, Going Slowly, Do Not Disturb, or Need a Hug.
Each card combines a large Chinese title, a pixel battery face, and gentle copy.
The hug card asks people to check permission first and offers sitting together
as an alternative. This is a communication tool, with no scores or game rounds.



This preview uses the application's actual LVGL code on a desktop; it is not a
photo of the device.

## Controls

| Screen | UP / DOWN | OK | Hold OK |
| --- | --- | --- | --- |
| Badge | Change card | Lock display | Open quiet-time setup |
| Locked badge | Ignored | Ignored | Unlock |
| Setup | Choose 5 / 15 / 30 minutes | Start | Return |
| Quiet time | Ignored | Pause / resume | End and return |
| Finished | Ignored | Return to previous card | Return to previous card |

The timer never changes your selected social status. Finishing quiet time does
not mean you are ready to talk. The pixel battery is a self-selected metaphor;
the top-right percentage is the actual device battery, or `--%` if unavailable.

Locked badges, quiet time (including pause), and the finished screen remain
visible at 35% backlight after one idle minute. Other screens turn off after
three idle minutes; the first press wakes the screen without changing the card.
Continuous display consumes power; battery life has not been measured.

## Build and implementation

With ESP-IDF 5.5.3 activated:

```bash
FAP_APP=social_battery ./tools/validate.sh
```

The app boots directly into its badge. The merged image is written to
`build/FoloToy-AI-Passport-full.bin`. Existing app selection remains available.

Everything runs offline without accounts, microphones, radios, or Flash writes.
Selections reset on restart. A bounded queue transfers button input to the
LVGL task without blocking the button callback. The state machine uses elapsed
monotonic time and is covered by `tests/test_social_battery_state.c`, including
pause boundaries, late callbacks, locking, and preservation of the chosen card.
Exit stops timers before deleting UI objects.

The UI uses project-authored LVGL shapes and two small Chinese font subsets.
Regenerate them with `python3 tools/generate_social_battery_fonts.py`; the
default source is LVGL's bundled Source Han Sans SC font under SIL OFL 1.1,
whose license is in `assets/fonts/OFL-NotoSansCJK.txt`.

Device acceptance still needs button/long-press checks, readable Chinese text,
lock and wake behavior, timer completion, battery fallback, and power measurement.

## Device capture



The saved firmware was installed and verified successfully. This fresh serial
capture confirms startup and Chinese rendering; physical-button behavior and
battery life still require device checks.
