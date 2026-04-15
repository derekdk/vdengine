# Postmortem Remediation Plan

Remediation plan for all open issues from [SHOWCASE_DEMO_POSTMORTEM.md](SHOWCASE_DEMO_POSTMORTEM.md).

## Issue Status Summary

This table reflects the current repository status after the fixes implemented in this PR. The phase-by-phase sections below remain as remediation notes/history.

| # | Postmortem Ref | Issue | Current Status | Severity |
|---|---|---|---|---|
| R1 | 1.1 | Camera2D orthographic projection silently overridden by `GameCamera::applyTo()` | **Fixed in codebase** | Critical |
| R2 | 1.4 | `setAspectRatio()` called after projection matrix captured in `renderMultiViewport()` | **Fixed in codebase** | Medium |
| R3 | 2.3 | SceneGroup re-enters already-active scenes due to stale group state | **Fixed in codebase** | High |
| R4 | 2.1 | `PhysicsBodyDef.position` silently overrides entity position | **Fixed in codebase** | High |
| R5 | 2.2 | TextEntity requires non-obvious `update(0)` call before text is visible | **Open** | Medium |
| R6 | 2.4 | No `Camera2D::getVisibleRect()` API to query actual visible bounds | **Fixed in codebase** | Medium |
| R7 | 2.5 | No velocity-based movement helper; `applyForce()` hard to tune | **Fixed in codebase** | Medium |
| R8 | 5 | Showcase demo: SceneGroup disabled, workarounds in place, vestigial constants | **Open** | Medium |

---

## Phase 1: Critical Rendering Fixes (R1, R2)

These fix the core rendering pipeline. All downstream work depends on correct camera projection.

### 1a. Fix Camera2D orthographic projection in `GameCamera::applyTo()` (R1)

**Problem:** `GameCamera::applyTo()` unconditionally calls `cam.setPerspective(60.0f, ...)`, overwriting Camera2D's orthographic projection. Every 2D scene renders through a perspective frustum.

**Files:**
- `src/api/GameCamera.cpp` — `applyTo()` method
- `include/vde/api/GameCamera.h` — Camera2D class, GameCamera base

**Fix:** Camera2D overrides `applyTo()` to call `cam.setOrthographic()` (or equivalent) instead of `cam.setPerspective()`. The base `GameCamera::applyTo()` keeps the perspective path. Camera2D computes the orthographic matrix from its `viewWidth`, `viewHeight`, zoom, and position, and applies it to the VulkanContext camera.

**Approach:**
1. Add a `virtual applyTo()` override in Camera2D that sets the VulkanContext camera to orthographic projection using the Camera2D's viewport dimensions, zoom, and position.
2. The base GameCamera::applyTo() remains unchanged (perspective path).
3. Ensure `Camera::setOrthographic()` exists on the low-level camera, or add it if missing.

**Tests:**
- Unit test: Camera2D→applyTo() produces an orthographic projection matrix that matches the declared viewport dimensions.
- Unit test: Points at the declared Camera2D viewport edges project to normalized device coordinates within [-1, 1].

### 1b. Fix `setAspectRatio()` ordering in `renderMultiViewport()` (R2)

**Problem:** In `Game::renderMultiViewport()`, the projection matrix is captured before `setAspectRatio()` is called, so the aspect ratio correction is always one frame late.

**Files:**
- `src/api/Game.cpp` — `renderMultiViewport()` (~lines 2072-2130)

**Fix:** Move `setAspectRatio()` call *before* the projection matrix is captured. The sequence must be:
1. `setAspectRatio(vpAspect)`
2. `applyTo(context)`
3. `getProjectionMatrix()` → store in UBO

**Tests:**
- Unit test: After `setAspectRatio()` + `applyTo()`, the captured projection matrix reflects the new aspect ratio (not the previous one).

---

## Phase 2: State Management Fixes (R3, R4)

These fix silent state corruption bugs in the scene and physics systems.

### 2a. Track per-scene active state to fix SceneGroup double-onEnter (R3)

**Problem:** `setActiveScene("arena")` defers the scene switch via `m_pendingScene`. If `setActiveSceneGroup({arena, station})` is called afterward but before `processPendingSceneChange()` runs, `m_activeSceneGroup` is stale. The diff treats "arena" as new and calls `onEnter()` again, duplicating entities and resetting state.

Even after the pending change processes, the fundamental issue is that group membership is used as a proxy for active state, which breaks across `setActiveScene()` → `setActiveSceneGroup()` transitions.

**Decision:** Track per-scene active state independently of group membership.

