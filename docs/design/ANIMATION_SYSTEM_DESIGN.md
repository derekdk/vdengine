# Generic Animation System Design

## Summary

VDE already has two strong pieces of animation-adjacent infrastructure:

- `Scene` can split work into `updateGameLogic()`, `updateAudio()`, and `updateVisuals()`.
- `Transition` and `TransitionManager` already use a clean `onStart` -> `update` -> `onComplete` lifecycle driven by deterministic `deltaTime`.

Those are the right building blocks, but the current scheduler plumbing does not fully honor that model yet. The animation plan should therefore include a small engine correction, not just a new API layer.

What is missing is a reusable animation layer that lets API users schedule authored motion without open-coding timers, progress accumulation, easing math, and object-lifetime checks in every scene or entity.

This proposal adds a generic animation system with these properties:

- built-in easing algorithms plus custom easing functions
- animations can be scheduled against entities or other objects
- each animation supports start, update, and completion handlers
- scene ownership keeps animation lifetime aligned with scene lifetime
- entity and object attachments cancel automatically when the target is no longer resolvable
- common transform and color tweening stays easy, while the primitive remains generic

The design intentionally does not try to become a skeletal animation graph, state machine framework, or ECS subsystem.

The engine changes that this design depends on now live in the companion plan:

- [ANIMATION_ENGINE_CHANGES_PLAN.md](ANIMATION_ENGINE_CHANGES_PLAN.md)

## Why This Fits The Current Engine

The proposal is grounded in current repo behavior, not a parallel architecture.

### Existing patterns worth preserving

1. `Transition` already models time-driven work with:
   - deterministic progress
   - explicit `onStart()` and `onComplete()` hooks
   - a frame context carrying `deltaTime`, `elapsed`, and `duration`

2. `Scene` already has a visual-only callback via `updateVisuals(float deltaTime)`.
    That is still the right conceptual home for authored motion, but the scheduler placement of that callback should be corrected before the generic animator is implemented.

3. `Scene` owns entities and resolves them by `EntityId`.
   That gives us a safe attachment model for entity-bound animations without storing raw entity pointers.

4. Current examples repeatedly hand-roll the same patterns:
   - bobbing and pulsing in `examples/transition_demo/main.cpp`
   - sprite rotation and scale pulses in `examples/resource_demo/main.cpp`
   - frame-accumulator sprite animation in `examples/sidescroller/main.cpp`
   - centralized but custom procedural motion in `examples/parallax_demo`

5. `Timing.h` already establishes that VDE prefers small reusable timing helpers over ad hoc accumulators.

### Important constraints from the current codebase

- The system must be driven by frame `deltaTime`, not wall-clock queries.
- The system must remain scheduler-driven, but the current phase wiring should be corrected rather than baked into the public animation API.
- The system should not require a new global singleton.
- The system should not replace `TransitionManager`, which owns render targets and compositing state.
- The system should not assume that every animatable object is a `shared_ptr`.

## Should The Engine Change?

Yes, but narrowly.

The engine does not need an ECS rewrite, a game-global animation singleton, or a new
ownership model. It does need a cleaner phase and playback foundation so the generic
animation system lands on the right seam.

That engine work is now captured in the companion plan:

- [ANIMATION_ENGINE_CHANGES_PLAN.md](ANIMATION_ENGINE_CHANGES_PLAN.md)

At a high level, that plan does four things:

1. correct the phase model so scene-local visual work runs after post-physics sync
2. remove false scene-to-scene dependencies from the scheduler graph
3. add a scene-owned timing/event layer for multiple timed callbacks per scene
4. tighten thread-affinity rules so the worker-thread path stays safe as the feature grows

### Changes that are not needed

- no game-global animation singleton
- no rewrite of `EntityId` or scene ownership
- no forced conversion of cameras, lights, or scene helpers to `shared_ptr`
- no replacement of `TransitionManager`
- no full engine-wide time-domain abstraction in v1

## Problem Statement

Today, authored animation in VDE falls into three buckets:

1. Manual procedural code in scenes.
   Examples compute `sin()`, `cos()`, accumulators, and phase offsets directly in `update()`.

