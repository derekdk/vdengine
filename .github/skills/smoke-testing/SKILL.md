```skill
---
name: smoke-testing
description: Guide for running smoke tests in VDE and interpreting the results. Use this when running, debugging, or extending automated smoke tests for examples and tools.
---

# VDE Smoke Testing

This skill describes how to run smoke tests, interpret their results, add smoke tests for new executables, and troubleshoot failures.

## When to use this skill

- Running smoke tests for examples, tools, or both
- Interpreting smoke test output and diagnosing failures
- Adding a smoke test for a new example or tool
- Filtering smoke tests to run a subset
- Troubleshooting a failing smoke test

## Required: Smoke tests are long-running

Smoke tests are not a quick command. A full run usually takes 2-4 minutes because the script launches every discovered example and tool one at a time.

### Preferred approach for AI agents: use verify.ps1

Rather than running `smoke-test.ps1` directly and polling terminal output, AI agents should use `verify.ps1`, which captures all output to a workspace-relative log file that can be read with `read_file` without permission prompts:

```powershell
.\scripts\verify.ps1
# Then use read_file on logs/verify-latest.log
```

`verify.ps1` runs Build → Unit Tests → Smoke Tests and writes to:

| File | Purpose |
|------|---------|
| `logs/verify-latest.log` | Always overwritten with the latest run |
| `logs/verify-YYYYMMDD-HHmmss.log` | Timestamped archive |

Read the log after it completes:
```
read_file("logs/verify-latest.log", startLine=1, endLine=80)
```

The smoke stage result appears in the final summary at the bottom of the log:
```
  SMOKE TESTS : PASSED
  OVERALL: ALL STAGES PASSED
