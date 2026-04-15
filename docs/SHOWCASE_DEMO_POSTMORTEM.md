# Showcase Demo Post-Mortem: Issues & Improvement Areas

A brutally honest assessment of every issue encountered while building the Cosmic Defender showcase demo, and what each one reveals about the engine, the API, the AI workflow, and the development tooling.

---

## 1. Engine Bugs Discovered

These are defects in the VDE engine itself — not user error, not API design opinions. Broken code.

### 1.1 Camera2D Orthographic Projection Was Silently Ignored (Fixed)

**File:** `src/api/GameCamera.cpp:27`

Before this fix, `GameCamera::applyTo()` unconditionally called `cam.setPerspective(60.0f, ...)` on the VulkanContext camera. That overwrote Camera2D's orthographic projection with a perspective projection. 2D scenes were effectively rendering through a perspective frustum (FOV=60°, camera at z=10), and the Camera2D viewport dimensions were cosmetic — the visible half-height was `tan(30°) × 10 ≈ 5.77`, regardless of what was passed to `Camera2D(viewWidth, viewHeight)`.

**Impact:** `Camera2D(20, 15)` claims the visible Y range is ±7.5, but only ±5.77 is visible. Every existing 2D demo that "works" only works because developers positioned things in the center and never tested the edges. This cost **5+ debugging rounds** of trial-and-error repositioning before the root cause was identified.

**Severity:** Critical. The entire 2D camera system is lying about its coordinate space.

### 1.2 Physics Damping Formula Reverses Velocity

**File:** `src/api/PhysicsScene.cpp:337`

The damping was: `velocity *= (1.0f - linearDamping)`. With `linearDamping = 8.0`, this evaluates to `velocity *= -7.0` — **reversing and amplifying** velocity every physics step. Not only is this not damping, it's an oscillating divergence. The formula is also not timestep-scaled, so behavior varies with frame rate.

**Impact:** Any `linearDamping` value > 1.0 causes catastrophic behavior. The player ship was launched off-screen instantly on any key press. This cost **4 iterations** of reducing the force multiplier from 10× → 3× → 1.5× → 0.5×, each time the AI blamed the magnitude instead of the formula. Every existing physics test set `linearDamping = 0.0f` to avoid the bug, and the one test using `1.0f` only coincidentally works because `1.0 - 1.0 = 0.0`.

**Severity:** Critical. The physics engine's damping is fundamentally broken for its intended use case.

### 1.3 Offscreen Render Pass Incompatible with Pipelines

**File:** `src/VulkanContext.cpp:805-826`

The offscreen render pass (`m_offscreenRenderPass`) was created with **2** subpass dependencies, but all graphics pipelines (sprite, mesh) were created with `m_renderPass` which has **1** dependency. Vulkan requires render pass compatibility when binding pipelines, and dependency count must match.

**Impact:** Every scene transition in every demo produced Vulkan validation errors: `dependencyCount is incompatible between VkRenderPass (from VkCommandBuffer) and VkRenderPass (from VkPipeline), 2 != 1`. This was reported 10+ times per transition before the validation layer's duplicate message limit silenced it.

**Severity:** High. Violated Vulkan spec. Worked by luck on permissive drivers.

### 1.4 `setAspectRatio()` Called After Projection Matrix Is Captured

**File:** `src/api/Game.cpp:2108` (approx)

In `renderMultiViewport()`, the projection matrix is captured on line ~2094 via `getProjectionMatrix()`, but `setAspectRatio(vpAspect)` is called on line ~2108 — after the matrix is already stored in the UBO. For Camera2D this is a no-op anyway (see 1.1), but for perspective cameras this means the aspect ratio correction is always one frame late.

**Severity:** Medium. Subtle visual artifact for perspective cameras in multi-viewport setups.

---

## 2. API Design Issues

These aren't bugs — the code does what it's told. But the API makes it unnecessarily easy to write incorrect code and unnecessarily hard to write correct code.

### 2.1 PhysicsBodyDef.position Silently Overrides Entity Position

When you call `entity->setPosition(5, 3, 0)` and then `entity->createPhysicsBody(def)`, the entity's position is thrown away and replaced with `def.position` (which defaults to `{0, 0}`). Then `syncFromPhysics()` forces z=0 every frame. There is no warning, no assertion, no documentation at the call site.