2. Manual per-entity animation subclasses.
   The `AnimatedSpriteEntity` in `examples/sidescroller/main.cpp` tracks frame timers and UVs inside the entity.

3. One special-purpose lifecycle system.
   `TransitionManager` is clean and robust, but it only solves screen transitions.

That leads to four recurring problems:

- boilerplate timers and progress math in examples
- no standard easing vocabulary across systems
- no generic way to bind an animation to entity or object lifetime
- no common handler model for start, update, and completion

## Goals

- Provide a generic scheduling primitive for time-based animations.
- Support built-in easing algorithms and custom easing callbacks.
- Allow animations to target entities or arbitrary resolvable objects.
- Expose `onStart`, `onUpdate`, and `onComplete` handlers.
- Keep authored visual motion in the visual phase by default.
- Make common transforms and color tweens concise.
- Keep the core small enough that future sprite animation and overlay animation can reuse it.

## Non-Goals

- skeletal animation, blend trees, or GPU skinning
- a full animation graph or state machine authoring system
- replacing `TransitionManager`
- replacing scene-specific procedural systems like large parallax world motion
- automatic ECS/component migration
- editor timelines or animation import pipelines

## Proposed Architecture

The system should be split into two layers.

### Layer 1: Engine Utility Layer

This layer is reusable and does not depend on `Scene` or `Entity`.

Candidate files:

- `include/vde/Easing.h`
- `include/vde/PlaybackClock.h`
- `include/vde/Tween.h`

Responsibilities:

- define the standard easing enum
- centralize delay, duration, pause, speed, and playback-mode bookkeeping for time-driven systems
- evaluate easing curves deterministically
- provide typed interpolation helpers for simple tweenable values

This layer should be header-only where practical.

It should build on the existing philosophy of `Timing.h`: keep timing helpers small and reusable. `Cooldown` and `RepeatingTimer` remain the right tools for gameplay timers; the new playback helper is for authored playback state.

### Layer 2: API Animation Service

This layer is scene-aware and lives in the game API.

Candidate files:

- `include/vde/api/Animator.h`
- `src/api/Animator.cpp`

Responsibilities:

- own active animation jobs for one scene
- resolve targets safely each frame
- invoke start, update, and completion handlers
- expose convenience tween helpers for common entity properties

This layer should be owned by `Scene`, not by `Game` globally.

## Core Types

### Easing

```cpp
namespace vde {

enum class AnimationEasing : uint8_t {
    Linear,
    EaseInQuad,
    EaseOutQuad,
    EaseInOutQuad,
    EaseInCubic,
    EaseOutCubic,
    EaseInOutCubic,
    EaseInSine,
    EaseOutSine,
    EaseInOutSine,
    EaseOutBack,
    EaseOutBounce,
    EaseOutElastic,
    Custom
};

float evaluateEasing(AnimationEasing easing, float t);

}  // namespace vde
```

Design notes:

- `t` is always clamped to `[0, 1]` before evaluation.
- built-in curves should be allocation-free and deterministic
- `Custom` uses a stored callable in the higher-level animation request

This enum should be reused by future systems instead of creating parallel enums. In particular, the overlay-sheet design should consume this type rather than shipping a separate `SheetEasing` surface.

### Tween utility

```cpp
namespace vde {

template <typename T>
T tweenValue(const T& from, const T& to, float easedProgress);

}  // namespace vde
```

Initial built-in support should cover:

- `float`
- `glm::vec2`
- `glm::vec3`
- `glm::vec4`
- `Color`
- `Position`
- `Scale`

`Rotation` should stay callback-driven in v1. VDE stores Euler rotations today, and a rushed public interpolation rule would be harder to undo later.

### Playback clock

The generic animator and transition system both need the same core timing behavior:

- delay handling
- duration tracking
- pause/resume
- speed scaling
- playback modes such as once, loop, and ping-pong
- linear and eased progress snapshots

That should live in a small shared helper instead of being reimplemented independently in `TransitionManager` and `Animator`.

