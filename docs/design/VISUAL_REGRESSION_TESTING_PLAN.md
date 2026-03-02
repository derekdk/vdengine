# Visual Regression Testing Plan

## Problem Statement

The four-scene 3D demo shipped a bug where only one of four viewports was
rendering.  The existing smoke tests confirmed the application launched and
exited cleanly but could not detect the visual defect.  This plan proposes
incremental improvements—from low-cost API additions to full screenshot
comparison—so that similar issues are caught automatically in CI.

---

## Tier 1 — Render Diagnostics API (Low cost, high value)

### Goal

Expose lightweight counters and flags that a smoke-test script or unit test
can query after a few frames.  A mismatch triggers a non-zero exit code,
which the existing `smoke-test.ps1` infrastructure already interprets as a
failure.

### Proposed API Additions

```cpp
// --- Game.h -----------------------------------------------------------

/// Per-scene render stats collected during the last frame.
struct SceneRenderStats {
    std::string sceneName;
    uint32_t    drawCalls      = 0;   // vkCmdDraw* count inside this scene
    uint32_t    entitiesDrawn  = 0;   // entities whose render() was called
    ViewportRect viewport;            // viewport used
    bool        wasRendered    = false; // true if the scene's render callback fired
};

/// Return render stats for every scene in the active group from the last frame.
const std::vector<SceneRenderStats>& getLastFrameRenderStats() const;

/// Return the number of scenes that were actually rendered last frame.
uint32_t getRenderedSceneCount() const;
```

### Collection

Inside `drawFrameMultiScene()` / `renderSingleViewport()`, increment the
counters before/after each scene's render callback.  The vector is swapped
each frame so reads are always from the *previous* completed frame.

### Smoke-Test Integration

Add a new `.vdescript` command (or use `print` + exit-code logic):

```
# smoke_four_scene.vdescript
wait 500ms
assert rendered_scene_count == 4
assert scene "crystal"    viewport_width  > 0
assert scene "metropolis"  viewport_width  > 0
assert scene "nature"      viewport_width  > 0
assert scene "cosmos"      viewport_width  > 0
exit
```

This requires a small extension to the input-script parser:

| Command | Meaning |
|---------|---------|
| `assert rendered_scene_count == N` | Fail with exit 1 if mismatch |
| `assert scene "name" <field> <op> <value>` | Query `SceneRenderStats` by name |

If the assertion grammar is too heavy, a simpler alternative is a
**Game callback hook**:

```cpp
void Game::onValidate(uint64_t frameNumber) override {
    if (frameNumber == 30) {  // after a few rendered frames
        auto& stats = getLastFrameRenderStats();
        if (stats.size() != 4) { reportTestFailure("Expected 4 scenes"); }
        for (auto& s : stats) {
            if (!s.wasRendered) { reportTestFailure(s.sceneName + " not rendered"); }
        }
    }
}
```

Each example can override `onValidate` with its own invariants.

**Effort:** ~2 days

---

## Tier 2 — Viewport Validation in Existing Unit Tests (No GPU)

### Goal

Catch configuration bugs (wrong viewport rects, missing scenes in group)
at the unit-test level without requiring a running Vulkan device.

### Proposed Tests

Add to `tests/SceneGroup_test.cpp`:

```cpp
TEST(SceneGroupTest, QuadViewportsCoverWindow) {
    auto group = SceneGroup::createWithViewports("quad", {
        {"a", ViewportRect::topLeft()},
        {"b", ViewportRect::topRight()},
        {"c", ViewportRect::bottomLeft()},
        {"d", ViewportRect::bottomRight()},
    });
    EXPECT_EQ(group.size(), 4u);

    // Every quadrant should be present
    float totalArea = 0.0f;
    for (auto& e : group.entries) {
        totalArea += e.viewport.width * e.viewport.height;
        EXPECT_GT(e.viewport.width, 0.0f);
        EXPECT_GT(e.viewport.height, 0.0f);
    }
    EXPECT_FLOAT_EQ(totalArea, 1.0f);  // full window covered
}
```

