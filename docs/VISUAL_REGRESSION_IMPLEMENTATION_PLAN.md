# Visual Regression Testing — Implementation Plan

This document breaks the [Visual Regression Testing Plan](VISUAL_REGRESSION_TESTING_PLAN.md)
into concrete work items across two tracks: **Input Script Extensions** and
**Game API Additions**.  Each work item lists the files to create or modify,
the new types / commands, acceptance criteria, and dependencies on other
items.

---

## Current State

| Area | What exists today |
|------|-------------------|
| Input scripting | `wait`, `press`, `keydown`/`keyup`, `click`, `mousedown`/`mouseup`, `mousemove`, `scroll`, `print`, `label`/`loop`, `exit`, `screenshot` (filename only — no pixel capture) |
| Render diagnostics | None — no per-scene stats, no frame validation hooks |
| Assertions | None — scripts can only drive input and print messages |
| Screenshot capture | `screenshot` command logs filename; Vulkan readback is a TODO stub |
| Viewport coverage | `ViewportRect` has factory methods and hit testing; no `coversFullWindow()` helper |
| Unit tests | `SceneGroup_test.cpp` tests data construction; `Scene_test.cpp` tests viewport get/set; no `createWithViewports()` tests |
| Smoke scripts | Generic `smoke_quick.vdescript` / `smoke_test.vdescript`; no example-specific validation scripts for multi-viewport demos |

---

## Track A — Input Script Extensions

### A1. `assert` command family

**Goal:** Let `.vdescript` files validate engine state and fail with exit
code 1 on mismatch, turning smoke tests into lightweight integration tests.

#### New `InputCommandType` values

```cpp
// InputScript.h
enum class InputCommandType {
    // ... existing ...
    Assert,           ///< assert <expression> — generic assertion
    AssertSceneCount, ///< assert rendered_scene_count == N
    AssertScene,      ///< assert scene "name" <field> <op> <value>
};
```

#### Script syntax

```
# Exact scene count
assert rendered_scene_count == 4

# Per-scene field checks
assert scene "crystal"    was_rendered  == true
assert scene "crystal"    draw_calls    > 0
assert scene "crystal"    viewport_width > 0
assert scene "metropolis" was_rendered  == true

# Entity count
assert scene "nature"     entities_drawn >= 3

# Shorthand: assert the scene is not blank (requires Tier 4 hashing)
assert scene "cosmos"     not_blank
```

#### Parser changes (`InputScript.cpp`)

| Token pattern | Parsed as |
|---------------|-----------|
| `assert rendered_scene_count <op> <int>` | `AssertSceneCount` with op + value |
| `assert scene "<name>" <field> <op> <value>` | `AssertScene` with scene name, field enum, op, value |

New `ScriptCommand` fields:

```cpp
struct ScriptCommand {
    // ... existing ...
    // Assertion fields
    std::string assertSceneName;            ///< For AssertScene
    std::string assertField;                ///< "was_rendered", "draw_calls", etc.
    enum class CompareOp { Eq, Ne, Lt, Le, Gt, Ge };
    CompareOp   assertOp   = CompareOp::Eq;
    double      assertValue = 0.0;          ///< RHS of comparison
};
```

#### Execution (`Game::processInputScript`)

When an `Assert*` command executes:

1. Query `getLastFrameRenderStats()` (see B1).
2. Evaluate the comparison.
3. On failure: print `[VDE:InputScript] ASSERT FAILED: ...` to stderr,
   set exit code to 1, and continue (or optionally `exit` immediately via
   a `--assert-abort` flag).

#### Acceptance criteria

- [ ] `assert rendered_scene_count == 4` passes in four\_scene\_3d\_demo.
- [ ] `assert rendered_scene_count == 4` **fails** when one scene is
      removed from the group (test with a modified script).
- [ ] Unknown field names produce a parse error with line number.
- [ ] Unit tests in `InputScript_test.cpp` cover all `assert` parse
      variants (existing test file pattern).

#### Dependencies

- **B1** (SceneRenderStats API) must land first — assertions query it.

#### Effort: ~1.5 days

---

### A2. `screenshot` pixel capture

**Goal:** Make the existing `screenshot` command actually write PNG files
using Vulkan framebuffer readback.

#### Script syntax (unchanged)

```
screenshot output/four_scene.png
```

