[简体中文](README.zh_CN.md) | English

# Night Call

A pocket suspense story told through three connected late-night phone calls.
You are the only person still answering. Guide a stranded passenger, recover a
recording from an unlit archive, and find out why the final caller has your number.

## Play

- Short UP/DOWN: select one of two responses. Short OK: commit the response.
- Long UP: replay the current Mandarin dialogue.
- Long DOWN: toggle mute. All dialogue is also shown in Chinese subtitles.
- Long OK during a call: pause; resume or return to the duty room.
- In the duty room, UP/DOWN cycles through resume, new call, archive, sound and help.
- After three idle minutes and once speech ends, the screen turns off. The first
  short or long button gesture only wakes it; it does not select a response.

There are three chapters, 22 conversation scenes, six collectible clues and seven
endings, including a finale that requires completing both earlier true rescues.
All chapters are available from the start. Choices are not timed. Routes can loop
back to revisit evidence. Endings offer discoveries to pursue on the next call.
The archive hides unreached endings rather than spoiling them.

Each response queues a save. Wait until the saving notice disappears before
switching off power. The latest call, clues, endings and volume survive restart
after a successful save. Starting another chapter replaces the active call;
collected clues and endings remain. A write/load error stays visible. No other
application's NVS namespace is erased.

The UI is entirely Chinese. Twenty-nine locally generated Mandarin TTS clips are
embedded for offline playback. Responses use physical buttons; no speech
recognition, microphone recording, accounts or networking are needed.

## Build and review

This independent worktree is on `feature/night-call`. The selected firmware is
Night Call by default; no application selector is required. Other applications
in the parent working directory are unaffected.

Use ESP-IDF 5.5.3 and follow [the environment guide](docs/development/engineering/environment-setup.md).
Run `./tools/validate.sh` for repository checks, host logic/audio/storage tests,
production LVGL rendering checks, and the verified merged image.

- [Architecture and limits](main/apps/night_call/README.md)
- Review materials and submission evidence
- Offline visual/audio review
- [Changelog](docs/CHANGELOG.md)

The verified firmware has been installed and submitted to the official community
review queue as project 177, revision 267 (pending). Actual USB screenshots and
the final 3:4 cover are in the review package. Boot and repeated capture checks
passed; the package lists the remaining manual hardware checks.
