# Animation Engine Changes Plan

> Design Dependency: [ANIMATION_SYSTEM_DESIGN.md](ANIMATION_SYSTEM_DESIGN.md)
> Created: 2026-04-26
> Last Updated: 2026-04-27
> Status: Planned

## Summary

This document isolates the engine work required before the generic animation API should ship.
The companion design document defines the public animation model. This plan defines the
scheduler, scene-runtime, and playback changes needed to make that model correct in
multi-scene workloads.

The goal is not to add one more callback. The goal is to make the frame graph robust enough
that:

- multiple scenes can update in the same frame without false dependencies
- each scene can host multiple animations and timed events at once
- physics-synced scenes get a stable post-physics visual pass
- future scene-local runtime services can plug into the same phase model without rewriting
  the whole scheduler

## Verified Current Gaps

The companion design already identified the high-level mismatch. The current code confirms
the specific engine gaps that need to be addressed.

1. `TaskPhase` has no real `Visual` or timed-event phase.
   `include/vde/api/Scheduler.h` currently stops at `PostPhysics`, `PreRender`, and `Render`.

2. `Scene::updateVisuals()` is not actually scheduled as a visual step.
   `Game::rebuildSchedulerGraph()` currently registers `scene.visuals.*` as
   `TaskPhase::GameLogic`, then runs physics and post-physics afterwards.

3. Scene updates are not independent.
   The scheduler graph is built through a running `prevTask`, so later scenes often depend on
   earlier scenes simply because of list order, not because of any true gameplay dependency.

4. There is no first-class scene-owned timed-event runner.
   The repo already has `Cooldown` and `RepeatingTimer`, but it does not have an engine-owned
   service that can manage multiple one-shot and repeating callbacks per scene.

5. Thread-affinity is too loose for the future worker-thread path.
   Many frame tasks are currently registered with `mainThreadOnly = false`, including input,
   window operations, scene callbacks, and pre-render work. Single-threaded execution hides that
   today, but the graph is not ready for safe parallel execution of those tasks.

## Goals

- Add a real scene-local timing and visual foundation for the animation system.
- Keep scene lifetime as the ownership boundary for animations and timed events.
- Remove false scene-to-scene dependencies from the scheduler graph.
- Preserve legacy `update()`-only behavior, while explicitly allowing phase-callback ordering fixes.
- Keep the design additive: no global singleton, no ECS rewrite, no cross-scene lifetime model.
- Make future scheduler parallelism safer by tightening thread-affinity rules now.

## Non-Goals

- replacing `TransitionManager`
- introducing a game-global animation or timing singleton
- replacing `Cooldown` or `RepeatingTimer` for simple gameplay code
- adding a public callback for every possible frame stage in v1
- building a full timeline editor, sequence graph, or scripting runtime
- a fully parallel constraint solver or rigid-body contact graph (v1 keeps sequential resolution)

## Design Rules

### 1. Scene ownership remains the lifetime boundary

Animations, timed callbacks, and any related playback state should remain scene-owned.
When a scene exits or is destroyed, its runtime services stop with it.

### 2. Explicit dependencies only

The scheduler should encode real dataflow, not ordering by accident.
If scene B only needs to render after scene A, that should not force scene B's logic,
physics, timed events, or visuals to wait on scene A.

### 3. Main-thread by default

Any task that mutates scene objects, touches windowing APIs, or talks to engine runtime state
should default to `mainThreadOnly = true`. Only tasks proven to be thread-safe should opt out.

### 4. Prefer scene services over more public virtuals

The current `updateGameLogic()`, `updateAudio()`, and `updateVisuals()` callbacks are enough
for v1. New engine functionality should mostly arrive as scene-owned services wired into the
scheduler, not as a growing list of public scene virtual methods.

## Target Phase Model

The engine should move to this effective phase order:

| Phase | Purpose |
|-------|---------|
| `Input` | Scripted input, queued OS/window operations |
| `GameLogic` | User scene logic, entity updates, AI, state changes |
| `Physics` | Per-scene physics simulation |
| `PostPhysics` | Transform sync and other post-simulation reconciliation |
| `Timed` | Scene-owned timed callbacks and delayed triggers |
| `Audio` | Scene audio preparation plus global audio flush |
| `Visual` | `updateVisuals()` plus scene-owned animation playback |
| `PreRender` | Clear color, camera/light prep, render setup |
| `Render` | Scene rendering and transition composition |

In the current scheduler, exact numeric `TaskPhase` ordering is semantically important because
it is used as a topological tie-breaker. `Timed` and `Visual` should be inserted with explicit
ordinal placement so the effective order remains:

`Input < GameLogic < Physics < PostPhysics < Timed < Audio < Visual < PreRender < Render`.

With that requirement in place, two additional details still matter:

1. `Timed` and `Visual` must happen after `PostPhysics` so authored presentation logic sees the
   final simulation state for the frame.
2. `Visual` must happen before `PreRender` and `Render` so the animation system is the last
   visual writer for the frame.

`Timed` is a deliberate addition. It gives the engine a clean place for delayed callbacks,
repeaters, and other authored event triggers without forcing them into `GameLogic` or
`updateVisuals()`.

## Target Graph Shape

The frame graph should stop chaining scenes together with a single rolling dependency.
Instead, each scene should have its own local phase chain.

### Per-scene chain

For each update scene:

```text
frame.root
  -> scene.logic
  -> scene.physics        (optional)
  -> scene.postPhysics    (optional)
  -> scene.timed
  -> scene.audio
  -> scene.visuals
  -> scene.animations
```

