# API Utility Library Plan

This document outlines a phased plan for adding a compact gameplay utility library to the VDE API.

The need is broader than any one retired example. Current examples still carry repeated local helpers for 2D math, bounds checks, simple kinematics, timers, sprite-sheet animation, random numbers, and asset location/loading. That is a sign that the public API has solid rendering and scene primitives but still lacks a thin gameplay-authoring layer for small and mid-sized games.

The goal is to make common game code shorter and clearer without hiding engine ownership, lifetime, or rendering rules.

## Evidence From Current Examples

Current examples already show recurring authoring pain points:

- `breakout_demo` implements local AABB intersection and ball-velocity normalization.
- `sidescroller` implements a local `Physics2D` struct, platform bounds, and sprite-sheet animation timing.
- `asteroids_demo` repeats angle-to-direction math and manual speed clamping.
- `audio_demo` manually searches multiple asset directories and hand-rolls clip-loading fallback logic.
- `physics_audio_demo` and `parallel_physics_demo` use ad hoc `rand()`-style randomness rather than deterministic streams.

The plan should address those repeated needs without baking example-specific game rules into the API.

## Problem Summary

The current API is missing a lightweight utility layer for these repeated tasks:

- 2D vector math and scalar helpers used in moment-to-moment gameplay code.
- Rectangle and bounds helpers for overlap checks, containment, and point clamping.
- Simple non-physics movement helpers for games that do not want full rigid-body simulation.
- Conversion helpers between `glm::vec2`, `Position`, and `WorldBounds2D`.
- Reusable timing helpers for cooldowns, animation stepping, and periodic events.
- Seedable randomness for deterministic tests, replays, and content spawning.
- Minimal sprite-sheet helpers so games do not need to reimplement UV frame math.
- Asset location and resource-loading conveniences that remove hard-coded path-search boilerplate.

That is a signal that the public API is missing a lightweight gameplay-authoring layer.

At the same time, the API already has important foundational types:

- `Position`, `Direction`, `Rotation`, and `Scale` in `include/vde/api/GameTypes.h`
- `WorldBounds2D` in `include/vde/api/WorldBounds.h`
- `CameraBounds2D` and pixel/world mapping in `include/vde/api/CameraBounds.h`
- `Scene`, `Game`, `ResourceManager`, and `SpriteEntity`
- `PhysicsScene` for cases that need full simulation
- Broad `glm::vec2` usage across physics and 2D-facing APIs

Because of that existing surface area, the right direction is to add a compact authoring layer on top of current types, not to introduce a parallel mini-engine.

## Goals

- Remove repeated gameplay boilerplate from examples and small games.
- Make common top-down, side-view, arcade, and 2D UI-adjacent code shorter and easier to read.
- Reuse existing API types instead of creating parallel concepts.
- Keep foundational helpers deterministic, small, and easy to unit test.
- Provide seedable randomness and reusable timers so tests and scripted runs stay predictable.
- Expose mature helpers through `GameAPI.h` so simple games get them by default.
- Keep scene-aware convenience explicit about resource ownership, failure behavior, and upload timing.

## Non-Goals

- Replacing GLM with a custom linear algebra library.
- Building a second physics engine alongside `PhysicsScene`.
- Shipping a full animation graph or state-machine framework in this utility layer.
- Turning `Scene` into an opaque catch-all object that hides core engine behavior.
- Hiding asset upload, resource lifetime, or load failures behind silent magic.

## Design Principles

### 1. Build on existing API types

`glm::vec2` is already the de facto 2D vector type across camera, physics, bounds, and input-adjacent code. `Position` remains the gameplay-facing 3D type. The utility layer should bridge those types, not replace them.

### 2. Keep the core deterministic

Math, bounds, kinematics, timers, and RNG should produce repeatable results from explicit inputs. If a helper uses randomness, it should take or own a seedable random stream rather than touch global state.

### 3. Separate foundational helpers from scene-aware convenience

Foundational helpers should stay pure or nearly pure. Scene/game-aware convenience should live in a separate layer so low-level utilities remain easy to test, reason about, and reuse.

### 4. Prefer small value types and free functions over frameworks

