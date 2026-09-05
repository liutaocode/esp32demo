<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Idiom Pet



Host-rendered LVGL screens, not device photographs.

An offline Chinese idiom adventure for independent elementary readers, roughly
ages 7–10 / grades 2–4. Read a short scenario, choose the idiom that fits, and
hatch an original pixel companion. There is no timer or speed ranking.

## Experience

- Three islands contain eight idioms each: fables, learning habits, and friendship.
- Each round has four distinct questions with three shuffled answer choices.
  Previously missed content comes first, then unseen content, then practiced content.
- Every answer opens an explanation that stays until OK is pressed. Mistakes return
  after the initial four questions with a different scenario. Incorrect retries do
  not count as learned: repeat until correct or return home whenever desired.
- Completing the round and its reviews unlocks a companion. Each island offers two
  companions in a fixed order, with no random rarity or purchase mechanic. Later
  rounds revisit the second companion. The result shows first-attempt accuracy,
  invites the child to explain an idiom aloud, and suggests a break.
- The six-entry album and learned/pending masks persist locally in NVS. Readiness
  and save failures are visible. Wait for the saved indication before powering off;
  the active round is not resumed, but saved mistakes are prioritized next time.

The launch hypothesis is that visible collection progress plus short, explainable
stories gives children something to share and parents a clear educational purpose.
It has not been validated with children, teachers, or market testing. Question copy
is original and is not a textbook-mapped curriculum or a measure of mastery.
Game-like learning draws inspiration from [Duolingo ABC](https://abc.duolingo.com/how-we-teach);
explanation and retrieval are informed by [Retrieval Practice](https://www.retrievalpractice.org/feedback/).
These references do not establish this application's educational effectiveness.

## Spoken explanations

Every wrong answer, including a review retry, automatically plays a Mandarin TTS
explanation: the correct idiom followed by its meaning. On the explanation screen,
UP replays it and DOWN stops it; OK continues and hold OK returns home, both
cancelling the old speech. Correct answers do not autoplay, but UP can still read
the explanation. The text remains available if audio initialization or playback
fails, with a visible audio-unavailable hint.

All 24 explanations are generated offline with the macOS Tingting voice at rate
190, resampled to 16 kHz mono, and embedded as IMA ADPCM. The pack is about 1.39 MB;
the worker decodes 512 samples (1 KB PCM) per chunk and owns all codec operations.
The worker has no UI references; leaving a page invalidates playback and queues a
stop without blocking LVGL. Regenerate after changing a meaning:

```bash
python3 tools/generate_idiom_pet_audio.py --synthesize
python3 tools/generate_idiom_pet_audio.py --check
```

WAVs and the exact transcripts, voice, sample counts and hashes are in
`assets/music/idiom_pet/`. Firmware does not require networking or a TTS service.

## Controls

| Screen | UP / DOWN | OK | Hold OK |
| --- | --- | --- | --- |
| Home | Choose island | Start four stories | Open album |
| Question | Choose answer | Submit | Return home |
| Explanation | Replay / stop TTS | Continue after reading | Return home |
| Hatch | No action | Open album | Return home |
| Album | Previous / next pet | Return home | Return home |

Idle for one minute to dim the display, or three minutes to turn the backlight off.
The first key after screen-off only wakes it. Battery is shown above the card;
an unavailable battery reading is shown as `--%`. Missing buttons leave a visible
error. No account, networking, or personal data is involved in this version.

## Implementation and build

`idiom_pet_state.c` contains hardware-independent progression and review logic;
`idiom_pet_catalog.c` is the single question source. `idiom_pet.c` draws the LVGL
interface and drains a static input queue. The button callback never takes the
LVGL lock or writes storage. The save worker owns copied snapshots and no UI
pointers; exit closes event acceptance and deletes both UI timers before the screen.
NVS uses only the `idiom_pet` namespace and `progress_v1` key; initialization errors
never erase other applications' data. Failed storage leaves a playable RAM session.

Activate ESP-IDF 5.5.3, then build this application without changing the default app:

```bash
FAP_APP=idiom_pet ./tools/validate.sh
```

The complete gate runs host tests and verifies the merged
`build/FoloToy-AI-Passport-full.bin`, including the 3 MB app limit and protected
identity/Recovery regions. Rebuild the font after changing Chinese strings:

```bash
python3 tools/generate_idiom_pet_font.py
```

## Device acceptance still required

1. Read the longest story and explanation; verify Chinese glyphs and all controls
   fit, and every button acts once. Hold OK from a question to return home.
2. Finish a round with mistakes; check changed-scenario reviews and pet unlocks.
   Listen to initial/review error explanations; verify UP replay, DOWN stop, OK
   and hold-OK cancellation, pronunciation, volume, clean endings and audio failure.
3. Wait for saved status, power-cycle, and confirm album and pending content remain.
4. Check dimming, screen-off, wake-only input, unavailable battery/storage handling,
   repeated entry/exit, and heap stability across 30 rounds.
5. Install the verified merged firmware through the mini-program and confirm the
   existing five-second UP Recovery path on provisioned hardware.

A host build or render does not establish full hardware acceptance. The later
installation verified the flash write, Chinese home-screen boot and serial
capture. Community project 157, revision 247 is pending review; see the
publication record. The remaining
device checks above have not been completed.
