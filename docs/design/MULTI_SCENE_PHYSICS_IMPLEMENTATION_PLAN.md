# Multi-Scene Physics & Scheduler — Implementation Plan

> **Design Reference:** [MULTI_SCENE_PHYSICS_DESIGN.md](MULTI_SCENE_PHYSICS_DESIGN.md)  
> **API Gap Analysis:** [API_SUGGESTION_SPLIT_SCREEN_VIEWPORTS.md](../API_SUGGESTION_SPLIT_SCREEN_VIEWPORTS.md)  
> **Created:** 2026-02-08  
> **Last Updated:** 2026-02-08  
> **Status:** Complete

---

## Progress Summary

| Phase | Description | Status | Tests Pass | Examples Work |
|-------|-------------|--------|------------|---------------|
| 0 | [Pre-flight & Baseline](#phase-0-pre-flight--baseline) | **Done** | 423/423 | 10/10 |
| 1 | [Scheduler Foundation (Single-Threaded)](#phase-1-scheduler-foundation-single-threaded) | **Done** | 444/444 | 10/10 |
| 2 | [Multi-Scene Support](#phase-2-multi-scene-support) | **Done** | 457/457 | 10/10 |
| 3 | [Per-Scene Viewports & Split-Screen Rendering](#phase-3-per-scene-viewports--split-screen-rendering) | **Done** | 489/489 | 10/10 |
| 4 | [Scene Phase Callbacks & Audio Event Queue](#phase-4-scene-phase-callbacks--audio-event-queue) | **Done** | 513/513 | 10/10 |
| 5 | [Physics Scene](#phase-5-physics-scene) | **Done** | 545/545 | 11/11 |
| 6 | [Physics Entities & Sync](#phase-6-physics-entities--sync) | **Done** | 568/568 | 11/11 |
| 7 | [Thread Pool & Parallel Physics](#phase-7-thread-pool--parallel-physics) | **Done** | 589/589 | 12/12 |
| 8 | [Advanced Physics Features](#phase-8-advanced-physics-features) | **Done** | 604/604 | 13/13 |

---

## Phase 0: Pre-flight & Baseline

**Goal:** Confirm every existing test passes and every existing example builds+runs before making any changes. Establish the baseline so regressions are immediately detectable.

### Tasks

- [x] **0.1** Build all targets (library, tests, examples) with Ninja in Debug
- [x] **0.2** Run `vde_tests` — all tests green (423/423 passed)
- [x] **0.3** Build and launch each example to verify they run:
  - [x] `vde_triangle_example`
  - [x] `vde_simple_game_example`
  - [x] `vde_sprite_demo`
  - [x] `vde_world_bounds_demo`
  - [x] `vde_materials_lighting_demo`
  - [x] `vde_resource_demo`
  - [x] `vde_audio_demo`
  - [x] `vde_sidescroller`
  - [x] `vde_wireframe_viewer`
  - [x] `vde_multi_scene_demo`
- [x] **0.4** Document any pre-existing failures or warnings — none found

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
```

All tests pass. All examples launch without crash.

### Completion Criteria

- [x] No regressions from current state
- [x] Baseline documented in this section

**Baseline (2026-02-08):** 423 tests passed, 10 examples built, CMake deprecation warnings from GLM (cosmetic only).

---

## Phase 1: Scheduler Foundation (Single-Threaded)

**Goal:** Introduce the `Scheduler` class with single-threaded task graph execution. Refactor `Game::run()` to dispatch through the scheduler. **No functional change to existing behavior.**

### New Files

| File | Purpose |
|------|---------|
| `include/vde/api/Scheduler.h` | `Scheduler`, `TaskId`, `TaskDescriptor`, `TaskPhase` |
| `src/api/Scheduler.cpp` | Topological sort, single-threaded `execute()` |
| `tests/Scheduler_test.cpp` | Unit tests for scheduler |

### Modified Files

| File | Change |
|------|--------|
| `include/vde/api/Game.h` | Add `Scheduler` member, `getScheduler()` accessor |
| `src/api/Game.cpp` | Refactor `run()` to call `m_scheduler.execute()` |
| `include/vde/api/GameAPI.h` | Add `#include "Scheduler.h"` |
| `CMakeLists.txt` | Add new source/header files |
| `tests/CMakeLists.txt` | Add `Scheduler_test.cpp` |

### Tasks

- [x] **1.1** Create `TaskPhase` enum, `TaskId`, `TaskDescriptor` structs in `Scheduler.h`
- [x] **1.2** Implement `Scheduler` class with `addTask()`, `removeTask()`, `execute()`, `clear()`
  - Topological sort based on `dependsOn` edges
  - Single-threaded execution in sorted order
  - Phase-based ordering as tiebreaker (Input < Physics < ... < Render)
- [x] **1.3** Add `Scheduler m_scheduler` to `Game`; build a default single-scene graph that mirrors the existing `run()` loop body exactly
- [x] **1.4** Refactor `Game::run()` so the loop body becomes: `processInput() → m_scheduler.execute() → drawFrame()`
  - The scheduler graph contains tasks for: Update (calls `scene->update()`), Audio, PreRender (camera apply, etc.), Render
- [x] **1.5** Write `Scheduler_test.cpp`:
  - Task registration and ID uniqueness
  - Topological sort correctness (diamond dependencies, linear chain)
  - Phase ordering as tiebreaker
  - `clear()` empties the graph
  - `removeTask()` removes a single task
  - Cycle detection (throw on cycle)
  - Empty graph execute is a no-op
- [x] **1.6** Register new files in `CMakeLists.txt` and `tests/CMakeLists.txt`

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
# Plus run every existing example to verify zero behavioral change
```

### Completion Criteria

- [x] All existing tests pass (no regressions)
- [x] New `Scheduler_test` tests pass (21 tests)
- [x] All existing examples run identically to Phase 0 baseline
- [x] `Game::run()` delegates to `Scheduler::execute()`

---

## Phase 2: Multi-Scene Support

**Goal:** Allow multiple scenes to be active simultaneously via `SceneGroup`. The scheduler builds a multi-scene graph. Scenes can be tagged for background updates.

### New Files

| File | Purpose |
|------|---------|
| `include/vde/api/SceneGroup.h` | `SceneGroup` struct |
| `tests/SceneGroup_test.cpp` | Unit tests |

### Modified Files

| File | Change |
|------|--------|
| `include/vde/api/Scene.h` | Add `m_continueInBackground`, `m_updatePriority`, setters/getters |
| `include/vde/api/Game.h` | Add `setActiveSceneGroup()`, `getActiveSceneGroup()`, scene ordering helpers |
| `src/api/Game.cpp` | `setActiveScene()` wraps group-of-one; `rebuildSchedulerGraph()` supports N scenes |
| `src/api/Scene.cpp` | Implement new fields |
| `include/vde/api/GameAPI.h` | Add `#include "SceneGroup.h"` |
| `CMakeLists.txt` | Add new files |
| `tests/CMakeLists.txt` | Add `SceneGroup_test.cpp` |

### Tasks

- [x] **2.1** Create `SceneGroup` struct in `SceneGroup.h` with `name`, `sceneNames`, `SceneGroup::create()` helper
- [x] **2.2** Add `setContinueInBackground()` / `getContinueInBackground()` and `setUpdatePriority()` / `getUpdatePriority()` to `Scene`
- [x] **2.3** Add `setActiveSceneGroup()` to `Game`; refactor `setActiveScene()` to create a single-scene group internally
- [x] **2.4** Implement `rebuildSchedulerGraph()` in `Game.cpp` that creates tasks for each scene in the active group:
  - Input → Update(scene1) → Update(scene2) → ... → PreRender(scene1) → PreRender(scene2) → Render
  - Background scenes also get Update tasks
- [x] **2.5** Wire `setActiveSceneGroup` / `setActiveScene` to call `rebuildSchedulerGraph()`
- [x] **2.6** Write `SceneGroup_test.cpp`:
  - Construction & `create()` helper
  - Empty group
  - Verify scene ordering
- [x] **2.7** Add Scene background/priority tests to `Scene_test.cpp`
- [x] **2.8** Update `multi_scene_demo` example to demonstrate `setActiveSceneGroup()` with two scenes rendering simultaneously

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
# Run multi_scene_demo and verify both scenes render in the same frame
# Run all other examples to verify no regressions
```

### Completion Criteria

- [x] All existing tests pass
- [x] New tests pass
- [x] `setActiveScene("x")` behaves identically to before (backwards compatible)
- [x] `setActiveSceneGroup(...)` activates multiple scenes
- [x] `multi_scene_demo` visually demonstrates simultaneous scene rendering
- [x] All other examples run without regression

**Completed (2026-02-08):** 457 tests passed (444 existing + 9 SceneGroup + 4 Scene background/priority), 10 examples built and verified.

---

## Phase 3: Per-Scene Viewports & Split-Screen Rendering

**Goal:** Enable true split-screen rendering where each scene in a `SceneGroup` renders to its own viewport sub-region with its own camera. Add input focus routing so interactive split-screen is possible. This addresses the deficiencies identified in [API_SUGGESTION_SPLIT_SCREEN_VIEWPORTS.md](../API_SUGGESTION_SPLIT_SCREEN_VIEWPORTS.md).

**Motivation:** Phase 2 added `SceneGroup` for simultaneous multi-scene updates, but all scenes render through a single full-window viewport using only the primary scene's camera. The `quad_viewport_demo` shows the limitation — it simulates split-screen in a single scene with manual entity positioning, precluding independent 3D cameras and coordinate systems.

### New Files

| File | Purpose |
|------|---------|
| `include/vde/api/ViewportRect.h` | `ViewportRect` struct (normalized 0-1 coordinates) |
| `tests/ViewportRect_test.cpp` | Unit tests for ViewportRect |

### Modified Files

| File | Change |
|------|--------|
| `include/vde/api/Scene.h` | Add `setViewportRect()`, `getViewportRect()`, `ViewportRect m_viewportRect` |
| `src/api/Scene.cpp` | Default viewport (0,0,1,1 = full window) |
| `include/vde/api/Game.h` | Add `setFocusedScene()`, `getFocusedScene()`, `getSceneAtScreenPosition()` |
| `src/api/Game.cpp` | Update `rebuildSchedulerGraph()` render task: per-scene viewport/scissor + camera; implement focus tracking and mouse-to-viewport mapping |
| `include/vde/api/SceneGroup.h` | Add optional `ViewportRect` to `SceneGroupEntry` struct for layout-based viewport assignment |
| `include/vde/VulkanContext.h` | Add `getSwapChainExtent()` accessor (if not already public) |
| `include/vde/api/GameAPI.h` | Add `#include "ViewportRect.h"` |
| `CMakeLists.txt` | Add new files |
| `tests/CMakeLists.txt` | Add `ViewportRect_test.cpp` |

### Tasks

- [x] **3.1** Define `ViewportRect` struct in `ViewportRect.h`:
  - `float x, y, width, height` — normalized [0,1] coordinates (origin top-left)
  - `static ViewportRect fullWindow()` — returns `{0, 0, 1, 1}`
  - `static ViewportRect topLeft()` — returns `{0, 0, 0.5, 0.5}`
  - `static ViewportRect topRight()` — returns `{0.5, 0, 0.5, 0.5}`
  - `static ViewportRect bottomLeft()` — returns `{0, 0.5, 0.5, 0.5}`
  - `static ViewportRect bottomRight()` — returns `{0.5, 0.5, 0.5, 0.5}`
  - `static ViewportRect leftHalf()` / `rightHalf()` / `topHalf()` / `bottomHalf()`
  - `bool contains(float normalizedX, float normalizedY) const` — hit test for input routing
  - `VkViewport toVkViewport(uint32_t swapchainWidth, uint32_t swapchainHeight) const`
  - `VkRect2D toVkScissor(uint32_t swapchainWidth, uint32_t swapchainHeight) const`
- [x] **3.2** Add `setViewportRect()` / `getViewportRect()` to `Scene`:
  - Default is `ViewportRect::fullWindow()` — backwards compatible
  - Stored as `ViewportRect m_viewportRect` member
- [x] **3.3** Update `SceneGroup` to support optional viewport layout:
  - Add `SceneGroupEntry` struct: `{ std::string sceneName; ViewportRect viewport; }`
  - Add `SceneGroup::createWithViewports()` factory that accepts entries with explicit viewports
  - Existing `SceneGroup::create()` continues to work (all scenes get `fullWindow()` viewport)
  - When `setActiveSceneGroup()` is called with entries that have viewports, apply them to the scenes
- [x] **3.4** Update `rebuildSchedulerGraph()` render task to handle per-scene viewports:
  - For each scene in the active group:
    1. Compute `VkViewport` and `VkRect2D` from the scene's `ViewportRect` and swapchain extent
    2. Call `vkCmdSetViewport()` and `vkCmdSetScissor()` on the command buffer
    3. Apply the scene's camera to the uniform buffer (update UBO with scene's view/projection)
    4. Update lighting UBO for the scene's `LightBox`
    5. Call `scene->render()`
  - The render callback already receives `VkCommandBuffer cmd` — use it for viewport/scissor calls
  - The primary scene's clear color is still used for the render pass clear (unchanged)
- [x] **3.5** Update `rebuildSchedulerGraph()` PreRender task:
  - Remove the single-camera-apply logic; camera apply moves into the render loop (per-scene)
  - PreRender still sets the clear color from the primary scene
- [x] **3.6** Add input focus tracking to `Game`:
  - `void setFocusedScene(const std::string& sceneName)` — sets which scene receives keyboard input
  - `Scene* getFocusedScene()` — returns the focused scene (defaults to primary)
  - `Scene* getSceneAtScreenPosition(double mouseX, double mouseY)` — maps pixel coords to scene via `ViewportRect::contains()`
  - Update `processInput()`: route keyboard events to focused scene's input handler; route mouse events to the scene under the cursor
  - Default behavior: if no focused scene is set, input goes to the primary scene (backwards compatible)
- [x] **3.7** Write `ViewportRect_test.cpp`:
  - Default construction is full window
  - Static factory methods produce correct coordinates
  - `contains()` hit testing (interior, edge, exterior)
  - `toVkViewport()` produces correct pixel values for various swapchain sizes
  - `toVkScissor()` produces correct pixel values
  - Quad layout (4 quadrants tile the full window with no overlap)
- [x] **3.8** Add per-scene viewport/camera rendering tests to `Scene_test.cpp`:
  - Scene with custom viewport rect stores and retrieves it
  - Default viewport is full window
- [x] **3.9** Upgrade `quad_viewport_demo` to use true per-scene viewports:
  - Create 4 separate scenes (Space, Forest, City, Ocean) instead of 1
  - Each scene has its own camera and coordinate system
  - Use `SceneGroup::createWithViewports()` to assign quadrant viewports
  - Each scene has its own background color (drawn as a fullscreen quad within its viewport)
  - Input focus switches between quadrants (1-4 keys or mouse click)
  - Demonstrates independent 3D cameras per quadrant
- [x] **3.10** Update `multi_scene_demo` to optionally demonstrate viewport support:
  - Add a mode toggle that switches between full-screen scene switching and side-by-side viewport rendering

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
# Run quad_viewport_demo — each quadrant should be an independent scene with its own camera
# Run multi_scene_demo — existing behavior unchanged; optional viewport mode works
# Run all other examples — single-scene viewport (fullWindow) is default, no visual change
```

### Completion Criteria

- [x] All existing tests pass
- [x] New `ViewportRect_test` tests pass (28 tests)
- [x] Scene viewport tests pass (4 new tests in Scene_test.cpp)
- [x] `quad_viewport_demo` renders 4 independent scenes with independent cameras/viewports
- [x] Input focus correctly routes to the focused/hovered scene
- [x] Default viewport is fullWindow — all existing examples render identically to Phase 2
- [x] `setActiveScene("x")` still works (single scene gets fullWindow viewport)
- [x] All other examples run without regression

---

## Phase 4: Scene Phase Callbacks & Audio Event Queue

**Goal:** Split `Scene::update()` into `updateGameLogic()`, `updateAudio()`, `updateVisuals()`. Add audio event queue to Scene. Move `AudioManager::update()` into the scheduler as `audio.global`.

### Modified Files

| File | Change |
|------|--------|
| `include/vde/api/Scene.h` | Add `updateGameLogic()`, `updateAudio()`, `updateVisuals()`, `enablePhaseCallbacks()`, `usesPhaseCallbacks()`, `AudioEvent` queue, `playSFX()`, `playSFXAt()`, `queueAudioEvent()` |
| `src/api/Scene.cpp` | Implement phase callbacks, audio queue drain in default `updateAudio()` |
| `src/api/Game.cpp` | Update `rebuildSchedulerGraph()` to split GameLogic/Audio/Visual tasks when `usesPhaseCallbacks()` is true; add `audio.global` task |
| `include/vde/api/AudioEvent.h` | `AudioEvent` struct (optional — could be in Scene.h) |
| `tests/Scene_test.cpp` | Add phase callback tests |
| `tests/AudioEvent_test.cpp` | Unit tests for AudioEvent queue |
| `tests/CMakeLists.txt` | Add new test files |

### Tasks

- [x] **4.1** Define `AudioEvent` struct (Type enum, clip, volume, pitch, loop, position, soundId)
- [x] **4.2** Add `enablePhaseCallbacks()`, `usesPhaseCallbacks()`, and the three virtual methods to `Scene`:
  - `virtual void updateGameLogic(float deltaTime)` — no-op default
  - `virtual void updateAudio(float deltaTime)` — default drains audio queue
  - `virtual void updateVisuals(float deltaTime)` — no-op default
- [x] **4.3** Add `queueAudioEvent()`, `playSFX()`, `playSFXAt()` convenience methods to `Scene`
- [x] **4.4** Implement default `updateAudio()` to drain `m_audioEventQueue` via `AudioManager`
- [x] **4.5** Ensure backwards compatibility: if `usesPhaseCallbacks()` returns false, scheduler calls `update()` as a single task in the GameLogic phase — identical to current behavior
- [x] **4.6** Update `rebuildSchedulerGraph()`:
  - For scenes with `usesPhaseCallbacks() == true`: create separate GameLogic, Audio, Visual tasks with proper dependencies
  - For legacy scenes: single Update task in GameLogic phase
  - Add `audio.global` task that calls `AudioManager::getInstance().update()` after all per-scene audio tasks
- [x] **4.7** Write `AudioEvent_test.cpp`:
  - AudioEvent construction with various types
  - Queue add/drain cycle
  - Empty queue drain is safe
- [x] **4.8** Add phase callback tests to `Scene_test.cpp`:
  - Default scene does not use phase callbacks
  - Scene with `enablePhaseCallbacks()` reports `usesPhaseCallbacks() == true`
  - Phase callbacks are called in correct order (mock/stub test)

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
# Run all examples — they all use legacy update(), so behavior is unchanged
# Specifically verify audio_demo still plays sounds correctly
```

### Completion Criteria

- [x] All existing tests pass
- [x] New AudioEvent and phase callback tests pass
- [x] `audio_demo` example works identically
- [x] All other examples work identically (they use `update()`, not phase callbacks)
- [x] `AudioManager::update()` is called via scheduler `audio.global` task, not inline in `Game::run()`

**Completed (2026-02-08):** 513 tests passed (489 existing + 15 AudioEvent + 5 Scene audio queue + 4 Scene phase callback), 10 examples built and verified.

---

## Phase 5: Physics Scene

**Goal:** Implement `PhysicsScene` with fixed-timestep accumulator, body management, and AABB collision detection. Physics is opt-in per scene.

### New Files

| File | Purpose |
|------|---------|
| `include/vde/api/PhysicsTypes.h` | `PhysicsConfig`, `PhysicsBodyDef`, `PhysicsBodyState`, `PhysicsShape`, `PhysicsBodyType`, `PhysicsBodyId`, `CollisionEvent` |
| `include/vde/api/PhysicsScene.h` | `PhysicsScene` class |
| `src/api/PhysicsScene.cpp` | Accumulator, body storage, broadphase, impulse resolution |
| `tests/PhysicsScene_test.cpp` | Unit tests |

### Modified Files

| File | Change |
|------|--------|
| `include/vde/api/Scene.h` | Add `enablePhysics()`, `disablePhysics()`, `hasPhysics()`, `getPhysicsScene()` |
| `src/api/Scene.cpp` | Physics ownership |
| `src/api/Game.cpp` | Insert Physics and PostPhysics tasks in scheduler when scene has physics |
| `include/vde/api/GameAPI.h` | Add `#include "PhysicsScene.h"`, `#include "PhysicsTypes.h"` |
| `CMakeLists.txt` | Add new files |
| `tests/CMakeLists.txt` | Add `PhysicsScene_test.cpp` |

### Tasks

- [x] **5.1** Define all physics types in `PhysicsTypes.h`:
  - `PhysicsConfig` (fixedTimestep, gravity, maxSubSteps, iterations)
  - `PhysicsBodyId`, `INVALID_PHYSICS_BODY_ID`
  - `PhysicsShape` enum (Box, Circle, Sphere, Capsule)
  - `PhysicsBodyType` enum (Static, Dynamic, Kinematic)
  - `PhysicsBodyDef` (type, shape, position, rotation, extents, mass, friction, restitution, damping, isSensor)
  - `PhysicsBodyState` (position, rotation, velocity, isAwake)
  - `CollisionEvent` (bodyA, bodyB, contactPoint, normal, depth)
  - `CollisionCallback` typedef
- [x] **5.2** Implement `PhysicsScene` (pimpl):
  - `createBody()`, `destroyBody()`, `getBodyState()`
  - `applyForce()`, `applyImpulse()`, `setLinearVelocity()`, `setBodyPosition()`
  - `step(float deltaTime)` with fixed-timestep accumulator
  - `getInterpolationAlpha()`, `getLastStepCount()`
  - Internal: velocity integration, AABB broadphase, impulse-based collision resolution
  - Start with 2D AABB-only collision for simplicity
- [x] **5.3** Add `enablePhysics()`, `disablePhysics()`, `hasPhysics()`, `getPhysicsScene()` to `Scene`
- [x] **5.4** Update `rebuildSchedulerGraph()` to insert Physics and PostPhysics tasks for scenes with physics enabled
- [x] **5.5** Write `PhysicsScene_test.cpp`:
  - Create scene with default config
  - Create/destroy bodies
  - Body falls under gravity after `step()`
  - Fixed timestep accumulator (step with various dt values, verify step count)
  - Interpolation alpha is in [0, 1)
  - Static bodies don't move
  - Kinematic bodies can be repositioned
  - AABB collision between two dynamic boxes
  - Collision callbacks fire on contact
  - `setGravity()` changes gravity
  - Sensor bodies trigger callbacks but don't respond
  - `getBodyCount()` / `getActiveBodyCount()` are accurate
- [x] **5.6** Create `examples/physics_demo/` — falling boxes with static ground:
  - Several dynamic boxes spawned at varying heights
  - Static ground platform
  - Boxes fall and land on ground
  - Console prints body count and step count
  - Uses `ExampleBase.h` pattern

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
# Run physics_demo and observe boxes falling and colliding with ground
# Run all other examples to verify no regressions
```

### Completion Criteria

- [x] All existing tests pass
- [x] `PhysicsScene_test` passes (all sub-tests)
- [x] `physics_demo` example shows correct physics behavior
- [x] All other examples unaffected
- [x] Physics is fully opt-in — scenes without `enablePhysics()` have no overhead

**Completed (2026-02-08):** 545 tests passed (513 existing + 32 PhysicsScene), 11 examples built and verified (10 existing + physics_demo).

---

## Phase 6: Physics Entities & Sync

**Goal:** Create `PhysicsEntity`, `PhysicsMeshEntity`, `PhysicsSpriteEntity` that bind visual entities to physics bodies. Implement automatic PostPhysics transform sync with interpolation.

### New Files

| File | Purpose |
|------|---------|
| `include/vde/api/PhysicsEntity.h` | `PhysicsEntity`, `PhysicsMeshEntity`, `PhysicsSpriteEntity` |
| `src/api/PhysicsEntity.cpp` | Implementation of sync, force helpers, lifecycle |
| `tests/PhysicsEntity_test.cpp` | Unit tests |

### Modified Files

| File | Change |
|------|--------|
| `src/api/Game.cpp` | PostPhysics task iterates entities for `syncFromPhysics()` |
| `include/vde/api/GameAPI.h` | Add `#include "PhysicsEntity.h"` |
| `CMakeLists.txt` | Add new files |
| `tests/CMakeLists.txt` | Add `PhysicsEntity_test.cpp` |
| `examples/physics_demo/main.cpp` | Update to use `PhysicsSpriteEntity` |

### Tasks

- [x] **6.1** Create `PhysicsEntity` base class (extends `Entity`):
  - `createPhysicsBody(const PhysicsBodyDef&)` — creates body in scene's `PhysicsScene`
  - `getPhysicsBodyId()`, `getPhysicsState()`
  - `applyForce()`, `applyImpulse()`, `setLinearVelocity()` — delegate to PhysicsScene
  - `syncFromPhysics(float interpolationAlpha)` — interpolated position copy
  - `syncToPhysics()` — reverse sync for kinematic bodies
  - `setAutoSync()` / `getAutoSync()`
  - `onAttach()` / `onDetach()` lifecycle
  - Stores `m_prevPosition`, `m_prevRotation` for interpolation
- [x] **6.2** Create `PhysicsSpriteEntity` (extends both visual SpriteEntity features + PhysicsEntity):
  - Renders as sprite, driven by physics
- [x] **6.3** Create `PhysicsMeshEntity` (extends both visual MeshEntity features + PhysicsEntity):
  - Renders as mesh, driven by physics
- [x] **6.4** Implement PostPhysics sync in scheduler: iterate all entities, dynamic_cast to `PhysicsEntity*`, call `syncFromPhysics(alpha)` if `getAutoSync()`
- [x] **6.5** Write `PhysicsEntity_test.cpp`:
  - Create PhysicsEntity and attach to Scene with physics
  - `createPhysicsBody()` succeeds and returns valid ID
  - `syncFromPhysics()` updates entity position from body
  - Interpolation blends between previous and current positions
  - `syncToPhysics()` copies entity position to body
  - `setAutoSync(false)` prevents automatic sync
  - Force/impulse helpers delegate correctly
  - `onDetach()` cleans up physics body
- [x] **6.6** Update `physics_demo` to use `PhysicsSpriteEntity`:
  - Visual sprites are driven by physics
  - Player entity with keyboard input (`applyForce()` / `applyImpulse()`)
  - Interactive demo with ExampleBase pattern

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
# Run physics_demo — sprites should visually fall and collide
# Run all other examples to verify no regressions
```

### Completion Criteria

- [x] All existing tests pass
- [x] `PhysicsEntity_test` passes
- [x] `physics_demo` shows visually correct physics-driven sprites
- [x] Interpolation produces smooth motion even at mismatched physics/render rates
- [x] All other examples unaffected

**Completed (2026-02-08):** 568 tests passed (545 existing + 23 PhysicsEntity), 11 examples built and verified. PhysicsEntity uses mixin pattern (no diamond inheritance) with automatic PostPhysics sync via scheduler. Physics demo updated to use PhysicsSpriteEntity with player controls.

---

## Phase 7: Thread Pool & Parallel Physics

**Goal:** Add a `ThreadPool` and integrate it into the `Scheduler` so independent scene physics can run on worker threads.

### New Files

| File | Purpose |
|------|---------|
| `include/vde/api/ThreadPool.h` | `ThreadPool` class |
| `src/api/ThreadPool.cpp` | Worker threads, task queue, futures |
| `tests/ThreadPool_test.cpp` | Unit tests |

### Modified Files

| File | Change |
|------|--------|
| `src/api/Scheduler.cpp` | Dispatch non-main-thread tasks to thread pool |
| `include/vde/api/Scheduler.h` | Add `setWorkerThreadCount()` |
| `CMakeLists.txt` | Add new files |
| `tests/CMakeLists.txt` | Add `ThreadPool_test.cpp` |

### Tasks

- [x] **7.1** Implement `ThreadPool`:
  - Constructor takes thread count, spawns workers
  - `submit(F&& func)` returns `std::future<void>`
  - `waitAll()` blocks until all submitted tasks complete
  - Destructor joins all workers
  - Proper shutdown with condition variable
- [x] **7.2** Wire `Scheduler::setWorkerThreadCount()`:
  - 0 = single-threaded (current behavior)
  - N > 0 = create `ThreadPool(N)`, dispatch tasks not marked `mainThreadOnly` to pool
- [x] **7.3** Update scheduler `execute()`:
  - Tasks marked `mainThreadOnly` run on calling thread
  - Other tasks submitted to thread pool
  - Respect dependency ordering (don't submit until dependencies complete)
- [x] **7.4** Write `ThreadPool_test.cpp`:
  - Submit single task, verify future completes
  - Submit multiple independent tasks, verify all complete
  - `waitAll()` blocks until done
  - Zero-thread-count gracefully degrades (runs inline)
  - Destructor joins cleanly even with pending tasks
  - Tasks execute on different threads (verify via thread IDs)
- [x] **7.5** Create `examples/parallel_physics_demo/`:
  - Two scenes, each with physics
  - `setWorkerThreadCount(2)`
  - Console prints which thread ran each physics step
  - Uses ExampleBase pattern
- [x] **7.6** Thread-safety audit:
  - Verify `PhysicsScene` has no shared mutable state across scenes
  - Verify no global mutable state is accessed during physics tasks
  - Document any thread-safety constraints found

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
# Run parallel_physics_demo — physics should work correctly with parallel execution
# Run all other examples (they use 0 worker threads, unchanged behavior)
```

### Completion Criteria

- [x] All existing tests pass
- [x] `ThreadPool_test` passes
- [x] `parallel_physics_demo` runs without data races (test under ThreadSanitizer if available)
- [x] Single-threaded mode (`setWorkerThreadCount(0)`) is identical to Phase 6 behavior
- [x] All other examples unaffected

**Completed (2026-02-08):** 589 tests passed (568 existing + 12 ThreadPool + 9 Scheduler+ThreadPool integration), 12 examples built and verified. PhysicsScene uses pimpl with no shared mutable state — safe for parallel execution. Physics step tasks dispatch to worker threads; all other tasks remain on main thread. Collision callbacks in parallel_physics_demo confirm physics runs on worker threads with distinct thread IDs.

---

## Phase 8: Advanced Physics Features

**Goal:** Collision callbacks, raycasting, AABB queries, and a comprehensive physics-with-audio example.

### New Files

| File | Purpose |
|------|---------|
| `examples/physics_audio_demo/main.cpp` | Collision → game logic → audio chain demo |

### Modified Files

| File | Change |
|------|--------|
| `src/api/PhysicsScene.cpp` | Add raycast, AABB query, per-body collision callbacks |
| `include/vde/api/PhysicsScene.h` | Add raycast/query API |
| `include/vde/api/Scene.h` | Add `getEntityByPhysicsBody()` helper |
| `tests/PhysicsScene_test.cpp` | Add raycast/query tests |
| `examples/CMakeLists.txt` | Add new example |
| `tests/CMakeLists.txt` | Update if new test files added |

### Tasks

- [x] **8.1** Implement collision callbacks:
  - `setOnCollisionBegin(CollisionCallback)` — fires when two bodies start overlapping (new pair tracking)
  - `setOnCollisionEnd(CollisionCallback)` — fires when overlap ends (previous-vs-current pair diff)
  - `setBodyOnCollisionBegin(PhysicsBodyId, CollisionCallback)` — per-body begin callback
  - `setBodyOnCollisionEnd(PhysicsBodyId, CollisionCallback)` — per-body end callback
  - Callbacks fire during `step()`, user records events for processing in GameLogic phase
- [x] **8.2** Implement raycast:
  - `bool raycast(origin, direction, maxDistance, outHit)` using ray-AABB slab intersection
  - Returns closest hit body, point, normal, distance
  - `RaycastHit` struct added to `PhysicsTypes.h`
- [x] **8.3** Implement AABB query:
  - `std::vector<PhysicsBodyId> queryAABB(min, max)`
  - Returns all bodies overlapping the region
- [x] **8.4** Add `Scene::getEntityByPhysicsBody(PhysicsBodyId)` helper
  - Iterates entities, checks `PhysicsSpriteEntity` and `PhysicsMeshEntity` via `dynamic_cast`
- [x] **8.5** Add tests (15 new tests):
  - Collision end callback fires on separation
  - Collision end not fired when still overlapping
  - Per-body collision begin callback
  - Per-body collision end callback
  - Raycast hits closest body
  - Raycast misses when no body in path
  - Raycast respects max distance
  - Raycast Y direction
  - Raycast zero direction returns false
  - AABB query returns correct bodies
  - AABB query returns empty for empty region
  - AABB query on empty scene
  - getEntityByPhysicsBody finds entity
  - getEntityByPhysicsBody returns null for invalid ID
  - getEntityByPhysicsBody returns null for unknown ID
- [x] **8.6** Create `examples/physics_audio_demo/`:
  - Demonstrates the full Collision → GameLogic → Audio chain
  - Dynamic entities collide; game logic decides outcome; audio cues are queued
  - Uses `enablePhaseCallbacks()` and the three-phase model
  - Per-body callbacks on ground detect box impacts
  - Raycast and AABB query demonstrated periodically
  - getEntityByPhysicsBody resolves entity names from collision events
  - Uses ExampleBase pattern with auto-terminate

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
# Run physics_audio_demo — verify collision sounds play correctly
# Run all other examples to verify no regressions
```

### Completion Criteria

- [x] All existing tests pass (589 existing tests)
- [x] Raycast and query tests pass (5 new tests)
- [x] Collision callback tests pass (4 new tests: begin/end + per-body begin/end)
- [x] getEntityByPhysicsBody tests pass (3 new tests)
- [x] `physics_audio_demo` demonstrates the full physics → logic → audio pipeline
- [x] All other examples unaffected (13 examples total)

**Completed (2026-02-08):** 604 tests passed (589 existing + 15 Phase 8), 13 examples built and verified. Added `RaycastHit` struct, per-body collision callbacks, ray-AABB slab intersection, AABB spatial query, `getEntityByPhysicsBody()` entity lookup. Collision begin/end now use proper pair tracking (previousPairs vs currentPairs). Physics audio demo demonstrates the full 3-phase pipeline with all new features.

---

## Appendix A: File Change Manifest

Complete list of all files created or modified across all phases.

### New Files

| File | Phase |
|------|-------|
| `include/vde/api/Scheduler.h` | 1 |
| `src/api/Scheduler.cpp` | 1 |
| `tests/Scheduler_test.cpp` | 1 |
| `include/vde/api/SceneGroup.h` | 2 |
| `tests/SceneGroup_test.cpp` | 2 |
| `include/vde/api/ViewportRect.h` | 3 |
| `tests/ViewportRect_test.cpp` | 3 |
| `include/vde/api/AudioEvent.h` | 4 |
| `tests/AudioEvent_test.cpp` | 4 |
| `include/vde/api/PhysicsTypes.h` | 5 |
| `include/vde/api/PhysicsScene.h` | 5 |
| `src/api/PhysicsScene.cpp` | 5 |
| `tests/PhysicsScene_test.cpp` | 5 |
| `examples/physics_demo/main.cpp` | 5 |
| `include/vde/api/PhysicsEntity.h` | 6 |
| `src/api/PhysicsEntity.cpp` | 6 |
| `tests/PhysicsEntity_test.cpp` | 6 |
| `include/vde/api/ThreadPool.h` | 7 |
| `src/api/ThreadPool.cpp` | 7 |
| `tests/ThreadPool_test.cpp` | 7 |
| `examples/parallel_physics_demo/main.cpp` | 7 |
| `examples/physics_audio_demo/main.cpp` | 8 |

### Modified Files

| File | Phases Modified |
|------|----------------|
| `include/vde/api/Game.h` | 1, 2, 3 |
| `src/api/Game.cpp` | 1, 2, 3, 4, 5, 6 |
| `include/vde/api/Scene.h` | 2, 3, 4, 5, 8 |
| `src/api/Scene.cpp` | 2, 3, 4, 5 |
| `include/vde/api/SceneGroup.h` | 3 |
| `include/vde/api/GameAPI.h` | 1, 2, 3, 4, 5, 6 |
| `include/vde/VulkanContext.h` | 3 |
| `CMakeLists.txt` | 1, 2, 3, 5, 6, 7 |
| `tests/CMakeLists.txt` | 1, 2, 3, 4, 5, 6, 7, 8 |
| `examples/CMakeLists.txt` | 5, 7, 8 |
| `tests/Scene_test.cpp` | 2, 3, 4 |
| `examples/multi_scene_demo/main.cpp` | 2, 3 |
| `examples/quad_viewport_demo/main.cpp` | 3 |
| `examples/physics_demo/main.cpp` | 6 |

---

## Appendix B: Backwards Compatibility Guarantees

Each phase must preserve these invariants:

1. **`setActiveScene("name")`** continues to work identically — internally creates a single-scene group
2. **Default viewport is full-window** — scenes without `setViewportRect()` render to the entire window, identical to pre-Phase 3 behavior
3. **`Scene::update(float)`** continues to be called for scenes that don't call `enablePhaseCallbacks()`
4. **`AudioManager::update()`** continues to be called once per frame
5. **Single-threaded by default** — `setWorkerThreadCount(0)` is the default; no threading unless explicitly enabled
6. **No physics by default** — scenes without `enablePhysics()` have no physics overhead
7. **Input routing defaults to primary scene** — if `setFocusedScene()` is not called, input goes to the primary scene as before
8. **All existing examples** compile and run identically after every phase
9. **All existing tests** pass after every phase

---

## Appendix C: Test Matrix

### Existing Tests (must pass at every phase)

| Test File | Tests |
|-----------|-------|
| `Camera_test.cpp` | Camera math, projection |
| `CameraBounds_test.cpp` | 2D camera bounds |
| `Entity_test.cpp` | Entity creation, transform |
| `GameCamera_test.cpp` | Game camera types |
| `HexGeometry_test.cpp` | Hex math |
| `LightBox_test.cpp` | Lighting configurations |
| `Material_test.cpp` | Material properties |
| `Mesh_test.cpp` | Mesh geometry |
| `ResourceManager_test.cpp` | Resource load/cache |
| `Scene_test.cpp` | Scene entity/resource management |
| `ShaderCache_test.cpp` | Shader caching |
| `SpriteEntity_test.cpp` | Sprite entity props |
| `Texture_test.cpp` | Texture loading |
| `Types_test.cpp` | Engine types |
| `Window_test.cpp` | Window creation |
| `WorldBounds_test.cpp` | World bounds |
| `WorldUnits_test.cpp` | Unit conversions |

### New Tests (added progressively)

| Test File | Phase | Key Scenarios |
|-----------|-------|---------------|
| `Scheduler_test.cpp` | 1 | Task ordering, dependencies, cycle detection, clear/remove |
| `SceneGroup_test.cpp` | 2 | Group creation, scene ordering |
| `ViewportRect_test.cpp` | 3 | Viewport factories, contains hit test, Vulkan conversion |
| `AudioEvent_test.cpp` | 4 | Event types, queue add/drain |
| `PhysicsScene_test.cpp` | 5 | Accumulator, bodies, gravity, collision, callbacks |
| `PhysicsEntity_test.cpp` | 6 | Body creation, sync, interpolation, lifecycle |
| `ThreadPool_test.cpp` | 7 | Task submission, concurrency, shutdown |

### Examples (must run at every phase)

| Example | Phase Added | Demonstrates |
|---------|-------------|-------------|
| `vde_triangle_example` | Pre-existing | Basic Vulkan rendering |
| `vde_simple_game_example` | Pre-existing | Game API basics |
| `vde_sprite_demo` | Pre-existing | SpriteEntity |
| `vde_world_bounds_demo` | Pre-existing | World bounds |
| `vde_materials_lighting_demo` | Pre-existing | Materials & lighting |
| `vde_resource_demo` | Pre-existing | Resource management |
| `vde_audio_demo` | Pre-existing | Audio system |
| `vde_sidescroller` | Pre-existing | Platformer mechanics |
| `vde_wireframe_viewer` | Pre-existing | Shape rendering |
| `vde_multi_scene_demo` | Pre-existing (updated Phase 2, 3) | Scene management, optional viewports |
| `vde_quad_viewport_demo` | Pre-existing (upgraded Phase 3) | True split-screen with independent cameras |
| `vde_physics_demo` | 5 | Physics simulation |
| `vde_parallel_physics_demo` | 7 | Multi-threaded physics |
| `vde_physics_audio_demo` | 8 | Physics → logic → audio chain |

---

## Appendix D: Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| Scheduler refactor breaks game loop timing | High | Phase 1 graph mirrors exact existing loop; diff test with frame timing |
| Per-scene viewport breaks single-scene rendering | High | Default viewport is `fullWindow()` (0,0,1,1); single-scene path unchanged |
| Per-scene UBO update per viewport is expensive | Medium | Only update UBO when camera/viewport differs; profile with 4+ viewports |
| Multi-scene rendering overwhelms GPU | Medium | Scenes share same swapchain; limit to N renderable scenes |
| Per-scene clear color not supported in single render pass | Low | Draw fullscreen background quad per viewport; document limitation |
| Input focus routing conflicts with existing input handlers | Medium | Default to primary scene when no focus set; focus is opt-in |
| Physics accumulator drift at low FPS | Low | `maxSubSteps` cap prevents spiral of death |
| Thread pool introduces data races | High | Shared-nothing design during physics; PostPhysics sync on main thread; ThreadSanitizer testing |
| `dynamic_cast<PhysicsEntity*>` overhead in PostPhysics | Low | Only runs once per entity per frame; profile and optimize if needed |
| Audio event queue adds frame of latency | Low | 16ms is below perception threshold; power users can bypass queue |

---

## Appendix E: Design Decisions Log

Record notable decisions made during implementation here.

| Date | Decision | Rationale |
|------|----------|-----------|
| 2026-02-08 | ThreadPool uses `std::packaged_task` with `std::future` per submission, plus atomic busy-counter + condition_variable for `waitAll()` | Simpler than shared_future duplication; busy-counter ensures waitAll doesn't return while a worker is mid-execution |
| 2026-02-08 | Scheduler multi-threaded path uses level-based frontier dispatch (all ready tasks submitted together, then waited on) rather than fine-grained per-task futures | Matches engine's batch-per-frame pattern; avoids complex per-task synchronization |
| 2026-02-08 | Physics step tasks set `mainThreadOnly = false`; all other tasks remain `mainThreadOnly = true` | Only physics has verified thread-safety (pimpl, no shared state). Rendering, input, audio all touch global/shared state and must stay on main thread |
| 2026-02-08 | Thread-safety audit: PhysicsScene::Impl has no global/static mutable state | Each scene's physics is fully isolated — safe for parallel step() calls |
| 2026-02-08 | Collision begin now uses pair tracking (previousPairs vs currentPairs) instead of firing every step | Properly models begin/end semantics; existing tests unaffected since they only call step() once |
| 2026-02-08 | Per-body collision callbacks stored in `Impl` maps keyed by `PhysicsBodyId` | Lightweight; callbacks cleaned up when body is destroyed (naturally, since map entries reference the body) |
| 2026-02-08 | Raycast uses ray-AABB slab method (2D, X and Y slabs) | Standard efficient algorithm; returns closest hit with surface normal computed from hit face |
| 2026-02-08 | `getEntityByPhysicsBody()` uses `dynamic_cast` to both `PhysicsSpriteEntity*` and `PhysicsMeshEntity*` | Simple and correct; O(n) scan acceptable for typical entity counts; avoids needing a separate lookup table |