Most of these gaps can be closed with free functions, small structs, and tiny utility classes. New base classes should be the exception, not the default response.

### 5. Optimize for repeated authoring work

The library should target the things developers write repeatedly: clamp, move, overlap, tick, resolve asset paths, load a common resource, and animate a sprite sheet. It should not try to abstract every possible game rule.

## Recommended Design Direction

### 1. Use `glm::vec2` as the base 2D vector type

The current API already uses `glm::vec2` heavily in camera, bounds, physics, and example gameplay code. Adding a custom `vde::Vec2` type would create conversion churn with limited benefit.

Recommendation:

- Add helpers that operate on `glm::vec2`
- Keep `Position` as the 3D gameplay-facing type
- Add explicit conversion helpers between `glm::vec2` and `Position`
- Revisit a custom wrapper only if clear ergonomics problems remain after the helper layer lands

### 2. Extend `WorldBounds2D` before inventing new geometry types

`WorldBounds2D` already represents a 2D axis-aligned region and already provides `fromCenter`, `contains`, and `center`. That makes it a better home for rectangle-style gameplay helpers than introducing a near-duplicate `Rect2D` type.

Recommendation:

- Extend `WorldBounds2D` with overlap and point utility operations
- Use `WorldBounds2D` in manual movement helpers
- Only add new geometry types if a clearly different abstraction is needed later

### 3. Add a small utility toolkit, not one monolithic module

The current gaps fall into six layers:

- math and conversions
- bounds and simple collision geometry
- manual kinematics
- randomness and timing
- sprite authoring
- scene-aware asset convenience

These should ship in phases, with each layer useful on its own.

### 4. Keep scene-aware convenience explicit

If a helper loads resources or mutates entities, it should be obvious from the API shape. Prefer names like `resolveAssetPath`, `loadTextureAsset`, `applyFrame`, or `advance` over hidden auto-behavior.

## Proposed Library Shape

### A. `include/vde/api/Math2D.h`

Header-only free functions for common 2D gameplay math.

Candidate surface:

```cpp
namespace vde::math2d {

float clamp(float value, float minValue, float maxValue);
float saturate(float value);
float lerp(float from, float to, float t);
float inverseLerp(float from, float to, float value);
bool nearlyZero(float value, float epsilon = 0.0001f);
bool nearlyEqual(float a, float b, float epsilon = 0.0001f);

float lengthSquared(const glm::vec2& value);
float distanceSquared(const glm::vec2& a, const glm::vec2& b);
glm::vec2 normalizeOrZero(const glm::vec2& value, float epsilon = 0.0001f);
glm::vec2 lerp(const glm::vec2& from, const glm::vec2& to, float t);
glm::vec2 moveToward(const glm::vec2& current, const glm::vec2& target, float maxDelta);
glm::vec2 perpendicularLeft(const glm::vec2& value);
glm::vec2 perpendicularRight(const glm::vec2& value);
glm::vec2 directionFromAngleDegrees(float angleDegrees);
float angleDegreesFromUp(const glm::vec2& direction);
float angleDegreesFromRight(const glm::vec2& direction);

Position toPosition(const glm::vec2& value, float z = 0.0f);
glm::vec2 toVec2(const Position& value);

}  // namespace vde::math2d
```

Notes:

- Keep this header small and pure.
- Prefer simple, non-allocating helpers.
- Avoid wrapping GLM functions when calling GLM directly is already clearer.

### B. `include/vde/api/WorldBounds.h` enhancements

Add more gameplay-oriented helpers directly to `WorldBounds2D`.

Candidate additions:

- `bool intersects(const WorldBounds2D& other) const`
- `bool contains(const WorldBounds2D& other) const`
- `glm::vec2 size() const`
- `glm::vec2 halfExtents() const`
- `glm::vec2 clampPoint(const glm::vec2& point) const`
- `glm::vec2 closestPoint(const glm::vec2& point) const`
- `WorldBounds2D translated(const glm::vec2& delta) const`
- `WorldBounds2D expanded(Meters amountX, Meters amountY) const`
- `static WorldBounds2D fromCenter(const glm::vec2& center, const glm::vec2& size)`