```

See the `ai-verification` skill for the full workflow, log management commands, and `verify.ps1` parameters.

### When running smoke-test.ps1 directly

If you must run `smoke-test.ps1` directly (e.g. for an interactive filtered run):

- Expect partial output first and **poll for more output** until the run finishes
- Do not announce success or failure until you see the final summary block

The run is only complete when you see one of these final markers:

- `All smoke tests PASSED!`
- `SMOKE TESTS FAILED`

For broader rules about long-running commands, timeouts, and truncated output, also consult the `terminal-management` skill.

## Overview

Smoke tests verify that every VDE example and tool can launch, render, and exit cleanly. The system **auto-discovers** all `vde_*.exe` executables in the build directory — you never need to maintain a hardcoded list. Each executable runs with a `.vdescript` input script that automates startup, brief interaction, and clean exit via the `--input-script` CLI argument.

## Quick Reference

| Task | Command |
|------|---------|
| Run priority 1 smoke tests | `.\scripts\smoke-test.ps1` |
| Run all (priority 1 + 2) | `.\scripts\smoke-test.ps1 -Extended` |
| Examples only | `.\scripts\smoke-test.ps1 -Category Examples` |
| Tools only | `.\scripts\smoke-test.ps1 -Category Tools` |
| Filter by name | `.\scripts\smoke-test.ps1 -Filter "*physics*"` |
| Build first | `.\scripts\smoke-test.ps1 -Build` |
| Verbose output | `.\scripts\smoke-test.ps1 -Verbose` |
| Build + smoke test | `.\scripts\smoke-test.ps1 -Build -Verbose` |
| VS Code task | Run Task → `scripts: smoke-test` |

## Parameters

| Parameter | Values | Default | Description |
|-----------|--------|---------|-------------|
| `-Category` | `All`, `Examples`, `Tools` | `All` | Which category of executables to test |
| `-Filter` | Wildcard pattern | (none) | Filter executable names (e.g. `"*physics*"`, `"vde_vlauncher*"`) |
| `-Extended` | switch | `$false` | Include priority 2 examples (default run only tests priority 1) |
| `-Generator` | `MSBuild`, `Ninja` | `Ninja` | Which build system output to test |
| `-Config` | `Debug`, `Release` | `Debug` | Build configuration |
| `-Build` | switch | `$false` | Build the project before testing |
| `-Verbose` | switch | `$false` | Show detailed error output for failures |

## Smoke Priority Model

Each example declares a **smoke priority** (1 or 2) in its `vde.toml` file:

- **Priority 1** — Core examples that cover unique API subsystems. Run by default in every smoke test invocation, `verify.ps1`, and CI. The priority-1 set is chosen so that every canonical API section (`core`, `entity`, `resource`, `input`, `camera`, `lighting`, `physics`, `audio`, `multi_scene`, `transitions`, `text`, `ui`, `storage`, `world_bounds`) is covered by at least one priority-1 example.
- **Priority 2** — Extended examples that provide additional coverage or showcase variations of already-covered features. Only included when `-Extended` is passed.

Tools always run regardless of priority.

### When to use `-Extended`

- Before merging a feature branch that touches many subsystems
- When investigating a failure that only reproduces in less-common examples
- Periodic full-coverage CI runs

### Assigning priority to a new example

When adding a new example, set its priority in `examples/<name>/vde.toml`:

```toml
[smoke]
scripts = ["smoke_my_demo.vdescript"]
priority = 1
sections = ["entity", "input"]
```

Use **priority 1** if the example is the **only** (or primary) smoke coverage for a canonical API section. Use **priority 2** if other priority-1 examples already cover the same sections.

## How Discovery Works

The script finds executables automatically:

- **Examples:** Scans `build_ninja/examples/` (Ninja) or `build/examples/<Config>/` (MSBuild) for `vde_*.exe` files
- **Tools:** Recursively scans `build_ninja/tools/` (Ninja) or `build/tools/` (MSBuild) for `vde_*.exe` files in subdirectories

An **exclude list** filters out executables that don't support the Game API input script system (e.g. `vde_triangle_example.exe`).

When a new example or tool is added and built, it is automatically discovered on the next smoke test run.

## Smoke Script Selection

Each example's smoke script and priority are read from its `vde.toml` file under the `[smoke]` or `[smoke.<targetName>]` section. Tool smoke scripts are mapped explicitly in `smoke-test.ps1`. If no script is specified, the fallback is `smoke_quick.vdescript`.

Smoke scripts live in `smoketests/scripts/` and follow the naming convention `smoke_<name>.vdescript`.

### Tool smoke script map

Tools still use an explicit mapping in `smoke-test.ps1`:

| Executable | Script |
|-----------|--------|
| `vde_vlauncher.exe` | `smoke_vlauncher.vdescript` |
| `vde_geometry_repl.exe` | `smoke_geometry_repl.vdescript` |
| `vde_resource_editor.exe` | `smoke_resource_editor.vdescript` |
| All others | `smoke_quick.vdescript` (fallback) |

## Interpreting Results

### Output format

The script prints results as it runs. Early lines are not the final result; keep polling until the summary block appears:

```
==========================================
VDE Smoke Test Script
==========================================
...
Smoke Set: Normal (priority 1 examples only)
Selected 18 executable(s) to test (from 35 discovered):
  Examples: 15
  Tools:    3
  Priority 2 examples excluded: 17

Running smoke tests...
==========================================

--- Examples ---
  Testing: vde_asteroids_demo.exe PASSED
  Testing: vde_breakout_demo.exe PASSED
  ...

--- Tools ---
  Testing: vde_geometry_repl.exe PASSED
  Testing: vde_vlauncher.exe PASSED

==========================================
Smoke Test Summary
==========================================
Total: 18 (discovered: 35, skipped: 0)
  Examples: 15 run, 15 passed, 0 failed
  Tools: 3 run, 3 passed, 0 failed
Passed: 18