```cpp
namespace vde {

class PlaybackClock {
    public:
        void configure(float duration, float delay, float speed, AnimationPlayback playback);

        void setPaused(bool paused);
        void setSpeed(float speed);
        void update(float deltaTime);

        bool hasStarted() const;
        bool isComplete() const;
        AnimationContext snapshot(AnimationEasing easing) const;
};

}  // namespace vde
```

This is intentionally a utility, not a user-facing sequencing system.

## Scene-Facing API

### Playback and options

```cpp
namespace vde {

enum class AnimationPlayback : uint8_t {
    Once,
    Loop,
    PingPong
};

struct AnimationOptions {
    float duration = 0.0f;
    float delay = 0.0f;
    float speed = 1.0f;
    AnimationPlayback playback = AnimationPlayback::Once;
    AnimationEasing easing = AnimationEasing::EaseOutCubic;
    bool startPaused = false;
};

struct AnimationContext {
    float deltaTime = 0.0f;
    float elapsed = 0.0f;
    float duration = 0.0f;
    float linearProgress = 0.0f;
    float easedProgress = 0.0f;
    uint32_t cycleIndex = 0;
    bool reversePass = false;
};

}  // namespace vde
```

Rules:

- `duration <= 0` is treated as immediate
- `speed` must be positive
- `delay < 0` is clamped to zero
- `linearProgress` is the raw `[0, 1]` progress
- `easedProgress` is the value after easing evaluation

### Target binding

Animations need a safe way to find their target every frame.

```cpp
namespace vde {

template <typename T>
class AnimationBinding {
  public:
    static AnimationBinding<T> entity(EntityId id);
    static AnimationBinding<T> weak(std::weak_ptr<T> object);
    static AnimationBinding<T> resolver(std::function<T*()> resolve);

    T* resolve(Scene& scene) const;
};

}  // namespace vde
```

Binding semantics:

- `entity(id)` resolves through `Scene::getEntity(id)` each frame
- `weak()` resolves through `weak_ptr::lock()` each frame
- `resolver()` is the escape hatch for scene-owned objects such as cameras, lights, or other non-entity objects

This gives us three safe attachment styles without forcing all objects into the same ownership model.

Examples of what `resolver()` is for:

- `Camera2D*` returned by `getCamera()`
- a scene-owned gameplay helper object stored as a member
- a lightweight tool/view-model object that is not an `Entity`

Rule: raw pointers are never stored directly by the animation system. A raw pointer is only ever produced by a resolver at the moment it is used.

### Handlers

```cpp
namespace vde {

struct AnimationCallbacks {
    std::function<void(const AnimationContext&)> onStart;
    std::function<void(const AnimationContext&)> onUpdate;
    std::function<void(const AnimationContext&)> onComplete;
};

template <typename T>
struct BoundAnimationCallbacks {
    std::function<void(T&, const AnimationContext&)> onStart;
    std::function<void(T&, const AnimationContext&)> onUpdate;
    std::function<void(T&, const AnimationContext&)> onComplete;
};

}  // namespace vde
```

Callback rules:

- `onStart` fires once, after any delay has elapsed, before the first `onUpdate`
- `onUpdate` fires every active frame, including the completion frame
- `onComplete` fires once after the final `onUpdate` when the animation reaches its natural end
- cancellation does not call `onComplete`
- if the target cannot be resolved when the animation should start or continue, the animation is cancelled silently

This mirrors the transition lifecycle already used by `TransitionManager`.

### Handle

```cpp
namespace vde {

class AnimationHandle {
  public:
    bool isValid() const;
    bool isActive() const;

    void cancel();
    void pause();
    void resume();

    void setSpeed(float speed);
    float getSpeed() const;
};

}  // namespace vde
```

The handle is intentionally small. Sequencing can be built in user code by starting a second animation from `onComplete`.

### Animator service

```cpp
namespace vde {

class Animator {
  public:
    AnimationHandle schedule(const AnimationOptions& options, AnimationCallbacks callbacks);

    template <typename T>
    AnimationHandle schedule(const AnimationBinding<T>& binding,
                             const AnimationOptions& options,
                             BoundAnimationCallbacks<T> callbacks);

    template <typename T, typename Value>
    AnimationHandle tween(const AnimationBinding<T>& binding,
                          const Value& from,
                          const Value& to,
                          const AnimationOptions& options,
                          std::function<void(T&, const Value&)> setter);

    void update(float deltaTime);
    void cancelAll();
    void pauseAll();
    void resumeAll();
    void setGlobalSpeed(float speed);
};

}  // namespace vde
```

