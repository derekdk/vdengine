# FLIP Integration Plan

## Summary

Replace the RMSE metric in VDE's `compare` command with NVIDIA FLIP (ꟻLIP), a perceptual image difference evaluator that models human vision. This makes golden-image comparison tolerant of minor GPU/driver differences while still catching bugs humans would notice.

## Why FLIP over RMSE

| Metric | Driver noise | Catches real bugs | Anti-aliasing tolerance |
|--------|:---:|:---:|:---:|
| RMSE (current) | Poor — flags harmless rounding differences | Yes | Poor |
| FLIP | Good — models human contrast sensitivity | Yes | Good |

FLIP computes a per-pixel error (0 = identical, 1 = maximally different) weighted by two perceptual pipelines:
- **Color pipeline** — models human color sensitivity using Hunt-adjusted CIELab, spatial filtering at the viewing PPD
- **Feature pipeline** — detects edge/structure differences using Gaussian derivative filters

The mean FLIP score is the single scalar used for pass/fail comparison against a threshold.

## Integration Approach: Embed FLIP.h in Engine

FLIP v1.7 ships as a **single header** (`FLIP.h`, ~3000 lines, BSD-3 license). Embed it in `third_party/flip/` and call it from `handleCompare()`.

### Why embed, not external tool

- No new build dependency or external install
- Comparison runs in-process — no subprocess overhead, no file I/O for intermediate results
- Same `compare actual.png golden.png 0.05` syntax works unchanged
- Can optionally save the FLIP error heatmap for debugging

## Implementation Steps

### Step 1: Add FLIP header to third_party

```
third_party/
  flip/
    FLIP.h          ← single header from NVlabs/flip v1.7
    LICENSE          ← BSD-3-Clause license file
```

Fetch from: `https://github.com/NVlabs/flip` tag v1.7 (commit `b475eb4`).

No CMake changes needed — `FLIP.h` is included directly by `InputScriptExecutor.cpp`.

### Step 2: Replace RMSE with FLIP in handleCompare()

Current implementation (`InputScriptExecutor.cpp`):
```cpp
double computeImageRmse(const unsigned char* imageA, const unsigned char* imageB, size_t sampleCount);
// Used in handleCompare() to produce a single 0-1 score
```

New implementation:

```cpp
#include "FLIP.h"  // third_party/flip/FLIP.h

double computeFlipMeanError(const unsigned char* imageA, const unsigned char* imageB,
                            int width, int height) {
    // Convert sRGB uint8 → linear RGB float (FLIP expects 3-channel linear RGB in [0,1])
    const int pixelCount = width * height;
    std::vector<float> refLinear(pixelCount * 3);
    std::vector<float> testLinear(pixelCount * 3);
    
    for (int i = 0; i < pixelCount; ++i) {
        // stb_image loads as RGBA; take RGB, convert sRGB→linear
        for (int c = 0; c < 3; ++c) {
            float srgb = static_cast<float>(imageA[i * 4 + c]) / 255.0f;
            refLinear[i * 3 + c] = FLIP::color3::sRGBToLinearRGB(srgb);
            
            srgb = static_cast<float>(imageB[i * 4 + c]) / 255.0f;
            testLinear[i * 3 + c] = FLIP::color3::sRGBToLinearRGB(srgb);
        }
    }
    
    FLIP::Parameters params;
    // Default PPD assumes 0.7m viewing distance, 3840px width, 0.7m monitor
    // For render verification at 800x600, adjust:
    params.PPD = FLIP::calculatePPD(0.7f, 800.0f, 0.4f);  // ~30 PPD
    
    float meanError = 0.0f;
    float* errorMap = nullptr;
    
    FLIP::evaluate(refLinear.data(), testLinear.data(),
                   width, height, false /*useHDR*/, params,
                   false /*applyMagmaMap*/, true /*computeMean*/,
                   meanError, &errorMap);
    
    delete[] errorMap;  // We only need the mean score
    return static_cast<double>(meanError);
}
```

Replace the call in `handleCompare()`:
```cpp
// Before:
const double rmse = computeImageRmse(actualImage, goldenImage, sampleCount);

// After:
const double error = computeFlipMeanError(actualImage, goldenImage, actualWidth, actualHeight);
```

The threshold comparison and pass/fail logging remain identical — FLIP's mean error is already in [0, 1].

### Step 3: Update log messages

