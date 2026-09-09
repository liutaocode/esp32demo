[简体中文](README.md)

# Applications and source code

The application list includes published applications with source links and build instructions. The original 40 applications include 31 root build selectors and 9 standalone projects. The added bookshelf TTS edition, reader and bean applications build from their own directories. These are reviewed development snapshots, not byte-for-byte reproductions of community firmware. Pocket Arcade has a published version and a newer draft revision.

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

Each entry lists its source link, build selection and main features. For `FAP_APP=name`, run `FAP_APP=name ./tools/validate.sh` at the repository root. For a standalone project marked `cd path`, enter that directory and run `idf.py reconfigure` followed by `./tools/validate.sh`. Activate ESP-IDF 5.5.3 first.

| Community ID | Application | Source / build selector |
| --- | --- | --- |
| 233 | Qwen Voice Bean · Published | [qwen-voice-bean](examples/qwen-voice-bean/README.md)<br>Standalone: `cd examples/qwen-voice-bean`<br>Network voice chat, Agent tasks, phone setup, button interruption |
| 222 | Pocket Bookshelf — TTS Edition · Published | [bookshelf-tts](examples/bookshelf-tts/README.md)<br>Standalone project: `cd examples/bookshelf-tts`<br>offline narration, three sample books and volume controls |
| 221 | Pocket Reader · Chinese TTS Demo · Published | [chinese-tts](examples/chinese-tts/README.md)<br>Standalone project: `cd examples/chinese-tts`<br>twelve examples, six speeds, custom-text API; approximately **930 KB** voice bank |
| 220 | Mouthy Bean | [mouthy-bean](examples/mouthy-bean/README.md)<br>Standalone project: `cd examples/mouthy-bean`<br>listening ears, randomized thinking, button interactions and eight expressions; inspired by Xiaoshi Diary |
| 213 | Pocket Bookshelf | [ebook](main/apps/ebook)<br>Root build: `FAP_APP=ebook`<br>Plain-text reading, phone uploads, bookmarks and reading progress |
| 204 | Pocket Arcade | [pocket_arcade](main/apps/pocket_arcade)<br>Root build: `FAP_APP=pocket_arcade`<br>32 mini-games, play instructions, scores and collectible stars |
| 203 | Jelly Squeeze | [jelly_squeeze](main/apps/jelly_squeeze)<br>Root build: `FAP_APP=jelly_squeeze`<br>Shape jelly to pass gates; two difficulties and collectible flavors |
| 201 | Clean Sweep — Falling Blocks on Three Keys | [clean_sweep](main/apps/clean_sweep)<br>Root build: `FAP_APP=clean_sweep`<br>Three-button falling blocks; endless play and a twenty-line sprint |
| 195 | Tally Click | [tally-click](applications/tally-click)<br>Standalone project: `cd applications/tally-click`<br>Increment/decrement counter, pause lock and ten recent records |
| 194 | Excuse Call | [excuse_call](main/apps/excuse_call)<br>Root build: `FAP_APP=excuse_call`<br>Simulated incoming calls, five ringtones and one-button start/stop |
| 193 | English Listening Practice | [listening-island](applications/listening-island)<br>Standalone project: `cd applications/listening-island`<br>English sentence playback, listening quizzes and mistake review |
| 189 | Pocket Breach | [pocket_breach](main/apps/pocket_breach)<br>Root build: `FAP_APP=pocket_breach`<br>First-person shooting, three maps and twelve enemy waves |
| 188 | Cat Is Here | [cat-is-here](applications/cat-is-here)<br>Standalone project: `cd applications/cat-is-here`<br>Pet and play with a cat; meows, purring, customization and memories |
| 187 | Lane Leap | [lane_leap](main/apps/lane_leap)<br>Root build: `FAP_APP=lane_leap`<br>Three-lane running, jumps, obstacles and coins |
| 186 | Code Theater | [code_theater](main/apps/code_theater)<br>Root build: `FAP_APP=code_theater`<br>Animated coding companion, simulated terminal states and button interactions |
| 183 | Haihunhou Museum Pocket Tour | [haihunhou-museum](applications/haihunhou-museum)<br>Standalone project: `cd applications/haihunhou-museum`<br>Haihunhou artifact illustrations with narrated stories and details |
| 182 | Sanxingdui Museum Pocket Tour | [sanxingdui-museum](applications/sanxingdui-museum)<br>Standalone project: `cd applications/sanxingdui-museum`<br>Sanxingdui artifact guide with page narration and detail views |
| 178 | Rhythm Agent / Listen, Tap, Unlock | [rhythm-agent](applications/rhythm-agent)<br>Standalone project: `cd applications/rhythm-agent`<br>Repeat a heard rhythm using buttons; training and challenge modes |
| 177 | Night Call — Stay on the Line | [night-call](applications/night-call)<br>Standalone project: `cd applications/night-call`<br>Voiced branching story with three calls, clues and multiple endings |
| 176 | Deadline Station | [deadline_station](main/apps/deadline_station)<br>Root build: `FAP_APP=deadline_station`<br>Conference deadline countdowns, favorites, checklists and focus timer |
| 173 | Pocket Hype — Bring Your Own Applause | [pocket_hype](main/apps/pocket_hype)<br>Root build: `FAP_APP=pocket_hype`<br>Scene-based voice lines and sound effects, surprise responses and quizzes |
| 172 | One More Ricochet | [ricochet_rush](main/apps/ricochet_rush)<br>Root build: `FAP_APP=ricochet_rush`<br>Aim bouncing balls at numbered bricks across thirty rounds |
| 171 | Daily Memory | [memory_garden](main/apps/memory_garden)<br>Root build: `FAP_APP=memory_garden`<br>Picture-sequence recall, three practice rounds and a garden result card |
| 170 | One More Fruit | [fruit_merge](main/apps/fruit_merge)<br>Root build: `FAP_APP=fruit_merge`<br>Merge adjacent matching fruits, preview the next fruit and undo once |
| 169 | Lawn Lab: A Plants vs. Zombies Field Guide | [pvz_almanac](main/apps/pvz_almanac)<br>Root build: `FAP_APP=pvz_almanac`<br>Talking plant and zombie guide with five-question clue challenges |
| 168 | Lao Luo Quotes: Pocket Quote Radio | [laoluo_quotes](main/apps/laoluo_quotes)<br>Root build: `FAP_APP=laoluo_quotes`<br>Twelve quote cards read with a neutral synthetic voice |
| 167 | Focus Post | [focus_post](main/apps/focus_post)<br>Root build: `FAP_APP=focus_post`<br>Deliver mail to the target animal; observation and waiting practice |
| 166 | Down 100 | [down_100](main/apps/down_100)<br>Root build: `FAP_APP=down_100`<br>Descend platforms, avoid spikes, collect gems and build combos |
| 164 | Needle Rush | [needle_rush](main/apps/needle_rush)<br>Root build: `FAP_APP=needle_rush`<br>Launch pins into rotating discs, avoid obstacles and clear ten stages |
| 162 | Word Sprite — Listen, Learn, Grow | [word_sprite](main/apps/word_sprite)<br>Root build: `FAP_APP=word_sprite`<br>Hear English, choose Chinese meanings, review mistakes and grow a sprite |
| 161 | Balloon Rush | [just_seen](main/apps/just_seen)<br>Root build: `FAP_APP=just_seen`<br>Timed balloon inflation; risk another round or bank the points |
| 160 | Math Rail | [math_rail](main/apps/math_rail)<br>Root build: `FAP_APP=math_rail`<br>Random arithmetic, two-button answers, mistake practice and station tickets |
| 159 | Pocket Pond | [pocket_pond](main/apps/pocket_pond)<br>Root build: `FAP_APP=pocket_pond`<br>Catch or bank fish; probability hints, collection and medals |
| 158 | Math Train | [math_train](main/apps/math_train)<br>Root build: `FAP_APP=math_train`<br>Four arithmetic levels, answer feedback and mistake review |
| 157 | Idiom Pet: Read Stories, Hatch Friends | [idiom_pet](main/apps/idiom_pet)<br>Root build: `FAP_APP=idiom_pet`<br>Idiom story quizzes, spoken explanations and collectible pets |
| 156 | Vibe Check — Who Are You Today? | [vibe_check](main/apps/vibe_check)<br>Root build: `FAP_APP=vibe_check`<br>Five binary-choice questions, narration and playful personality cards |
| 155 | Cloud Hop | [cloud_hop](main/apps/cloud_hop)<br>Root build: `FAP_APP=cloud_hop`<br>Floating-island jumps, precision combos and same-course rematches |
| 154 | Perfect Slice | [perfect_slice](main/apps/perfect_slice)<br>Root build: `FAP_APP=perfect_slice`<br>Cut cakes to target proportions; two difficulties and ten scored cuts |
| 153 | Social Battery — You Decide When to Connect | [social_battery](main/apps/social_battery)<br>Root build: `FAP_APP=social_battery`<br>Four social status cards, display lock and quiet-time sessions |
| 152 | Stack Rush | [stack_rush](main/apps/stack_rush)<br>Root build: `FAP_APP=stack_rush`<br>Align stacked floors, regain width with precision and reach fifty floors |
| 151 | Tomato Bloom | [tomato_bloom](main/apps/tomato_bloom)<br>Root build: `FAP_APP=tomato_bloom`<br>Focus timer, growing tomatoes, breaks and harvest statistics |
| 150 | Suzhou Ten Sights Audio Guide | [suzhou-travel](applications/suzhou-travel)<br>Standalone project: `cd applications/suzhou-travel`<br>Illustrated and narrated guide to ten Suzhou sights |
| 149 | Six Arts Museum Pocket Tour | [six-arts-museum](applications/six-arts-museum)<br>Standalone project: `cd applications/six-arts-museum`<br>Museum exhibits with narrated stories and detail views |
| 148 | Talking Minecraft Guide | [minecraft_guide](main/minecraft_guide.c)<br>Root build: `FAP_APP=minecraft_guide`<br>Twenty illustrated characters and items with automatic page narration |

Build, host tests, and device tests are reported separately. See the [documentation index](docs/README.md) and [archive skill](skills/plays-archive/SKILL.md).
