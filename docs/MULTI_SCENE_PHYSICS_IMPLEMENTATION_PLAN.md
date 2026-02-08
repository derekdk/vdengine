# Multi-Scene Physics & Scheduler — Implementation Plan

> **Design Reference:** [MULTI_SCENE_PHYSICS_DESIGN.md](MULTI_SCENE_PHYSICS_DESIGN.md)  
> **Created:** 2026-02-08  
> **Last Updated:** 2026-02-08  
> **Status:** Planning

---

## Progress Summary

| Phase | Description | Status | Tests Pass | Examples Work |
|-------|-------------|--------|------------|---------------|
| 0 | [Pre-flight & Baseline](#phase-0-pre-flight--baseline) | **Done** | 423/423 | 10/10 |
| 1 | [Scheduler Foundation (Single-Threaded)](#phase-1-scheduler-foundation-single-threaded) | **Done** | 444/444 | 10/10 |
| 2 | [Multi-Scene Support](#phase-2-multi-scene-support) | Not Started | — | — |
| 3 | [Scene Phase Callbacks & Audio Event Queue](#phase-3-scene-phase-callbacks--audio-event-queue) | Not Started | — | — |
| 4 | [Physics Scene](#phase-4-physics-scene) | Not Started | — | — |
| 5 | [Physics Entities & Sync](#phase-5-physics-entities--sync) | Not Started | — | — |
| 6 | [Thread Pool & Parallel Physics](#phase-6-thread-pool--parallel-physics) | Not Started | — | — |
| 7 | [Advanced Physics Features](#phase-7-advanced-physics-features) | Not Started | — | — |

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

- [ ] **2.1** Create `SceneGroup` struct in `SceneGroup.h` with `name`, `sceneNames`, `SceneGroup::create()` helper
- [ ] **2.2** Add `setContinueInBackground()` / `getContinueInBackground()` and `setUpdatePriority()` / `getUpdatePriority()` to `Scene`
- [ ] **2.3** Add `setActiveSceneGroup()` to `Game`; refactor `setActiveScene()` to create a single-scene group internally
- [ ] **2.4** Implement `rebuildSchedulerGraph()` in `Game.cpp` that creates tasks for each scene in the active group:
  - Input → Update(scene1) → Update(scene2) → ... → PreRender(scene1) → PreRender(scene2) → Render
  - Background scenes also get Update tasks
- [ ] **2.5** Wire `setActiveSceneGroup` / `setActiveScene` to call `rebuildSchedulerGraph()`
- [ ] **2.6** Write `SceneGroup_test.cpp`:
  - Construction & `create()` helper
  - Empty group
  - Verify scene ordering
- [ ] **2.7** Add Scene background/priority tests to `Scene_test.cpp`
- [ ] **2.8** Update `multi_scene_demo` example to demonstrate `setActiveSceneGroup()` with two scenes rendering simultaneously

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
# Run multi_scene_demo and verify both scenes render in the same frame
# Run all other examples to verify no regressions
```

### Completion Criteria

- [ ] All existing tests pass
- [ ] New tests pass
- [ ] `setActiveScene("x")` behaves identically to before (backwards compatible)
- [ ] `setActiveSceneGroup(...)` activates multiple scenes
- [ ] `multi_scene_demo` visually demonstrates simultaneous scene rendering
- [ ] All other examples run without regression

---

## Phase 3: Scene Phase Callbacks & Audio Event Queue

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

- [ ] **3.1** Define `AudioEvent` struct (Type enum, clip, volume, pitch, loop, position, soundId)
- [ ] **3.2** Add `enablePhaseCallbacks()`, `usesPhaseCallbacks()`, and the three virtual methods to `Scene`:
  - `virtual void updateGameLogic(float deltaTime)` — no-op default
  - `virtual void updateAudio(float deltaTime)` — default drains audio queue
  - `virtual void updateVisuals(float deltaTime)` — no-op default
- [ ] **3.3** Add `queueAudioEvent()`, `playSFX()`, `playSFXAt()` convenience methods to `Scene`
- [ ] **3.4** Implement default `updateAudio()` to drain `m_audioEventQueue` via `AudioManager`
- [ ] **3.5** Ensure backwards compatibility: if `usesPhaseCallbacks()` returns false, scheduler calls `update()` as a single task in the GameLogic phase — identical to current behavior
- [ ] **3.6** Update `rebuildSchedulerGraph()`:
  - For scenes with `usesPhaseCallbacks() == true`: create separate GameLogic, Audio, Visual tasks with proper dependencies
  - For legacy scenes: single Update task in GameLogic phase
  - Add `audio.global` task that calls `AudioManager::getInstance().update()` after all per-scene audio tasks
- [ ] **3.7** Write `AudioEvent_test.cpp`:
  - AudioEvent construction with various types
  - Queue add/drain cycle
  - Empty queue drain is safe
- [ ] **3.8** Add phase callback tests to `Scene_test.cpp`:
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

- [ ] All existing tests pass
- [ ] New AudioEvent and phase callback tests pass
- [ ] `audio_demo` example works identically
- [ ] All other examples work identically (they use `update()`, not phase callbacks)
- [ ] `AudioManager::update()` is called via scheduler `audio.global` task, not inline in `Game::run()`

---

## Phase 4: Physics Scene

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

- [ ] **4.1** Define all physics types in `PhysicsTypes.h`:
  - `PhysicsConfig` (fixedTimestep, gravity, maxSubSteps, iterations)
  - `PhysicsBodyId`, `INVALID_PHYSICS_BODY_ID`
  - `PhysicsShape` enum (Box, Circle, Sphere, Capsule)
  - `PhysicsBodyType` enum (Static, Dynamic, Kinematic)
  - `PhysicsBodyDef` (type, shape, position, rotation, extents, mass, friction, restitution, damping, isSensor)
  - `PhysicsBodyState` (position, rotation, velocity, isAwake)
  - `CollisionEvent` (bodyA, bodyB, contactPoint, normal, depth)
  - `CollisionCallback` typedef
- [ ] **4.2** Implement `PhysicsScene` (pimpl):
  - `createBody()`, `destroyBody()`, `getBodyState()`
  - `applyForce()`, `applyImpulse()`, `setLinearVelocity()`, `setBodyPosition()`
  - `step(float deltaTime)` with fixed-timestep accumulator
  - `getInterpolationAlpha()`, `getLastStepCount()`
  - Internal: velocity integration, AABB broadphase, impulse-based collision resolution
  - Start with 2D AABB-only collision for simplicity
- [ ] **4.3** Add `enablePhysics()`, `disablePhysics()`, `hasPhysics()`, `getPhysicsScene()` to `Scene`
- [ ] **4.4** Update `rebuildSchedulerGraph()` to insert Physics and PostPhysics tasks for scenes with physics enabled
- [ ] **4.5** Write `PhysicsScene_test.cpp`:
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
- [ ] **4.6** Create `examples/physics_demo/` — falling boxes with static ground:
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

- [ ] All existing tests pass
- [ ] `PhysicsScene_test` passes (all sub-tests)
- [ ] `physics_demo` example shows correct physics behavior
- [ ] All other examples unaffected
- [ ] Physics is fully opt-in — scenes without `enablePhysics()` have no overhead

---

## Phase 5: Physics Entities & Sync

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

- [ ] **5.1** Create `PhysicsEntity` base class (extends `Entity`):
  - `createPhysicsBody(const PhysicsBodyDef&)` — creates body in scene's `PhysicsScene`
  - `getPhysicsBodyId()`, `getPhysicsState()`
  - `applyForce()`, `applyImpulse()`, `setLinearVelocity()` — delegate to PhysicsScene
  - `syncFromPhysics(float interpolationAlpha)` — interpolated position copy
  - `syncToPhysics()` — reverse sync for kinematic bodies
  - `setAutoSync()` / `getAutoSync()`
  - `onAttach()` / `onDetach()` lifecycle
  - Stores `m_prevPosition`, `m_prevRotation` for interpolation
- [ ] **5.2** Create `PhysicsSpriteEntity` (extends both visual SpriteEntity features + PhysicsEntity):
  - Renders as sprite, driven by physics
- [ ] **5.3** Create `PhysicsMeshEntity` (extends both visual MeshEntity features + PhysicsEntity):
  - Renders as mesh, driven by physics
- [ ] **5.4** Implement PostPhysics sync in scheduler: iterate all entities, dynamic_cast to `PhysicsEntity*`, call `syncFromPhysics(alpha)` if `getAutoSync()`
- [ ] **5.5** Write `PhysicsEntity_test.cpp`:
  - Create PhysicsEntity and attach to Scene with physics
  - `createPhysicsBody()` succeeds and returns valid ID
  - `syncFromPhysics()` updates entity position from body
  - Interpolation blends between previous and current positions
  - `syncToPhysics()` copies entity position to body
  - `setAutoSync(false)` prevents automatic sync
  - Force/impulse helpers delegate correctly
  - `onDetach()` cleans up physics body
- [ ] **5.6** Update `physics_demo` to use `PhysicsSpriteEntity`:
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

- [ ] All existing tests pass
- [ ] `PhysicsEntity_test` passes
- [ ] `physics_demo` shows visually correct physics-driven sprites
- [ ] Interpolation produces smooth motion even at mismatched physics/render rates
- [ ] All other examples unaffected

---

## Phase 6: Thread Pool & Parallel Physics

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

- [ ] **6.1** Implement `ThreadPool`:
  - Constructor takes thread count, spawns workers
  - `submit(F&& func)` returns `std::future<void>`
  - `waitAll()` blocks until all submitted tasks complete
  - Destructor joins all workers
  - Proper shutdown with condition variable
- [ ] **6.2** Wire `Scheduler::setWorkerThreadCount()`:
  - 0 = single-threaded (current behavior)
  - N > 0 = create `ThreadPool(N)`, dispatch tasks not marked `mainThreadOnly` to pool
- [ ] **6.3** Update scheduler `execute()`:
  - Tasks marked `mainThreadOnly` run on calling thread
  - Other tasks submitted to thread pool
  - Respect dependency ordering (don't submit until dependencies complete)
- [ ] **6.4** Write `ThreadPool_test.cpp`:
  - Submit single task, verify future completes
  - Submit multiple independent tasks, verify all complete
  - `waitAll()` blocks until done
  - Zero-thread-count gracefully degrades (runs inline)
  - Destructor joins cleanly even with pending tasks
  - Tasks execute on different threads (verify via thread IDs)
- [ ] **6.5** Create `examples/parallel_physics_demo/`:
  - Two scenes, each with physics
  - `setWorkerThreadCount(2)`
  - Console prints which thread ran each physics step
  - Uses ExampleBase pattern
- [ ] **6.6** Thread-safety audit:
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

- [ ] All existing tests pass
- [ ] `ThreadPool_test` passes
- [ ] `parallel_physics_demo` runs without data races (test under ThreadSanitizer if available)
- [ ] Single-threaded mode (`setWorkerThreadCount(0)`) is identical to Phase 5 behavior
- [ ] All other examples unaffected

---

## Phase 7: Advanced Physics Features

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

- [ ] **7.1** Implement collision callbacks:
  - `setOnCollisionBegin(CollisionCallback)` — fires when two bodies start overlapping
  - `setOnCollisionEnd(CollisionCallback)` — fires when overlap ends
  - Callbacks fire during `step()`, user records events for processing in GameLogic phase
- [ ] **7.2** Implement raycast:
  - `bool raycast(origin, direction, maxDistance, outHit)`
  - Returns closest hit body, point, normal, distance
- [ ] **7.3** Implement AABB query:
  - `std::vector<PhysicsBodyId> queryAABB(min, max)`
  - Returns all bodies overlapping the region
- [ ] **7.4** Add `Scene::getEntityByPhysicsBody(PhysicsBodyId)` helper
- [ ] **7.5** Add tests:
  - Collision callback fires on overlap
  - Collision end callback fires on separation
  - Raycast hits closest body
  - Raycast misses when no body in path
  - AABB query returns correct bodies
  - AABB query returns empty for empty region
- [ ] **7.6** Create `examples/physics_audio_demo/`:
  - Demonstrates the full Collision → GameLogic → Audio chain from design doc Section 4.7
  - Dynamic entities collide; game logic decides outcome; audio cues are queued
  - Uses `enablePhaseCallbacks()` and the three-phase model
  - Sound effects play on collision
  - Uses ExampleBase pattern

### Verification

```powershell
./scripts/build-and-test.ps1 -BuildConfig Debug
# Run physics_audio_demo — verify collision sounds play correctly
# Run all other examples to verify no regressions
```

### Completion Criteria

- [ ] All existing tests pass
- [ ] Raycast and query tests pass
- [ ] Collision callback tests pass
- [ ] `physics_audio_demo` demonstrates the full physics → logic → audio pipeline
- [ ] All other examples unaffected

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
| `include/vde/api/AudioEvent.h` | 3 |
| `tests/AudioEvent_test.cpp` | 3 |
| `include/vde/api/PhysicsTypes.h` | 4 |
| `include/vde/api/PhysicsScene.h` | 4 |
| `src/api/PhysicsScene.cpp` | 4 |
| `tests/PhysicsScene_test.cpp` | 4 |
| `examples/physics_demo/main.cpp` | 4 |
| `include/vde/api/PhysicsEntity.h` | 5 |
| `src/api/PhysicsEntity.cpp` | 5 |
| `tests/PhysicsEntity_test.cpp` | 5 |
| `include/vde/api/ThreadPool.h` | 6 |
| `src/api/ThreadPool.cpp` | 6 |
| `tests/ThreadPool_test.cpp` | 6 |
| `examples/parallel_physics_demo/main.cpp` | 6 |
| `examples/physics_audio_demo/main.cpp` | 7 |

### Modified Files

| File | Phases Modified |
|------|----------------|
| `include/vde/api/Game.h` | 1, 2 |
| `src/api/Game.cpp` | 1, 2, 3, 4, 5 |
| `include/vde/api/Scene.h` | 2, 3, 4, 7 |
| `src/api/Scene.cpp` | 2, 3, 4 |
| `include/vde/api/GameAPI.h` | 1, 2, 3, 4, 5 |
| `CMakeLists.txt` | 1, 2, 4, 5, 6 |
| `tests/CMakeLists.txt` | 1, 2, 3, 4, 5, 6, 7 |
| `examples/CMakeLists.txt` | 4, 6, 7 |
| `tests/Scene_test.cpp` | 2, 3 |
| `examples/multi_scene_demo/main.cpp` | 2 |
| `examples/physics_demo/main.cpp` | 5 |

---

## Appendix B: Backwards Compatibility Guarantees

Each phase must preserve these invariants:

1. **`setActiveScene("name")`** continues to work identically — internally creates a single-scene group
2. **`Scene::update(float)`** continues to be called for scenes that don't call `enablePhaseCallbacks()`
3. **`AudioManager::update()`** continues to be called once per frame
4. **Single-threaded by default** — `setWorkerThreadCount(0)` is the default; no threading unless explicitly enabled
5. **No physics by default** — scenes without `enablePhysics()` have no physics overhead
6. **All existing examples** compile and run identically after every phase
7. **All existing tests** pass after every phase

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
| `AudioEvent_test.cpp` | 3 | Event types, queue add/drain |
| `PhysicsScene_test.cpp` | 4 | Accumulator, bodies, gravity, collision, callbacks |
| `PhysicsEntity_test.cpp` | 5 | Body creation, sync, interpolation, lifecycle |
| `ThreadPool_test.cpp` | 6 | Task submission, concurrency, shutdown |

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
| `vde_multi_scene_demo` | Pre-existing (updated Phase 2) | Scene management |
| `vde_physics_demo` | 4 | Physics simulation |
| `vde_parallel_physics_demo` | 6 | Multi-threaded physics |
| `vde_physics_audio_demo` | 7 | Physics → logic → audio chain |

---

## Appendix D: Risk Register

| Risk | Impact | Mitigation |
|------|--------|------------|
| Scheduler refactor breaks game loop timing | High | Phase 1 graph mirrors exact existing loop; diff test with frame timing |
| Multi-scene rendering overwhelms GPU | Medium | Scenes share same swapchain; limit to N renderable scenes |
| Physics accumulator drift at low FPS | Low | `maxSubSteps` cap prevents spiral of death |
| Thread pool introduces data races | High | Shared-nothing design during physics; PostPhysics sync on main thread; ThreadSanitizer testing |
| `dynamic_cast<PhysicsEntity*>` overhead in PostPhysics | Low | Only runs once per entity per frame; profile and optimize if needed |
| Audio event queue adds frame of latency | Low | 16ms is below perception threshold; power users can bypass queue |

---

## Appendix E: Design Decisions Log

Record notable decisions made during implementation here.

| Date | Decision | Rationale |
|------|----------|-----------|
| — | — | — |