Produces `output/four_scene_frame_0042.png` (frame number injected as
today).

#### Implementation steps

1. **`VulkanContext::captureFramebuffer()`** — new public method:

   ```cpp
   // VulkanContext.h
   std::vector<uint8_t> captureFramebuffer(uint32_t& outWidth, uint32_t& outHeight);
   ```

   - After the render pass, transition swapchain image to
     `TRANSFER_SRC_OPTIMAL`.
   - `vkCmdCopyImageToBuffer` into a staging buffer.
   - Fence-wait, map, copy to `std::vector<uint8_t>` (RGBA).
   - Transition image back to `PRESENT_SRC_KHR`.
   - Reference: Vulkan spec §19.4, existing `BufferUtils.cpp` patterns.

2. **`Game::captureScreenshot(path)`** — new public method:

   ```cpp
   // Game.h
   bool captureScreenshot(const std::string& outputPath);
   ```

   - Calls `captureFramebuffer()`, encodes to PNG via `stb_image_write.h`
     (already available in third\_party through stb\_image).
   - Returns false on failure (e.g., headless, path not writable).

3. **Wire into `processInputScript()`** — replace the TODO stub with a
   call to `captureScreenshot(framePath)`.

#### Acceptance criteria

- [ ] `screenshot test_output.png` in a `.vdescript` produces a valid
      PNG file with correct dimensions.
- [ ] PNG pixel data matches what is on screen (manual spot-check).
- [ ] Failure to write (bad path) logs a warning but does not crash.

#### Dependencies

- None (standalone Vulkan work), but pairs well with A4.

#### Effort: ~3 days

---

### A3. `wait_frames` command

**Goal:** Deterministic frame-count waits instead of wall-clock
milliseconds, useful for tests that need to run after "N rendered frames"
regardless of machine speed.

#### Script syntax

```
wait_frames 10        # Wait exactly 10 rendered frames
wait_frames 1         # Wait until the next frame
```

#### Implementation

New `InputCommandType::WaitFrames` and `ScriptCommand::waitFrames` field.

In `processInputScript()`:

```cpp
case InputCommandType::WaitFrames:
    if (state.frameWaitCounter == 0) {
        state.frameWaitCounter = cmd.waitFrames;
    }
    state.frameWaitCounter--;
    if (state.frameWaitCounter > 0) {
        return;  // Still waiting
    }
    state.currentCommand++;
    break;
```

#### Acceptance criteria

- [ ] `wait_frames 5` followed by `print` outputs on frame N+5.
- [ ] `wait_frames 0` is a parse error.
- [ ] Unit test in `InputScript_test.cpp` for parsing.

#### Dependencies

- None.

#### Effort: ~0.5 days

---

### A4. `compare` command (golden image comparison)

**Goal:** Compare a screenshot against a golden reference and fail if they
diverge beyond a threshold.

#### Script syntax

```
screenshot output/four_scene.png
compare output/four_scene_frame_0042.png golden/four_scene.png 0.02
```

| Argument | Meaning |
|----------|---------|
| First path | Actual image (just captured) |
| Second path | Golden reference image |
| Number | RMSE threshold (0.0–1.0, where 0 = identical) |

#### Implementation

New `InputCommandType::Compare`:

1. Load both images via `stb_image`.
2. Compute per-pixel RMSE (or per-channel mean absolute error).
3. If RMSE > threshold → print `ASSERT FAILED: image mismatch` +
   set exit code 1.
4. Optionally write a diff image highlighting divergent pixels.

#### Acceptance criteria

- [ ] Comparing an image against itself with threshold 0 passes.
- [ ] Comparing two different images fails with a meaningful message.
- [ ] Mismatched dimensions produce a clear error.

#### Dependencies

- **A2** (screenshot capture) — need actual PNG files to compare.

#### Effort: ~2 days

---

### A5. `set` / variable substitution in scripts

**Goal:** Allow scripts to use engine-provided variables for
resolution-independent coordinates and conditional logic.

#### Script syntax

```
set HALF_W ${WINDOW_WIDTH} / 2
click ${HALF_W} ${HALF_H}

# Built-in variables (read-only, auto-populated)
# ${WINDOW_WIDTH}   — current window width in pixels
# ${WINDOW_HEIGHT}  — current window height in pixels
# ${FRAME}          — current frame number
# ${FPS}            — current FPS
```

