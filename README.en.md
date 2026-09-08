[简体中文](README.md)

# Published application sources

Sources for 40 published community applications: 31 root build selectors and 9 standalone projects. These are reviewed development snapshots, not byte-for-byte reproductions of community firmware. Pocket Arcade has a published version and a newer draft revision.

## Build

```bash
# ESP-IDF 5.5.3; resolve pinned dependencies on a fresh clone.
idf.py reconfigure
FAP_APP=vibe_check ./tools/validate.sh
# Standalone example
cd applications/suzhou-travel
idf.py reconfigure
./tools/validate.sh
```

## Applications

| Community ID | Application | Source / build selector |
| --- | --- | --- |
| 213 | Pocket Bookshelf | [ebook](main/apps/ebook) |
| 204 | Pocket Arcade | [pocket_arcade](main/apps/pocket_arcade) |
| 203 | Jelly Squeeze | [jelly_squeeze](main/apps/jelly_squeeze) |
| 201 | Clean Sweep — Falling Blocks on Three Keys | [clean_sweep](main/apps/clean_sweep) |
| 195 | Tally Click | [tally-click](applications/tally-click) |
| 194 | Excuse Call | [excuse_call](main/apps/excuse_call) |
| 193 | English Listening Practice | [listening-island](applications/listening-island) |
| 189 | Pocket Breach | [pocket_breach](main/apps/pocket_breach) |
| 188 | Cat Is Here | [cat-is-here](applications/cat-is-here) |
| 187 | Lane Leap | [lane_leap](main/apps/lane_leap) |
| 186 | Code Theater | [code_theater](main/apps/code_theater) |
| 183 | Haihunhou Museum Pocket Tour | [haihunhou-museum](applications/haihunhou-museum) |
| 182 | Sanxingdui Museum Pocket Tour | [sanxingdui-museum](applications/sanxingdui-museum) |
| 178 | Rhythm Agent / Listen, Tap, Unlock | [rhythm-agent](applications/rhythm-agent) |
| 177 | Night Call — Stay on the Line | [night-call](applications/night-call) |
| 176 | Deadline Station | [deadline_station](main/apps/deadline_station) |
| 173 | Pocket Hype — Bring Your Own Applause | [pocket_hype](main/apps/pocket_hype) |
| 172 | One More Ricochet | [ricochet_rush](main/apps/ricochet_rush) |
| 171 | Daily Memory | [memory_garden](main/apps/memory_garden) |
| 170 | One More Fruit | [fruit_merge](main/apps/fruit_merge) |
| 169 | Lawn Lab: A Plants vs. Zombies Field Guide | [pvz_almanac](main/apps/pvz_almanac) |
| 168 | Lao Luo Quotes: Pocket Quote Radio | [laoluo_quotes](main/apps/laoluo_quotes) |
| 167 | Focus Post | [focus_post](main/apps/focus_post) |
| 166 | Down 100 | [down_100](main/apps/down_100) |
| 164 | Needle Rush | [needle_rush](main/apps/needle_rush) |
| 162 | Word Sprite — Listen, Learn, Grow | [word_sprite](main/apps/word_sprite) |
| 161 | Balloon Rush | [just_seen](main/apps/just_seen) |
| 160 | Math Rail | [math_rail](main/apps/math_rail) |
| 159 | Pocket Pond | [pocket_pond](main/apps/pocket_pond) |
| 158 | Math Train | [math_train](main/apps/math_train) |
| 157 | Idiom Pet: Read Stories, Hatch Friends | [idiom_pet](main/apps/idiom_pet) |
| 156 | Vibe Check — Who Are You Today? | [vibe_check](main/apps/vibe_check) |
| 155 | Cloud Hop | [cloud_hop](main/apps/cloud_hop) |
| 154 | Perfect Slice | [perfect_slice](main/apps/perfect_slice) |
| 153 | Social Battery — You Decide When to Connect | [social_battery](main/apps/social_battery) |
| 152 | Stack Rush | [stack_rush](main/apps/stack_rush) |
| 151 | Tomato Bloom | [tomato_bloom](main/apps/tomato_bloom) |
| 150 | Suzhou Ten Sights Audio Guide | [suzhou-travel](applications/suzhou-travel) |
| 149 | Six Arts Museum Pocket Tour | [six-arts-museum](applications/six-arts-museum) |
| 148 | Talking Minecraft Guide | [minecraft_guide](main/minecraft_guide.c) |

Build, host tests, and device tests are reported separately. See the [documentation index](docs/README.md) and [archive skill](skills/plays-archive/SKILL.md).