### Legacy compatibility path

When phase callbacks are disabled, the engine should register a single `scene.update` task in
`GameLogic` and treat that task as the scene's "final update task" for barrier collection.
Physics, post-physics, timed, audio, and visual barriers should depend on that legacy final task
exactly as they do for phase-callback scenes.

### Cross-scene relationship

Scene A and scene B should each have the same local chain, but neither chain should depend on
the other unless the engine introduces an explicit gameplay dependency later.

That means:

- no `prevTask` chain across scenes for logic
- no `prevTask` chain across scenes for timed callbacks
- no `prevTask` chain across scenes for visuals or animations

Define deterministic scene ordering explicitly before task registration:

- `updatePriority` ascending
- active-scene-group index (when available)
- scene name lexicographically

Scene ordering should remain a deterministic tiebreaker, not a semantic dependency.

### Global barriers

Some work still needs whole-frame barriers. Those barriers should be expressed explicitly by
collecting task ids instead of by serializing all scene tasks.

Recommended model:

- `game.update` depends on input and window operations
- every `scene.logic.*` depends on `game.update`
- every later scene phase depends only on that same scene's prior phase
- `audio.global` depends on all `scene.audio.*` tasks
- `scene.preRender` depends on all final per-scene visual tasks and `audio.global`
- `scene.render` depends on `scene.preRender` and, when active, `transition.update`

This keeps the graph understandable and leaves room for later parallel execution.

## Scene Runtime Services

The new scheduler phases need scene-owned services that can actually use them.

### 1. Shared playback state

Extract a reusable playback helper from the current transition implementation.

Candidate file:

- `include/vde/PlaybackClock.h`

Responsibilities:

- delay handling
- duration tracking
- pause and resume
- speed scaling
- loop and ping-pong bookkeeping
- progress snapshots for callbacks and tween evaluation

This should be reused by both `TransitionManager` and the new `Animator`.

### 2. Scene-owned timed events

Add a small scene-owned service for delayed and repeating callbacks.

Candidate files:

- `include/vde/api/TimedEvents.h`
- `src/api/TimedEvents.cpp`

Responsibilities:

- schedule one-shot callbacks after a delay
- schedule repeating callbacks at a fixed interval
- allow cancellation, pause, and local speed scaling
- cancel everything automatically when the scene exits or is destroyed
- run in the `Timed` phase, after post-physics and before visuals

This is not a replacement for `Cooldown` or `RepeatingTimer`.
Those remain the lightweight choice for small gameplay loops inside one class.
The timed-event service is for engine-managed authored callbacks that should integrate with the
frame graph and scene lifetime.

### 3. Scene-owned animator

The `Animator` planned in [ANIMATION_SYSTEM_DESIGN.md](ANIMATION_SYSTEM_DESIGN.md) should be
owned by `Scene` and updated automatically in the `Visual` phase.

Responsibilities:

- run after `updateVisuals()`
- use the shared playback helper
- keep multiple animations independent within one scene
- cancel bound animations safely when their targets disappear
- stop naturally when the owning scene stops updating

## Hook Strategy For Future Functionality

The scheduler should become easier to extend without turning `Game::rebuildSchedulerGraph()`
into a giant feature switchboard.

### Internal helper structure

Introduce a small internal struct that records the task ids for one scene:

```cpp
struct SceneFrameTasks {
    TaskId gameLogic       = INVALID_TASK_ID;
    // Physics is broken into three scheduler-level sub-phases (all worker-eligible).
    // Each sub-phase is registered at TaskPhase::Physics but encodes its position
    // through explicit dependsOn edges so the topological sort sequences them correctly
    // while still allowing cross-scene parallelism at each sub-phase.
    TaskId physicsIntegrate  = INVALID_TASK_ID;  // force accumulation + integration
    TaskId physicsBroadPhase = INVALID_TASK_ID;  // AABB overlap detection
    TaskId physicsResolve    = INVALID_TASK_ID;  // impulse resolution + staging events
    TaskId postPhysics     = INVALID_TASK_ID;    // main-thread: transform sync + callback dispatch
    TaskId timed           = INVALID_TASK_ID;
    TaskId audio           = INVALID_TASK_ID;
    TaskId visuals         = INVALID_TASK_ID;
    TaskId animations      = INVALID_TASK_ID;
    TaskId finalVisual     = INVALID_TASK_ID;
};
```

The physics sub-phase task chain for a single scene looks like:

```text
scene.logic
  -> scene.physics.integrate    (worker, mainThreadOnly=false)
  -> scene.physics.broadPhase   (worker, mainThreadOnly=false)
  -> scene.physics.resolve      (worker, mainThreadOnly=false; sequential in v1)
  -> scene.postPhysics          (main-thread: sync transforms, dispatch staged callbacks)
```

`physicsIntegrate` for scene A and `physicsIntegrate` for scene B have no dependency
edge between them, so the scheduler is free to run both on workers simultaneously.
The same is true of `physicsBroadPhase` and `physicsResolve` across scenes.
This gives the thread pool work to do without requiring a contact-graph coloring strategy
in v1.

This makes it much easier to:

- build the graph without accidental scene-to-scene edges
- collect the correct barrier dependencies
- add future per-scene services without rewriting the whole function

### Public hook guidance

For v1, do not add new public scene virtuals just to make the graph feel complete.
The current public phase callbacks are enough.