#### Implementation

- `InputScriptState` gains a `std::unordered_map<std::string, double> variables`.
- Before each frame, engine populates built-in variables.
- During parsing, `${VAR}` tokens are recorded; at execution time they are
  resolved from the map.
- `set` command evaluates simple arithmetic expressions (+-*/) and stores
  the result.

#### Acceptance criteria

- [ ] `click ${WINDOW_WIDTH} ${WINDOW_HEIGHT}` clicks bottom-right corner.
- [ ] `set X 100` followed by `click ${X} ${X}` clicks (100, 100).
- [ ] Undefined variable produces a runtime error with line number.

#### Dependencies

- None, but enhances A1 assertions (`assert scene "x" draw_calls > ${MIN_DRAWS}`).

#### Effort: ~2 days

---

## Track B — Game API Additions

### B1. `SceneRenderStats` and `getLastFrameRenderStats()`

**Goal:** Expose per-scene render counters that assertions and tests can
query.

#### New types (`Game.h`)

```cpp
/// Per-scene render statistics from the last completed frame.
struct SceneRenderStats {
    std::string sceneName;
    uint32_t    drawCalls      = 0;   ///< vkCmdDraw* invocations
    uint32_t    entitiesDrawn  = 0;   ///< entities whose render() was called
    ViewportRect viewport;            ///< viewport used for this scene
    bool        wasRendered    = false; ///< true if render callback fired
};
```

#### New methods (`Game.h`)

```cpp
/// Get render stats from the last completed frame.
const std::vector<SceneRenderStats>& getLastFrameRenderStats() const;

/// Get the count of scenes that were actually rendered last frame.
uint32_t getRenderedSceneCount() const;
```

#### Collection points (`Game.cpp`)

- In `renderMultiViewport()`: for each scene, create a `SceneRenderStats`
  entry before calling the render callback. After the callback, set
  `wasRendered = true` and capture the draw call / entity counts.
- In `renderSingleViewport()`: same, but for the single active group.
- Swap a double-buffered `std::vector<SceneRenderStats>` each frame so that
  readers always see the previous (complete) frame's data.

#### Draw call counting

Instrument `Scene::render()` to increment a thread-local or scene-local
counter each time a draw command is issued:

```cpp
// Scene.h
uint32_t getLastFrameDrawCalls() const;
uint32_t getLastFrameEntitiesDrawn() const;
```

Entities already call `vkCmdDraw*` in their `render()` methods; add a
counter that resets at the start of each `Scene::render()` call.  

#### Acceptance criteria

- [ ] `getRenderedSceneCount()` returns 4 when four\_scene\_3d\_demo runs.
- [ ] `getLastFrameRenderStats()[0].wasRendered` is `true` for each scene.
- [ ] `drawCalls > 0` for every scene that has entities.
- [ ] Stats from one frame's render do not leak into the next frame.

#### Dependencies

- None (foundational — everything else builds on this).

#### Effort: ~2 days

---

### B2. `onValidate()` virtual callback

**Goal:** Let games and examples declare per-frame validation logic that
runs only during automated testing.

#### New virtual method (`Game.h`)

```cpp
protected:
    /// Called after rendering when running with --input-script or --validate.
    /// Override to check invariants. Default implementation is empty.
    /// @param frameNumber  Current frame count (starts at 0)
    virtual void onValidate(uint64_t frameNumber) { (void)frameNumber; }
```

#### Gating

Only called when `m_inputScriptState != nullptr` or a new `--validate`
CLI flag is present.  This ensures zero cost in normal gameplay.

#### Helper methods

```cpp
/// Report a test failure (prints message, sets exit code to 1).
void reportTestFailure(const std::string& message);

/// Check if any test failure has been reported.
bool didTestFail() const;
```

These wrap the exit-code bookkeeping and standardise the `[VDE:TEST]`
output prefix that `smoke-test.ps1` can grep for.

#### Example usage (four\_scene\_3d\_demo)

```cpp
void onValidate(uint64_t frameNumber) override {
    if (frameNumber < 5) return;  // let things settle

    auto& stats = getLastFrameRenderStats();
    if (stats.size() != 4) {
        reportTestFailure("Expected 4 scenes, got " +
                          std::to_string(stats.size()));
    }
    for (auto& s : stats) {
        if (!s.wasRendered) {
            reportTestFailure(s.sceneName + " was not rendered");
        }
        if (s.drawCalls == 0) {
            reportTestFailure(s.sceneName + " had 0 draw calls");
        }
    }
}
```

