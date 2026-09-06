<p align="right">
  <a href="PREPARATION.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Clean Sweep submission preparation

![Clean Sweep community cover](../../assets/images/clean_sweep/community-cover.png)

The material is ready and the firmware is installed on the device and confirmed to boot. **Nothing has been submitted** to the community.

![Clean Sweep device home page](../../assets/images/clean_sweep/runtime-home.png)

760768 bytes were written from `0x0`, ending at `0xB9BC0` — before the protected `cardid` (`0x356000`) and Recovery (`0x700000`), neither of which was touched, and no erase-flash was run. The written data verified, and the boot log shows buttons and the fuel gauge ready. The image above is the device framebuffer read back over USB; captures of a round in progress and of the clear animation are still missing.

## Public fields awaiting confirmation

**English title:** Clean Sweep — Falling Blocks on Three Keys

**English description:**

Turn AI Passport into a pocket block machine. Three keys are enough: up moves left, down moves right, a single click on OK rotates, and a double click drops the piece to the floor.

A ghost outline always marks where the piece will land, and a touched-down piece still has half a second of grace for one more nudge. Every cleared line flashes white and bursts apart while the rows above collapse into place; a four-line clear shakes the well, and a banner shows what the clear was worth.

Two modes: endless classic speeds up as you clear and ends when the stack reaches the top, and a twenty-line sprint measures how fast you can finish. Consecutive clears add a combo bonus, and emptying the board adds a perfect-clear bonus.

The result page shows a puzzle number; the same number always produces the same piece order, so sharing it sets up a fair rematch. The interface is Simplified Chinese, fully offline, with no account, no network, and no voice. Take a break for your eyes between rounds.

The Chinese title and description are in [简体中文](PREPARATION.zh_CN.md). The single structured draft of every field is [publication.json](publication.json), where `confirmed` and `upload` are both `false`.

**Public repository:** [esp32demo](https://github.com/liutaocode/esp32demo). The application entry point is [README.md](README.md). The new local directories are not pushed yet.

## Files and checksums

| Item | File | Bytes | SHA-256 |
| --- | --- | --- | --- |
| Merged firmware | `../../build/clean-sweep/FoloToy-AI-Passport-full.bin` | 760768 | `1827741062b74fdadb7f6704493cffe4966c16de8ad3d3b6bec3a92cefba7389` |
| Community cover | `../../assets/images/clean_sweep/community-cover.png` | 88324 | `82c415bb5ba421e3f211d40bfef35c43602dc27d78480e9c2b392230209f1989` |
| Device home capture | `../../assets/images/clean_sweep/runtime-home.png` | 5397 | `05b59eff6eb8c11d521fd7f3a3daf37b87bacc05517aebfb6c1345acd69bd65c` |

- Cover: 1152 × 1536, portrait 3:4, composed by [tools/render_clean_sweep_cover.py](../../tools/render_clean_sweep_cover.py) around the real LVGL host render, so it promises nothing beyond what the render shows.
- Interface screenshots: [assets/images/clean_sweep](../../assets/images/clean_sweep), all produced by `tools/test_clean_sweep_ui.py` running the real application code.
- Application size 695232 bytes; the mini-program BLE install contract (3 MB limit, `cardid`, Recovery partition, five-second UP-key hook) passes.
- Partition table, protected ranges, and per-segment verification of the merged image pass.

## Next steps

1. Capture a device screen during play; the serial protocol cannot press the buttons, so someone has to play on the board.
2. Developer confirms the bilingual title and description.
3. Check the site authorization, preview every field, and submit only after explicit approval.

Build: PASS (`FAP_APP=clean_sweep ./tools/validate.sh --firmware`)
Host tests: PASS (state-machine regression and per-frame LVGL host UI checks)
Device tests: PARTIAL (flash verification, boot log, home-page framebuffer; no button acceptance run)
Unverified: on-device press latency and feel, animation smoothness, real screen colors, sustained operation, community moderation.

## Submission result

Submitted successfully; the official status is **pending** moderation. Project 201, revision 298, slug `community-4e55d1d4`.

Pre-upload checks: the official helper validated the firmware, cover and serial capture (three-segment image, exact 1152 × 1536 portrait 3:4, fresh framebuffer with its receipt). Listing the account's 36 existing projects confirmed no duplicate title, so this went in as a new project rather than a revision. The publisher ZIP's SHA-256 is `d5a246365f498b9518d0c94c6e308c7e5df155f8d05893d654da8ee2ff5529a9`, matching the official ZIP recorded for this repository's previous submission. Authorization reused the still-valid creator token; the helper never handles a password.