Preferred extension path:

1. keep `updateGameLogic()`, `updateAudio()`, and `updateVisuals()` as the public surface
2. add scene-owned services that the engine ticks in the new phases
3. revisit public callbacks only if a concrete external use case remains awkward

That keeps the API surface smaller and more reversible.

## Threading Rules

The scheduler already has a worker-thread path, so this plan should tighten the rules now rather
than after animation code starts depending on unsafe behavior.

### Engine thread pool at startup

The engine must create its worker threads at game startup, not lazily.

- `Game` calls `m_scheduler.setWorkerThreadCount(N)` during initialization, before the first
  frame.
- The minimum is **3 worker threads**. This guarantees that physics sub-phases from different
  scenes can overlap even when the game starts with no tuning.
- Default formula: `max(3, hardware_concurrency - 1)` (leave one core for the main thread and
  the OS). Cap at a sensible maximum (e.g. 16) to avoid over-subscription on large machines.
- If the application explicitly calls `setWorkerThreadCount()` before `run()`, that value
  replaces the default. Values below 3 are clamped to 3 when physics-capable scenes are active.
- The pool persists for the entire game lifetime. It is **not** recreated per-frame or
  per-scene rebuild.

The existing `ThreadPool` implementation already uses `std::condition_variable` for worker
sleep/wake. Workers block efficiently on an empty queue and wake immediately when a task is
submitted. No changes to `ThreadPool` internals are needed to support this guarantee; the
only change is when `setWorkerThreadCount()` is called and how the minimum is enforced.

### Scheduler thread pool exposure

Physics sub-phase task lambdas call `PhysicsScene` methods directly — they do **not** submit
further work to the thread pool. The scheduler's internal pool is therefore not exposed to
physics code, and no `Scheduler::getThreadPool()` accessor is needed in v1.

If a future phase adds intra-task chunk dispatch (see Phase 6+ discussion in Phase 5), that
accessor can be added at that point with full deadlock analysis.

### Main-thread-only tasks

These tasks must be `mainThreadOnly = true`:

- scripted input dispatch
- window and OS operations
- `game.update`
- `scene.update()` / `updateGameLogic()`
- `scene.postPhysics` — transform sync, entity reconciliation, and collision callback dispatch
- `scene.updateAudio()`
- `scene.updateVisuals()`
- timed-event callbacks
- animation callbacks
- pre-render setup
- transition update and composite preparation

### Worker-eligible tasks

These tasks run off the main thread:

- `scene.physics.integrate` — integrates each body's position and velocity independently
- `scene.physics.broadPhase` — detects AABB overlap candidates
- `scene.physics.resolve` — applies impulse resolution (sequential within one scene in v1)

Collision callbacks are **not** dispatched from any of these tasks. They are staged into a
per-scene event buffer during `physicsResolve` and flushed on the main thread in
`postPhysics`.

Non-physics tasks should remain main-thread-only unless they can prove all of the following:

- no entity or component mutation
- no scene callback invocation
- no engine-global access
- no windowing or render-context touch points

The rule remains: a task starts as main-thread-only and opts out only with evidence.

## Implementation Phases

## Phase 1: Scheduler Correction

**Goal:** Fix the frame graph before adding new runtime services.

### Tasks

1. Add `TaskPhase::Timed` and `TaskPhase::Visual`.
2. Update `Scene` documentation so its phase descriptions match the real runtime.
3. Refactor `Game::rebuildSchedulerGraph()` so scene tasks are built per scene instead of through
   a global `prevTask` chain.
4. Move `updateVisuals()` into the real `Visual` phase after post-physics.
5. Keep render ordering deterministic without turning update priority into a false dependency.
6. Correct `mainThreadOnly` flags for frame and scene tasks.

### Acceptance Criteria

- `updateVisuals()` never runs before `PostPhysics`
- two active scenes can both reach their visual phase without depending on each other's logic task
- legacy `update()`-only scenes remain behavior-compatible
- phase-callback scenes intentionally move visual updates to post-physics `Visual`

### Unit Tests

**File:** `tests/Scheduler_test.cpp` (new)

| Test Name | Assertion |
|-----------|-----------|
| `PhaseOrder_TimedAfterPostPhysics` | `TaskPhase::Timed` ordinal > `TaskPhase::PostPhysics` |
| `PhaseOrder_VisualAfterTimed` | `TaskPhase::Visual` ordinal > `TaskPhase::Timed` |
| `PhaseOrder_VisualBeforePreRender` | `TaskPhase::Visual` ordinal < `TaskPhase::PreRender` |
| `MultiScene_NoFalseDependency` | Two scenes registered in the scheduler share no dependency edge between their logic tasks |
| `MultiScene_DeterministicOrder` | Equal-priority scenes execute in the same document-specified order across multiple graph rebuilds |
| `LegacyScene_UpdateCallbackFires` | A scene using only `update()` fires exactly once per frame without regression |
| `VisualPhase_AfterPostPhysics` | For a physics-enabled scene, the `updateVisuals()` callback timestamp is strictly after `postPhysics` |
| `MainThreadOnly_SceneCallbacks` | Scene logic, visuals, and audio tasks are all registered with `mainThreadOnly = true` |

No example is required for Phase 1 — the scheduler correction is an internal engine change with
no new user-visible feature surface.

## Phase 2: Timed Event Foundation

**Goal:** Add a scene-owned timing service that can drive multiple delayed or repeating callbacks.

### Tasks

