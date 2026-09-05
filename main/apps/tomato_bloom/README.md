<p align="right">
  <a href="README.zh_CN.md">Simplified Chinese</a> · <strong>English</strong>
</p>

# Tomato Bloom



Tomato Bloom turns a Pomodoro timer into a tiny focus garden. A seed grows into
a ripe tomato while the user works, and each completed session adds to a
shareable harvest card. The visual reward makes elapsed focus tangible without
adding accounts, feeds, notifications, or network dependencies.

The app offers 15-minute Quick Sprout, 25-minute Classic Bloom, and 45-minute
Deep Roots sessions. It schedules a five-minute break after ordinary sessions
and a 15-minute reset after every fourth harvest. Progress and harvest totals
last for the current power-on session; the first version intentionally writes
nothing to flash.

## See it in action



## Controls

- `UP` / `DOWN`: choose a focus duration on the setup screen.
- `OK`: start, pause, resume, or advance to the next phase.
- Hold `OK`: abandon the current timer or skip a break without losing harvested
  tomatoes.

The interface is fully localized in Simplified Chinese. The timer keeps running
while the view dims, so a quiet focus session stays accurate without demanding
attention.

Build this application variant with:

```bash
FAP_APP=tomato_bloom ./tools/validate.sh --firmware
```