Change `RMSE` references to `FLIP` in output:
```
[VDE:InputScript] compare PASSED (FLIP 0.012 <= 0.05)
[VDE:InputScript] ASSERT FAILED at line 5: image mismatch — FLIP 0.142 > threshold 0.05
```

### Step 4: Optional — save FLIP error heatmap on failure

When a comparison fails, save a Magma-colorized error heatmap PNG for visual debugging:

```cpp
if (error > cmd.compareThreshold) {
    // Save heatmap
    float* magmaMap = nullptr;
    FLIP::evaluate(refLinear.data(), testLinear.data(),
                   width, height, false, params,
                   true /*applyMagmaMap*/, false, meanError, &magmaMap);
    
    // magmaMap is 3-channel float sRGB — convert to uint8 and save
    std::string heatmapPath = cmd.argument + ".flip_diff.png";
    // ... write with stbi_write_png
    delete[] magmaMap;
    
    std::cerr << "[VDE:InputScript] FLIP error heatmap saved: " << heatmapPath << std::endl;
}
```

This produces a visual diff image showing exactly where the rendering diverged, using FLIP's standard Magma color scale (blue = no error, yellow/white = high error).

### Step 5: Update and add tests

See **[Testing Plan](#testing-plan)** below for the full test matrix.

### Step 6: Add render verification scripts

See **[Render Verification Scripts](#render-verification-scripts)** below.

### Step 7: Calibrate thresholds

FLIP scores differently than RMSE. Typical threshold ranges:

| Scenario | RMSE threshold | FLIP threshold | Notes |
|----------|:-:|:-:|-------|
| Identical images | 0.0 | 0.0 | |
| Same GPU, different run | 0.001 | 0.001 | Nearly identical |
| AMD vs NVIDIA | 0.01–0.03 | 0.005–0.02 | FLIP is more tolerant here |
| Minor visual bug | 0.02–0.05 | 0.01–0.03 | FLIP scores lower for noise |
| Missing viewport / wrong shader | 0.1+ | 0.1+ | Both metrics catch gross errors |

Recommended default threshold for `compare` commands: **0.05** (FLIP) vs the current 0.02 (RMSE).

Run the golden comparison against a few examples on multiple GPUs to calibrate the exact threshold per example.

## Backward Compatibility

The `compare` command syntax is **unchanged**:
```vdescript
compare actual.png golden.png 0.05
```

The only change is that the threshold now represents FLIP mean error instead of RMSE. Existing scripts may need threshold adjustments — FLIP and RMSE don't produce identical scores. Since there are no golden images in the repo yet (`smoketests/verification_images/` is empty), this is a clean transition.

## Risks and Mitigations

| Risk | Mitigation |
|------|------------|
| FLIP is slower than RMSE (~50ms for 800×600 vs ~1ms) | Acceptable for test-time only; not called during gameplay |
| FLIP.h is ~3000 lines, increases compile time of InputScriptExecutor.cpp | Only one TU includes it; could wrap in a separate .cpp if needed |
| FLIP's internal `int3` union conflicts with CUDA/platform types | VDE doesn't use CUDA; no conflict. The `#ifndef FLIP_ENABLE_CUDA` path is clean |
| Threshold values differ from RMSE | No existing golden images to migrate; document recommended thresholds |

## Optional Future Work

- **`compare` with `--heatmap` flag** — always save the Magma diff image for CI artifact collection
- **Per-viewport FLIP** — crop each viewport region and compare independently, catching single-viewport regressions
- **PPD auto-calculation** — derive PPD from the actual window size recorded in screenshot metadata
- **`compare_flip` vs `compare_rmse`** — keep both metrics available via separate commands if RMSE is ever needed for exact-match scenarios

## Effort

~4 days:
- Day 1: Add FLIP header, implement `computeFlipMeanError`, update `handleCompare`, update tests
- Day 2: Write unit tests, generate initial golden images, calibrate thresholds
- Day 3: Implement `render-verify.ps1`, write `verify_*.vdescript` scripts, wire into `verify.ps1`
- Day 4: Add heatmap output on failure, end-to-end testing of full pipeline, update docs

---

## Testing Plan

### T1. Unit tests for FLIP integration (`tests/FlipCompare_test.cpp`)

New test file focused on the `computeFlipMeanError()` function. Tests run without GPU; they use pre-built test images loaded from `tests/data/`.

**Test data files** to add to `tests/data/`:

| File | Description | Size |
|------|-------------|------|
| `solid_red_8x8.png` | 8×8 solid red (255,0,0) | 268 B |
| `solid_red_8x8_copy.png` | Byte-identical copy of above | 268 B |
| `solid_blue_8x8.png` | 8×8 solid blue (0,0,255) | 268 B |
| `solid_red_8x8_noise.png` | 8×8 red with ±3/255 per-channel noise | 268 B |
| `gradient_32x32.png` | 32×32 horizontal R gradient | ~1 KB |
| `gradient_32x32_shifted.png` | Same gradient shifted 1px right | ~1 KB |
| `checkerboard_16x16.png` | 16×16 black/white checkerboard | ~512 B |
| `blank_16x16.png` | 16×16 all-black | ~256 B |

These are tiny synthetic PNGs generated once (by a script or manually) and committed to the repo.

**Test cases:**

```cpp
/// @file tests/FlipCompare_test.cpp
/// Unit tests for the FLIP-based image comparison used by the compare command.

namespace vde::test {

// --- Identical images ---

TEST(FlipCompareTest, IdenticalImagesReturnZero) {
    // Load solid_red_8x8.png twice
    // computeFlipMeanError(imgA, imgA, 8, 8) → 0.0
    EXPECT_DOUBLE_EQ(result, 0.0);
}

TEST(FlipCompareTest, ByteIdenticalCopiesReturnZero) {
    // Load solid_red_8x8.png and solid_red_8x8_copy.png
    EXPECT_DOUBLE_EQ(result, 0.0);
}

// --- Completely different images ---

TEST(FlipCompareTest, RedVsBlueReturnsHighError) {
    // solid_red_8x8.png vs solid_blue_8x8.png
    // Expect FLIP score well above 0.1 (perceptually very different)
    EXPECT_GT(result, 0.1);
    EXPECT_LE(result, 1.0);
}

// --- Minor noise tolerance ---

TEST(FlipCompareTest, SmallNoiseReturnsBelowReasonableThreshold) {
    // solid_red_8x8.png vs solid_red_8x8_noise.png (±3/255 noise)
    // FLIP should report this as very small perceptual difference
    EXPECT_LT(result, 0.05);
    EXPECT_GT(result, 0.0);
}

// --- Structural/edge differences ---

TEST(FlipCompareTest, ShiftedGradientDetectsEdgeDifference) {
    // gradient_32x32.png vs gradient_32x32_shifted.png (1px shift)
    // FLIP's feature pipeline should detect the edge shift
    EXPECT_GT(result, 0.0);
    EXPECT_LT(result, 0.5);  // not catastrophically different
}

// --- Blank detection ---

TEST(FlipCompareTest, CheckerboardVsBlankIsHighError) {
    // checkerboard_16x16.png vs blank_16x16.png
    EXPECT_GT(result, 0.2);
}

// --- Score bounds ---

TEST(FlipCompareTest, ResultIsAlwaysInZeroOneRange) {
    // Run several comparisons, assert 0.0 <= result <= 1.0 for all
    for (auto& [a, b] : imagePairs) {
        double r = computeFlipMeanError(a, b, w, h);
        EXPECT_GE(r, 0.0);
        EXPECT_LE(r, 1.0);
    }
}

// --- Symmetry ---

TEST(FlipCompareTest, ComparisonIsSymmetric) {
    // computeFlipMeanError(A, B) == computeFlipMeanError(B, A)
    EXPECT_NEAR(resultAB, resultBA, 1e-6);
}

} // namespace vde::test
```

**Registration** — add `FlipCompare_test.cpp` to `VDE_TEST_SOURCES` in `tests/CMakeLists.txt`.

### T2. Existing parse tests remain unchanged

The 7 existing `InputScriptParseLine` tests for the `compare` command (`ParsesCompare`, `ParsesCompareZeroThreshold`, `CompareMissingArgsIsError`, etc.) are unaffected — they test syntax parsing, not the comparison metric.

### T3. Integration test: compare command end-to-end (`tests/InputScript_test.cpp`)

Add tests that exercise `handleCompare()` through the script executor with real image files:

```cpp
// Extend InputScriptExecutor tests
TEST_F(InputScriptExecutorTest, CompareIdenticalImagesPassesWithAnyThreshold) {
    // Write identical test images to temp dir
    // Run script: "compare img_a.png img_b.png 0.01"
    // Expect: assertionFailed == false
}

TEST_F(InputScriptExecutorTest, CompareDifferentImagesFailsWithTightThreshold) {
    // Write very different test images to temp dir
    // Run script: "compare red.png blue.png 0.01"
    // Expect: assertionFailed == true
}

TEST_F(InputScriptExecutorTest, CompareDifferentImagesPassesWithLooseThreshold) {
    // Same pair as above but threshold = 1.0
    // Expect: assertionFailed == false
}

TEST_F(InputScriptExecutorTest, CompareOutputMessageContainsFLIPNotRMSE) {
    // Capture stdout/stderr, verify log says "FLIP" not "RMSE"
}
```

### T4. Smoke-level golden image test (end-to-end with GPU)

After golden images are generated (see render verification scripts below), one or two smoke scripts serve as an end-to-end integration test of the full pipeline:

```vdescript
# verify_textured_cube.vdescript — end-to-end FLIP test
wait startup
wait_frames 30
screenshot render_verify_output/textured_cube.png
wait 100
compare render_verify_output/textured_cube.png smoketests/golden/textured_cube.png 0.05
exit
```

If `compare` passes, the full stack works: framebuffer capture → PNG save → FLIP comparison → threshold check.

If it fails, the script sets `assertionFailed`, producing a non-zero exit code that `smoke-test.ps1` detects.

### T5. Test data generation script

Add `tests/generate_test_images.py` (or `tests/generate_test_images.ps1`) that creates the synthetic PNG files for T1:

```python
#!/usr/bin/env python3
"""Generate small synthetic test PNGs for FlipCompare_test.cpp."""
from PIL import Image
import numpy as np

# solid_red_8x8.png
Image.fromarray(np.full((8, 8, 3), [255, 0, 0], dtype=np.uint8)).save("data/solid_red_8x8.png")

# solid_blue_8x8.png
Image.fromarray(np.full((8, 8, 3), [0, 0, 255], dtype=np.uint8)).save("data/solid_blue_8x8.png")

# solid_red_8x8_noise.png — red with ±3 noise
red = np.full((8, 8, 3), [255, 0, 0], dtype=np.int16)
noise = np.random.randint(-3, 4, (8, 8, 3), dtype=np.int16)
noisy = np.clip(red + noise, 0, 255).astype(np.uint8)
Image.fromarray(noisy).save("data/solid_red_8x8_noise.png")

# gradient_32x32.png
grad = np.zeros((32, 32, 3), dtype=np.uint8)
grad[:, :, 0] = np.tile(np.linspace(0, 255, 32, dtype=np.uint8), (32, 1))
Image.fromarray(grad).save("data/gradient_32x32.png")

# gradient_32x32_shifted.png — same, shifted 1px right
shifted = np.roll(grad, 1, axis=1)
Image.fromarray(shifted).save("data/gradient_32x32_shifted.png")

# checkerboard_16x16.png
cb = np.zeros((16, 16, 3), dtype=np.uint8)
cb[::2, ::2] = 255; cb[1::2, 1::2] = 255
Image.fromarray(cb).save("data/checkerboard_16x16.png")

# blank_16x16.png
Image.fromarray(np.zeros((16, 16, 3), dtype=np.uint8)).save("data/blank_16x16.png")
```

Run once, commit the PNGs to `tests/data/`. The script is for reproducibility, not run in CI.

---

## Render Verification Scripts

### Overview

Render verification adds a new test stage that captures screenshots from running examples and compares them against golden reference images using FLIP. This runs alongside (not replacing) the existing smoke tests.

### Architecture

```
verify.ps1
├── Build
├── Unit Tests
├── Smoke Tests                  (existing — launch + exit + assertions)
├── Render Verification Tests    (NEW — golden image comparison via FLIP)
└── Summary
```

### S1. `scripts/render-verify.ps1` — Orchestrator

A new PowerShell script modeled on `smoke-test.ps1`. Reuses the same executable discovery and metadata infrastructure, but reads from a `[render_verify]` section in `vde.toml` and runs `verify_*.vdescript` scripts.

**Parameters** (mirror `smoke-test.ps1`):

| Parameter | Values | Default | Description |
|-----------|--------|---------|-------------|
| `-Category` | `All`, `Examples`, `Tools` | `All` | Which category to verify |
| `-Filter` | Wildcard pattern | (none) | Filter by executable name |
| `-Extended` | switch | `$false` | Include priority 2 examples |
| `-Generator` | `MSBuild`, `Ninja` | `Ninja` | Build system |
| `-Config` | `Debug`, `Release` | `Debug` | Build configuration |
| `-Build` | switch | `$false` | Build first |
| `-UpdateGolden` | switch | `$false` | Capture new golden images instead of comparing |
| `-Verbose` | switch | `$false` | Show detailed output |

**Key differences from `smoke-test.ps1`:**

1. **Reads `[render_verify]` instead of `[smoke]`** from `vde.toml`
2. **`-UpdateGolden` mode** — runs the same scripts but copies captured screenshots to `smoketests/golden/` as the new reference, skipping the `compare` step
3. **Output directory** — each run writes screenshots to `render_verify_output/` (gitignored)
4. **Heatmap collection** — on failure, collects `.flip_diff.png` heatmaps to `logs/render_diffs/`
5. **Longer timeout** — 20 seconds (screenshot capture + FLIP comparison takes longer than a simple smoke test)

**Execution flow:**

```
For each executable with [render_verify] metadata:
  1. Resolve verify_*.vdescript from vde.toml
  2. Clear render_verify_output/ for this example
  3. Run: vde_<name>.exe --input-script verify_<name>.vdescript
     - Script does: wait startup → wait_frames N → screenshot → compare → exit
  4. Check exit code + stderr for ASSERT FAILED
  5. On failure: copy .flip_diff.png heatmap to logs/render_diffs/
  6. Report PASS/FAIL
```

**Summary output format** (matches smoke-test.ps1):
```
=== Render Verification Results ===
  vde_textured_cube_demo     PASSED (FLIP 0.008 <= 0.05)
  vde_materials_lighting_demo PASSED (FLIP 0.012 <= 0.05)
  vde_sprite_demo            FAILED (FLIP 0.187 > 0.05) → logs/render_diffs/sprite_demo.flip_diff.png
  vde_four_scene_3d_demo     PASSED (assertions OK, FLIP 0.003 <= 0.05)

Render verification: 3 passed, 1 failed, 0 skipped
```

### S2. `vde.toml` schema extension

Each example that participates in render verification adds a `[render_verify]` section:

```toml
# examples/textured_cube_demo/vde.toml
[smoke]
scripts = ["smoke_textured_cube.vdescript"]
priority = 1
sections = ["entity", "resource"]

[render_verify]
scripts = ["verify_textured_cube.vdescript"]
priority = 1
golden = "textured_cube.png"
threshold = 0.05
```

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `scripts` | string[] | Yes | Verify script filenames |
| `priority` | int | No (default 1) | 1 = core, 2 = extended |
| `golden` | string | No | Golden image filename (in `smoketests/golden/`) |
| `threshold` | float | No (default 0.05) | FLIP threshold override for this example |

### S3. Verify scripts (`smoketests/scripts/verify_*.vdescript`)

Each canary example gets a verification script. These follow a standard pattern:

#### `verify_textured_cube.vdescript`
```vdescript
# Render verification: textured cube demo
# Captures frame after cube rotation stabilizes, compares against golden image
wait startup
wait_frames 30

# Capture deterministic frame
screenshot render_verify_output/textured_cube.png
wait 100

# Compare against golden reference using FLIP
compare render_verify_output/textured_cube.png smoketests/golden/textured_cube.png 0.05

exit
```

#### `verify_materials_lighting.vdescript`
```vdescript
# Render verification: materials and lighting demo
# Tests default material rendering at startup (before any user input)
wait startup
wait_frames 30

screenshot render_verify_output/materials_lighting.png
wait 100
compare render_verify_output/materials_lighting.png smoketests/golden/materials_lighting.png 0.05

exit
```

#### `verify_sprite_demo.vdescript`
```vdescript
# Render verification: sprite demo
# Captures initial sprite layout
wait startup
wait_frames 30

screenshot render_verify_output/sprite_demo.png
wait 100
compare render_verify_output/sprite_demo.png smoketests/golden/sprite_demo.png 0.05

exit
```

#### `verify_four_scene_3d.vdescript`
```vdescript
# Render verification: four-scene 3D demo
# Validates all 4 viewports render correctly, then captures screenshot
wait startup
wait_frames 30

# Structural assertions first (fast fail before expensive screenshot)
assert rendered_scene_count == 4
assert scene "crystal" was_rendered == true
assert scene "metropolis" was_rendered == true
assert scene "nature" was_rendered == true
assert scene "cosmos" was_rendered == true
assert scene "crystal" not_blank
assert scene "metropolis" not_blank
assert scene "nature" not_blank
assert scene "cosmos" not_blank

# Golden image comparison
screenshot render_verify_output/four_scene_3d.png
wait 100
compare render_verify_output/four_scene_3d.png smoketests/golden/four_scene_3d.png 0.05

exit
```

#### `verify_physics_demo.vdescript`
```vdescript
# Render verification: physics demo
# Captures initial physics scene before simulation progresses
wait startup
wait_frames 10

# Early capture — scene 1 initial state is deterministic
screenshot render_verify_output/physics_demo.png
wait 100
compare render_verify_output/physics_demo.png smoketests/golden/physics_demo.png 0.05

exit
```

#### `verify_text_metrics_demo.vdescript`
```vdescript
# Render verification: text metrics demo
# Text rendering must be pixel-accurate for readability
wait startup
wait_frames 30

screenshot render_verify_output/text_metrics_demo.png
wait 100
compare render_verify_output/text_metrics_demo.png smoketests/golden/text_metrics_demo.png 0.03

exit
```

#### `verify_breakout_demo.vdescript`
```vdescript
# Render verification: breakout demo
# Captures initial game state (paddle, ball, bricks)
wait startup
wait_frames 20

screenshot render_verify_output/breakout_demo.png
wait 100
compare render_verify_output/breakout_demo.png smoketests/golden/breakout_demo.png 0.05

exit
```

#### `verify_imgui_demo.vdescript`
```vdescript
# Render verification: ImGui demo
# Captures default ImGui layout at startup
wait startup
wait_frames 30

screenshot render_verify_output/imgui_demo.png
wait 100
compare render_verify_output/imgui_demo.png smoketests/golden/imgui_demo.png 0.06

exit
```

### S4. Canary example selection

These 8 examples cover distinct rendering subsystems:

| Example | Covers | Priority | Threshold | Why |
|---------|--------|:---:|:---:|-----|
| `textured_cube_demo` | 3D texturing, depth buffer, rotation | 1 | 0.05 | Canonical 3D rendering test |
| `materials_lighting_demo` | Shading, lighting, PBR materials | 1 | 0.05 | Shader correctness |
| `sprite_demo` | 2D rendering, texture atlas, sprites | 1 | 0.05 | 2D rendering pipeline |
| `four_scene_3d_demo` | Multi-viewport, scene groups | 1 | 0.05 | Viewport layout + assertions |
| `physics_demo` | Entity rendering, physics visualization | 1 | 0.05 | Entity system rendering |
| `text_metrics_demo` | Text rendering, font metrics | 1 | 0.03 | Text must be pixel-accurate |
| `breakout_demo` | 2D game rendering, collision shapes | 2 | 0.05 | Game-style 2D rendering |
| `imgui_demo` | ImGui overlay, UI rendering | 2 | 0.06 | UI framework integration |

Priority 1 = always run; Priority 2 = run with `-Extended`.

### S5. Golden image management

**Storage:** `smoketests/golden/` tracked with Git LFS.

```
smoketests/
  golden/
    textured_cube.png
    materials_lighting.png
    sprite_demo.png
    four_scene_3d.png
    physics_demo.png
    text_metrics_demo.png
    breakout_demo.png
    imgui_demo.png
```

**Generation:**
```powershell
# Generate all golden images from current build
.\scripts\render-verify.ps1 -UpdateGolden

# Generate golden for a single example
.\scripts\render-verify.ps1 -UpdateGolden -Filter "*textured_cube*"
```

In `-UpdateGolden` mode, the script:
1. Runs each `verify_*.vdescript` (which captures screenshots)
2. Copies the output PNGs from `render_verify_output/` to `smoketests/golden/`
3. Skips the `compare` step (the golden image doesn't exist yet or is being replaced)
4. Reports which golden images were created/updated

**Regeneration policy:**
- After intentional visual changes (new materials, lighting, UI layout)
- Before merging branches that change rendering behavior
- Per-GPU-vendor golden sets if cross-vendor CI is added (future)

**Gitignore additions:**
```
# .gitignore
render_verify_output/
logs/render_diffs/
```

### S6. Wire into `verify.ps1`

Add render verification as a new stage in `verify.ps1`:

```powershell
# After smoke tests, before summary:
if (-not $SkipRenderVerify) {
    Run-Stage "RENDER VERIFY" {
        & pwsh -NoProfile -File "$PSScriptRoot/render-verify.ps1" -Generator $Generator -Config $Config
    }
}
```

New parameter: `-SkipRenderVerify` (default `$false`).

**Updated stage summary:**
```
  BUILD        : PASSED
  UNIT TESTS   : PASSED
  SMOKE TESTS  : PASSED
  RENDER VERIFY: PASSED
  OVERALL: ALL STAGES PASSED
```

### S7. VS Code task

Add to `.vscode/tasks.json`:

```json
{
    "label": "scripts: render-verify",
    "type": "shell",
    "command": "powershell",
    "args": [
        "-NoProfile", "-ExecutionPolicy", "Bypass",
        "-File", "${workspaceFolder}/scripts/render-verify.ps1"
    ],
    "group": "test"
}
```

### S8. `render-verify.ps1` `-UpdateGolden` workflow

The golden image update workflow avoids the chicken-and-egg problem (can't compare without golden images):

```
┌─────────────────────────────────────────────────────┐
│  First time / after visual change:                  │
│  1. .\scripts\render-verify.ps1 -UpdateGolden       │
│     → Runs verify scripts, captures screenshots     │
│     → Copies to smoketests/golden/                  │
│                                                     │
│  2. git add smoketests/golden/*.png                 │
│     git commit -m "Update golden images"            │
│                                                     │
│  Normal verification:                               │
│  3. .\scripts\render-verify.ps1                     │
│     → Captures screenshots                          │
│     → Compares against golden/ using FLIP           │
│     → Reports pass/fail                             │
└─────────────────────────────────────────────────────┘
```

---

## File Changes (Complete)

| File | Change |
|------|--------|
| `third_party/flip/FLIP.h` | **New** — single header from NVlabs |
| `third_party/flip/LICENSE` | **New** — BSD-3 license |
| `src/api/InputScriptExecutor.cpp` | Replace `computeImageRmse` with `computeFlipMeanError`, update log strings, add heatmap on failure |
| `tests/FlipCompare_test.cpp` | **New** — unit tests for FLIP integration |
| `tests/data/solid_red_8x8.png` | **New** — test image |
| `tests/data/solid_blue_8x8.png` | **New** — test image |
| `tests/data/solid_red_8x8_noise.png` | **New** — test image |
| `tests/data/gradient_32x32.png` | **New** — test image |
| `tests/data/gradient_32x32_shifted.png` | **New** — test image |
| `tests/data/checkerboard_16x16.png` | **New** — test image |
| `tests/data/blank_16x16.png` | **New** — test image |
| `tests/generate_test_images.py` | **New** — generates test PNGs |
| `tests/CMakeLists.txt` | Add `FlipCompare_test.cpp` to `VDE_TEST_SOURCES` |
| `tests/InputScript_test.cpp` | Add integration tests for `handleCompare()` with FLIP |
| `CMakeLists.txt` | Add `third_party/flip` to include paths |
| `scripts/render-verify.ps1` | **New** — render verification orchestrator |
| `scripts/verify.ps1` | Add render verification stage + `-SkipRenderVerify` param |
| `smoketests/scripts/verify_textured_cube.vdescript` | **New** |
| `smoketests/scripts/verify_materials_lighting.vdescript` | **New** |
| `smoketests/scripts/verify_sprite_demo.vdescript` | **New** |
| `smoketests/scripts/verify_four_scene_3d.vdescript` | **New** |
| `smoketests/scripts/verify_physics_demo.vdescript` | **New** |
| `smoketests/scripts/verify_text_metrics_demo.vdescript` | **New** |
| `smoketests/scripts/verify_breakout_demo.vdescript` | **New** |
| `smoketests/scripts/verify_imgui_demo.vdescript` | **New** |
| `smoketests/golden/*.png` | **New** (8 files) — golden reference images (Git LFS) |
| `examples/*/vde.toml` | Add `[render_verify]` section to 8 examples |
| `.gitignore` | Add `render_verify_output/` and `logs/render_diffs/` |
| `.vscode/tasks.json` | Add `scripts: render-verify` task |
