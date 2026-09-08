[简体中文](README.zh_CN.md) | English

# Mouthy Bean

An offline, Mandarin-speaking yellow-face companion. Pat it, tease it, or talk
nearby: its ears rise when sound is detected, it shows a thinking prompt for a
random 1.2–3.2 seconds after you pause, then picks a preset response. Creative
inspiration: Xiaoshi Diary. This is an independently developed, unofficial work.

![Mouthy Bean cover](assets/images/mouthy-bean-cover.png)

## Controls

| Input | Face | Settings / expression collection |
| --- | --- | --- |
| UP click | Pat | Previous item |
| DOWN click | Tease | Next item |
| OK click | Request a response | Change, open or return |
| Hold OK | Open settings | Return to the face |

A double click counts as one interaction. Holding UP/DOWN does not repeat.
Three consecutive pats reveal a grin; five consecutive teases make it laugh;
alternating pats and teases reveals confusion. Actions in a combo must be less
than seven seconds apart. Eight expressions have discovery hints in the album.
There are no daily obligations or penalties for leaving it alone.

Forty original, pre-synthesized Mandarin lines play fully offline with matching
captions. This is a fixed dialogue bank, not runtime arbitrary-text TTS. The four
volume levels are mute / quiet / normal / loud (0/55/75/90%); normal is the default.
The earlier quiet default migrates once to normal, preserving mute. Discoveries
and preferences persist in a dedicated NVS namespace, with writes coalesced over
about two seconds. Idle remarks are spaced roughly 45–75 seconds apart. After
two minutes without interaction it says a sleep line, closes its eyes and dims.
A button or detected sound wakes it. Dimming is not measured low-power sleep.

## Sound and privacy

Sound activity lasting about 160 ms triggers listening; approximately one second
of quiet starts the randomized thinking pause. Sustained noise for 15 seconds
suppresses a reply until the room quiets. Background noise and music can trigger
it. It does not recognize words or use a language model. Playback is half-duplex:
voice barge-in is not supported, but buttons interrupt playback. A 600 ms buffer
drain and microphone suppression period follows playback to avoid self-triggering.

No microphone recording is saved or uploaded. The app makes no network
connections. Only expression discoveries and preferences are saved locally.
Audio failures retain the face and captions; storage failures leave the current
session playable and show a notice. No device identity, credentials, local logs,
serial receipts, or prebuilt firmware are included in this source package.

## Build

Clone the full repository: this example reuses the root BSP, screenshot service
and permanent-Recovery boot hook without modifying them. Activate ESP-IDF 5.5.3:

```bash
cd examples/mouthy-bean
./tools/validate.sh
```

The complete gate resolves pinned dependencies, runs host state/audio/UI checks,
builds in an isolated directory, and verifies the merged image. Output:
`build/FoloToy-AI-Passport-full.bin`. It never flashes a device. The 3 MB app limit,
identity region at `0x356000`, Recovery at `0x700000`, and five-second UP boot hook
remain intact. No root application selector is changed.

After dependencies have been resolved, run focused checks with:

```bash
python3 tools/test_mouthy_bean.py --ui
./tools/validate.sh --static
./tools/validate.sh --firmware
```

Fonts are reproducible with the pinned font converter in
`tools/generate_mouthy_bean_font.py` and the existing Source Han Sans SC font.
Speech packing is reproducible from the included WAV files with
`tools/generate_mouthy_bean_audio.py`; `--synthesize` additionally requires macOS
Tingting and ffmpeg. Ordinary builds need neither service nor speech synthesis.
Source Han Sans is covered by the repository's
[font license](assets/fonts/OFL-SourceHanSansSC.txt).

## Validation and release history

Community submission: project 220, revision 349, slug `community-a3ab31f7`.
The service returned `approved` when checked on 2026-09-08. This standalone
source packaging is based on version 1.0.1; it is not a claim of byte-identical
reproduction of the submitted binary.

Host checks cover all forty lines and eight faces, Chinese glyphs, clipping and
label overlap, random timing, combos, noise suppression, audio streaming and
cancellation, persistence, failure degradation and UI lifecycle. The original
firmware was installed and its startup and thinking prompt captured on hardware.
This newly packaged source still needs a separate device install for equivalence
verification. Acoustic thresholds, loudness, voice interruption limits, physical
buttons, power-cycle persistence, a thirty-minute soak and battery life remain
explicit device checks. Host previews are not device screenshots.
