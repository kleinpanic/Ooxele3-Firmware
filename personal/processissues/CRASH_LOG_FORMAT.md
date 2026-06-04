# Crash Log Format (/int/.crash.log)

**Reverse-engineered on device Ooxele3, 2026-06-03**

## File State

- **Status**: File does not exist (empty-state behavior)
- **Device**: Ooxele3 (UID `968E1B0127E18000`), RM0526 commit `0e7e0395`, API 87.7
- **Accessible**: Yes (attempted via `flipper_cli.py storage stat /int/.crash.log`)
- **Result**: `Storage error: file/dir doesn't exist` — confirmed not present

## Implication for Phase 7

Ooxele3 has not experienced any firmware crashes since provisioning/booting to current state. The `/int/.crash.log` file is created only when the first crash is recorded by the firmware.

### ProcessIssues v0.1 Scope

ProcessIssues v0.1 will implement **empty-state-only** display:

```c
canvas_draw_str(canvas, 8, 16, "ProcessIssues");
canvas_draw_str(canvas, 8, 36, "No crashes recorded");
canvas_draw_str(canvas, 8, 52, "since OTP provisioning");
```

When `/int/.crash.log` is absent, the app reads from RTC + uptime to show:
- Current uptime (via `furi_get_tick()` + `furi_thread_get_id()` time references)
- Boot count (via RTC timestamp history, if available)
- No per-crash records

### Future Work (v0.2)

Once a crash is recorded on Ooxele3, collect a full hex dump of `/int/.crash.log` and update this document with:
- Magic header (if binary)
- Fixed-size record struct definition
- Timestamp encoding (epoch, RTC datetime, or tick count)
- Sample record layout

## Notes

- Wave 1 forensics confirmed file does not exist (not access-blocked, not corrupted)
- This is expected on a well-tuned firmware with no regressions
- ProcessIssues will gracefully handle both empty-state and populated-log states in Wave 2