1. Add `PlaybackClock` as a shared core utility.
2. Add a scene-owned timed-event service and handles.
3. Wire timed events into the `Timed` phase.
4. Keep lifetime, cancellation, and pause/resume local to the scene.
5. Ensure no per-frame heap allocation after a timed event is created.

### Acceptance Criteria

- multiple timed callbacks can run simultaneously inside one scene
- timed callbacks in scene A do not stall or serialize scene B
- destroying or exiting a scene cancels its timed callbacks cleanly

### Unit Tests

**File:** `tests/PlaybackClock_test.cpp` (new)

| Test Name | Assertion |
|-----------|-----------|
| `PlaybackClock_ProgressAdvances` | After `tick(dt)`, progress equals `dt / duration` |
| `PlaybackClock_PauseHaltsProgress` | `pause()` then `tick(dt)` leaves progress unchanged |
| `PlaybackClock_ResumeRestoresProgress` | `resume()` after `pause()` advances progress again |
| `PlaybackClock_SpeedScaling` | `setSpeed(2.0)` doubles effective progress per tick |
| `PlaybackClock_LoopWraps` | Progress wraps to 0 after completing a cycle with loop mode |
| `PlaybackClock_PingPongReverses` | Direction reverses each cycle in ping-pong mode |
| `PlaybackClock_DelayHoldsAtZero` | Progress stays 0 during the configured delay window |
| `PlaybackClock_CompletionFiredOnce` | Completion callback fires exactly once after full duration |

**File:** `tests/TimedEvents_test.cpp` (new)

| Test Name | Assertion |
|-----------|-----------|
| `OneShotFires_Once` | One-shot callback fires exactly once after the configured delay |
| `OneShotFires_AtCorrectFrame` | Callback fires in the first frame where cumulative time exceeds delay |
| `RepeatingFires_CorrectCount` | After elapsed time = N × interval, callback has fired exactly N times |
| `RepeatingFires_WithLargeDeltaTime` | A single large `deltaTime` fires multiple callbacks without skipping any |
| `CancelBeforeFire_Suppresses` | Cancelling a handle before the delay prevents the callback from firing |
| `CancelInsideCallback_Safe` | Cancelling the handle from within its own callback does not crash or corrupt state |
| `PauseResume_Deterministic` | Pausing mid-delay and resuming produces the same result as an uninterrupted delay |
| `SpeedScale_AffectsInterval` | `setSpeed(2.0)` halves the effective wall-clock interval |
| `SceneTeardown_CancelsAll` | Destroying the scene before any callback fires leaves no dangling invocations |
| `SceneTeardown_NoCompletionCallback` | Scene teardown does not invoke completion handlers for outstanding callbacks |
| `MultiScene_Independent` | Timed events in scene A do not affect timed events in scene B |

### Example / Demo

**Name:** `timed_events_demo`
**Scaffold:** `scripts/new-example.ps1 -Name timed_events_demo -Title "Timed Events Demo"`

Demonstrates:
- A one-shot delayed callback that updates an on-screen label after 1 second
- A repeating callback that increments a visible counter every 500 ms
- Pause and resume of timers via keyboard input
- Mid-flight cancellation of a scheduled callback
- Simultaneous timed events across two independent scenes

The example self-validates: it tracks that all expected callbacks fired within the run window
and exits with code 0 on success, code 1 on failure.

**Smoke script:** `smoketests/scripts/smoke_timed_events_demo.vdescript`
Waits for startup, waits 2.5 seconds (enough for ≥ 4 repeating ticks), then exits and checks
the process exit code.

## Phase 3: Animator Integration

**Goal:** Land the generic animator on top of the corrected scheduler foundation.

### Tasks

1. Add scene-owned `Animator` and handles.
2. Reuse `PlaybackClock` instead of duplicating transition playback state.
3. Update `Animator` in the `Visual` phase immediately after `updateVisuals()`.
4. Keep entity, weak-object, and resolver bindings scene-local.
5. Confirm transition freeze behavior works naturally by stopping scene updates, not by special
   animator logic.

### Acceptance Criteria

- multiple animations can run in multiple active scenes in the same frame
- physics-synced entities are not overwritten after the animation pass
- background scenes and transition destination scenes behave consistently with scene ownership

### Unit Tests

**File:** `tests/Animator_test.cpp` (new)

| Test Name | Assertion |
|-----------|-----------|
| `Animator_SingleAnim_Completes` | A one-shot animation reaches progress 1.0 and fires its completion callback |
| `Animator_MultipleAnims_IndependentProgress` | Two animations with different durations advance independently per tick |
| `Animator_LoopMode_Wraps` | A loop animation wraps to 0 after each cycle without stopping |
| `Animator_PingPongMode_Reverses` | A ping-pong animation reverses direction each cycle |
| `Animator_VisualPhaseOrdering` | The animator tick occurs after `updateVisuals()` within the same frame |
| `Animator_PostPhysicsOrdering` | The animator tick does not precede `postPhysics` for the owning scene |
| `Animator_BindingLifetime_StopsOnTargetDestroy` | Destroying the target entity stops the animation cleanly without crashing |
| `Animator_SceneTeardown_CancelsAll` | Destroying the scene stops all running animations without invoking completion callbacks |
| `Animator_MultiScene_Independent` | Animators in scene A and scene B advance independently in the same frame |
| `Animator_PauseResume` | Pausing the animator halts progress; resuming continues from the same point |
| `Animator_SpeedScaling` | `setSpeed(2.0)` causes the animation to complete in half the expected wall-clock time |
| `Animator_TransitionSourceFreeze` | Stopping scene updates halts animation without requiring special-case animator logic |

