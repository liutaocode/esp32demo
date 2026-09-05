<p align="right">
  <a href="README.zh_CN.md">Simplified Chinese</a> · <strong>English</strong>
</p>

# Daily Memory



Daily Memory is for people interested in preventing memory decline in middle
and older age. Its picture recall, sequence games, and family interaction
encourage a habit of everyday mental activity. There is no clinical evidence
that this app prevents or delays memory decline.

It is a calm, offline recall game designed for middle-aged and older adults and
the family members who play alongside them. Each of three rounds shows a short
sequence of clearly named garden cards. The player then recalls the cards in
order, growing a shareable pixel garden as they go.

The experience uses encouraging feedback instead of failure screens. A missed
card is shown again immediately, and the final card counts both blooms and new
shoots. It stores no answers or health data and needs no account, phone, or
network connection.

## Play



1. Press `OK` to begin a three-round session.
2. Watch the three, four, or five cards and say each name aloud.
3. During recall, use `UP` and `DOWN` to browse the six cards, then press `OK`
   to plant the selected card.
4. Photograph the final garden card, play again, or invite a family member to
   help remember the next sequence.

The state machine is hardware-independent and covered by
`tests/test_memory_garden_state.c`. Card timing uses an LVGL timer, so button
callbacks never block. The app shows battery status, dims after one minute of
inactivity, and turns off the backlight after three minutes.

Build this application variant with:

```bash
FAP_APP=memory_garden ./tools/validate.sh --firmware
```

## Health boundary

Daily Memory is a recreational cognitive activity, not a medical device,
screening tool, diagnosis, treatment, or guarantee that memory decline or
dementia can be prevented or delayed. A score should not be interpreted as a
measure of cognitive health.
Anyone concerned about new or worsening memory changes should consult a
qualified healthcare professional.
