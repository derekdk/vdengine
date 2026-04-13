# Render Verification Tests — Options Plan

## Problem

Smoke tests verify that examples launch and exit cleanly, but they cannot detect visual defects (blank viewports, missing meshes, broken shaders, depth errors). We need a "render verification" test suite that validates what actually appears on screen.

## Current Infrastructure

VDE already has the building blocks in place:

| Capability | Status | Location |
|------------|--------|----------|
| `screenshot` command | **Working** — captures swapchain → PNG via `stbi_write_png` | `InputScriptExecutor.cpp`, `Game.cpp`, `VulkanContext::captureFramebuffer()` |
| `compare` command | **Working** — loads two PNGs, computes RMSE, fails if above threshold | `InputScriptExecutor::handleCompare()` |
| `assert` commands | **Working** — `assert rendered_scene_count`, `assert scene "name" field op value` | `InputScriptExecutor::handleAssert*()` |
| `smoke-test.ps1` | **Working** — auto-discovers executables, runs `.vdescript` scripts, checks exit codes and stderr | `scripts/smoke-test.ps1` |
| `vde.toml` metadata | **Working** — per-example smoke scripts, priority, sections | `examples/*/vde.toml` |
| `verification_images/` | **Reserved** — empty directory in `smoketests/` | `smoketests/verification_images/` |

The `compare` command and screenshot capture are fully functional. What's missing is the orchestration layer, golden image management, and a decision on which verification strategy to use.

---

## Option A: Golden Image Comparison (Pixel-Level)

**Approach:** Capture a screenshot at a deterministic frame, compare it pixel-by-pixel against a stored reference image using the existing `compare` command.

### How It Works

1. Each verifiable example gets a `verify_*.vdescript` script:
   ```vdescript
   wait startup
   wait 500
   screenshot test_output/physics_demo.png
   wait 100
   compare test_output/physics_demo.png golden/physics_demo.png 0.02
   exit
   ```

2. Golden images stored in `smoketests/golden/` tracked with Git LFS.

3. A `render-verify.ps1` script (or new `-Visual` flag on `smoke-test.ps1`) discovers and runs `verify_*.vdescript` scripts instead of `smoke_*.vdescript`.

4. `vde.toml` gets a new `[render_verify]` section:
   ```toml
   [render_verify]
   scripts = ["verify_physics_demo.vdescript"]
   priority = 1
   ```

### Pros

- **Catches everything** — any pixel-level change is detected (shader bugs, layout shifts, missing geometry, wrong colors)
- **Already implemented** — `screenshot` + `compare` commands work today
- **Simple conceptually** — "does it look right?" is the most direct question

### Cons

- **GPU-dependent** — AMD vs NVIDIA vs Intel may produce slightly different pixels for the same scene (anti-aliasing, rounding, driver differences)
- **Resolution-dependent** — different window sizes produce different golden images; must fix window size
- **Fragile** — intentional visual changes (new materials, different lighting) require regenerating all affected golden images
- **Storage** — golden PNGs are large; requires Git LFS
- **Threshold tuning** — RMSE thresholds need per-example calibration

### Mitigation

- Fix window size to 800×600 in verify scripts (or use a `set window_size 800 600` command)
- Store golden images per-GPU-vendor if needed (e.g., `golden/nvidia/`, `golden/amd/`)
- Provide `render-verify.ps1 -UpdateGolden` to regenerate baselines
- Use per-example thresholds in `vde.toml`

### Effort: ~3 days

Most infrastructure exists. Work is: golden image generation script, `vde.toml` schema extension, `render-verify.ps1` orchestrator, and writing `verify_*.vdescript` scripts for each example.

---

## Option B: Structural Assertions Only (No Pixel Reads)

**Approach:** Use the existing `assert` commands to validate engine state (scene counts, entity counts, viewport dimensions) without capturing any pixels.

### How It Works

1. Verification scripts use only assertions:
   ```vdescript
   wait startup
   wait 500
   assert rendered_scene_count == 4
   assert scene "crystal"    was_rendered == true
   assert scene "crystal"    draw_calls   > 0
   assert scene "metropolis" was_rendered == true
   assert scene "nature"     entities_drawn >= 3
   assert scene "cosmos"     entities_drawn >= 1
   exit
   ```

2. Same discovery and execution flow as smoke tests.

3. Assertions fail with `ASSERT FAILED` on stderr → `smoke-test.ps1` already detects this as a failure.

### Pros