### Example / Demo

**Name:** `animation_demo`
**Scaffold:** `scripts/new-example.ps1 -Name animation_demo -Title "Animation Demo"`

Demonstrates:
- A one-shot position tween (cube slides from point A to B then triggers a completion callback)
- A looping rotation animation on a second entity
- A ping-pong scale animation on a third entity
- Multiple scenes active simultaneously with fully independent animation playback
- A completion callback that chains into a second animation (sequence pattern)

The example self-validates: it tracks that each animation type completed at least one cycle
and exits with code 0 on success, code 1 on failure.

**Smoke script:** `smoketests/scripts/smoke_animation_demo.vdescript`
Waits for startup, waits 4 seconds (covers multiple cycles of each animation type), then exits
and checks the process exit code.

## Phase 4: Hardening, Docs, and Consumers

**Goal:** Prove the new phase model and adopt it in a few visible places.

### Tasks

1. Extend `tests/Scheduler_test.cpp` with cross-scene and threading edge cases (see Unit Tests below).
2. Extend `tests/TimedEvents_test.cpp` with precision and interleaving edge cases.
3. Extend `tests/Animator_test.cpp` with callback ordering and weak-binding edge cases.
4. Update docs that currently describe `updateVisuals()` as a phase the runtime does not yet
   implement.
5. Create `multi_scene_animation_demo` to exercise timed events and animations across two
   independent scenes in one running application.

### Acceptance Criteria

- docs, code, and examples all describe the same phase model
- the scheduler graph remains readable and deterministic
- `multi_scene_animation_demo` demonstrates timed callbacks and scene-bound animation
  in parallel across two scenes and passes its smoke test

### Unit Tests

**Additions to `tests/Scheduler_test.cpp`:**

| Test Name | Assertion |
|-----------|-----------|
| `TransitionFrame_DestinationSceneIndependent` | A transition frame updates the destination scene without adding a dependency on unrelated scenes |
| `WorkerMode_NoSceneCallbackOffMainThread` | When worker threads are active, no scene logic, audio, or visual callback executes off the main thread |

**Additions to `tests/TimedEvents_test.cpp`:**

| Test Name | Assertion |
|-----------|-----------|
| `TimedEvent_PauseResume_PreservesElapsed` | Elapsed time before `pause()` is preserved exactly across a pause/resume cycle |
| `TimedEvent_MultiScene_Interleaved` | Timed events ticked in alternating scenes produce correct independent outcomes |

**Additions to `tests/Animator_test.cpp`:**

| Test Name | Assertion |
|-----------|-----------|
| `Animator_CallbackOrder_SameFrame` | When multiple animations complete in the same frame, callbacks fire in registration order |
| `Animator_WeakObjectBinding_DropsCleanly` | A weak-ref binding whose target has expired is removed silently without triggering the callback |

### Example / Demo

**Name:** `multi_scene_animation_demo`
**Scaffold:** `scripts/new-example.ps1 -Name multi_scene_animation_demo -Title "Multi-Scene Animation Demo"`

Demonstrates:
- Two simultaneously active scenes each hosting independent timed events and animations
- Visual proof that scene A's animation cycle is not gated on scene B completing its own
- A timed callback in scene A that triggers an animation after a configurable delay
- A repeating timed event in scene B that alternates a material or color on each tick

The example self-validates: both scenes must complete their full cycle before the auto-exit
deadline; missing any cycle exits with code 1.

**Smoke script:** `smoketests/scripts/smoke_multi_scene_animation_demo.vdescript`
Waits for startup, waits 5 seconds (both scenes must complete at least two full cycles), then
exits and checks the process exit code.

## Phase 5: Physics Parallelism and Engine Thread Pool

**Goal:** Move physics simulation work onto worker threads so the main thread is free during
physics steps, and give the engine a stable, minimum-size thread pool that is always ready.

### Background

The current `PhysicsScene::singleStep()` is monolithic: it integrates all bodies, then detects
collisions, resolves impulses, and fires callbacks — all in sequence on the main thread.

The scheduler already has the plumbing for worker tasks (`ThreadPool`, `setWorkerThreadCount()`),
but the pool defaults to zero workers (single-threaded mode), and physics tasks are never
actually dispatched off the main thread today.

This phase addresses both gaps.

### Design: Thread Pool Startup Guarantee

- `Game::run()` calls `m_scheduler.setWorkerThreadCount(N)` before the first frame loop
  iteration if the application has not already done so explicitly.
- Default `N = max(3, static_cast<size_t>(std::thread::hardware_concurrency()) - 1)`.
  Cap `N` at 16 to prevent over-subscription on very large machines.
- If the application has already called `getScheduler().setWorkerThreadCount()` with a value
  ≥ 3, the engine respects that value and skips the default initialization.
- If the application passed a value < 3, the engine clamps it to 3 and logs a warning.
  This clamping happens in `Game::run()`, not in `Scheduler::setWorkerThreadCount()`, so the
  scheduler remains general-purpose.
- Worker threads start immediately in `setWorkerThreadCount()` (the current behavior) and
  block on an empty condition variable — efficient sleep until the first task arrives.

The minimum of 3 provides meaningful inter-scene parallelism: with 3 workers and 3 physics
scenes, every scene's integrate task can run simultaneously; with 2 scenes and 3 workers,
both integrate tasks run in parallel and the third worker picks up broad-phase work as
integrate tasks complete.