The generic `schedule()` API is the primitive. `tween()` is convenience on top of it.

## Scene Integration

### Scene surface

```cpp
class Scene {
  public:
    Animator& animations();
    const Animator& animations() const;

  private:
    Animator m_animator;
};
```

Why scene-owned:

- scene lifetime naturally bounds animation lifetime
- entity resolution already lives at scene scope
- background-update and transition behavior already route through scene update rules
- no new game-wide singleton is required

### Scheduler integration

Animator updates should be wired into the scheduler, not left to user discipline.

The current engine should be adjusted here before the final public API lands.
The exact scheduler graph, timed-event phase, scene-independence model, and thread-affinity
rules are defined in [ANIMATION_ENGINE_CHANGES_PLAN.md](ANIMATION_ENGINE_CHANGES_PLAN.md).
The requirements below are the public-design contract that plan must satisfy.

#### Required phase model

The animation design does not require one exact enum layout, but it does require one exact
relationship: the generic animator must run after gameplay and post-physics synchronization,
but before pre-render work and draw submission.

The companion engine plan may insert one or more scene-local service phases between
`PostPhysics` and `Visual` to support timed events and future scene runtime services.
That is acceptable as long as the animation pass still remains the final visual writer.

#### Legacy scene path

For scenes using plain `update(float dt)`:

1. `scene.update(dt)` runs in `GameLogic`
2. physics and post-physics run as they do today
3. any scene-local timing services the engine adds run before the animation pass
4. `Scene::animations().update(dt)` runs in the real `Visual` phase
5. render happens later as usual

#### Phase-callback path

For scenes using phase callbacks:

1. `updateGameLogic(dt)`
2. `updateAudio(dt)`
3. physics and post-physics, when enabled
4. any scene-local timing services the engine adds run before the visual pass
5. `updateVisuals(dt)` in the real `Visual` phase
6. `Scene::animations().update(dt)` immediately after `updateVisuals(dt)` in that same phase

This ordering is intentional. It means authored animation wins the final visual value for the frame without interfering with gameplay decisions made earlier in the frame.

It also fixes the current semantic mismatch where `updateVisuals()` is documented as a visual callback but is presently inserted into the `GameLogic` chain.

### Why not run animations inside `Entity::update()`?

Because the system is meant for authored visual motion, not for replacing gameplay or physics updates.

Running after scene logic has several advantages:

- gameplay can make decisions from non-eased state
- animation can override final presentation for the current frame
- one scene-level pass can animate entities, camera, lights, or other scene-owned objects uniformly

## Convenience Helpers

The primitive callback API is flexible but too verbose for the common case. VDE should ship a small convenience layer on top of it.

Suggested helpers:

```cpp
AnimationHandle animatePosition(EntityId entityId,
                                const glm::vec3& from,
                                const glm::vec3& to,
                                const AnimationOptions& options);

AnimationHandle animateScale(EntityId entityId,
                             const Scale& from,
                             const Scale& to,
                             const AnimationOptions& options);

AnimationHandle animateColor(EntityId entityId,
                             const Color& from,
                             const Color& to,
                             const AnimationOptions& options);
```

Rules for convenience helpers:

- keep them limited to obviously common cases
- implement them in terms of `tween()`
- do not add dozens of special-purpose helpers in v1

## Example Usage

### 1. Entity-bound animation

This covers the current `transition_demo` title bob and similar authored entity motion.