- **Zero GPU readback cost** — no staging buffers, no image loads
- **GPU-independent** — works identically on any hardware
- **No golden images to maintain** — assertions are self-contained in `.vdescript`
- **Already working** — assert commands are fully implemented
- **Fast** — adds milliseconds, not seconds

### Cons

- **Cannot catch visual bugs** — a scene can report `was_rendered == true` and `draw_calls > 0` while rendering garbage (wrong texture, wrong shader, depth corruption)
- **Coarse** — knows *that* something was drawn, not *what* was drawn
- **Requires per-example knowledge** — each example needs custom expected values

### Effort: ~1 day

Write `verify_*.vdescript` scripts with assertions for each example. No engine changes needed.

---

## Option C: Viewport Content Hashing (Lightweight Visual Check)

**Approach:** After rendering, hash the pixels in each viewport region. Compare hashes against known-good values. Catches "blank viewport" and "completely wrong content" without storing full images.

### How It Works

1. Add a `screenshot_hash` or `assert viewport_hash` command that:
   - Reads back the framebuffer (existing `captureFramebuffer()`)
   - Computes a perceptual hash (average-hash or dHash) for the full frame or per-viewport
   - Compares against an expected hash with Hamming-distance tolerance

2. Verification script:
   ```vdescript
   wait startup
   wait 500
   assert frame not_blank
   assert scene "crystal"   not_blank
   assert scene "metropolis" not_blank
   exit
   ```

3. Optional: store known-good hashes in `vde.toml`:
   ```toml
   [render_verify]
   scripts = ["verify_four_scene.vdescript"]
   expected_hashes = { crystal = "a3f8c012", metropolis = "7b2e4d91" }
   hash_tolerance = 8  # Hamming distance
   ```

### Pros

- **Catches blank/missing viewports** — the #1 class of visual bug
- **Lightweight** — 64-bit hash per viewport, no large golden files
- **Tolerant of minor differences** — perceptual hashing handles anti-aliasing and minor driver variation
- **Simpler golden management** — hash values are small strings, not binary files

### Cons

- **Cannot catch subtle bugs** — slight color shifts, small geometry errors, minor texture issues won't change the perceptual hash enough
- **Requires new engine code** — perceptual hashing, per-viewport readback
- **Still GPU-dependent** — perceptual hashes may differ across vendors for borderline cases
- **New concept** — hashing adds complexity to the assertion system

### Effort: ~3–4 days

New: per-viewport pixel readback helper, perceptual hash function, `not_blank` assertion, optional `viewport_hash` assertion. Must also decide on hash algorithm (average-hash is simple; dHash is more robust).

---

## Option D: Hybrid — Assertions + Selective Golden Images

**Approach:** Combine structural assertions (Option B) for all examples with golden image comparison (Option A) for a curated subset of "visual canary" examples.

### How It Works

1. **Every example** gets a `verify_*.vdescript` with structural assertions (scene count, entity count, was_rendered).

2. **Selected examples** additionally capture a screenshot and compare against a golden image. Choose examples that cover distinct visual features:
   - `four_scene_3d_demo` — multi-viewport layout
   - `materials_lighting_demo` — shading correctness
   - `sprite_demo` — 2D rendering / texture sampling
   - `textured_cube_demo` — 3D texturing, depth buffer
   - `text_metrics_demo` — text rendering

3. `vde.toml` supports both:
   ```toml
   [render_verify]
   scripts = ["verify_materials.vdescript"]
   priority = 1
   
   [render_verify.golden]
   images = ["golden/materials_lighting.png"]
   threshold = 0.03
   ```

4. `render-verify.ps1` runs all verify scripts (fast assertion pass), then runs golden-image scripts (slower pixel pass).

### Pros

- **Best coverage** — structural assertions catch configuration bugs fast; golden images catch visual bugs for key examples
- **Manageable golden set** — only 5–10 golden images instead of 30+
- **Layered failure diagnosis** — assertion failure = structural bug, golden mismatch = visual bug
- **Incremental** — start with assertions only (day 1), add golden images later

### Cons

- **Two systems to maintain** — assertion scripts + golden images
- **Still has golden image downsides** — GPU variance, resolution dependence (for the subset)
- **More complex orchestration** — two-phase verification

### Effort: ~2 days (assertions) + ~2 days (golden subset) = ~4 days total

---

## Option E: Deterministic Frame Dump + External Diff Tool

**Approach:** Instead of comparing inside the engine, dump screenshots to a known directory and run an external comparison tool (ImageMagick, custom PowerShell, or a purpose-built `vde_image_diff.exe`).