This gives games a canonical AABB type without introducing a second rectangle representation.

### C. `include/vde/api/Kinematics2D.h`

Stateless helpers for non-physics 2D gameplay movement.

This module should cover the manual-movement case without overlapping too much with `PhysicsScene`.

Candidate surface:

```cpp
namespace vde::kinematics2d {

struct MoveResult {
    glm::vec2 position;
    bool blockedX = false;
    bool blockedY = false;
};

WorldBounds2D boundsFromCenterSize(const glm::vec2& center,
                                   const glm::vec2& size);

bool canOccupy(const WorldBounds2D& candidate,
               std::span<const WorldBounds2D> blockers,
               const WorldBounds2D& worldBounds);

MoveResult moveAxisSeparated(const glm::vec2& start,
                             const glm::vec2& delta,
                             const glm::vec2& size,
                             std::span<const WorldBounds2D> blockers,
                             const WorldBounds2D& worldBounds);

}  // namespace vde::kinematics2d
```

Rules for this module:

- Keep it AABB-only in the initial version.
- Keep it deterministic and stateless.
- Do not add forces, impulses, restitution, or solver behavior already handled by `PhysicsScene`.
- Return axis-block information so gameplay code can react without recomputing collisions.
- Position it as a helper for simple arcade movement, not as a full collision framework.

### D. `include/vde/api/Random.h`

Seedable randomness for gameplay code, tests, and scripted runs.

Candidate surface:

```cpp
namespace vde {

class RandomStream {
  public:
    explicit RandomStream(uint32_t seed = 0u);
    static RandomStream fromEntropy();

    void reseed(uint32_t seed);
    uint32_t seed() const;

    float unit();
    float range(float minValue, float maxValue);
    int rangeInt(int minInclusive, int maxInclusive);
    bool chance(float probability);

    glm::vec2 unitDirection2D();
    glm::vec2 inside(const WorldBounds2D& bounds);
};

}  // namespace vde
```

Notes:

- Back this with `std::mt19937` or a similarly stable engine, not `rand()`.
- Seeded construction should be deterministic.
- Keep the surface small and predictable rather than trying to mirror a full RNG library.

### E. `include/vde/api/Timing.h`

Small reusable time/state helpers for common gameplay patterns.

Candidate surface:

```cpp
namespace vde {

class Cooldown {
  public:
    explicit Cooldown(float durationSeconds = 0.0f);

    void setDuration(float durationSeconds);
    void start();
    void reset();
    void finish();

    void advance(float deltaTime);
    bool ready() const;
    bool tryConsume();

    float remaining() const;
    float progress() const;
};

class RepeatingTimer {
  public:
    explicit RepeatingTimer(float intervalSeconds = 0.0f);

    void setInterval(float intervalSeconds);
    void reset();
    int advance(float deltaTime);
};

}  // namespace vde
```

This should cover:

- one-shot cooldowns
- recurring events
- spawn intervals
- blink/flash timing
- animation stepping without open-coded accumulators everywhere

### F. `include/vde/api/SpriteAnimation2D.h`

Minimal sprite-sheet animation helpers for frame-based 2D games.

Candidate surface:

```cpp
namespace vde {

struct SpriteSheetLayout {
    int frameCount = 1;
    int columns = 1;

    glm::vec4 uvRectForFrame(int frame) const;
};

struct SpriteAnimationClip {
    std::vector<int> frames;
    float frameTime = 0.1f;
    bool looping = true;
};

class SpriteAnimationPlayer {
  public:
    void setLayout(const SpriteSheetLayout& layout);
    void play(const SpriteAnimationClip& clip, bool restart = true);
    void pause();
    void stop();
    void setFrame(int frame);
    void advance(float deltaTime);
    void applyTo(SpriteEntity& sprite) const;
};

}  // namespace vde
```

Rules for this module:

- Keep it focused on frame sequencing and UV application.
- Do not turn it into a full animation graph or blending system.
- Make it usable both from plain scenes and from user-defined entity subclasses.

### G. Asset location and resource convenience