```cpp
auto titleId = title->getId();

animations().schedule<MeshEntity>(
    AnimationBinding<MeshEntity>::entity(titleId),
    {
        .duration = 0.35f,
        .playback = AnimationPlayback::PingPong,
        .easing = AnimationEasing::EaseInOutSine,
    },
    {
        .onStart = [](MeshEntity& entity, const AnimationContext&) {
            entity.setColor(Color{1.0f, 0.95f, 0.3f, 1.0f});
        },
        .onUpdate = [](MeshEntity& entity, const AnimationContext& ctx) {
            float y = 2.0f + (0.3f * ctx.easedProgress);
            entity.setPosition(0.0f, y, 0.0f);
        },
        .onComplete = [](MeshEntity& entity, const AnimationContext&) {
            entity.setColor(Color{0.9f, 0.9f, 0.2f, 1.0f});
        },
    }
);
```

### 2. Scene-owned object animation

This covers cameras, lights, UI models, or other non-entity objects.

```cpp
animations().schedule<Camera2D>(
    AnimationBinding<Camera2D>::resolver([this]() {
        return dynamic_cast<Camera2D*>(getCamera());
    }),
    {
        .duration = 0.20f,
        .easing = AnimationEasing::EaseOutCubic,
    },
    {
        .onUpdate = [](Camera2D& camera, const AnimationContext& ctx) {
            camera.setZoom(1.0f + (0.2f * ctx.easedProgress));
        },
    }
);
```

### 3. Shared-object animation

This covers objects already managed by `shared_ptr`.

```cpp
animations().schedule<MyPanelModel>(
    AnimationBinding<MyPanelModel>::weak(m_panel),
    {
        .duration = 0.25f,
        .easing = AnimationEasing::EaseOutBack,
    },
    {
        .onStart = [](MyPanelModel& panel, const AnimationContext&) {
            panel.visible = true;
        },
        .onUpdate = [](MyPanelModel& panel, const AnimationContext& ctx) {
            panel.offset = 1.0f - ctx.easedProgress;
        },
        .onComplete = [](MyPanelModel& panel, const AnimationContext&) {
            panel.offset = 0.0f;
        },
    }
);
```

## Lifetime And Cancellation Rules

This is the most important design area.

### Entity attachments

- store `EntityId`, not raw entity pointers
- resolve through `Scene::getEntity()` each frame
- if the entity is removed, cancel the animation

### Weak object attachments

- resolve by `weak_ptr::lock()` each frame
- if the object expires, cancel the animation

### Resolver attachments

- call the resolver each frame
- if it returns `nullptr`, cancel the animation
- users are responsible for making the resolver safe

### Scene exit

When a scene exits or is destroyed:

- all active animations in its `Animator` are destroyed
- `onComplete` is not fired for those animations
- no cross-scene carry-over occurs in v1

This matches the fact that scene ownership is the lifetime boundary.

### Cancellation inside callbacks

The runner must tolerate:

- `handle.cancel()` from inside `onUpdate`
- scene code removing the target during a callback
- scene code scheduling another animation from `onComplete`

The implementation should defer structural cleanup until after the current animation tick finishes.

## Relationship To Existing Systems

### Transition system

`TransitionManager` should remain separate.

Reasons:

- it owns special Vulkan resources
- it composites two offscreen scenes
- it is not just value interpolation

What should be shared:

- lifecycle vocabulary (`onStart`, `update`, `onComplete`)
- timing context shape (`deltaTime`, `elapsed`, `duration`, progress)
- easing evaluation via the common `Easing.h`

Future cleanup opportunity:

- built-in transitions can optionally expose `AnimationEasing` or use the common easing helpers internally
- built-in transitions can adopt the shared `PlaybackClock` helper instead of maintaining a private version of the same playback state

### Sprite animation

The planned sprite animation API should not be replaced by `Animator`.

Instead:

- sprite animation remains a focused frame-sequencing system
- it can reuse the same easing/timing helpers where useful
- frame-event callbacks belong to sprite animation, not to generic `Animator`

That keeps the generic system small while still enabling later reuse.

### Overlay sheets and UI motion

The existing overlay-sheet design introduced `SheetEasing` and `SheetAnimation`.
That should be collapsed into the generic animation vocabulary instead of shipping a second easing API.

Recommended direction:

- replace `SheetEasing` with `AnimationEasing`
- implement overlay motion through the scene animation service or a thin wrapper around it

## What The System Should Not Try To Own

