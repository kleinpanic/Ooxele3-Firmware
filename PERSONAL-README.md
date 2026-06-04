# Personal Fork — kleinpanic/flipperzero-firmware-wPlugins

Personal customization fork of `RogueMaster/flipperzero-firmware-wPlugins` for **Ooxele3** (Flipper Zero hw v12, region 4).

## Branch Model

| Branch | Tracks | Purpose |
|---|---|---|
| `420` (default — inherited from upstream) | upstream/420 | Mirror of RogueMaster upstream. **Do not commit user customizations here.** |
| `personal/main` | branched off local `420` | All user customizations. Sync with upstream via merge from `420`. |
| `feature/<name>` | branched off `personal/main` | Feature work; merge back to `personal/main` when done. |

## Remotes

```
origin    git@github.com:kleinpanic/flipperzero-firmware-wPlugins.git (this fork)
upstream  https://github.com/RogueMaster/flipperzero-firmware-wPlugins.git
```

## Sync Cadence

**Weekly minimum.** RogueMaster pushes ~46 commits/day on `420`; falling behind monthly creates large merge conflicts in binary assets.

```bash
git fetch upstream
git checkout 420 && git merge --ff-only upstream/420
git checkout personal/main && git merge --no-ff 420
git push origin 420 personal/main
```

**Always merge, never rebase** — RogueMaster has binary asset files (animations, IR/NFC dumps); rebase corrupts them per `.planning/research/PITFALLS.md`.

## License

GPL-3.0 (inherited from upstream). Do NOT remove the LICENSE file or any GPL notices from individual source files. This is a hard project constraint per FORK-08.

## Project Linkage

This fork is managed via the broader Flipper0 project at:
`~/codeWS/Projects/Flipper0/.planning/`

Phase 1 of that project created this fork on 2026-06-02 after flashing
RogueMaster RM0526-0321-0.420.0-0e7e039 to Ooxele3 (see `phase-1-complete` tag).
