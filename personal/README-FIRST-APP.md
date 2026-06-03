# Flipper0 — First App Development Guide

This guide walks through building, sideloading, and running your first `.fap` on Ooxele3 (RogueMaster RM0526). The toolchain is reproducible and protected against accidental firmware writes.

## Prerequisites

- Debian Linux host (or compatible)
- Python 3.8+ (Python 3.13 verified)
- USB connection to Ooxele3 at `/dev/ttyACM0`
- Personal fork cloned at `~/.local/srcs/gitclones/flipperzero-firmware-wPlugins/`

## One-time setup per clone

```bash
cd ~/.local/srcs/gitclones/flipperzero-firmware-wPlugins
# Install the pre-commit secret-scan hook for THIS clone
git config core.hooksPath personal/hooks
```

That single `git config` line points git at `personal/hooks/pre-commit`, which scans staged blobs for hardware UIDs, BLE MACs, GPG fingerprints, WiFi credentials, RSA pubkey bodies, region/provision blobs. Every clone of the personal fork needs this line; it lives in `.git/config` (local, not committed). If you clone fresh, re-run it.

## Activate the toolchain (per session)

```bash
cd ~/.local/srcs/gitclones/flipperzero-firmware-wPlugins
source personal/toolchain/ufbt-env.sh
```

This sources the venv, exports `UFBT_HOME`, and prepends `personal/toolchain/` to PATH so the `ufbt` wrapper intercepts firmware-write subcommands (`flash`, `update`, etc.).

Expected output:

```
ufbt-env: venv active; mode=local; SDK=RM0526 0e7e0395; UFBT_HOME=...
```

## Create a new app

```bash
mkdir personal/my_app
cd personal/my_app
ufbt create APPID=my_app
```

This drops a `application.fam`, `my_app.c`, `my_app.png` icon, `images/`, and a `.github/` CI scaffold into the current dir.

Edit `application.fam` and set `fap_category="Main"` (the only RM0526 device-side category guaranteed to exist by default; `Tools`, `Games`, etc. work too but `Main` is the conventional user-app location).

Edit `my_app.c` — at minimum, replace the stub with a real entry point. See `personal/hello_world/hello_world.c` for a ViewPort + draw/input callback template.

## Build the app

Create a `Makefile` (copy `personal/hello_world/Makefile`):

```makefile
FORK := $(HOME)/.local/srcs/gitclones/flipperzero-firmware-wPlugins
API_CHECK := $(FORK)/personal/toolchain/api_check.sh

.PHONY: api-check build launch
api-check:
	@bash $(API_CHECK)
build: api-check
	@ufbt build
launch: api-check
	@ufbt launch
```

Then:

```bash
make build
```

This runs `api_check.sh` first (compares SDK version vs cached device API; fails loudly on mismatch with literal `API_MISMATCH: SDK=X.Y device=A.B — re-pin SDK via fbt updater_package`), then `ufbt build`. Output: `.ufbt-rm0526/build/my_app.fap`.

If `api_check.sh` reports `API_MISMATCH`, re-pin the SDK:

```bash
cd ~/.local/srcs/gitclones/flipperzero-firmware-wPlugins
git checkout rm0526-pin   # or whatever commit matches the device
./fbt updater_package
ufbt update --local "$(ls dist/f7-*/flipper-z-f7-sdk-*.zip | head -1)" --hw-target=f7
git checkout personal/main
```

## Sideload + run on device

```bash
make launch
```

This runs `api_check.sh` again then `ufbt launch`, which:
1. Builds the `.fap` (cached if unchanged)
2. Opens a USB CDC RPC session to `/dev/ttyACM0`
3. Uploads `my_app.fap` to `/ext/apps/Main/my_app.fap`
4. Sends `loader open` — the app runs on the device

To exit the app, press BACK on the device. The host-side `flipper_cli.py loader close` does NOT force-kill a foreground FAP (Flipper firmware quirk; FAPs own their own exit handler).

## Capture device logs from the host

While an app runs on the device:

```bash
python3 ~/codeWS/Projects/Flipper0/.planning/tools/cli_log.py /tmp/my_app.log
```

Each device log line is captured with an ISO-8601 UTC microsecond timestamp. Ctrl-C to stop. `FLIPPER_PORT` env var overrides `/dev/ttyACM0`.

## What's blocked (and why)

The `ufbt` wrapper at `personal/toolchain/ufbt` blocks any subcommand starting with `flash` or `update`:

```
$ ufbt flash_usb
BLOCKED: firmware-write subcommand 'flash_usb' requires /goal gate. Use ufbt launch for app sideload.
```

This prevents the most common mistake — `ufbt flash_usb` looks like an app-sideload command but actually re-flashes the device with the OFW release firmware (this exact mistake regressed Ooxele3 from RM0526 to OFW 1.4.3 on 2026-06-03). Use `ufbt launch` (or `make launch`) for app sideload.

The host-side `.planning/tools/plan-firmware-write-gate.sh` provides a second layer: it scans plan documents (Markdown with `<action>` blocks and fenced bash) and refuses to execute any plan containing firmware-write triggers unless the same plan body echoes the backup-manifest SHA256 + a confirmed AskUserQuestion battery ≥95% gate.

## Committing changes

The pre-commit hook will scan staged blobs. If a real secret leaks in, you'll see:

```
PRE-COMMIT BLOCK [path]: <pattern category>
```

Fix the file or extend the exemption list in `personal/hooks/pre-commit`. Never use `--no-verify` on a real commit — the hook is the safety net that keeps device-specific identifiers (UID, BLE MAC, region keys) out of GitHub.

All commits must be GPG-signed by key `88081AEE10008045` (Klein Panic) — configured globally; do not pass `--no-gpg-sign`.

## Reference

- Personal fork repo: `~/.local/srcs/gitclones/flipperzero-firmware-wPlugins/`
- Project planning: `~/codeWS/Projects/Flipper0/.planning/`
- ufbt docs: https://github.com/flipperdevices/flipperzero-ufbt
- Phase 3 plans: `~/codeWS/Projects/Flipper0/.planning/phases/03-app-development-toolchain/`