==========================================
All smoke tests PASSED!
==========================================
```

### Result statuses

| Status | Meaning |
|--------|---------|
| **PASSED** | Executable exited with code 0 within the timeout |
| **FAILED (exit code: N)** | Executable exited with a non-zero exit code |
| **FAILED (exit code: timeout)** | Executable did not exit within 12 seconds |
| **skipped** | Smoke script not found or executable not in build output |

### Exit codes

| Script exit code | Meaning |
|------------------|---------|
| `0` | All tests passed |
| `1` | One or more tests failed, or no executables found |

### Verbose mode

Use `-Verbose` to see:
- Which smoke script each executable uses
- Error output lines (filtered to lines containing error/warning/assert/validation/failed/exception/fatal keywords)
- Per-failure error details in the summary

## Adding a Smoke Test for a New Executable

### Step 1: Build and verify auto-discovery

After adding a new example or tool and building, run:

```powershell
.\scripts\smoke-test.ps1
```

The new executable will be auto-discovered and tested with `smoke_quick.vdescript`.

### Step 2: Add vde.toml metadata

Create or update `examples/<name>/vde.toml` with the smoke section:

```toml
[smoke]
scripts = ["smoke_my_demo.vdescript"]
priority = 1
sections = ["entity", "input"]
```

- Set **priority = 1** if this is the primary (or only) smoke coverage for its API sections.
- Set **priority = 2** if other priority-1 examples already cover the same sections.
- Use the canonical section identifiers from `API-DOC.md`.

For tools, add the mapping to `$toolSmokeScriptMap` in `scripts/smoke-test.ps1` instead.

### Step 3 (optional): Create a custom smoke script

If the executable needs specific interaction beyond "launch and exit":

1. Create `smoketests/scripts/smoke_<name>.vdescript`:

```vdescript
# Smoke test for <name>
# Tests that <name> launches and key features work
wait startup
wait 1s

# Test specific interactions
press SPACE
wait 500

wait 2s
exit
```

### Step 4: Exclude an executable (rare)

If an executable should never be smoke-tested (e.g. it doesn't use the Game API):

```powershell
$excludeList = @(
    'vde_triangle_example.exe'
    'vde_my_special_exe.exe'    # Reason for exclusion
)
```

## Smoke Script Quick Reference

Smoke scripts use the `.vdescript` format. See the **scripted-input** skill for the full command reference.

### Minimal smoke script (smoke_quick.vdescript)

```vdescript
# Quick smoke test — shortest for fast CI validation
wait startup
wait 1000
exit
```

### Typical custom smoke script

```vdescript
# Smoke test for <name>
# Tests <what it tests>
wait startup
wait 1s

# Interact with the application
press SPACE
wait 500
press F1
wait 500
click 400 300
wait 300

wait 2s
exit
```

### Key patterns

- Always start with `wait startup` to ensure the first frame renders
- Use `wait <ms>` or `wait <N>s` for delays between actions
- Always end with `exit` for clean shutdown
- Keep total duration under **10 seconds** (12-second timeout)
- Test key interactions but don't exhaustively test all features

## Troubleshooting

### Test times out

- The smoke script takes too long. Reduce `wait` durations.
- The executable is stuck. Run it manually with `--input-script` to observe behavior.
- Ensure the script ends with `exit`.

### Test fails to start

- Executable not found in the build directory. Rebuild with `.\scripts\build.ps1`.
- Missing DLLs or shaders. Ensure the build copied all dependencies.

### Test exits with non-zero code

Run with `-Verbose` to see error output:

```powershell
.\scripts\smoke-test.ps1 -Filter "vde_failing_example*" -Verbose
```

Then run the executable manually to reproduce:

```powershell
.\build_ninja\examples\vde_failing_example.exe --input-script smoketests\scripts\smoke_quick.vdescript
```

### Vulkan validation errors in output

Validation layer warnings appear in key output lines. These may not cause a non-zero exit code but should be investigated. Use `-Verbose` to see them.

### New executable not discovered

- Ensure it was built (check build output).
- Ensure the executable name starts with `vde_`.
- Ensure it's not in the `$excludeList`.
- Check the build directory matches the `-Generator` parameter.

### Environment contamination

The script clears `VDE_INPUT_SCRIPT` to prevent stale environment variable contamination. If you set this variable manually in your shell, it will be cleared for the duration of the smoke test run.
```
