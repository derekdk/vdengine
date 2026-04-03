---
name: ai-verification
description: Canonical guide for AI agents running build/test/smoke verification in VDE. Use this instead of manually chaining scripts or redirecting to temp files.
---

# AI Verification Workflow

This skill defines the exact commands AI agents should use to run verification and read results in VDE. It replaces the manual pattern of redirecting to `$env:TEMP` and calling `Get-Content`, which requires reading files outside the workspace and may trigger permission prompts.

## When to use this skill

- Running build + unit tests + smoke tests as part of completing a feature or fix
- Getting verification output into a readable file rather than relying on terminal streaming
- Understanding why `read_file` is preferred over terminal output for log inspection

## Core Problem This Solves

The old manual workflow used `$env:TEMP` for log capture:

```powershell
# OLD - requires workspace-external file access (triggers permission prompts)
$logFile = Join-Path $env:TEMP 'vde-verify.log'
& .\scripts\test.ps1 -ProblemsOnly *> $logFile
Get-Content $logFile -Tail 30
```

The new workflow writes to `logs/` inside the workspace:

```powershell
# NEW - log is workspace-relative; read_file works directly
.\scripts\verify.ps1
# Then use read_file on logs/verify-latest.log
```

---

## The Canonical Verification Command

```powershell
.\scripts\verify.ps1
```

This runs: **Build → Unit Tests → Smoke Tests** and writes all output to two files:

| File | Purpose |
|------|---------|
| `logs/verify-latest.log` | Always overwritten with the latest run |
| `logs/verify-YYYYMMDD-HHmmss.log` | Timestamped archive of each run |

**After running:** use `read_file` on `logs/verify-latest.log` (not terminal output) to see results. The file is inside the workspace so no permission prompts occur.

---

## Reading Results

After `verify.ps1` completes, read the log with `read_file`:

```
read_file("logs/verify-latest.log", startLine=1, endLine=100)
```

Navigate the log by looking for section markers:

```
============================================
  Stage: UNIT TESTS
============================================
... ProblemsOnly output ...
  STAGE RESULT: UNIT TESTS PASSED
============================================
```

The final summary is always at the bottom:

```
============================================
  VERIFICATION SUMMARY
============================================
  BUILD : PASSED
  UNIT TESTS : PASSED
  SMOKE TESTS : FAILED
  OVERALL: VERIFICATION FAILED
============================================
```

If the log is long, read the tail first (last ~80 lines) to see the summary, then read the relevant section for a failing stage.

---

## Common Patterns

### Full verification (all gates)

```powershell
.\scripts\verify.ps1
```

Then:
```
read_file("logs/verify-latest.log", startLine=1, endLine=60)   # summary at top
```
Or check just the summary:
```
read_file("logs/verify-latest.log", endLine=-1, startLine=-80) # last 80 lines
```

### Fast inner loop: single test suite, no build or smoke

```powershell
.\scripts\verify.ps1 -SkipBuild -SkipSmoke -Filter "EmojiFont*"
```

### After a code change: build + tests, skip slow smoke

```powershell
.\scripts\verify.ps1 -SkipSmoke
```

### Full verification targeting one smoke test

```powershell
.\scripts\verify.ps1 -SmokeFilter "*emoji*"
```

### Release configuration

```powershell
.\scripts\verify.ps1 -Config Release
```

---

## Script Parameters

| Parameter | Default | Description |
|----------|---------|-------------|
| `-SkipBuild` | — | Skip the build stage; use when code hasn't changed |
| `-SkipSmoke` | — | Skip smoke tests; faster iteration on unit test failures |
| `-Filter <pattern>` | `*` | GoogleTest filter for unit tests (e.g. `"EmojiFont*"`) |
| `-SmokeFilter <pattern>` | — | Exe wildcard for smoke tests (e.g. `"*emoji*"`) |
| `-Generator` | `Ninja` | `Ninja` or `MSBuild` |
| `-Config` | `Debug` | `Debug` or `Release` |

---

## Log Management

### List available logs

```powershell
.\scripts\show-log.ps1 -List
```

### View the latest log in the terminal

```powershell
.\scripts\show-log.ps1 -Tail 60
```

### View a specific archived log

```powershell
.\scripts\show-log.ps1 -Path logs/verify-20260403-153045.log
```

Note: for AI agents, using `read_file` on the `logs/` path directly is always preferred over running `show-log.ps1` in the terminal, because `read_file` bypasses terminal truncation entirely.

---

## When to Use Individual Scripts Instead

Use `verify.ps1` for final verification gates. Use individual scripts for interactive development:

| Situation | Command |
|-----------|---------|
| Iterating on a single failing test | `.\scripts\test.ps1 -Filter "Suite.Test"` |
| Watching build progress live (colored) | `.\scripts\build.ps1` |
| Running a specific smoke test interactively | `.\scripts\smoke-test.ps1 -Filter "*emoji*"` |
| Full gate after completing a feature | `.\scripts\verify.ps1` |

The individual scripts output to the terminal in real-time with colors. `verify.ps1` buffers output until each stage completes (needed for reliable log capture).

---

## Important Notes

- `verify.ps1` always passes `-ProblemsOnly` to `test.ps1` and `smoke-test.ps1`. Unit test pass/fail noise is suppressed; only warnings and failures appear in the log.
- Build output is not filtered — full CMake/Ninja output is in the log.
- If build fails, remaining stages are skipped automatically.
- The `logs/` directory is created automatically on first run; it is in `.gitignore`.

## References

- `build-tool-workflows` — individual script parameters and options
- `test-fix-loop` — tight red-green loop for a single failing test
- `completing-work` — mandatory verification gates before announcing completion
- `terminal-management` — general rules for running commands and avoiding truncation
