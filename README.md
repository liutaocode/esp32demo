[简体中文](README.zh_CN.md)

# ESP32 Demo — Submitted applications

Source, offline assets and host tests for 21 submitted applications. Each firmware runs one application. Vibe Check is the default.

## Build

Activate ESP-IDF 5.5.3, then run from the repository root:

```bash
./tools/validate.sh --static
FAP_APP=vibe_check ./tools/validate.sh
FAP_APP=minecraft_guide ./tools/validate.sh --firmware
```

Replace `FAP_APP` with a selector below. The output is `build/FoloToy-AI-Passport-full.bin`. Rebuild when switching applications.

Read the [BLE installation and Recovery contract](docs/development/engineering/ble-recovery-compatibility.md) before installation.

## Applications

| Selector | Application | Source |
| --- | --- | --- |
| `balloon_rush` | Balloon Rush | [source](main/apps/balloon_rush/balloon_rush.c) |
| `cloud_hop` | Cloud Hop | [source](main/apps/cloud_hop/cloud_hop.c) |
| `down_100` | Down 100 | [source](main/apps/down_100/down_100.c) |
| `focus_post` | Focus Post | [source](main/apps/focus_post/focus_post.c) |
| `fruit_merge` | One More Fruit | [source](main/apps/fruit_merge/fruit_merge.c) |
| `idiom_pet` | Idiom Pet: Read Stories, Hatch Friends | [source](main/apps/idiom_pet/idiom_pet.c) |
| `laoluo_quotes` | Lao Luo Quotes: Pocket Quote Radio | [source](main/apps/laoluo_quotes/laoluo_quotes.c) |
| `math_rail` | Math Rail | [source](main/apps/math_rail/math_rail.c) |
| `math_train` | Math Train | [source](main/apps/math_train/math_train.c) |
| `memory_garden` | Daily Memory | [source](main/apps/memory_garden/memory_garden.c) |
| `needle_rush` | Needle Rush | [source](main/apps/needle_rush/needle_rush.c) |
| `perfect_slice` | Perfect Slice | [source](main/apps/perfect_slice/perfect_slice.c) |
| `pocket_pond` | Pocket Pond | [source](main/apps/pocket_pond/pocket_pond.c) |
| `pvz_almanac` | Lawn Lab: A Plants vs. Zombies Field Guide | [source](main/apps/pvz_almanac/pvz_almanac.c) |
| `ricochet_rush` | One More Ricochet | [source](main/apps/ricochet_rush/ricochet_rush.c) |
| `social_battery` | Social Battery — You Decide When to Connect | [source](main/apps/social_battery/social_battery.c) |
| `stack_rush` | Stack Rush | [source](main/apps/stack_rush/stack_rush.c) |
| `tomato_bloom` | Tomato Bloom | [source](main/apps/tomato_bloom/tomato_bloom.c) |
| `vibe_check` | Vibe Check — Who Are You Today? | [source](main/apps/vibe_check/vibe_check.c) |
| `word_sprite` | Word Sprite — Listen, Learn, Grow | [source](main/apps/word_sprite/word_sprite.c) |
| `minecraft_guide` | Talking Minecraft Guide | [source](main/minecraft_guide.c) |

## Assets and validation

See [asset credits](assets/README.md) and the [documentation index](docs/README.md). A passing build does not establish device validation.

Based on [FoloToy AI Passport](https://github.com/FoloToy/ai-passport), retaining its license and attribution.