#### Acceptance criteria

- [ ] `onValidate()` is called every frame when `--input-script` is set.
- [ ] `onValidate()` is **not** called during normal gameplay.
- [ ] `reportTestFailure()` prints to stderr and sets exit code 1.
- [ ] `didTestFail()` returns true after any failure is reported.

#### Dependencies

- **B1** (render stats) — validators will query stats.

#### Effort: ~0.5 days

---

### B3. `addRenderAssertion()` — declarative structural checks

**Goal:** Let examples register named boolean assertions that are
evaluated automatically after each render, without writing `onValidate()`.

#### New methods (`Game.h`)

```cpp
/// Register a named assertion evaluated after each render.
/// Only checked when running with --input-script / --validate.
void addRenderAssertion(const std::string& name, std::function<bool()> check);

/// Remove a previously registered assertion.
void removeRenderAssertion(const std::string& name);
```

#### Storage (`Game.h` private)

```cpp
struct RenderAssertion {
    std::string name;
    std::function<bool()> check;
};
std::vector<RenderAssertion> m_renderAssertions;
```

#### Evaluation (`Game.cpp`)

After rendering and `onValidate()`, iterate `m_renderAssertions`.  For
each one where `check()` returns false, call
`reportTestFailure("Render assertion failed: " + name)`.

Skip evaluation for the first N frames (configurable, default 5) to allow
scene setup to complete.

#### Acceptance criteria

- [ ] An assertion that returns false is reported as a test failure.
- [ ] Removing an assertion stops it from being evaluated.
- [ ] Assertions are not evaluated during normal gameplay.

#### Dependencies

- **B2** (`reportTestFailure()` infrastructure).

#### Effort: ~0.5 days

---

### B4. `ViewportRect::coversFullWindow()` helper

**Goal:** Unit-testable validation that a set of viewports tiles the
window without gaps or overlaps.

#### New static method (`ViewportRect.h`)

```cpp
/// Check if the union of the given rects covers the full [0,1]×[0,1]
/// window without significant gaps.
/// @param rects  Set of viewport rects to check
/// @param epsilon  Tolerance for floating-point gaps (default 0.001)
/// @return true if the rects collectively cover the window
static bool coversFullWindow(const std::vector<ViewportRect>& rects,
                             float epsilon = 0.001f);
```

#### Implementation strategy

For the initial version, compute the sum of all rect areas and check that
it equals 1.0 (within epsilon).  A more rigorous implementation could
rasterise a 100×100 grid and check every cell is covered.

#### Acceptance criteria

- [ ] `coversFullWindow({topLeft(), topRight(), bottomLeft(), bottomRight()})` → true.
- [ ] `coversFullWindow({topLeft(), topRight()})` → false (only 50%).
- [ ] Overlapping rects that still cover the window → true (area ≥ 1.0).
- [ ] Unit test in `ViewportRect_test.cpp`.

#### Dependencies

- None.

#### Effort: ~0.5 days

---

### B5. Per-scene draw call / entity counting in `Scene`

**Goal:** Give `SceneRenderStats` real numbers for `drawCalls` and
`entitiesDrawn`.

#### New members (`Scene.h`)

```cpp
/// Reset per-frame render counters (called by Game before render).
void resetFrameCounters();

/// Increment draw call counter (called by entities during render).
void incrementDrawCalls(uint32_t count = 1);

/// Increment entity render counter.
void incrementEntitiesDrawn(uint32_t count = 1);

/// Get counters for the frame that just rendered.
uint32_t getLastFrameDrawCalls() const;
uint32_t getLastFrameEntitiesDrawn() const;
```

#### Instrumentation

- In `MeshEntity::render()` and `SpriteEntity::render()`: call
  `getScene()->incrementDrawCalls()` and
  `getScene()->incrementEntitiesDrawn()` around each `vkCmdDraw*`.
- Physics entities inherit from these, so they get counting for free.

#### Acceptance criteria

- [ ] A scene with 5 cubes reports `drawCalls >= 5` and
      `entitiesDrawn == 5`.
- [ ] A scene with no entities reports `drawCalls == 0`.
- [ ] Counters reset to zero each frame.