Add a `ViewportRect` helper:

```cpp
// ViewportRect.h
static bool coversFullWindow(const std::vector<ViewportRect>& rects);
// Returns true if the union of all rects covers [0,1]×[0,1] without gaps.
```

**Effort:** ~0.5 days

---

## Tier 3 — Frame-Capture Screenshot Testing (Medium cost)

### Goal

Capture the framebuffer after N frames and compare it against a golden
reference image.  Detect visual regressions including depth, blending, and
layout errors.

### Design

```
┌──────────────┐      ┌──────────────┐      ┌──────────────┐
│  Run demo    │ ───▶ │  Capture     │ ───▶ │  Compare     │
│  (headless   │      │  frame N to  │      │  against     │
│   or normal) │      │  PNG         │      │  golden PNG  │
└──────────────┘      └──────────────┘      └──────────────┘
```

### Implementation Steps

1. **Add readback API to `VulkanContext`**

   ```cpp
   /// Copy the current swapchain image to a CPU-readable buffer.
   /// Returns RGBA pixel data (width × height × 4 bytes).
   std::vector<uint8_t> captureFramebuffer(uint32_t& outWidth, uint32_t& outHeight);
   ```

   Uses `vkCmdCopyImageToBuffer` after the render pass, with a staging
   buffer and fence wait.  This is the heaviest piece of new Vulkan code.

2. **Add `screenshot` scripted-input command**

   ```
   # smoke_four_scene_visual.vdescript
   wait 1000ms
   screenshot four_scene_3d_demo.png
   exit
   ```

   The `screenshot` command calls `captureFramebuffer()`, writes to
   `<output_dir>/<filename>`, and records the path for later comparison.

3. **Add `Game::captureScreenshot(path)` API method**

   Wraps the `VulkanContext` readback + stb_image_write PNG encode.
   Can also be called from `onValidate()`.

4. **Image comparison tool / script**

   A small CLI or PowerShell script:

   ```
   vde_image_diff.exe --reference golden/four_scene.png \
                      --actual   output/four_scene.png  \
                      --threshold 0.02
   ```

   Metrics: per-pixel RMSE over a configurable threshold.  For
   split-screen tests, compare each quadrant independently to isolate
   which viewport diverged.

5. **Golden image management**

   - Store golden PNGs in `tests/golden/` tracked with Git LFS
   - Re-generate with `--update-golden` flag
   - Allow per-GPU tolerance profiles (AMD vs NVIDIA may differ slightly)

### CI Integration

```yaml
- name: Visual regression
  run: |
    ./scripts/smoke-test.ps1 -Filter "*four_scene*" -Visual
    ./scripts/compare-screenshots.ps1 -GoldenDir tests/golden -Threshold 0.02
```

**Effort:** ~5–7 days

---

## Tier 4 — Per-Viewport Content Hashing (Low overhead alternative to screenshots)

### Goal

A lighter alternative to full screenshot comparison.  Hash each viewport's
rendered pixels and compare against known-good hashes.  Catches the
"viewport is blank / shows wrong content" class of bugs without storing
large golden images.

### Design

After `drawFrameMultiScene()`, read back only the pixels in each viewport
rect, compute a perceptual hash (average-hash or dHash), and log it.

```cpp
struct ViewportHash {
    std::string sceneName;
    uint64_t    hash;        // perceptual hash of rendered pixels
    bool        isBlank;     // true if all pixels are the same color
};

std::vector<ViewportHash> getViewportHashes() const;
```

### Assertions

```
assert scene "crystal" not_blank
assert scene "cosmos"  not_blank
```

This catches:
- Viewport rendering nothing (blank)
- Viewport rendering the wrong scene (hash ≠ expected)
- Depth buffer corruption (scene looks completely wrong)

**Effort:** ~3 days

---

## Tier 5 — Structural Render Assertions (API-level, no pixel reads)

