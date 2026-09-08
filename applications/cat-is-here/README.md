[简体中文](README.zh_CN.md)

# Cat Is Here

A small cat that is happy to keep you company. Call it over, stroke its head,
or roll a toy into its room. It blinks, breathes, washes its face, stretches,
naps, and sometimes brings you a leaf. After a cuddle it stays close for a while.
There are no hunger bars, chores, penalties, or daily streaks.

Production renderer host preview, not a device capture

## Three buttons

| Button | In the room |
| --- | --- |
| Up | Stroke the cat. Three strokes within eight-second gaps invite a belly roll. |
| Confirm, short press | Call the cat over with one of three meows. |
| Confirm, double press | Start a 32-second dance with sunglasses and an original beat. Any button stops it. |
| Down | Play with the selected toy. |
| Confirm, long press | Open the nest; long press again to return. |

In the nest, Up/Down selects a row and Confirm changes the value. Choose one of
four names, three coat colors, three toys, gentle sound or silence, and desk or
portable use. The final row opens six small shared memories; any short button
press returns. Nothing needs to be unlocked before you can use it.

The interface is entirely Simplified Chinese. Confirm uses three meow variants, Up uses three purr recordings, and Down uses three original toy sounds. Each group randomly varies without consecutive repeats. Double Confirm adds an original dance groove; silence also mutes dancing. Sound starts only after interaction. There is no speech recognition,
TTS, microphone recording, account, or network connection.

## Quiet companionship

The display dims after one minute. Desk mode stays dimly visible; portable mode
turns the backlight off after three minutes. The first press wakes an extinguished
screen without performing an action. The nest returns home after 30 idle seconds.
Names, coat, toy, sound, mode and memories are saved locally, normally after 1.5
seconds of quiet and at most five seconds after a pending change. A power loss
before that save can lose the latest changes. A storage error leaves a usable
session and shows a Chinese notice. An unavailable battery displays an unknown
status rather than a fabricated number.

## Development and delivery

This is an isolated worktree on `feature/cat-is-here`. The parent workspace and
its other applications are unchanged. The baseline commit is recorded in
the review package. No install or publication is part of the
build commands below.

```bash
source "${IDF_PATH}/export.sh"
./tools/validate.sh
```

On another machine, activate an ESP-IDF 5.5.3 environment using the
[environment guide](docs/development/engineering/environment-setup.md).
For a clean checkout, run the firmware gate first to resolve locked dependencies,
then the complete gate. The UI host check uses that same locked LVGL source.

- [Design and implementation](docs/cat-is-here.md)
- Review materials and pending device acceptance
- Host interaction animation
- [Hardware and baseline documentation](docs/README.md)

The screenshots above come from the production renderer running on the host.
They are not evidence of device behavior. The application has now been installed
for the creator to try; see the actual device capture.
Boot and fresh capture passed. Version 0.1.2, with its final cover, is submitted and awaiting moderation; remaining device acceptance is listed in the review package.