#### Dependencies

- None (used by B1).

#### Effort: ~1 day

---

### B6. Exit code management

**Goal:** Standardise how test results map to process exit codes so that
CI scripts can simply check `$LASTEXITCODE`.

#### Design

```cpp
// Game.h
void setExitCode(int code);
int  getExitCode() const;
```

- Default exit code is 0.
- `reportTestFailure()` sets exit code to 1 (if not already non-zero).
- Assert commands (A1) set exit code to 1 on failure.
- `runExample()` / `runTool()` return `game.getExitCode()` from `main()`.

#### Changes to `ExampleBase.h` / `ToolBase.h`

```cpp
// ExampleBase.h — runExample()
return game.getExitCode();
```

Currently `runExample` returns 0 unconditionally.  This change ensures
assertion failures propagate to the process exit code.

#### Acceptance criteria

- [ ] A passing test exits with code 0.
- [ ] A failing assertion exits with code 1.
- [ ] Multiple failures still exit with code 1 (not a higher number).

#### Dependencies

- None (foundational).

#### Effort: ~0.5 days

---

## Track C — Smoke Test Scripts

### C1. Example-specific validation scripts

Create targeted `.vdescript` files that exercise the assertion system for
each multi-viewport example.

| Script | Example | Key assertions |
|--------|---------|----------------|
| `smoke_four_scene_3d.vdescript` | four\_scene\_3d\_demo | 4 scenes rendered, all `was_rendered`, all `draw_calls > 0`, all `viewport_width > 0` |
| `smoke_quad_viewport.vdescript` | quad\_viewport\_demo | 4 scenes rendered, all `was_rendered`, viewports cover window |
| `smoke_parallel_physics.vdescript` | parallel\_physics\_demo | 2 physics scenes + coordinator rendered |
| `smoke_multi_scene_viewport.vdescript` | multi\_scene\_demo | Press V for split mode, then assert 2 scenes rendered |

#### Example: `smoke_four_scene_3d.vdescript`

```vdescript
# smoke_four_scene_3d.vdescript — Validate quad viewport rendering
wait startup
wait_frames 10

# All four scenes must have rendered
assert rendered_scene_count == 4
assert scene "crystal"    was_rendered  == true
assert scene "crystal"    draw_calls    > 0
assert scene "metropolis" was_rendered  == true
assert scene "metropolis" draw_calls    > 0
assert scene "nature"     was_rendered  == true
assert scene "nature"     draw_calls    > 0
assert scene "cosmos"     was_rendered  == true
assert scene "cosmos"     draw_calls    > 0

print PASS: all four scenes rendered correctly
exit
```

#### Acceptance criteria

- [ ] Each script exits 0 on a working build.
- [ ] Deliberately breaking a scene (removing from group) causes exit 1.

#### Dependencies

- **A1** (assert command), **A3** (wait\_frames), **B1** (render stats).

#### Effort: ~0.5 days

---

### C2. Update `smoke-test.ps1`

Extend the existing smoke test runner to:

1. Look for `ASSERT FAILED` or `TEST FAILED` in stdout/stderr and treat
   as failure even if exit code is 0 (defence in depth).
2. Accept a `-Visual` flag that also runs the `compare` scripts (when
   golden images are available).
3. Auto-discover example-specific scripts by matching
   `scripts/input/smoke_<example_name>.vdescript`.

#### Acceptance criteria

- [ ] `scripts/smoke-test.ps1` picks up new validation scripts and runs
      them.
- [ ] A test that prints `ASSERT FAILED` is reported as failure.

#### Dependencies

- **C1** (scripts exist).

#### Effort: ~0.5 days

---

## Track D — Unit Tests

### D1. `ViewportRect_test.cpp`

New test file covering:

- All factory methods return correct coordinates.
- `contains()` hit testing edge cases.
- `coversFullWindow()` with quad, half, and partial layouts.
- `toVkViewport()` and `toVkScissor()` pixel rounding.

#### Effort: ~0.5 days

---

### D2. `SceneGroup_test.cpp` additions

Add tests for:

- `createWithViewports()` preserves viewport rects.
- `hasViewports()` returns true/false correctly.
- Viewport rects in a quad layout pass `coversFullWindow()`.

#### Effort: ~0.5 days

---