**Impact:** The player ship, all enemies, and all bullets were invisible because they were at the origin despite explicit `setPosition()` calls. Took multiple debugging rounds to discover.

**Recommendation:** Either: (a) initialize `def.position` from the entity's current position if not explicitly set, (b) warn when `setPosition()` was called before `createPhysicsBody()` with a different position, or (c) remove `def.position` entirely and always use the entity's position.

### 2.2 TextEntity Requires a Non-Obvious Multi-Step Ritual

To get visible text you must call, in order:
1. `setFont(BitmapFont::small())`
2. `setText("...")`
3. `setStyle(TextStyle{...})`
4. `update(0.0f)`  ← not obvious that this is required
5. `sizeToFit(te, height)` ← custom helper, not part of the API

Missing any step produces invisible or incorrectly sized text with no error. `update(0.0f)` is the worst offender — nothing about the name suggests it's required for initial setup.

**Recommendation:** `setText()` or `setFont()` should trigger internal texture generation automatically. At minimum, add an assertion or warning when rendering a TextEntity that hasn't been `update()`d.

### 2.3 SceneGroup Calls onEnter() on Scenes Already Entered

When transitioning from `setActiveScene("arena")` to `setActiveSceneGroup({arena, station})`, the arena scene's `onEnter()` is called again because `setActiveSceneGroup()` diffs against the *previous group's* scene list. If arena was activated via `setActiveScene()`, it's in a single-scene group. The diff sees arena as "new" in the multi-scene group and calls `onEnter()` again.

**Impact:** Entity count doubled (38 → 76). All game state was reset. SceneGroup had to be disabled entirely to make the demo work.

**Recommendation:** Track per-scene entered/exited state independently of the group membership. Don't re-enter a scene that's already active.

### 2.4 No Way to Query Actual Visible Bounds

There is no API to ask "what world-space rectangle is actually visible on screen?" Given the Camera2D bug (1.1), the answer doesn't match what the camera claims. But even in a correct engine, there should be a `camera->getVisibleBounds()` or equivalent.

**Recommendation:** Add `Camera2D::getVisibleRect()` that returns the actual world-space AABB visible on screen.

### 2.5 `applyForce()` Accumulates Per-Frame Without Guidance

The API provides `applyForce()`, `applyImpulse()`, and `setLinearVelocity()`, but there's no documentation or type system guidance about when to use which. `applyForce()` accumulates every frame and interacts with the broken damping formula, making it nearly impossible to tune. Most games want velocity-based movement for player control.

**Recommendation:** Document the difference prominently. Consider a `setDesiredVelocity()` helper that handles force/damping math internally.

---

## 3. AI Agent Failures

These are failures in how the AI (me) approached the work. No excuses.

### 3.1 Generated 700 Lines of Code With ~25 Compile Errors

The initial implementation compiled with approximately 25 errors — wrong function names, missing parameters, incorrect types. This suggests the AI was generating from a stale or incomplete model of the API rather than verifying signatures against the actual headers.

**Root cause:** Insufficient pre-generation verification. Should have read API headers before writing implementations, not after.

### 3.2 Trial-and-Error Instead of Root-Cause Analysis (Camera)

When HUD text was clipped, the response was to iteratively move the text inward: y=6.5 → y=5.0 → y=4.3. **Five rounds** of "try a different number" before finally investigating *why* the coordinates didn't match the camera.

**Root cause:** Lazy debugging. The first time a position didn't match expectations, the correct response was to read `GameCamera::applyTo()` and `Camera::getProjectionMatrix()` to understand the actual coordinate space. Instead, the AI treated it as a tuning problem.

### 3.3 Blamed Force Magnitude Instead of Damping Formula (Physics)

When the player flew off screen on key press, the response was to reduce the force multiplier repeatedly: 10× → 3× → 1.5× → 1.0× → 0.5×. Even at 0.5× with mass=2 and damping=8, the ship was "still too fast." It took the user explicitly asking "why are we tied to frame rate?" to trigger actual investigation of the physics integration code.

