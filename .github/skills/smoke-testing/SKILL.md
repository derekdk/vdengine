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

## Overview

Smoke tests verify that every VDE example and tool can launch, render, and exit cleanly. The system **auto-discovers** all `vde_*.exe` executables in the build directory — you never need to maintain a hardcoded list. Each executable runs with a `.vdescript` input script that automates startup, brief interaction, and clean exit via the `--input-script` CLI argument.

## Quick Reference

| Task | Command |
|------|---------|
| Run all smoke tests | `.\scripts\smoke-test.ps1` |
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
| `-Generator` | `MSBuild`, `Ninja` | `Ninja` | Which build system output to test |
| `-Config` | `Debug`, `Release` | `Debug` | Build configuration |
| `-Build` | switch | `$false` | Build the project before testing |
| `-Verbose` | switch | `$false` | Show detailed error output for failures |

## How Discovery Works

The script finds executables automatically:

- **Examples:** Scans `build_ninja/examples/` (Ninja) or `build/examples/<Config>/` (MSBuild) for `vde_*.exe` files
- **Tools:** Recursively scans `build_ninja/tools/` (Ninja) or `build/tools/` (MSBuild) for `vde_*.exe` files in subdirectories

An **exclude list** filters out executables that don't support the Game API input script system (e.g. `vde_triangle_example.exe`).

When a new example or tool is added and built, it is automatically discovered on the next smoke test run.

## Smoke Script Selection

Each executable is tested with a `.vdescript` input script:

1. The script checks a **smoke script map** (in `smoke-test.ps1`) for executable-specific scripts
2. If no specific script is mapped, it falls back to `smoke_quick.vdescript`

Smoke scripts live in `smoketests/scripts/` and follow the naming convention `smoke_<name>.vdescript`.

### Current smoke script map

| Executable | Script |
|-----------|--------|
| `vde_physics_demo.exe` | `smoke_physics_demo.vdescript` |
| `vde_breakout_demo.exe` | `smoke_breakout.vdescript` |
| `vde_asteroids_demo.exe` | `smoke_asteroids.vdescript` |
| `vde_asteroids_physics_demo.exe` | `smoke_asteroids_physics.vdescript` |
| `vde_sprite_demo.exe` | `smoke_sprite.vdescript` |
| `vde_multi_scene_demo.exe` | `smoke_multi_scene.vdescript` |
| `vde_imgui_demo.exe` | `smoke_imgui.vdescript` |
| `vde_audio_demo.exe` | `smoke_audio.vdescript` |
| `vde_materials_lighting_demo.exe` | `smoke_materials.vdescript` |
| `vde_resource_demo.exe` | `smoke_resource.vdescript` |
| `vde_vlauncher.exe` | `smoke_vlauncher.vdescript` |
| `vde_geometry_repl.exe` | `smoke_geometry_repl.vdescript` |
| All others | `smoke_quick.vdescript` (fallback) |

## Interpreting Results

### Output format

The script prints results as it runs:

```
==========================================
VDE Smoke Test Script
==========================================
...
Discovered 22 executable(s) to test:
  Examples: 20
  Tools:    2

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
Total: 22 (discovered: 22, skipped: 0)
  Examples: 20 run, 20 passed, 0 failed
  Tools: 2 run, 2 passed, 0 failed
Passed: 22

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

### Step 2 (optional): Create a custom smoke script

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

2. Add the mapping to `$smokeScriptMap` in `scripts/smoke-test.ps1`:

```powershell
$smokeScriptMap = @{
    ...
    'vde_my_new_tool.exe' = 'smoke_my_new_tool.vdescript'
}
```

### Step 3: Exclude an executable (rare)

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