**Files:**
- `src/api/Game.cpp` — `setActiveScene()`, `setActiveSceneGroup()`, `processPendingSceneChange()`
- `include/vde/api/Game.h` — Add `m_activeSceneNames` (set of currently-entered scene names)

**Fix:**
1. Add `std::unordered_set<std::string> m_activeSceneNames` to Game.
2. When a scene's `onEnter()` is called, add its name to `m_activeSceneNames`.
3. When a scene's `onExit()` is called, remove its name from `m_activeSceneNames`.
4. In `setActiveSceneGroup()`, use `m_activeSceneNames` to determine which scenes need `onEnter()`/`onExit()` instead of diffing group sceneNames lists.
5. In `processPendingSceneChange()`, similarly use `m_activeSceneNames`.

**Tests:**
- `setActiveScene("A")` → process → `setActiveSceneGroup({"A", "B"})`: A's `onEnter()` must be called exactly once total.
- `setActiveSceneGroup({"A", "B"})` → `setActiveSceneGroup({"A", "C"})`: A stays active (no re-enter), B exits, C enters.
- `setActiveScene("A")` → (pending, not processed) → `setActiveSceneGroup({"A", "B"})`: A's `onEnter()` still called exactly once.

### 2b. Auto-inherit entity position in `PhysicsBodyDef` (R4)

**Problem:** `entity->setPosition(5, 3, 0)` followed by `entity->createPhysicsBody(def)` silently discards the entity's position because `def.position` defaults to `{0, 0}`.

**Decision:** Auto-inherit entity position if `def.position` is not explicitly set.

**Files:**
- `include/vde/api/PhysicsTypes.h` — `PhysicsBodyDef`
- `src/api/Entity.cpp` or `src/api/PhysicsScene.cpp` — `createPhysicsBody()`

**Fix:**
1. Change `PhysicsBodyDef::position` default to a sentinel value (e.g., `{NaN, NaN}` or add a `bool positionSet = false` flag).
2. In `createPhysicsBody()`, if `def.position` was not explicitly set, initialize it from the entity's current XY position.
3. When `def.position` is explicitly set (detected by non-NaN or `positionSet == true`), use it as before.

**Design detail — sentinel vs flag:**
- `std::optional<glm::vec2> position` is cleanest but changes the struct layout.
- A `bool positionExplicit = false` flag alongside `position` is simple and backward-compatible. Auto-set to `true` when user writes `def.position = {...}` is not possible without a setter.
- Recommend: Change type to `std::optional<glm::vec2> position = std::nullopt`. In `createPhysicsBody()`, resolve to entity position if nullopt. This is a breaking change for code that reads `def.position` directly — but that's rare.

**Tests:**
- `setPosition(5, 3, 0)` + `createPhysicsBody(def)` with default position → physics body at (5, 3).
- `createPhysicsBody(def)` with explicit `def.position = {7, 2}` → physics body at (7, 2).
- Entity with no prior `setPosition()` + default `def.position` → physics body at (0, 0).

---

## Phase 3: API Usability Improvements (R5, R6, R7)

These improve the developer experience without changing core behavior.

### 3a. TextEntity lazy auto-rebuild (R5)

**Problem:** TextEntity requires calling `update(0.0f)` before text becomes visible. The `update()` method name doesn't communicate its role in initialization.

**Decision:** Auto-rebuild on dirty — mark dirty on `setText()`/`setFont()`/`setStyle()`, rebuild texture on next render frame if dirty. No manual `update()` needed.

**Files:**
- `src/api/TextEntity.cpp` — `setText()`, `setFont()`, `setStyle()`, `update()`, `render()` (or wherever draw happens)
- `include/vde/api/TextEntity.h`

**Fix:**
1. `setText()`, `setFont()`, `setStyle()` already call `markDirty()`.
2. Move the `if (m_dirty) { rebuildTexture(); m_dirty = false; }` check from `update()` into the render path (e.g., `onDraw()` or the beginning of the sprite render call).
3. Alternatively, trigger rebuild at the start of `update()` AND at render time, so it works either way.
4. Ensure `rebuildTexture()` is safe to call during render (it should be, as it generates a CPU-side texture that then gets uploaded).

**Tests:**
- Create TextEntity, call `setFont()` + `setText()`, render without calling `update()` → text is visible (texture is non-placeholder).
- Change text via `setText()`, render → new text is visible without explicit `update()`.

### 3b. Add `Camera2D::getVisibleRect()` (R6)

**Problem:** No API to query the world-space rectangle actually visible on screen.

**Files:**
- `include/vde/api/GameCamera.h` — Camera2D class
- `src/api/GameCamera.cpp`