Current examples show that the bigger pain is not just loading a texture. It is finding the right asset path, handling reasonable fallbacks, and keeping resource ownership clear.

Candidate direction:

- Add asset search roots to `GameSettings` or a small asset-locator service owned by `Game`
- Add `Game::resolveAssetPath(relativePath)` for explicit path resolution
- Add `Scene::addTextureAsset(relativePath)` and `Scene::addAudioAsset(relativePath)` convenience entry points
- Add optional fallback texture creation via `Game::createSolidTexture(color, width = 8, height = 8)`
- Keep returned types compatible with the existing `ResourceManager` and `Scene::addResource` model

Important constraints:

- These helpers must preserve existing ownership and upload rules.
- They should complement `Scene::addResource` rather than bypass the resource system entirely.
- Path resolution order and failure behavior should be explicit and testable.

## File Plan

Initial file additions and changes:

- `include/vde/api/Math2D.h`
- `include/vde/api/Kinematics2D.h`
- `include/vde/api/Random.h`
- `include/vde/api/Timing.h`
- `include/vde/api/SpriteAnimation2D.h`
- `include/vde/api/WorldBounds.h` updates
- `include/vde/api/GameAPI.h` updates
- `include/vde/api/Game.h` and/or `include/vde/api/GameSettings.h` updates for asset resolution
- `include/vde/api/Scene.h` updates for resource convenience
- Root `CMakeLists.txt` updates for new public headers and tests

Planned test coverage:

- `tests/Math2D_test.cpp`
- `tests/Kinematics2D_test.cpp`
- `tests/Random_test.cpp`
- `tests/Timing_test.cpp`
- `tests/SpriteAnimation2D_test.cpp`
- `tests/AssetHelpers_test.cpp`

Possible implementation files if the asset layer is not header-only:

- `src/api/AssetHelpers.cpp`

## Phased Implementation Plan

### Phase 1: Audit and API Design Freeze

Goal: define the smallest useful utility surface before writing code.

Tasks:

- Audit helper duplication in `breakout_demo`, `sidescroller`, `asteroids_demo`, `audio_demo`, and other current examples
- Group duplicated logic into math, bounds, kinematics, timing, randomness, sprite authoring, and asset convenience
- Confirm which helpers should operate on `glm::vec2`, `Position`, `Meters`, `WorldBounds2D`, and `SpriteEntity`
- Freeze the initial public naming and namespace layout

Acceptance criteria:

- No custom `vde::Vec2` is introduced without documented justification.
- Every proposed helper maps to a current example or a clearly recurring authoring problem.
- The initial surface is small enough to stay teachable and testable.

### Phase 2: Math and Bounds Foundation

Goal: add the reusable 2D math layer and strengthen existing bounds types.

Tasks:

- Add `Math2D.h` as a header-only utility module
- Extend `WorldBounds2D` with overlap and convenience operations
- Add focused unit tests for clamp, normalization, interpolation, angle conversion, and bounds overlap
- Document the new helpers in API docs

Acceptance criteria:

- `breakout_demo` can replace local overlap and normalization helpers with API utilities.
- `asteroids_demo` can replace inline angle-to-direction code with `Math2D`.
- New helpers are available through `GameAPI.h`.

### Phase 3: Manual Kinematics Helpers

Goal: support simple top-down or side-view games that do not need full physics.

Tasks:

- Add `Kinematics2D.h`
- Implement occupancy and axis-separated movement helpers using `WorldBounds2D`
- Add unit tests covering blockers, world-bounds clamping, and movement along each axis
- Refactor one current 2D example or add a focused API sample that uses the new helpers

Acceptance criteria:

- A simple non-physics 2D game can move and collide against AABB blockers without defining local rectangle and occupancy helpers.
- The new movement helpers remain clearly scoped and do not duplicate `PhysicsScene`.

### Phase 4: Random and Timing Utilities

Goal: remove repeated ad hoc randomness and accumulator boilerplate.

Tasks:

- Add `Random.h` and `Timing.h`
- Add tests for deterministic seeding, range behavior, cooldown consumption, and repeat timers
- Refactor at least one example using `rand()`-style randomness to use `RandomStream`
- Refactor at least one example using manual cooldown or animation accumulators to use the timing helpers