### Goal

Let each example declare invariants about its rendered state that are
checked automatically.  No GPU readback needed.

### Proposed API

```cpp
class Game {
public:
    /// Register an assertion checked every frame after rendering.
    void addRenderAssertion(std::string name, std::function<bool()> check);

    /// Called automatically; logs + fails on first broken assertion.
    void validateRenderAssertions();
};
```

### Example Usage

```cpp
void FourScene3DDemo::onStart() override {
    // ... existing setup ...

    addRenderAssertion("all_scenes_in_group", [this]() {
        return getActiveSceneGroup().size() == 4;
    });

    addRenderAssertion("all_viewports_non_full", [this]() {
        for (auto& name : getActiveSceneGroup().sceneNames) {
            auto* scene = getScene(name);
            if (scene && scene->getViewportRect() == ViewportRect::fullWindow())
                return false;
        }
        return true;
    });

    addRenderAssertion("all_scenes_have_entities", [this]() {
        for (auto& name : getActiveSceneGroup().sceneNames) {
            auto* scene = getScene(name);
            if (!scene || scene->getEntities().empty())
                return false;
        }
        return true;
    });
}
```

Assertions are only evaluated when running with `--input-script` or a
`--validate` flag, so they have zero cost in normal gameplay.

**Effort:** ~1 day

---

## Recommended Implementation Order

| Priority | Tier | What | Catches the four-scene bug? | Effort |
|----------|------|------|-----------------------------|--------|
| **1** | Tier 1 | Render stats API + `onValidate` | ✅ Yes — `wasRendered == false` for 3 scenes | 2 days |
| **2** | Tier 5 | Structural assertions | ✅ Yes — viewport / entity checks | 1 day |
| **3** | Tier 2 | Unit-test viewport coverage | ✅ Yes — area ≠ 1.0 or size == 0 | 0.5 days |
| **4** | Tier 4 | Per-viewport content hashing | ✅ Yes — blank viewport detected | 3 days |
| **5** | Tier 3 | Full screenshot comparison | ✅ Yes — golden image mismatch | 5–7 days |

### Quick Wins (Week 1)

- Implement Tier 1 (`SceneRenderStats`) and Tier 5 (`addRenderAssertion`)
- Add `onValidate` overrides to `four_scene_3d_demo`, `quad_viewport_demo`,
  `multi_scene_demo`, and `parallel_physics_demo`
- Extend `smoke-test.ps1` to check for `TEST FAILED` in stdout

### Medium Term (Week 2–3)

- Implement Tier 2 unit tests for `ViewportRect` coverage helpers
- Implement Tier 4 viewport hashing for CI without golden images

### Long Term (Month 2+)

- Implement Tier 3 full screenshot capture and comparison
- Build golden image update workflow
- Add per-GPU tolerance profiles

---

## Files to Modify

| File | Changes |
|------|---------|
| `include/vde/api/Game.h` | Add `SceneRenderStats`, `getLastFrameRenderStats()`, `addRenderAssertion()` |
| `src/api/Game.cpp` | Collect stats in render methods, validate assertions |
| `include/vde/VulkanContext.h` | Add `captureFramebuffer()` (Tier 3) |
| `src/VulkanContext.cpp` | Implement framebuffer readback (Tier 3) |
| `include/vde/api/ViewportRect.h` | Add `coversFullWindow()` helper |
| `include/vde/api/InputScript.h` | Add `assert` / `screenshot` commands (Tier 1/3) |
| `src/api/InputScript.cpp` | Parse and execute new commands |
| `tests/SceneGroup_test.cpp` | Viewport coverage tests (Tier 2) |
| `tests/ViewportRect_test.cpp` | New test file for coverage helpers |
| `examples/four_scene_3d_demo/main.cpp` | Add `onValidate` override |
| `scripts/smoke-test.ps1` | Check stdout for assertion failures |
| `scripts/compare-screenshots.ps1` | New script for golden image comparison (Tier 3) |