**Fix:**
1. Add `struct Rect2D { float left, right, bottom, top; }` (or use an existing AABB type).
2. Add `Rect2D Camera2D::getVisibleRect() const` that computes the world-space AABB from viewport dimensions, zoom, and camera position.
3. Calculation: `halfW = viewWidth / (2 * zoom)`, `halfH = viewHeight / (2 * zoom)`, rect = `{pos.x - halfW, pos.x + halfW, pos.y - halfH, pos.y + halfH}`.

**Tests:**
- Default Camera2D(20, 15) at origin → visible rect is (-10, 10, -7.5, 7.5).
- Camera2D at position (5, 3) → rect is shifted by (5, 3).
- Camera2D with zoom=2 → visible rect is half-size.

### 3c. Add `setDesiredVelocity()` + force/impulse documentation (R7)

**Problem:** `applyForce()` accumulates per-frame and interacts poorly with damping for player movement. No guidance on when to use force vs impulse vs direct velocity.

**Decision:** Add `setDesiredVelocity()` AND improve documentation.

**Files:**
- `include/vde/api/PhysicsTypes.h` or `include/vde/api/PhysicsScene.h`
- `src/api/PhysicsScene.cpp`
- `API-DOC.md` — Add force/impulse/velocity guidance section

**Fix:**
1. Add `PhysicsScene::setDesiredVelocity(PhysicsBodyId, glm::vec2 targetVelocity, float acceleration)` that smoothly accelerates toward the target velocity.
2. Implementation: computes the required force internally based on mass and acceleration parameter, applies it, and leverages damping to prevent overshoot.
3. Document in API-DOC.md when to use each method:
   - `setLinearVelocity()` — Teleport-style instant speed change (e.g., knockback).
   - `applyImpulse()` — One-shot momentum change (e.g., jump, explosion).
   - `applyForce()` — Continuous force (e.g., gravity, wind, thrusters).
   - `setDesiredVelocity()` — Gameplay movement (e.g., player walk/run).

**Tests:**
- `setDesiredVelocity({5, 0}, 10)` → after several steps, velocity converges toward (5, 0).
- `setDesiredVelocity({0, 0}, 10)` → body decelerates to rest.

---

## Phase 4: Showcase Demo Rehabilitation (R8)

**Depends on:** Phases 1-3 (all engine fixes must be in place).

### 4a. Re-enable SceneGroup in showcase demo

**Files:**
- `examples/showcase_demo/main.cpp`

**Changes:**
1. Remove the `checkPendingSceneGroup()` workaround that disables SceneGroup.
2. Enable the arena + station SceneGroup with PIP viewport.
3. Remove any Camera2D workarounds that compensated for the broken projection.
4. Clean up vestigial `kArenaW`/`kArenaH` constants if they're no longer used for anything meaningful.

### 4b. Verify showcase demo features

- Title screen → ENTER → arena with HUD, enemies, physics.
- SceneGroup active: arena (main viewport) + station (PIP).
- Camera2D viewport edges match declared dimensions.
- Transitions between scenes work without validation errors.

---

## Phase 5: Full Verification

1. Build (all targets).
2. Run all unit tests.
3. Run all smoke tests.
4. Verify no Vulkan validation layer warnings/errors in output.
5. Manual spot-check of showcase demo.

---

## Execution Order & Dependencies

```
Phase 1a (Camera2D applyTo)
    └──→ Phase 1b (setAspectRatio ordering) ← depends on camera changes
              └──→ Phase 2a (SceneGroup active tracking) ← independent but sequential
                        └──→ Phase 2b (PhysicsBodyDef position) ← independent but sequential
                                  └──→ Phase 3a (TextEntity auto-rebuild)
                                  └──→ Phase 3b (Camera2D::getVisibleRect) ← depends on Phase 1a
                                  └──→ Phase 3c (setDesiredVelocity)
                                            └──→ Phase 4 (Showcase demo rehab) ← depends on all above
                                                      └──→ Phase 5 (Full verification)
```

Phase 3 items (3a, 3b, 3c) can be done in parallel. All other phases are sequential.

---

## Risk Notes

- **Phase 1a is the highest-risk change.** Modifying `applyTo()` affects every 2D scene in every demo.  All existing demos that "work" do so by coincidence within the perspective frustum. The fix will change actual rendered positions, so demos may need coordinate adjustments.
- **Phase 2b (`std::optional<glm::vec2>`)** is a minor breaking change. Any code that reads `def.position` directly (not through the physics body state) will need updating. Grep for `def.position` usage before implementing.
- **Phase 4** should not be attempted until all engine fixes are verified, to avoid debugging engine bugs through the demo.