Acceptance criteria:

- Migrated example code no longer uses `rand()`.
- Examples can express cooldowns, repeated events, and basic animation stepping without custom timer scaffolding.
- Seeded utility behavior is deterministic under test.

### Phase 5: Asset Location and Resource Convenience

Goal: reduce asset-loading boilerplate without obscuring ownership or failure rules.

Tasks:

- Decide whether asset location lives in `GameSettings`, `Game`, or a dedicated asset-locator helper
- Add explicit asset-path resolution with configurable search roots
- Add texture and audio loading convenience that builds on the existing resource model
- Add fallback solid-texture creation in an engine-owned location
- Add tests for search order, failure cases, and fallback behavior

Acceptance criteria:

- `audio_demo` no longer scans multiple hard-coded directories to find assets.
- Texture and audio convenience APIs preserve current `ResourceManager` semantics.
- Failure behavior remains explicit to callers.

### Phase 6: Sprite Authoring Conveniences

Goal: reduce sprite-sheet and frame-animation boilerplate for 2D games.

Tasks:

- Add `SpriteAnimation2D.h`
- Add tests covering UV mapping, looping, pausing, and frame stepping
- Refactor `sidescroller` or another current sprite-based example to use the new helper layer
- Document the intended scope so the feature does not grow into a full animation system

Acceptance criteria:

- A sprite-based example can animate through an API utility without custom UV frame math.
- The layer stays small and does not become an animation graph or ECS subsystem.

### Phase 7: Example Migration and Documentation

Goal: verify that the new library actually simplifies game authoring.

Tasks:

- Refactor multiple current examples to consume the new helpers
- Update docs and quick-start materials to show the intended workflow
- Add a short migration note for users who currently write raw helper blocks in examples

Acceptance criteria:

- At least three current examples become meaningfully shorter or easier to read.
- The new API is discoverable from `GameAPI.h` and documentation.
- The examples demonstrate both foundational helpers and scene-aware convenience without hiding core engine behavior.

## Risks and Guardrails

- If the project introduces a custom vector type too early, conversions will spread through camera, physics, and bounds code. Avoid that in v1.
- If `Kinematics2D` grows beyond AABB occupancy and simple movement, it will overlap confusingly with `PhysicsScene`.
- If random helpers fall back to hidden global state, tests and scripted runs will become harder to trust.
- If timing helpers grow into a general coroutine or sequencing system, the utility layer will lose focus.
- If sprite animation helpers turn into a state-graph framework, they will stop being lightweight.
- If asset helpers are too magical, they will hide load failures and resource-lifetime rules.
- If too many convenience functions are added without clear evidence from current examples, the API will accumulate noise faster than value.

## Recommended First Slice

The best first implementation slice is:

1. `Math2D.h`
2. `WorldBounds2D` enhancements
3. `Timing.h`
4. Refactor `breakout_demo` and `asteroids_demo` to consume those pieces

That slice delivers visible simplification with low architectural risk. It proves the core conventions around `glm::vec2`, `Position`, bounds, and lightweight authoring helpers before the plan expands into resources or sprite animation.

After that lands, the next highest-value slice is `Random.h` plus asset-path resolution, because both show up as repeated pain in current example code.

## Adjacent Needs To Track Separately

These are real authoring gaps, but they should stay outside the initial utility-library scope unless repeated evidence grows stronger:

- input action mapping and rebinding
- camera follow/dead-zone helpers
- tilemap and parallax systems
- full animation state machines or blend graphs
- save-game schema helpers

## Success Metric

This plan is successful if a simple game can:

- use `GameAPI.h`
- express common 2D math and bounds checks without writing its own helper block
- convert cleanly between `glm::vec2`, `Position`, and world bounds
- express cooldowns and repeated events without open-coded accumulators
- use deterministic randomness when tests or scripted runs require it
- resolve and load common assets without hard-coded search-path logic
- animate a sprite sheet without rewriting UV frame bookkeeping

That is the right level of simplification for VDE: common gameplay helpers in the API, without turning the engine into a giant catch-all utility layer.