### Design: Physics Sub-Phase Decomposition

Each scene's physics work is split into three **atomic** scheduler tasks at `TaskPhase::Physics`.
Each task runs entirely on whichever thread the scheduler dispatches it to. Tasks do **not**
submit further work to the thread pool internally — all parallelism comes from the scheduler
running independent scene tasks concurrently on worker threads. This avoids a class of
nested-pool deadlocks where a pool worker blocks in `waitAll()` while the remaining workers
are occupied by other outer tasks, leaving no threads available to drain the inner queue.

**Rule: no task lambda running on a worker thread may call `pool.submit()` or `pool.waitAll()`.
Tasks are leaf functions. Only the scheduler dispatcher submits tasks.**

Per-body integration parallelism within a single scene (chunked body dispatch) is deferred to
a future phase when a work-stealing or fork-join design can be used without blocking waits.

#### Sub-phase 1: `scene.physics.integrate`

- Runs `mainThreadOnly = false` (worker-eligible).
- Processes every body sequentially on the dispatched thread:
  save `prevState` for interpolation; apply gravity and accumulated forces to velocity;
  apply velocity damping; integrate position; clear the accumulated force buffer.
- Each body is independent. Cross-scene parallelism is the primary performance gain in v1.
- `scene.physics.integrate` depends only on `scene.logic`.

#### Sub-phase 2: `scene.physics.broadPhase`

- Runs `mainThreadOnly = false` (worker-eligible).
- Computes the AABB for each body and tests all pairs for AABB overlap (O(n²) in v1).
- Writes candidate pairs into `std::vector<CandidatePair> m_candidatePairs` in
  `PhysicsScene::Impl` (cleared at the start of this sub-phase). This vector is written
  only here and read only by `physicsResolve`. The scheduler dependency chain guarantees
  no concurrent access — no mutex is needed.
- Future: replace with a spatial hash or BVH for large scenes.
- `scene.physics.broadPhase` depends on `scene.physics.integrate`.

#### Sub-phase 3: `scene.physics.resolve`

- Runs `mainThreadOnly = false` (worker-eligible), sequential within a scene in v1.
- Reads `m_candidatePairs` produced by broad-phase.
- For each solver iteration: compute detailed collision info, apply positional correction,
  apply impulse-based velocity resolution.
- Collision begin/end events are **staged** into `std::vector<CollisionEvent> m_stagedEvents`
  in `PhysicsScene::Impl` (cleared at the top of this sub-phase, written only here, read only
  by `postPhysics`). The dependency edge `physicsResolve → postPhysics` is the sole
  synchronization — no mutex is needed.
- `previousPairs` (used for begin/end delta tracking) is updated at the end of this sub-phase.
  Frames execute sequentially and each scene's `physicsResolve` runs at most once per frame,
  so no two tasks ever access `previousPairs` for the same scene concurrently.
- The dead `activePairs` field (set but never read in the current code) is removed.
- `scene.physics.resolve` depends on `scene.physics.broadPhase`.

### Design: Callback Dispatch in PostPhysics

`scene.postPhysics` (main-thread-only) is guaranteed to run only after `physicsResolve`
completes for the same scene. This dependency edge is the sole synchronization for the
staged event buffer — no mutex is required.

`scene.postPhysics` is responsible for:

1. Calling `drainStagedEvents()` and dispatching `onCollisionBegin` / `onCollisionEnd` to
   user code on the main thread.
2. Calling `pe->syncFromPhysics(alpha)` on all physics entities (existing behavior, unchanged).
3. The staged buffer is cleared at the start of `physicsResolve` each frame, so calling
   `drainStagedEvents()` after `postPhysics` without an intervening `stepPhases()` returns an
   empty vector — not an error.

This removes all user-callback invocations from the physics worker tasks.

### PhysicsScene API Changes

```cpp
// Existing — remains unchanged for caller compatibility.
// Internally calls stepPhases(deltaTime, nullptr).
void step(float deltaTime);

// New internal entry point used by Game::rebuildSchedulerGraph().
// Manages the fixed-timestep accumulator and calls integrationStep(),
// broadPhaseStep(), resolveStep() in sequence for each sub-step.
// Each sub-phase method runs on the calling thread only — no pool dispatch internally.
// pool parameter is reserved for future use (intra-task chunk dispatch). Pass nullptr in v1.
void stepPhases(float deltaTime, ThreadPool* pool);

// New: move staged collision events out of the buffer.
// Returns an empty vector if called before stepPhases() or after already drained.
// Safe to call only from the main thread (postPhysics task).
std::vector<CollisionEvent> drainStagedEvents();
```

`PhysicsScene::step()` calls `stepPhases(deltaTime, nullptr)` internally, so all existing
call sites (tests, manual usage) continue to work without change.

`stepPhases()` preserves the fixed-timestep accumulator loop exactly: for each sub-step it
calls `integrationStep()`, `broadPhaseStep()`, `resolveStep()` in sequence. Staged events
accumulate across sub-steps and are drained once per frame by `postPhysics`.

### SceneFrameTasks with Sub-Phases

The per-scene struct adds the three physics sub-phase task ids:

```cpp
struct SceneFrameTasks {
    TaskId gameLogic         = INVALID_TASK_ID;
    TaskId physicsIntegrate  = INVALID_TASK_ID;
    TaskId physicsBroadPhase = INVALID_TASK_ID;
    TaskId physicsResolve    = INVALID_TASK_ID;
    TaskId postPhysics       = INVALID_TASK_ID;
    TaskId timed             = INVALID_TASK_ID;
    TaskId audio             = INVALID_TASK_ID;
    TaskId visuals           = INVALID_TASK_ID;
    TaskId animations        = INVALID_TASK_ID;
    TaskId finalVisual       = INVALID_TASK_ID;
};
```

When a scene does not have physics enabled, `physicsIntegrate`, `physicsBroadPhase`, and
`physicsResolve` remain `INVALID_TASK_ID` and the `postPhysics` task depends directly on
`gameLogic`.

### Dependency Chain Per Scene (Physics-Enabled)

```text
game.update
  -> scene.logic                   (main-thread)
  -> scene.physics.integrate       (worker; no internal pool dispatch)
  -> scene.physics.broadPhase      (worker; no internal pool dispatch)
  -> scene.physics.resolve         (worker; no internal pool dispatch)
  -> scene.postPhysics             (main-thread: drain staged events, sync transforms)
  -> scene.timed
  -> scene.audio
  -> scene.visuals
  -> scene.animations
  -> scene.preRender
  -> scene.render
```

### Cross-Scene Parallelism

Physics-enabled scene A and physics-enabled scene B have no dependency edges between their
physics sub-phase tasks. The scheduler is free to interleave them:

```text
game.update
  ├─> sceneA.logic -> sceneA.physics.integrate -> sceneA.physics.broadPhase -> ...
  └─> sceneB.logic -> sceneB.physics.integrate -> sceneB.physics.broadPhase -> ...
```

With 3 workers and 2 physics scenes, both integrate tasks can run in parallel. With 3 scenes,
all three integrate tasks can run in parallel. Because each sub-phase task is a leaf (no nested
pool submissions), there is **no deadlock risk** regardless of how many physics scenes are
active. A scene's physics sub-phase chain occupies exactly one worker thread at a time.

### PhysicsConfig — No New Fields in v1

The `integrateChunkSize` field is not added in v1. The `PhysicsConfig` struct is otherwise
unchanged.

### Phase 5 Tasks

1. Update `Game::run()` to call `setWorkerThreadCount(max(3, hardware_concurrency - 1))` at
   startup before the frame loop, respecting any prior explicit call ≥ 3, clamping values < 3
   with a log warning.
2. Split `PhysicsScene::singleStep()` into three private methods:
   - `integrationStep()` — per-body force + position integration
   - `broadPhaseStep()` — AABB overlap detection, writes `m_candidatePairs`
   - `resolveStep()` — impulse resolution, updates `previousPairs`, stages events into
     `m_stagedEvents`
3. Remove the dead `activePairs` field from `PhysicsScene::Impl`.
4. Add `m_candidatePairs` (`std::vector<CandidatePair>`) and `m_stagedEvents`
   (`std::vector<CollisionEvent>`) fields to `PhysicsScene::Impl`.
5. Add `PhysicsScene::stepPhases(float deltaTime, ThreadPool* pool)` (public).
6. Update `PhysicsScene::step()` to delegate to `stepPhases(deltaTime, nullptr)`.
7. Add `PhysicsScene::drainStagedEvents()` (public).
8. Update `Game::rebuildSchedulerGraph()` to register three physics sub-phase tasks per
   physics-enabled scene using `SceneFrameTasks::physicsIntegrate`, `physicsBroadPhase`,
   and `physicsResolve`. Task lambdas call `integrationStep()`, `broadPhaseStep()`, and
   `resolveStep()` directly (no internal pool dispatch).
9. Update `scene.postPhysics` task to call `drainStagedEvents()` and dispatch collision
   callbacks before syncing transforms.

### Phase 5 Acceptance Criteria

- Physics sub-phases execute on worker threads (verified via `ThreadPool::getWorkerThreadIds()`)
- Collision callbacks fire on the main thread only (verified via thread id check inside
  callbacks in tests)
- Two active physics scenes can have their integration and broad-phase tasks overlap in time
  (verified via timing or execution-order inspection in tests)
- Disabling the thread pool (threadCount = 0) falls back to single-threaded behavior and
  produces identical physics results
- `PhysicsScene::step()` continues to work correctly in isolation (used by existing unit tests)
- Worker thread count is at least 3 after `Game::run()` is called with no prior explicit setting

### Unit Tests

**Additions to `tests/PhysicsScene_test.cpp`** (file already exists):

| Test Name | Assertion |
|-----------|-----------|
| `StepPhases_MatchesStep_SingleBody` | `stepPhases(dt, nullptr)` produces the same body position as `step(dt)` for a single body |
| `StepPhases_MatchesStep_Collision` | `stepPhases` and `step` produce identical collision outcomes for a simple two-body collision |
| `DrainStagedEvents_EmptyAfterDrain` | Calling `drainStagedEvents()` a second time returns an empty vector |
| `DrainStagedEvents_CollectsAcrossSubSteps` | Events staged across multiple fixed-timestep sub-steps are all returned in a single drain call |
| `PhysicsWorkerTask_RunsOnWorkerThread` | The integrate, broadPhase, and resolve tasks each execute with a non-main-thread id |
| `CollisionCallback_MainThreadOnly` | `onCollisionBegin` and `onCollisionEnd` callbacks dispatched in `postPhysics` execute on the main thread |
| `MultiPhysicsScene_NoInterSceneDependency` | Two physics-enabled scenes have no scheduler dependency edge between their respective physics sub-phase tasks |
| `ThreadPoolStartup_MinimumThree` | After `Game::run()`, `Scheduler::getWorkerThreadCount()` returns ≥ 3 |
| `ThreadPoolStartup_ClampsBelow3` | Calling `setWorkerThreadCount(2)` before `run()` results in a count ≥ 3 after `run()` |
| `ThreadPoolStartup_RespectsExplicitAbove3` | Calling `setWorkerThreadCount(8)` before `run()` preserves 8 after `run()` |
| `PhysicsScene_Determinism` | Simulation with threadCount = 3 produces numerically identical body positions to threadCount = 0 for the same input sequence |