Not every moving thing in VDE should become an `Animator` job.

Examples that should remain scene-specific:

- large parallax worlds with hundreds of pieces sharing one procedural rule
- physics-driven transform updates
- sprite frame-state machines with named clips and frame events
- long-running simulation state driven by gameplay rules rather than authored timing

Rule of thumb:

- use `Animator` for authored, finite, handle-based motion
- use custom scene systems for bulk procedural simulation

## Implementation Notes

### Internal storage

The implementation can stay simple in v1:

- store active jobs in a vector
- identify them by stable animation id plus generation
- swap-erase completed jobs after update

That is enough for the expected scene-level animation counts.

### Scheduler flags

Animator callbacks mutate scene objects and should be treated as main-thread-only once the scheduler's worker-thread path matters.

Today the scheduler still executes sequentially, so this is not a blocker. It should still be treated as a design requirement so the animation system does not accidentally depend on callbacks being worker-safe.

### Per-frame work

Each active job does this:

1. apply scene/global speed and handle speed
2. process delay
3. resolve target if bound
4. compute linear progress
5. compute eased progress
6. fire callbacks in the correct order
7. finish, repeat, or reverse according to playback mode

No per-frame heap allocation should occur once the job is created.

### Immediate animations

For `duration <= 0`, process the whole lifecycle in one tick:

1. `onStart`
2. `onUpdate` with progress = 1
3. `onComplete`

This matches the current transition behavior for non-visual instant completion.

## Testing Strategy

Unit tests should cover at least:

- easing evaluation at 0, midpoint, and 1
- clamping of invalid duration, delay, and speed inputs
- callback order for normal completion
- entity removal cancels a bound animation
- weak object expiration cancels a bound animation
- resolver returning null cancels a bound animation
- loop and ping-pong playback semantics
- pause/resume and speed scaling
- zero-duration immediate completion
- starting a second animation from `onComplete`

Integration coverage should include:

- a scene animation that still runs correctly in legacy `update()` scenes
- a scene animation that runs in phase-callback scenes during the visual phase
- a transition scenario where source-scene freezing naturally freezes scene-bound animations because the scene itself stops updating
- a scheduler-order test proving `updateVisuals()` and `Animator` run after `PostPhysics` and before `PreRender`
- a regression test proving physics-synced entities are not overwritten after the animator runs

## Adoption Plan

### First consumers

The first migrations should be small and visible:

1. `examples/transition_demo/main.cpp`
   - title bob
   - circular showcase movement can stay custom if desired

2. `examples/resource_demo/main.cpp`
   - sprite rotation and scale pulse

3. overlay-sheet motion once that API lands

### Deliberate non-migration in v1

Do not force-convert these immediately:

- `examples/parallax_demo`
- bulk orbital scenes with many mathematically-related entities
- sprite-sheet frame players

Those systems teach different patterns and are not the right benchmark for a small authored-animation layer.

## Open Questions

These are the decisions worth revisiting before implementation, but they should not block the core design.

1. Should `Animator` support an optional `onCancel` callback in v1, or is silent cancellation enough initially?
2. Do we want a game-level animator later for cross-scene UI, or should that remain a separate overlay/UI concern?
3. Should `Rotation` interpolation wait for a quaternion-backed camera/entity path before becoming a first-class tween type?
4. Do we want a tiny sequence helper later, or is chaining through `onComplete` sufficient for the first release?

## Recommendation

Implement this in four small steps:

1. Land the engine foundation in [ANIMATION_ENGINE_CHANGES_PLAN.md](ANIMATION_ENGINE_CHANGES_PLAN.md).
2. Add `Easing.h`, `PlaybackClock.h`, and `Tween.h` as the shared timing/animation utility layer.
3. Add scene-owned `Animator` with bound and unbound scheduling plus handles, and wire it into the `Visual` phase.
4. Add only a few transform/color convenience helpers and migrate one or two examples.

That gives VDE a generic animation system that fits the current engine, fixes the current phase-model mismatch instead of designing around it, aligns with the transition lifecycle, supports easing, and solves the repetitive example boilerplate without overcommitting the engine to a much larger animation framework.