### D3. `InputScript_test.cpp` additions

Add tests for parsing the new commands:

- `assert rendered_scene_count == 4` → `AssertSceneCount`
- `assert scene "crystal" was_rendered == true` → `AssertScene`
- `wait_frames 10` → `WaitFrames`
- `compare a.png b.png 0.02` → `Compare`
- `set VAR 42` → `Set`
- Invalid assert syntax → parse error with line number.

#### Effort: ~0.5 days

---

## Implementation Order

The work items have a dependency graph that dictates ordering:

```
B6 (exit codes)
 └──▶ B5 (scene draw counters)
       └──▶ B1 (SceneRenderStats)
             ├──▶ B2 (onValidate)
             │     └──▶ B3 (addRenderAssertion)
             └──▶ A1 (assert commands)
                   └──▶ C1 (validation scripts)
                         └──▶ C2 (smoke-test.ps1 update)

A3 (wait_frames) ─── independent
B4 (coversFullWindow) ─── independent
D1/D2/D3 (unit tests) ─── after their feature lands

A2 (screenshot capture) ─── independent
 └──▶ A4 (compare command)

A5 (variable substitution) ─── independent, low priority
```

### Suggested sprint plan

| Sprint | Items | Outcome |
|--------|-------|---------|
| **Week 1** | B6, B5, B1, B2, B3, B4 | Core diagnostics API in place; `onValidate` works |
| **Week 2** | A1, A3, C1, C2, D1, D2, D3 | Assert commands + validation scripts running in CI |
| **Week 3** | A2 | Screenshot capture working |
| **Week 4** | A4 | Golden image comparison working |
| **Backlog** | A5 | Variable substitution (nice-to-have) |

### Total effort estimate

| Track | Items | Days |
|-------|-------|------|
| A (Input Script) | A1–A5 | ~9 |
| B (Game API) | B1–B6 | ~5 |
| C (Smoke Scripts) | C1–C2 | ~1 |
| D (Unit Tests) | D1–D3 | ~1.5 |
| **Total** | | **~16.5 days** |

---

## Files Created or Modified (Summary)

| File | Changes |
|------|---------|
| `include/vde/api/InputScript.h` | New command types: `Assert*`, `WaitFrames`, `Compare`, `Set`; new `ScriptCommand` fields |
| `src/api/InputScript.cpp` | Parser for new commands; variable substitution |
| `include/vde/api/Game.h` | `SceneRenderStats`, `getLastFrameRenderStats()`, `getRenderedSceneCount()`, `onValidate()`, `addRenderAssertion()`, `reportTestFailure()`, `setExitCode()`, `captureScreenshot()` |
| `src/api/Game.cpp` | Stats collection in render methods, assertion evaluation, screenshot capture, exit code management |
| `include/vde/api/Scene.h` | `resetFrameCounters()`, `incrementDrawCalls()`, `getLastFrameDrawCalls()` |
| `src/api/Scene.cpp` | Counter instrumentation |
| `include/vde/api/ViewportRect.h` | `coversFullWindow()` static method |
| `include/vde/VulkanContext.h` | `captureFramebuffer()` |
| `src/VulkanContext.cpp` | Framebuffer readback implementation |
| `examples/ExampleBase.h` | `runExample()` returns `game.getExitCode()` |
| `tools/ToolBase.h` | `runTool()` returns `game.getExitCode()` |
| `examples/four_scene_3d_demo/main.cpp` | `onValidate()` override |
| `examples/quad_viewport_demo/main.cpp` | `onValidate()` override |
| `examples/multi_scene_demo/main.cpp` | `onValidate()` override |
| `examples/parallel_physics_demo/main.cpp` | `onValidate()` override |
| `scripts/input/smoke_four_scene_3d.vdescript` | New validation script |
| `scripts/input/smoke_quad_viewport.vdescript` | New validation script |
| `scripts/input/smoke_parallel_physics.vdescript` | New validation script |
| `scripts/input/smoke_multi_scene_viewport.vdescript` | New validation script |
| `scripts/smoke-test.ps1` | Assert-failure detection, `-Visual` flag |
| `tests/ViewportRect_test.cpp` | New test file |
| `tests/SceneGroup_test.cpp` | New `createWithViewports()` tests |
| `tests/InputScript_test.cpp` | New assert/wait\_frames/compare parse tests |