### How It Works

1. Verify scripts only capture screenshots:
   ```vdescript
   wait startup
   wait 500
   screenshot render_verify_output/physics_demo.png
   exit
   ```

2. A PowerShell script runs all examples, then compares outputs:
   ```powershell
   # render-verify.ps1
   # Phase 1: Run all verify scripts (capture screenshots)
   # Phase 2: Compare each output against golden/
   foreach ($png in Get-ChildItem render_verify_output/*.png) {
       $golden = "smoketests/golden/$($png.Name)"
       magick compare -metric RMSE $png.FullName $golden null: 2>&1
       # or: vde_image_diff.exe --actual $png --reference $golden --threshold 0.02
   }
   ```

3. Diff images (highlighting changed pixels) saved to `render_verify_output/diffs/` for debugging.

### Pros

- **Visual diff output** — failed comparisons produce a diff image showing exactly what changed (red overlay on mismatched pixels)
- **Flexible tooling** — can swap in different comparison metrics (SSIM, perceptual hash, region-based) without engine changes
- **Engine stays simple** — only needs `screenshot`, no comparison logic in C++
- **Parallel comparison** — PowerShell can compare images in parallel after all examples finish

### Cons

- **External dependency** — requires ImageMagick or a custom tool to be installed
- **Two-phase execution** — capture then compare (slightly more complex than single-pass)
- **Same golden image problems** — GPU variance, resolution, maintenance

### Mitigation

- Build a lightweight `vde_image_diff.exe` tool using stb_image (already a dependency) to avoid requiring ImageMagick
- Or: use the existing in-engine `compare` command (Option A) and skip external tooling entirely

### Effort: ~3 days (with `vde_image_diff.exe`) or ~1 day (using ImageMagick)

---

## Comparison Matrix

| Criteria | A: Golden Images | B: Assertions | C: Hash | D: Hybrid | E: External Diff |
|----------|:---:|:---:|:---:|:---:|:---:|
| Catches blank viewports | Yes | Yes | Yes | Yes | Yes |
| Catches wrong geometry | Yes | No | Partial | Partial | Yes |
| Catches shader/color bugs | Yes | No | No | Partial | Yes |
| GPU-independent | No | **Yes** | Partial | Partial | No |
| No golden files needed | No | **Yes** | **Yes** | Partial | No |
| Maintenance burden | High | **Low** | Low | Medium | High |
| Engine changes needed | None | None | Medium | None | None |
| Visual diff output | No | No | No | No | **Yes** |
| Implementation effort | 3 days | **1 day** | 3–4 days | 4 days | 1–3 days |

---

## Recommendation

**Start with Option D (Hybrid)** in two phases:

### Phase 1 — Structural Assertions (Week 1)

- Write `verify_*.vdescript` scripts for all priority-1 examples using existing `assert` commands
- Add `[render_verify]` section to `vde.toml` for each example
- Add a `render-verify.ps1` script (or `-Visual` flag to `smoke-test.ps1`) that runs verify scripts
- Wire into `verify.ps1` as an optional stage

This gives immediate value with zero engine changes: catches removed scenes, missing entities, viewport misconfiguration.

### Phase 2 — Golden Image Canaries (Week 2–3)

- Pick 5–8 "canary" examples covering distinct visual features
- Write `verify_*.vdescript` scripts that capture screenshots + run `compare`
- Generate golden images at fixed 800×600 resolution
- Store in `smoketests/golden/` with Git LFS
- Add `render-verify.ps1 -UpdateGolden` to regenerate baselines
- Add visual diff output on failure (generate diff PNG highlighting mismatched pixels)

### Phase 3 — Blank Detection (Optional, Week 3+)

- Add `assert frame not_blank` / `assert scene "name" not_blank` commands
- Useful for examples that don't have golden images but should never render an empty frame
- Low cost, high value for catching "silent failure" bugs

### Integration with Existing Scripts

```
verify.ps1
├── Build
├── Unit Tests
├── Smoke Tests          (existing — launch + exit)
├── Render Verification  (new — assertions + golden images)
└── Summary
```

`render-verify.ps1` would follow the same pattern as `smoke-test.ps1`:
- Auto-discover executables via `vde.toml` metadata
- Support `-Filter`, `-Category`, `-Extended`, `-Config`, `-Generator` flags
- Report pass/fail/skip per example
- On golden image failure: save diff image to `logs/render_diffs/`
