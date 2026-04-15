---
name: render-verify
description: Guide for VDE's render verification system — golden-image comparison using NVIDIA FLIP. Use this when adding render verification to an example, running render-verify.ps1, or capturing and updating golden images.
---

# VDE Render Verification

Render verification is a separate tier above smoke testing. Smoke tests confirm that an example launches and exits cleanly; render verification confirms that what it draws on screen looks correct. It uses the [NVIDIA FLIP](https://github.com/NVlabs/flip) perceptual quality metric to compare rendered screenshots against committed golden images.

## When to use this skill

- Adding render verification to a new example
- Running `render-verify.ps1` and interpreting results
- Capturing or updating golden images with `-UpdateGolden`
- Debugging a FLIP comparison failure
- Understanding the path resolution rules for vdescript files

---

## Quick Reference

| Task | Command |
|------|---------|
| Capture golden images (all) | `.\scripts\render-verify.ps1 -UpdateGolden -Extended` |
| Capture one golden image | `.\scripts\render-verify.ps1 -UpdateGolden -Filter "*breakout*"` |
| Verify all (priority 1) | `.\scripts\render-verify.ps1` |
| Verify all including priority 2 | `.\scripts\render-verify.ps1 -Extended` |
| Verify one | `.\scripts\render-verify.ps1 -Filter "*breakout*"` |
| Verbose output | `.\scripts\render-verify.ps1 -Verbose` |
| Build first, then verify | `.\scripts\render-verify.ps1 -Build` |
| Via verify.ps1 (full suite) | `.\scripts\verify.ps1` (render verify is Stage 4) |
| Skip render verify in verify.ps1 | `.\scripts\verify.ps1 -SkipRenderVerify` |

---

## How It Works

1. **Discovery** — `render-verify.ps1` scans built example executables and reads `[render_verify]` sections from each example's `vde.toml`.
2. **Capture mode** (`-UpdateGolden`) — runs the example's `capture_script` (screenshot only; no compare step), then copies the output PNG to `smoketests/golden/`.
3. **Verify mode** (default) — runs the verify script, which takes a screenshot then runs `compare actual.png golden.png threshold`. FLIP measures perceptual error; failure exits with a non-zero code.

The FLIP metric is display-referred and sRGB-aware, tolerating minor GPU-to-GPU variation while catching meaningful regressions.

---

## Script and vde.toml Setup

### vde.toml `[render_verify]` section

```toml
[render_verify]
scripts = ["verify_my_demo.vdescript"]
capture_script = "capture_my_demo.vdescript"
priority = 1
golden = "my_demo.png"
threshold = 0.05
```

| Field | Required | Description |
|-------|----------|-------------|
| `scripts` | yes | Verify script filename (in `smoketests/scripts/`) |
| `capture_script` | yes | Capture-only script filename (no compare step) |
| `golden` | yes | Golden image filename (`smoketests/golden/<name>.png`) |
| `threshold` | yes | FLIP mean error threshold |
| `priority` | no | `1` = normal mode; `2` = only with `-Extended` |

**Threshold guidance:**

| Scene type | Threshold |
|------------|-----------|
| Static geometry / text | 0.03 |
| Standard 3D / sprites | 0.05 |
| UI overlays (ImGui) | 0.06 |

### Verify script (`smoketests/scripts/verify_<name>.vdescript`)

```vdescript
# Render verification: <description>
wait startup
wait_frames 30

screenshot render_verify_output/<name>.png
wait 100
compare render_verify_output/<name>.png ../../smoketests/golden/<name>.png 0.05

exit
```

### Capture script (`smoketests/scripts/capture_<name>.vdescript`)

Identical to verify except the `compare` line is omitted:

```vdescript
# Golden image capture: <description>
wait startup
wait_frames 30

screenshot render_verify_output/<name>.png
wait 100

exit
```

---

## Critical: Path Resolution

VDE examples call `setWorkingDirectoryToExecutablePath()` (or `vde::examples::setWorkingDirectoryToExecutablePath()` when called directly) **before** running the game loop. This changes the process working directory to the exe's own directory (e.g. `build_ninja/examples/`).

All relative paths in vdescript files resolve from this exe directory, **not** from the repo root and **not** from the PowerShell cwd.

| Path in vdescript | Resolves to |
|-------------------|-------------|
| `render_verify_output/<name>.png` | `build_ninja/examples/render_verify_output/<name>.png` |
| `../../smoketests/golden/<name>.png` | `<repo_root>/smoketests/golden/<name>.png` |

**Do not** use `smoketests/golden/<name>.png` in verify scripts — that resolves to `build_ninja/examples/smoketests/golden/` which does not exist.

### `configureInputScriptFromArgs` must be called before the cwd change

The script path CLI argument is resolved **before** the cwd change happens. This is handled automatically by `runExample()` / `runTool()`. If you write a custom `main()`, ensure you call `configureInputScriptFromArgs(game, argc, argv)` before `setWorkingDirectoryToExecutablePath()`.

### Custom `main()` examples must call `setWorkingDirectoryToExecutablePath()`

Examples with a custom `main()` that do not delegate to `runExample()` must explicitly call `vde::examples::setWorkingDirectoryToExecutablePath()` after `configureInputScriptFromArgs`, or their screenshot path will resolve from the PowerShell launch cwd instead of the exe directory — causing inconsistent behavior.

---

## Step-by-Step: Adding Render Verification to an Example

### Step 1 — Write the capture script

`smoketests/scripts/capture_<name>.vdescript`:
```vdescript
# Golden image capture: <name>
wait startup
wait_frames 30
screenshot render_verify_output/<name>.png
wait 100
exit
```

Use `wait_frames <N>` to reach a deterministic visual state before the screenshot. Prefer static or low-motion frames; dynamic scenes like physics simulations should be captured at frame 10 or earlier before simulation diverges.

### Step 2 — Write the verify script

`smoketests/scripts/verify_<name>.vdescript`:
```vdescript
# Render verification: <name>
wait startup
wait_frames 30
screenshot render_verify_output/<name>.png
wait 100
compare render_verify_output/<name>.png ../../smoketests/golden/<name>.png 0.05
exit
```

### Step 3 — Add `[render_verify]` to vde.toml

```toml
[render_verify]
scripts = ["verify_<name>.vdescript"]
capture_script = "capture_<name>.vdescript"
priority = 1
golden = "<name>.png"
threshold = 0.05
```

### Step 4 — Capture the golden image

```powershell
.\scripts\render-verify.ps1 -UpdateGolden -Filter "*<name>*" -Verbose
```

Check that `smoketests/golden/<name>.png` was created and looks correct.

### Step 5 — Verify the golden comparison passes

```powershell
.\scripts\render-verify.ps1 -Filter "*<name>*" -Verbose
```

Expect: `PASSED` with FLIP error below threshold.

---

## Running and Interpreting Results

### Output format

```
==========================================
Running render verification...
==========================================
  Verifying: vde_breakout_demo.exe with verify_breakout_demo.vdescript
  PASSED
  Verifying: vde_textured_cube_demo.exe with verify_textured_cube.vdescript
  PASSED

==========================================
Render Verification Summary
==========================================
Total: 8 (discovered: 8, skipped: 0)
Passed: 8

==========================================
All render verification tests PASSED!
==========================================
```

### Golden image not found

```
  FAILED — Golden image not found: my_demo.png - run with -UpdateGolden first.
```

Capture the golden first: `.\scripts\render-verify.ps1 -UpdateGolden -Filter "*my_demo*"`.

### Screenshot not found at expected path

```
  Screenshot not found at expected path: C:\...\build_ninja\examples\render_verify_output\my_demo.png
```

The capture script ran but the PNG was not created at the expected location. Possible causes:
- The exe exited before the screenshot command executed (script bug or timeout).
- The screenshot command executed with a cwd/path mismatch, so the PNG was written somewhere else than `build_ninja/examples/render_verify_output/`.
- Screenshot capture failed earlier in execution. `Game::captureScreenshot()` creates parent directories automatically, so a missing `render_verify_output/` directory usually means the capture step never ran or wrote to a different location.

Run the exe manually to diagnose:
```powershell
Set-Location build_ninja\examples
.\vde_my_demo.exe --input-script C:\...\smoketests\scripts\capture_my_demo.vdescript
```

### FLIP error exceeds threshold

```
  FAILED — FLIP mean error 0.087 exceeds threshold 0.05
```

Options:
1. **Raise the threshold** in `vde.toml` if the difference is acceptable (e.g. driver variation).
2. **Recapture the golden** with `-UpdateGolden` if the rendering intentionally changed.
3. **Fix the rendering regression** if this is an unintended change.

---

## Files and Directories

| Path | Purpose |
|------|---------|
| `smoketests/scripts/verify_<name>.vdescript` | Verify script (screenshot + compare) |
| `smoketests/scripts/capture_<name>.vdescript` | Capture script (screenshot only) |
| `smoketests/golden/<name>.png` | Committed golden reference image |
| `build_ninja/examples/render_verify_output/` | Runtime screenshot output (git-ignored) |
| `scripts/render-verify.ps1` | Render verification orchestrator |
| `third_party/flip/FLIP.h` | NVIDIA FLIP single-header library (v1.7, BSD-3) |

The `render_verify_output/` directory is added to `.gitignore` — do not commit runtime screenshots. Only commit files under `smoketests/golden/`.

---

## Integration with verify.ps1

`verify.ps1` runs render verification as Stage 4 after build, unit tests, and smoke tests:

```powershell
.\scripts\verify.ps1                   # All stages including render verify
.\scripts\verify.ps1 -SkipRenderVerify # Skip render verify (faster)
```

Render verify is skipped if no golden images exist for any example (all would be skipped anyway).
