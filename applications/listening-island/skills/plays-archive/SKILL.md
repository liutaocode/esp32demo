---
name: plays-archive
description: Archive published AI Passport applications, prepare privacy-reviewed source snapshots for the owner's GitHub branch, or contribute text-only plays summaries upstream. Use after community submission/publication when the developer requests source publication or archiving; do not publish firmware.
---

[简体中文](SKILL.zh_CN.md)

# Archive published applications

Choose the destination from the developer's request. Publishing source to the
owner's repository and contributing a text-only summary upstream are different
workflows. An explicit request to commit and push to the owner's main branch is
authorization for that destination; do not replace it with an upstream PR flow.

## Establish scope

1. Query the official publisher project's status and match its ID, title and
   source repository to the local implementation. A local folder, old submission
   JSON, Git commit or cover does not prove publication. Distinguish project
   publication from the latest revision: a published project can have a newer
   draft. Record uncertainty about the exact source revision instead of claiming
   a reproduced firmware. Include pending submissions only when requested.
2. Build an explicit application allowlist. Inventory independent repositories
   and Git worktrees as well as the main checkout. Exclude unrelated experiments.
3. Inspect Git status and remote refs. Use an isolated worktree for a mixed or
   dirty checkout, preserving existing user edits. Moving a worktree needs Git
   worktree-aware handling; a directory rename does not repair build caches.

## Owner's source repository

- Keep application logic, required fonts/audio, asset licenses, tests and build
  configuration together. Maintain a bilingual index with the community ID,
  source location and exact build selector or standalone build command.
- Copy dependencies from the build definition, then validate from the isolated
  candidate. A working original build can hide missing fonts or asset blobs.
  Include generator and test-manifest inputs (for example PCM/MP3 originals),
  even when firmware links only derived C data. Build scripts must create their
  ignored output directories rather than depend on a private review folder.
  Keep reusable BSP code separate from application code. Do not change runtime
  behavior simply to make an archive look smaller.
- Reject unknown selectors. A tag or directory name does not select firmware.
  Exercise every included build target and the host tests; report unsupported
  or failing targets explicitly. Preserve the BLE/Recovery contracts documented
  in `docs/development/engineering/ble-recovery-compatibility.md`.
- Before staging, inspect text, binary assets and the history that the push will
  expose. Check generated font comments and generator defaults for local paths;
  inspect commit author/committer addresses. Use a verified GitHub noreply identity
  for new commits. Preserve upstream attribution.
- Exclude tokens, credential stores, device QR secrets, capture receipts, serial
  identifiers, unsanitized logs and local publication requests. Firmware images
  and review media are not source dependencies. A `.bin` can instead be required
  ADPCM audio: classify by build usage, not extension alone.
- Inspect the actual staged blobs, not only the working directory. A successful
  secret-pattern scan is evidence for the patterns checked, not a guarantee.
  Do not echo suspected secret values into reports.
- Inspect ancestry before integration: merging a clean branch can reintroduce
  private metadata from another parent. If removing already-pushed history needs
  a non-fast-forward update, prepare and verify the clean candidate first, then
  obtain explicit authorization for that rewrite. Never force-push by default.
- Run host checks with the same supported Python environment used by ESP-IDF;
  a test that works in a newer system Python can fail on type annotations in
  the activated toolchain environment.
- Run `./tools/validate.sh`, commit only reviewed files, push the requested ref,
  and read back the remote commit ID. Do not claim a local commit was uploaded.
  Do not open an upstream PR unless requested.

## Upstream text-only archive

Only use this route when the developer requests upstream archiving. Base a
separate branch/worktree on upstream main and write paired summaries under
`plays/<username>/<app-name>/`. Include the community title and description,
verified interactions, submitted source URL and cover filename/format. Merge
relevant README facts into the summary. Do not include source code, firmware,
cover files or private publication records. Keep the fork owner's root README
out of upstream PRs. Push to the fork and open an upstream PR only when authorized.

## Reusable lessons and delivery

When asked to distill mistakes, use observed failures or explicit user corrections.
Write the failure, its consequence and a concrete prevention check. Do not copy
conversation transcripts, private paths, credentials or personal identifiers.
Do not turn a preference from one app into a universal product rule.

Report source scope, exclusions, destination branch and verified remote commit.
Report `Build`, `Host tests`, `Device tests` and `Unverified` separately. A build
or a historical screenshot is not device validation of a new source snapshot.