No new example is required for Phase 5. Physics parallelism is an internal engine optimization
with no new user-visible feature surface. The existing physics-heavy examples
(`breakout_demo`, `asteroids_demo`) serve as integration-level smoke tests that the
multi-threaded physics path does not regress observable behavior.

## Testing Matrix

Each phase section above includes a table of specific test names and assertions. This matrix
summarizes which files are introduced in which phase and which example smoke tests cover
user-visible functionality.

### Unit Test Files

| File | Phase Introduced | Domain |
|------|-----------------|--------|
| `tests/Scheduler_test.cpp` | Phase 1 (extended in Phase 4) | Phase order, multi-scene independence, thread affinity, transition frames |
| `tests/PlaybackClock_test.cpp` | Phase 2 | Playback state: progress, pause, resume, speed, loop, ping-pong, delay |
| `tests/TimedEvents_test.cpp` | Phase 2 (extended in Phase 4) | One-shot, repeating, cancel, pause/resume, speed scaling, scene teardown |
| `tests/Animator_test.cpp` | Phase 3 (extended in Phase 4) | Binding lifetime, phase ordering, playback modes, multi-scene, weak-ref cleanup |
| `tests/PhysicsScene_test.cpp` | Existing (extended in Phase 5) | Sub-phase decomposition, staged events, worker threads, determinism |

### Example Smoke Tests

| Example | Phase Introduced | Smoke Script |
|---------|-----------------|-------------|
| `timed_events_demo` | Phase 2 | `smoketests/scripts/smoke_timed_events_demo.vdescript` |
| `animation_demo` | Phase 3 | `smoketests/scripts/smoke_animation_demo.vdescript` |
| `multi_scene_animation_demo` | Phase 4 | `smoketests/scripts/smoke_multi_scene_animation_demo.vdescript` |

### Scheduler tests

- phase order proves `PostPhysics < Timed < Audio < Visual < PreRender`
- active multi-scene graphs contain no accidental scene-to-scene dependency edges
- equal-priority active/background scenes execute in documented deterministic order across runs
- transition frames still update the destination scene without stalling unrelated scenes
- worker-thread mode never dispatches scene callbacks or animation callbacks off the main thread

### Timed-event tests

- one-shot callbacks fire once at the correct frame
- repeating callbacks fire the correct number of times after a large `deltaTime`
- pause/resume and speed scaling behave deterministically
- cancellation inside a callback is safe
- scene teardown cancels outstanding callbacks without firing completion handlers

### Animation tests

- `updateVisuals()` runs before animator playback for the same scene
- animator playback runs after post-physics sync
- multiple scene animators can tick in the same frame
- transition-source freezing stops scene-bound animations by stopping that scene's updates

### Physics parallelism tests

- `physicsIntegrate`, `physicsBroadPhase`, and `physicsResolve` tasks execute on worker threads
  (verify via `ThreadPool::getWorkerThreadIds()` inside task lambdas)
- collision begin/end callbacks receive the correct thread id — main thread only
- two physics-enabled scenes produce no dependency edge between their physics sub-phase tasks
- staged events are empty after `drainStagedEvents()` is called
- results with threadCount=3 are numerically identical to results with threadCount=0 for the
  same input (determinism regression test)
- `PhysicsScene::step()` used standalone (no thread pool) continues to pass all existing
  `PhysicsScene_test.cpp` cases without modification

### Thread pool startup tests

- after `Game::run()`, `Scheduler::getWorkerThreadCount()` returns ≥ 3
- an application that calls `setWorkerThreadCount(2)` before `run()` sees it clamped to 3
- an application that calls `setWorkerThreadCount(8)` before `run()` sees 8 preserved

## Exit Criteria

The engine foundation is ready when all of these are true:

- the scheduler exposes real `Timed` and `Visual` phases
- scene update chains are independent across scenes unless a real dependency exists
- scene-owned timed events and scene-owned animations can both run in multiple scenes
- thread-affinity is strict enough that future worker-thread execution is safe by default
- the animation design document can rely on the engine phase model without documenting around it
- physics sub-phases run on worker threads and fire callbacks on the main thread only
- a minimum of 3 worker threads are active for the entire game lifetime
- all unit tests listed in the per-phase sections pass (Scheduler, PlaybackClock, TimedEvents,
  Animator, PhysicsScene)
- `timed_events_demo`, `animation_demo`, and `multi_scene_animation_demo` build, run, and pass
  their smoke tests without manual intervention

## Recommendation

Treat this plan as the dependency that must land before the public animation API is considered
complete. The animation system itself is relatively small. The risky part is the frame graph.
If the engine foundation is correct, the animator becomes a clean scene service. If the engine
foundation stays fuzzy, the public API will inherit that fuzziness and become harder to evolve.