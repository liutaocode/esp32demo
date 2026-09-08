[简体中文](published-source-review.zh_CN.md)

# Published source review: 2026-09-08

The official community API's 40 published projects define this source allowlist.
See the root [application index](../../../README.md) and the
[machine-readable catalog](../../../tools/published_apps.json). The snapshot has
31 root build targets and 9 standalone projects. These are reviewed local
sources, not a claim of byte-for-byte reproduction of community firmware.
Pocket Arcade has a published version and a newer draft revision.

- Build: PASS — ESP-IDF 5.5.3; all 40 targets passed compilation, merged-image
  verification and the BLE/Recovery contract checks. The largest application is
  `six-arts-museum` at 2750832 bytes, below the 3145728-byte limit.
- Host tests: PASS — the complete root gate and all 9 standalone project checks,
  including applicable font coverage, state, audio, UI and screenshot protocol
  tests. All 200 Pocket Arcade push-the-box levels are solvable.
- Device tests: NOT RUN.
- Unverified: device interaction, sound, networking and installation of this
  source snapshot. No device was connected, flashed or reset during this review.

The review fixed missing source-asset dependencies, a screenshot test targeting
an obsolete full-frame implementation, Python 3.9 annotation compatibility, and
a build-record script depending on a private review directory. Source whitespace
was normalized.

Staged files were checked for credentials, private keys, local user paths and
matches against the local publisher token. Build caches, device capture receipts
and private review records are excluded. These checks are not an absolute
assurance that no sensitive information exists. New commits use a GitHub noreply
identity, preserve upstream attribution and exclude the private local predecessor.

Observed failures and prevention steps are recorded in the
[archive skill](../../../skills/plays-archive/SKILL.md).