**Root cause:** The AI assumed its own code was wrong (force too high) rather than questioning the engine's physics behavior. Should have inspected `applyForce()` → `accumulatedForce` → integration loop on the first anomalous result.

### 3.4 Never Re-Enabled SceneGroup

SceneGroup was disabled as a debugging measure to isolate the double-onEnter bug. It was never re-enabled. The demo's header comment still claims it demonstrates "SceneGroup + ViewportRect (arena + 3D station picture-in-picture)" but this feature doesn't work.

**Root cause:** Lost track of temporary workarounds. The AI should have tracked this as an open item and revisited it.

### 3.5 Didn't Catch the Vulkan Validation Error Proactively

The render pass compatibility error existed from the first transition test. The AI ran `verify.ps1` multiple times and it passed (validation warnings don't fail smoke tests). The user had to point out the Vulkan errors.

**Root cause:** The AI treated "smoke test PASSED" as sufficient and didn't read the warning output. Validation layer warnings should have been investigated on first appearance.

---

## 4. Workflow & Tooling Issues

### 4.1 Smoke Test Script ENTER Key Doesn't Work

The scripted input system (`--input-script`) couldn't trigger the ENTER key to start the game from the title screen. This made it impossible to smoke-test the arena scene at all — the smoke test only validates the title screen.

**Impact:** The most complex scene in the demo (arena with physics, HUD, enemies) has zero automated test coverage.

### 4.2 No Validation Layer Warning Gate in Smoke Tests

Smoke tests check exit code but don't fail on Vulkan validation layer errors. This means render pass incompatibilities, descriptor issues, and synchronization bugs ship silently as long as the process doesn't crash. The Vulkan validation errors in this demo were only caught because the user manually looked at the output.

**Recommendation:** Add a `-Strict` mode to smoke tests that treats any Vulkan validation ERROR or WARNING (excluding known third-party layers like GalaxyOverlay) as a failure.

### 4.3 Existing 2D Demos Mask Camera Bug

Every existing 2D demo works with the broken Camera2D because they all center content with generous margins. The `breakout_demo`, `physics_demo`, `sidescroller`, and `sprite_demo` all use Camera2D with coordinates that happen to fall within the perspective frustum's visible area. Nobody has tested content near the stated viewport edges.

**Recommendation:** Add a test/demo that renders markers at the exact corners of the Camera2D viewport to verify they're visible. This would have caught bug 1.1 immediately.

### 4.4 No Physics Integration Tests With High Damping

Every physics test in `PhysicsScene_test.cpp` used `linearDamping = 0.0f`. The only test with non-zero damping used exactly `1.0f` (which coincidentally zeroes velocity). There was zero test coverage for the most common use case: moderate damping values (2-10) used for player movement.

---

## 5. Remaining Open Items

These issues exist in the demo as of this writing and have not been resolved:

| Item | Status | Impact |
|------|--------|--------|
| SceneGroup disabled (no 3D station PIP viewport) | **Open** | Major advertised feature doesn't work |
| Camera2D applyTo() still forces perspective | **Open** | All 2D scenes use workaround (compute frustum manually) |
| setAspectRatio() called after matrix capture | **Open** | One-frame-late aspect ratio for perspective cameras |
| SceneGroup double-onEnter bug | **Open** | Can't use SceneGroup without triggering duplicate setup |
| Smoke test can't test arena scene (ENTER key) | **Open** | Primary gameplay scene has no automated coverage |
| `kArenaW`/`kArenaH` constants are vestigial | **Minor** | Still defined but only used for physics walls, not camera |

---

## 6. Summary of Root Causes

| Category | Count | Pattern |
|----------|-------|---------|
| Engine bugs | 4 | Core rendering/physics systems with zero test coverage at edges |
| API foot-guns | 5 | Silent failures, non-obvious initialization order, missing queries |
| AI failures | 5 | Trial-and-error over investigation, lost context, missed warnings |
| Tooling gaps | 4 | Missing strictness modes, scripted input limitations, masked bugs |

The overarching theme: **the engine works for the happy path and falls apart at the edges.** Camera2D works if you don't use the edges. Physics works if damping is zero. Transitions work if you don't check validation. SceneGroup works if you only use it from startup. The showcase demo's purpose was to exercise every API feature together, and it exposed exactly these gaps.
