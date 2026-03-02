# Multi-Scene Physics & Scheduler Design

## Executive Summary

This document proposes a design for concurrent multi-scene support in VDE, where each scene owns a physics simulation that runs at its own fixed timestep, decoupled from visual rendering. A new **Scheduler** orchestrates per-frame work — input, physics, game logic, audio, and rendering — across all active scenes, with optional multi-threaded execution for independent physics updates. The scheduler's task graph expresses the natural dependency chain: **physics → game logic → audio + visuals**, ensuring audio cues are triggered by game events (not raw physics) and that each subsystem sees a consistent world state.

---

## 1. Problem Statement

The current VDE architecture has a single active scene per frame. Only the active scene receives `update()` and `render()` calls. This prevents:

- Multiple simulations running concurrently (e.g., overworld + dungeon interior)
- Physics running at a fixed timestep independent of the render framerate
- Parallel physics updates across independent scenes
- Entities that exist in both a physics world and a visual scene with different update cadences

The `API_SUGGESTION_MULTI_SCENE.md` document identified background-scene updates as the highest-priority gap. This design extends that idea into a full scheduler-based architecture with integrated physics.

---

## 2. Design Goals

1. **Decoupled update rates** — Physics simulates at a fixed timestep (e.g., 60 Hz) while rendering runs at the display refresh rate
2. **Multiple simultaneous scenes** — The game loop updates and renders N scenes per frame
3. **Deterministic scheduling** — A scheduler defines the order and grouping of per-frame operations
4. **Thread-safe parallelism** — Independent scene physics can run concurrently on separate threads
5. **Physics-visual entity binding** — A single game object can have a physics body and a visual entity that stay synchronized
6. **Incremental adoption** — Existing single-scene games continue to work unchanged; physics is opt-in per scene
7. **Minimal Vulkan changes** — Rendering remains single-threaded on the main thread (Vulkan command recording)

---

## 3. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────────┐
│                              Game                                    │
│  ┌──────────┐  ┌────────────┐  ┌─────────────────────────────────┐  │
│  │ Window   │  │ Scheduler  │  │ SceneManager                    │  │
│  └──────────┘  │            │  │  ┌───────────┐  ┌───────────┐   │  │
│                │ TaskGraph   │  │  │ Scene A   │  │ Scene B   │   │  │
│                │ ThreadPool  │  │  │┌─────────┐│  │┌─────────┐│   │  │
│                └────────────┘  │  ││PhysScene ││  ││PhysScene ││   │  │
│                                │  │└─────────┘│  │└─────────┘│   │  │
│                                │  └───────────┘  └───────────┘   │  │
│                                └─────────────────────────────────┘  │
├─────────────────────────────────────────────────────────────────────┤
│                        VulkanContext / Rendering                     │
└─────────────────────────────────────────────────────────────────────┘
```

### Key Components

| Component | Responsibility |
|-----------|---------------|
| **Scheduler** | Builds a per-frame task graph and dispatches tasks, respecting ordering and parallelism |
| **PhysicsScene** | Fixed-timestep rigid-body/collision simulation owned by a Scene |
| **PhysicsBody** | A physics representation (position, velocity, shape, mass) living inside a PhysicsScene |
| **PhysicsEntity** | An Entity subclass that binds a visual Entity to a PhysicsBody |
| **SceneGroup** | Lightweight tag that marks which scenes are "active" for update/render each frame |
| **ThreadPool** | Simple worker-thread pool used by the Scheduler for parallel task execution |

---

## 4. Scheduler

### 4.1 Concept

The Scheduler replaces the current hard-coded `Game::run()` loop body with a declarative task graph. Each frame the Scheduler executes the graph, which describes **what** to do, **in what order**, and **what can run in parallel**.

### 4.2 Task Types

```cpp
enum class TaskPhase {
    Input,       // Read input state (always main thread)
    Physics,     // Step physics simulations (parallelizable across scenes)
    PostPhysics, // Sync physics results back to entities (per-scene, main thread)
    GameLogic,   // Scene game logic: AI, scoring, state machines (per-scene)
    Audio,       // Audio cue submission, listener updates, AudioManager tick
    Visual,      // Animate, interpolate, prepare visual-only state (per-scene)
    PreRender,   // UBO writes, descriptor updates, camera apply
    Render       // Record Vulkan commands (always main thread)
};
```

The previous single `Update` phase is now split into three ordered phases — **GameLogic**, **Audio**, and **Visual** — to express the natural dependency chain:

```
Physics ──▶ PostPhysics ──▶ GameLogic ──▶ Audio
                                     └──▶ Visual ──▶ PreRender ──▶ Render
```

See [Section 4.7](#47-game-logic-audio-and-visual-dependency-graph) for the full rationale.

### 4.3 API

```cpp
namespace vde {

/// Identifies a schedulable unit of work
struct TaskId {
    uint32_t id;
};

/// Describes dependencies and threading for a task
struct TaskDescriptor {
    std::string name;                       // e.g. "scene1.physics"
    TaskPhase phase;
    std::function<void()> execute;          // The work to run
    bool mainThreadOnly = false;            // Force main-thread execution
    std::vector<TaskId> dependsOn;          // Tasks that must complete first
};

class Scheduler {
public:
    Scheduler();
    ~Scheduler();

    /// Set the number of worker threads (0 = single-threaded, default)
    void setWorkerThreadCount(uint32_t count);

    /// Register a task and get its ID
    TaskId addTask(const TaskDescriptor& desc);

    /// Remove a task
    void removeTask(TaskId id);

    /// Execute one frame's worth of tasks in dependency order
    void execute();

    /// Clear all tasks (e.g., when reconfiguring scenes)
    void clear();

    /// Convenience: build the default task graph for a set of active scenes
    void buildDefaultGraph(Game& game, const std::vector<Scene*>& activeScenes);

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace vde
```

### 4.4 Default Task Graph

For three active scenes, `buildDefaultGraph` produces:

```
Frame N:
  ┌─ Input.update ─────────────────────────────────────┐  (main thread)
  │                                                     │
  ├─ Scene1.physics.step ──┐                            │  (worker threads,
  ├─ Scene2.physics.step ──┤  parallel                  │   can run concurrently)
  ├─ Scene3.physics.step ──┘                            │
  │                                                     │
  ├─ Scene1.postPhysics ───┐                            │  (sync transforms,
  ├─ Scene2.postPhysics ───┤  sequential per scene      │   main thread)
  ├─ Scene3.postPhysics ───┘                            │
  │                                                     │
  ├─ Scene1.gameLogic ─────┐                            │  (AI, scoring, state,
  ├─ Scene2.gameLogic ─────┤  main thread per scene     │   collision responses)
  ├─ Scene3.gameLogic ─────┘                            │
  │                                                     │
  ├─ Scene1.audio ─────────┐                            │  (audio cue submit,
  ├─ Scene2.audio ─────────┤  main thread per scene     │   per-scene listener)
  ├─ Scene3.audio ─────────┘                            │
  ├─ audio.global ─────────                             │  (AudioManager::update)
  │                                                     │
  ├─ Scene1.visual ────────┐                            │  (animation, interp,
  ├─ Scene2.visual ────────┤  main thread per scene     │   particle systems)
  ├─ Scene3.visual ────────┘                            │
  │                                                     │
  ├─ Scene1.preRender ─────┐                            │  (UBO updates,
  ├─ Scene2.preRender ─────┤  sequential                │   descriptor writes)
  ├─ Scene3.preRender ─────┘                            │
  │                                                     │
  └─ Render (all scenes) ──────────────────────────────┘  (main thread, Vulkan)
```

Note that **audio** and **visual** both depend on **gameLogic** but are independent of each other. The scheduler can interleave or parallelize them when there are no cross-phase data dependencies.

### 4.5 Custom Scheduling

Power users can build entirely custom graphs:

```cpp
// Example: physics for scene2 runs at double rate (two steps per frame)
auto inputTask = scheduler.addTask({
    .name = "input",
    .phase = TaskPhase::Input,
    .execute = [&]() { game.processInput(); },
    .mainThreadOnly = true
});

auto phys1 = scheduler.addTask({
    .name = "scene1.physics",
    .phase = TaskPhase::Physics,
    .execute = [&]() { scene1->getPhysicsScene()->step(); },
    .dependsOn = {inputTask}
});

auto phys2a = scheduler.addTask({
    .name = "scene2.physics.step1",
    .phase = TaskPhase::Physics,
    .execute = [&]() { scene2->getPhysicsScene()->step(); },
    .dependsOn = {inputTask}
});

auto phys2b = scheduler.addTask({
    .name = "scene2.physics.step2",
    .phase = TaskPhase::Physics,
    .execute = [&]() { scene2->getPhysicsScene()->step(); },
    .dependsOn = {phys2a}  // must follow first step
});

// Visual updates depend on all physics completing
auto vis1 = scheduler.addTask({
    .name = "scene1.visuals",
    .phase = TaskPhase::Update,
    .execute = [&]() { scene1->update(dt); },
    .mainThreadOnly = true,
    .dependsOn = {phys1}
});
// ... etc.
```

### 4.7 Game Logic, Audio, and Visual Dependency Graph

The original design had a single `Update` phase that combined game logic, audio triggering, and visual preparation into one callback. This works for simple games but creates problems as scenes grow:

1. **Audio cues depend on game state** — A collision sound should play only after the game logic has processed the collision event and decided the outcome (e.g., did the player die or just bounce?). If audio runs inside the same callback as game logic, ordering within the callback is implicit and fragile.

2. **Visuals depend on game state** — Particle effects, screen shake, and animation triggers depend on decisions made in game logic (e.g., spawn explosion particles after health reaches zero).

3. **Audio and visuals are independent** — Sound effects don't depend on visual state and vice versa. They can run concurrently or in any order after game logic completes.

4. **AudioManager is global** — The `AudioManager::update()` call (which processes streaming, fades, and 3D positioning) should run once per frame after all scenes have submitted their audio cues, not interleaved between scene updates.

This leads to the following **per-scene dependency DAG**:

```
                    ┌──────────┐
                    │  Input   │
                    └────┬─────┘
                         │
                    ┌────▼─────┐
                    │ Physics  │    (fixed timestep, parallelizable)
                    └────┬─────┘
                         │
                    ┌────▼──────┐
                    │PostPhysics│   (sync body transforms → entities)
                    └────┬──────┘
                         │
                    ┌────▼──────┐
                    │ GameLogic │   (AI, scoring, collision response,
                    └──┬────┬──┘    state machines, event generation)
                       │    │
              ┌────────▼┐  ┌▼────────┐
              │  Audio   │  │ Visual  │  (independent of each other)
              └────┬─────┘  └────┬────┘
                   │             │
              ┌────▼─────────────▼────┐
              │      PreRender        │  (UBOs, descriptors, camera)
              └──────────┬────────────┘
                         │
                    ┌────▼─────┐
                    │  Render  │  (Vulkan command recording)
                    └──────────┘
```

Across multiple scenes, the cross-scene dependency graph looks like:

```
                         Input
                       /   |   \
                      /    |    \
               S1.phys  S2.phys  S3.phys          ← parallel
                  |        |        |
             S1.postP   S2.postP  S3.postP
                  |        |        |
             S1.game    S2.game   S3.game          ← per-scene logic
               / \       / \       / \
          S1.aud S1.vis S2.aud S2.vis S3.aud S3.vis  ← independent pairs
              \   /  \   |   /  \   /
               \ /    \  |  /    \ /
            audio.global (AudioManager::update)    ← once, after all cues
                         |
                     PreRender
                         |
                      Render
```

#### Scene Callbacks

To support this split, the `Scene` class gains three overridable methods replacing the single `update()`:

```cpp
class Scene {
public:
    // ... existing lifecycle methods ...

    /// Called during the GameLogic phase.
    /// Process game rules, AI, collision responses, state transitions.
    /// This is where you decide *what happened* this frame.
    virtual void updateGameLogic(float deltaTime);

    /// Called during the Audio phase.
    /// Submit audio cues (play sounds, update music, set listener position)
    /// based on the game state computed in updateGameLogic().
    virtual void updateAudio(float deltaTime);

    /// Called during the Visual phase.
    /// Update animations, particle systems, interpolation, camera follow.
    /// Prepare entities for rendering based on current game state.
    virtual void updateVisuals(float deltaTime);

    /// Legacy: Called if none of the above are overridden.
    /// Dispatches to updateGameLogic → updateAudio → updateVisuals internally.
    virtual void update(float deltaTime) override;
};
```

**Backwards compatibility:** If a subclass only overrides `update()`, the default implementations of `updateGameLogic`, `updateAudio`, and `updateVisuals` are no-ops, and the scheduler calls `update()` as a single combined task in the `GameLogic` phase — identical to the current behavior. The split only activates when the subclass overrides the individual methods.

Detection strategy:

```cpp
// Scene tracks which methods are overridden
bool Scene::usesPhaseCallbacks() const {
    // Returns true if any of the three phase methods are overridden.
    // Can be set via a flag in the subclass constructor or detected
    // via a registration call:
    return m_usesPhaseCallbacks;
}

// Subclass opts in:
class BattleScene : public vde::Scene {
public:
    BattleScene() { enablePhaseCallbacks(); }  // opts into the 3-phase model

    void updateGameLogic(float dt) override { /* ... */ }
    void updateAudio(float dt) override { /* ... */ }
    void updateVisuals(float dt) override { /* ... */ }
};
```

#### Audio Event Queue

Rather than calling `AudioManager::playSFX()` directly from `updateGameLogic()` (which would work but mixes concerns), scenes can queue audio events that are drained during the Audio phase:

```cpp
/// An audio event to be submitted during the Audio phase.
struct AudioEvent {
    enum class Type { PlaySFX, PlayMusic, Stop, SetVolume, SetPosition };
    Type type;
    std::shared_ptr<AudioClip> clip;
    float volume = 1.0f;
    float pitch = 1.0f;
    bool loop = false;
    Position position;          // For 3D spatial audio
    uint32_t soundId = 0;       // For Stop/SetVolume targeting existing sounds
};

class Scene {
public:
    // Queue an audio event from game logic (thread-safe if needed)
    void queueAudioEvent(const AudioEvent& event);

    // Queue a one-shot sound effect (convenience)
    void playSFX(const std::shared_ptr<AudioClip>& clip,
                 float volume = 1.0f, float pitch = 1.0f);

    // Queue a positional sound effect at an entity's location
    void playSFXAt(const std::shared_ptr<AudioClip>& clip,
                   const Position& pos, float volume = 1.0f);

protected:
    std::vector<AudioEvent> m_audioEventQueue;
};
```

During the Audio phase, the Scene's default `updateAudio()` drains the queue:

```cpp
void Scene::updateAudio(float /*deltaTime*/) {
    auto& audio = AudioManager::getInstance();

    for (auto& event : m_audioEventQueue) {
        switch (event.type) {
        case AudioEvent::Type::PlaySFX:
            audio.playSFX(event.clip, event.volume, event.pitch, event.loop);
            break;
        case AudioEvent::Type::PlayMusic:
            audio.playMusic(event.clip, event.volume, event.loop);
            break;
        case AudioEvent::Type::Stop:
            audio.stopSound(event.soundId);
            break;
        // ... etc
        }
    }
    m_audioEventQueue.clear();

    // Update 3D listener position from camera
    if (auto* cam = getCamera()) {
        auto pos = cam->getPosition();
        audio.setListenerPosition(pos.x, pos.y, pos.z);
    }
}
```

This pattern has several advantages:

- **Deterministic ordering**: All game logic runs first, then all audio cues are submitted in a well-defined order.
- **Batch-friendly**: The audio backend can batch-submit sounds for lower latency.
- **Thread-safe**: If game logic ever runs on a worker thread, the queue protects the AudioManager from concurrent access.
- **Debuggable**: The queue can be inspected, logged, or replayed.

#### Global Audio Tick

After all per-scene audio tasks complete, a single global `audio.global` task runs `AudioManager::getInstance().update(deltaTime)`. This:

- Processes streaming buffers
- Applies fade-in/fade-out curves
- Updates 3D spatialization
- Cleans up finished one-shot sounds

This replaces the current inline `AudioManager::getInstance().update(m_deltaTime)` call in `Game::run()`.

#### Collision → Game Logic → Audio Example

Here's a concrete example showing the full chain:

```cpp
class BattleScene : public vde::Scene {
public:
    BattleScene() { enablePhaseCallbacks(); }

    void onEnter() override {
        enablePhysics({.gravity = -15.0f});

        m_hitSound = addResource<vde::AudioClip>("sfx/hit.wav");
        m_deathSound = addResource<vde::AudioClip>("sfx/death.wav");

        // --- Physics collision callback (runs during Physics phase) ---
        getPhysicsScene()->setOnCollisionBegin(
            [this](const vde::CollisionEvent& event) {
                // DON'T play sounds here — we're on a worker thread
                // and game logic hasn't decided the outcome yet.
                // Instead, record the collision for game logic:
                m_pendingCollisions.push_back(event);
            });
    }

    void updateGameLogic(float dt) override {
        // Process collisions recorded during physics
        for (auto& col : m_pendingCollisions) {
            auto* entityA = getEntityByPhysicsBody(col.bodyA);
            auto* entityB = getEntityByPhysicsBody(col.bodyB);

            if (isBullet(entityA) && isEnemy(entityB)) {
                auto* enemy = static_cast<EnemyEntity*>(entityB);
                enemy->takeDamage(25.0f);

                if (enemy->isDead()) {
                    // Queue death sound + explosion visual
                    playSFXAt(getResource<vde::AudioClip>(m_deathSound),
                              enemy->getPosition(), 0.8f);
                    m_pendingExplosions.push_back(enemy->getPosition());
                    removeEntity(enemy->getId());
                } else {
                    // Queue hit sound
                    playSFXAt(getResource<vde::AudioClip>(m_hitSound),
                              enemy->getPosition(), 0.5f);
                }
            }
        }
        m_pendingCollisions.clear();
    }

    // updateAudio() — base class drains m_audioEventQueue automatically

    void updateVisuals(float dt) override {
        // Spawn particle effects for pending explosions
        for (auto& pos : m_pendingExplosions) {
            spawnExplosionParticles(pos);
        }
        m_pendingExplosions.clear();

        // Update existing particle systems
        updateParticles(dt);
    }

private:
    std::vector<vde::CollisionEvent> m_pendingCollisions;
    std::vector<vde::Position> m_pendingExplosions;
    vde::ResourceId m_hitSound;
    vde::ResourceId m_deathSound;
};
```

The execution order for this frame:

```
1. Physics        → detects bullet-enemy collision, appends to m_pendingCollisions
2. PostPhysics    → sync body transforms to entity positions
3. GameLogic      → processes collision: deals damage, queues playSFXAt + explosion
4. Audio          → drains audio queue: submits hit/death sounds to AudioManager
5. Visual         → spawns explosion particles at recorded positions
6. PreRender      → UBO updates
7. Render         → draw everything
```

No step accesses data that hasn't been produced by a prior step.

### 4.8 Thread Pool Implementation Strategy

```cpp
class ThreadPool {
public:
    explicit ThreadPool(uint32_t threadCount);
    ~ThreadPool();

    /// Submit a callable and get a future
    template <typename F>
    std::future<void> submit(F&& func);

    /// Wait for all submitted tasks to complete
    void waitAll();

private:
    std::vector<std::thread> m_workers;
    std::queue<std::function<void()>> m_tasks;
    std::mutex m_mutex;
    std::condition_variable m_cv;
    bool m_shutdown = false;
};
```

The thread pool is intentionally simple. Tasks are `std::function<void()>` with `std::future` for synchronization. The Scheduler uses the pool for tasks not marked `mainThreadOnly` when `workerThreadCount > 0`.

---

## 5. Physics Scene

### 5.1 Concept

Each `Scene` can optionally own a `PhysicsScene` — a self-contained physics simulation with its own fixed timestep, body list, and collision detection. The `PhysicsScene` accumulates real time and steps in fixed increments, producing interpolation alpha for smooth rendering.

### 5.2 PhysicsScene API

```cpp
namespace vde {

/// Configuration for a physics simulation
struct PhysicsConfig {
    float fixedTimestep = 1.0f / 60.0f;   // 60 Hz default
    float gravity = -9.81f;                 // Y-axis gravity
    uint32_t maxSubSteps = 8;               // Max catch-up steps per frame
    uint32_t velocityIterations = 6;
    uint32_t positionIterations = 2;
};

/// Handle to a physics body within a PhysicsScene
using PhysicsBodyId = uint64_t;
constexpr PhysicsBodyId INVALID_PHYSICS_BODY_ID = 0;

/// Shape types for collision
enum class PhysicsShape {
    Box,
    Circle,      // 2D
    Sphere,      // 3D
    Capsule,
    ConvexHull,
    StaticMesh   // Triangle mesh, static only
};

/// Describes how a body behaves
enum class PhysicsBodyType {
    Static,      // Never moves (walls, ground)
    Dynamic,     // Fully simulated (players, projectiles)
    Kinematic    // Moved by code, affects dynamics (platforms)
};

/// Properties for creating a physics body
struct PhysicsBodyDef {
    PhysicsBodyType type = PhysicsBodyType::Dynamic;
    PhysicsShape shape = PhysicsShape::Box;
    Position position;
    Rotation rotation;
    glm::vec3 halfExtents = {0.5f, 0.5f, 0.5f};  // For box
    float radius = 0.5f;                            // For circle/sphere
    float mass = 1.0f;
    float friction = 0.3f;
    float restitution = 0.2f;
    float linearDamping = 0.0f;
    float angularDamping = 0.05f;
    bool isSensor = false;  // Triggers callbacks but no collision response
};

/// Read-only snapshot of a body's state after physics step
struct PhysicsBodyState {
    Position position;
    Rotation rotation;
    glm::vec3 linearVelocity;
    glm::vec3 angularVelocity;
    bool isAwake;
};

/// Collision event data
struct CollisionEvent {
    PhysicsBodyId bodyA;
    PhysicsBodyId bodyB;
    Position contactPoint;
    glm::vec3 contactNormal;
    float penetrationDepth;
};

/// Callback for collision events
using CollisionCallback = std::function<void(const CollisionEvent&)>;

class PhysicsScene {
public:
    explicit PhysicsScene(const PhysicsConfig& config = {});
    ~PhysicsScene();

    // Configuration
    const PhysicsConfig& getConfig() const;
    void setGravity(float gravity);

    // Body management
    PhysicsBodyId createBody(const PhysicsBodyDef& def);
    void destroyBody(PhysicsBodyId id);
    PhysicsBodyState getBodyState(PhysicsBodyId id) const;

    // Apply forces/impulses
    void applyForce(PhysicsBodyId id, const glm::vec3& force);
    void applyImpulse(PhysicsBodyId id, const glm::vec3& impulse);
    void applyTorque(PhysicsBodyId id, const glm::vec3& torque);
    void setLinearVelocity(PhysicsBodyId id, const glm::vec3& velocity);
    void setAngularVelocity(PhysicsBodyId id, const glm::vec3& velocity);

    // Teleport (for kinematic bodies or resets)
    void setBodyPosition(PhysicsBodyId id, const Position& pos);
    void setBodyRotation(PhysicsBodyId id, const Rotation& rot);

    // Queries
    struct RaycastHit {
        PhysicsBodyId bodyId;
        Position point;
        glm::vec3 normal;
        float distance;
    };
    bool raycast(const Position& origin, const Direction& direction,
                 float maxDistance, RaycastHit& outHit) const;

    std::vector<PhysicsBodyId> queryAABB(const Position& min,
                                          const Position& max) const;

    // Simulation
    /// Accumulate real time and step physics in fixed increments.
    /// Returns the interpolation alpha (0..1) for rendering.
    float step(float deltaTime);

    /// Get interpolation alpha from last step() call
    float getInterpolationAlpha() const;

    /// Get the number of sub-steps executed in the last step() call
    uint32_t getLastStepCount() const;

    // Collision callbacks
    void setOnCollisionBegin(CollisionCallback callback);
    void setOnCollisionEnd(CollisionCallback callback);

    // Debug
    uint32_t getBodyCount() const;
    uint32_t getActiveBodyCount() const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace vde
```

### 5.3 Fixed-Timestep Accumulator

The core simulation loop inside `PhysicsScene::step()`:

```cpp
float PhysicsScene::step(float deltaTime) {
    m_accumulator += deltaTime;
    m_lastStepCount = 0;

    while (m_accumulator >= m_config.fixedTimestep
           && m_lastStepCount < m_config.maxSubSteps) {
        // Save previous state for interpolation
        savePreviousState();

        // Integrate
        integrateVelocities(m_config.fixedTimestep);
        broadPhase();
        narrowPhase();
        solveConstraints();
        integratePositions(m_config.fixedTimestep);

        m_accumulator -= m_config.fixedTimestep;
        m_lastStepCount++;
    }

    // Compute interpolation alpha for rendering
    m_interpolationAlpha = m_accumulator / m_config.fixedTimestep;
    return m_interpolationAlpha;
}
```

This ensures physics is deterministic regardless of render framerate. The interpolation alpha lets visuals smoothly blend between the two most recent physics states.

---

## 6. Physics-Visual Entity Binding

### 6.1 PhysicsEntity

A `PhysicsEntity` bridges the gap between a `PhysicsBody` (in the physics simulation) and a visual `Entity` (in the scene graph). It owns a reference to both.

```cpp
namespace vde {

/// A game entity that has both a physics body and a visual representation.
///
/// During physics step: the PhysicsBody is simulated.
/// During postPhysics: the visual Entity's transform is updated from the body.
/// During render: the visual Entity is rendered at the interpolated position.
class PhysicsEntity : public Entity {
public:
    PhysicsEntity();
    virtual ~PhysicsEntity();

    /// Create or reassign the physics body in the owning scene's PhysicsScene
    void createPhysicsBody(const PhysicsBodyDef& def);

    /// Get the physics body ID
    PhysicsBodyId getPhysicsBodyId() const { return m_bodyId; }

    /// Get the current physics state
    PhysicsBodyState getPhysicsState() const;

    // --- Force / impulse helpers (delegates to PhysicsScene) ---
    void applyForce(const glm::vec3& force);
    void applyImpulse(const glm::vec3& impulse);
    void setLinearVelocity(const glm::vec3& velocity);

    // --- Sync ---

    /// Copy physics body transform → visual entity transform.
    /// Called automatically during PostPhysics phase.
    /// Override for custom interpolation or animation blending.
    virtual void syncFromPhysics(float interpolationAlpha);

    /// Copy visual entity transform → physics body.
    /// Useful for kinematic bodies driven by animation.
    virtual void syncToPhysics();

    /// Set whether this entity auto-syncs from physics each frame
    void setAutoSync(bool enabled) { m_autoSync = enabled; }
    bool getAutoSync() const { return m_autoSync; }

    // Entity overrides
    void onAttach(Scene* scene) override;
    void onDetach() override;

protected:
    PhysicsBodyId m_bodyId = INVALID_PHYSICS_BODY_ID;
    bool m_autoSync = true;

    // Previous frame state for interpolation
    Position m_prevPosition;
    Rotation m_prevRotation;
};

} // namespace vde
```

### 6.2 Sync Flow

```
Physics Step (fixed dt)          PostPhysics (once per frame)       Render
┌──────────────────┐            ┌──────────────────────────┐      ┌────────────┐
│  Simulate bodies │ ────────▶  │  For each PhysicsEntity: │ ──▶  │ Render at  │
│  at 60 Hz        │            │    body.pos → entity.pos │      │ interpolated│
│                  │            │    (with interp alpha)   │      │ position    │
└──────────────────┘            └──────────────────────────┘      └────────────┘
```

The interpolation in `syncFromPhysics`:

```cpp
void PhysicsEntity::syncFromPhysics(float alpha) {
    auto state = getPhysicsState();

    // Interpolate between previous and current physics position
    float x = m_prevPosition.x + (state.position.x - m_prevPosition.x) * alpha;
    float y = m_prevPosition.y + (state.position.y - m_prevPosition.y) * alpha;
    float z = m_prevPosition.z + (state.position.z - m_prevPosition.z) * alpha;

    setPosition(x, y, z);
    // Similar for rotation with slerp

    // Store for next frame
    m_prevPosition = state.position;
    m_prevRotation = state.rotation;
}
```

### 6.3 Specialized Physics Entities

```cpp
/// A MeshEntity with physics. Renders a 3D mesh and simulates physics.
class PhysicsMeshEntity : public PhysicsEntity {
public:
    // Inherits all MeshEntity visual features + PhysicsEntity physics
    void setMesh(std::shared_ptr<Mesh> mesh);
    void setMeshId(ResourceId meshId);
    void setTexture(std::shared_ptr<Texture> texture);
    void setMaterial(std::shared_ptr<Material> material);
    void render() override;  // Delegates to MeshEntity rendering

private:
    // Visual properties (same as MeshEntity)
    std::shared_ptr<Mesh> m_mesh;
    std::shared_ptr<Texture> m_texture;
    std::shared_ptr<Material> m_material;
    ResourceId m_meshId = INVALID_RESOURCE_ID;
    ResourceId m_textureId = INVALID_RESOURCE_ID;
    Color m_color = Color::white();
};

/// A SpriteEntity with physics. 2D sprite + 2D physics body.
class PhysicsSpriteEntity : public PhysicsEntity {
public:
    // Inherits all SpriteEntity visual features + PhysicsEntity physics
    void setTexture(std::shared_ptr<Texture> texture);
    void setTextureId(ResourceId textureId);
    void setColor(const Color& color);
    void setUVRect(float u, float v, float w, float h);
    void render() override;

private:
    std::shared_ptr<Texture> m_texture;
    ResourceId m_textureId = INVALID_RESOURCE_ID;
    Color m_color = Color::white();
    // UV rect, etc.
};
```

---

## 7. Scene Integration

### 7.1 Scene Changes

The `Scene` class gains an optional `PhysicsScene`:

```cpp
class Scene {
public:
    // ... existing API ...

    // --- Physics ---

    /// Enable physics for this scene with the given configuration.
    /// Call during onEnter() or before the game loop starts.
    void enablePhysics(const PhysicsConfig& config = {});

    /// Disable and destroy the physics scene.
    void disablePhysics();

    /// Check if this scene has physics enabled.
    bool hasPhysics() const { return m_physicsScene != nullptr; }

    /// Get the physics scene (nullptr if not enabled).
    PhysicsScene* getPhysicsScene() { return m_physicsScene.get(); }
    const PhysicsScene* getPhysicsScene() const { return m_physicsScene.get(); }

    // --- Multi-scene support ---

    /// Mark this scene for background updates (update called even when not "active")
    void setContinueInBackground(bool enabled);
    bool getContinueInBackground() const;

    /// Set the scene's update priority (lower = earlier). Default is 0.
    void setUpdatePriority(int priority);
    int getUpdatePriority() const;

protected:
    std::unique_ptr<PhysicsScene> m_physicsScene;
    bool m_continueInBackground = false;
    int m_updatePriority = 0;
};
```

### 7.2 SceneGroup

A `SceneGroup` defines which scenes are simultaneously active:

```cpp
/// A named group of scenes that are all updated and rendered each frame.
struct SceneGroup {
    std::string name;
    std::vector<std::string> sceneNames;  // Ordered by update priority

    /// Convenience: create a group from scene names
    static SceneGroup create(const std::string& name,
                             std::initializer_list<std::string> scenes) {
        return {name, scenes};
    }
};
```

Game API additions:

```cpp
class Game {
public:
    // ... existing API ...

    // --- Multi-scene ---

    /// Set multiple scenes as simultaneously active.
    /// All scenes in the group receive update() and render() calls.
    void setActiveSceneGroup(const SceneGroup& group);

    /// Get the current scene group.
    const SceneGroup& getActiveSceneGroup() const;

    /// Convenience: activate a single scene (wraps setActiveSceneGroup)
    void setActiveScene(const std::string& name);  // existing, now wraps group

    // --- Scheduler ---

    /// Get the scheduler for advanced task configuration.
    Scheduler& getScheduler();
    const Scheduler& getScheduler() const;
};
```

---

## 8. Revised Game Loop

### 8.1 Default Loop (Backwards Compatible)

For single-scene games with no physics, the loop behaves identically to today:

```cpp
void Game::run() {
    onStart();
    if (m_activeScene) m_activeScene->onEnter();

    while (m_running && !m_window->shouldClose()) {
        m_window->pollEvents();
        updateTiming();
        processPendingSceneChange();

        // Scheduler handles everything
        m_scheduler.execute();

        m_vulkanContext->drawFrame();
        m_frameCount++;
    }

    vkDeviceWaitIdle(m_vulkanContext->getDevice());
    m_running = false;
}
```

### 8.2 Default Scheduler Graph Construction

When the user calls `setActiveScene("main")` or `setActiveSceneGroup(...)`, the Game rebuilds the scheduler graph:

```cpp
void Game::rebuildSchedulerGraph() {
    m_scheduler.clear();
    auto& scenes = getActiveScenesOrdered();

    // 1. Input (always first, main thread)
    auto inputTask = m_scheduler.addTask({
        .name = "input",
        .phase = TaskPhase::Input,
        .execute = [this]() { processInput(); },
        .mainThreadOnly = true
    });

    // 2. Physics steps (parallel across scenes)
    std::vector<TaskId> physTasks;
    for (auto* scene : scenes) {
        if (scene->hasPhysics()) {
            auto task = m_scheduler.addTask({
                .name = scene->getName() + ".physics",
                .phase = TaskPhase::Physics,
                .execute = [scene, this]() {
                    scene->getPhysicsScene()->step(m_deltaTime);
                },
                .dependsOn = {inputTask}
            });
            physTasks.push_back(task);
        }
    }

    // 3. Post-physics sync (per-scene, after its physics)
    std::vector<TaskId> syncTasks;
    for (size_t i = 0; i < scenes.size(); i++) {
        auto* scene = scenes[i];
        if (scene->hasPhysics()) {
            auto task = m_scheduler.addTask({
                .name = scene->getName() + ".postPhysics",
                .phase = TaskPhase::PostPhysics,
                .execute = [scene]() {
                    float alpha = scene->getPhysicsScene()
                                       ->getInterpolationAlpha();
                    for (auto& entity : scene->getEntities()) {
                        if (auto* pe = dynamic_cast<PhysicsEntity*>(
                                entity.get())) {
                            if (pe->getAutoSync()) {
                                pe->syncFromPhysics(alpha);
                            }
                        }
                    }
                },
                .mainThreadOnly = true,
                .dependsOn = {physTasks[i]}
            });
            syncTasks.push_back(task);
        }
    }

    // 4. Game logic (per-scene, after physics sync)
    std::vector<TaskId> logicDeps = syncTasks.empty()
                                        ? std::vector<TaskId>{inputTask}
                                        : syncTasks;
    std::vector<TaskId> logicTasks;
    for (auto* scene : scenes) {
        auto task = m_scheduler.addTask({
            .name = scene->getName() + ".gameLogic",
            .phase = TaskPhase::GameLogic,
            .execute = [scene, this]() {
                if (scene->usesPhaseCallbacks()) {
                    scene->updateGameLogic(m_deltaTime);
                } else {
                    // Legacy: single update() handles everything
                    scene->update(m_deltaTime);
                }
            },
            .mainThreadOnly = true,
            .dependsOn = logicDeps
        });
        logicTasks.push_back(task);
    }

    // 5. Audio (per-scene, after its game logic; independent of visuals)
    std::vector<TaskId> audioTasks;
    for (size_t i = 0; i < scenes.size(); i++) {
        auto* scene = scenes[i];
        if (scene->usesPhaseCallbacks()) {
            auto task = m_scheduler.addTask({
                .name = scene->getName() + ".audio",
                .phase = TaskPhase::Audio,
                .execute = [scene, this]() {
                    scene->updateAudio(m_deltaTime);
                },
                .mainThreadOnly = true,
                .dependsOn = {logicTasks[i]}
            });
            audioTasks.push_back(task);
        }
    }

    // 6. Visual (per-scene, after its game logic; independent of audio)
    std::vector<TaskId> visualTasks;
    for (size_t i = 0; i < scenes.size(); i++) {
        auto* scene = scenes[i];
        if (scene->usesPhaseCallbacks()) {
            auto task = m_scheduler.addTask({
                .name = scene->getName() + ".visual",
                .phase = TaskPhase::Visual,
                .execute = [scene, this]() {
                    scene->updateVisuals(m_deltaTime);
                },
                .mainThreadOnly = true,
                .dependsOn = {logicTasks[i]}
            });
            visualTasks.push_back(task);
        }
    }

    // 7. Global audio tick (after all per-scene audio tasks)
    std::vector<TaskId> preRenderDeps;
    {
        std::vector<TaskId> audioGlobalDeps = audioTasks;
        // Also depend on logic tasks for legacy scenes (no phase callbacks)
        for (size_t i = 0; i < scenes.size(); i++) {
            if (!scenes[i]->usesPhaseCallbacks()) {
                audioGlobalDeps.push_back(logicTasks[i]);
            }
        }
        auto audioGlobal = m_scheduler.addTask({
            .name = "audio.global",
            .phase = TaskPhase::Audio,
            .execute = [this]() {
                AudioManager::getInstance().update(m_deltaTime);
            },
            .mainThreadOnly = true,
            .dependsOn = audioGlobalDeps
        });

        // PreRender depends on: visual tasks + audio global
        preRenderDeps = visualTasks;
        preRenderDeps.push_back(audioGlobal);
        // Legacy scenes: depend on their logic task directly
        for (size_t i = 0; i < scenes.size(); i++) {
            if (!scenes[i]->usesPhaseCallbacks()) {
                preRenderDeps.push_back(logicTasks[i]);
            }
        }
    }

    // 8. PreRender (camera apply, UBO writes)
    std::vector<TaskId> preRenderTasks;
    for (auto* scene : scenes) {
        auto task = m_scheduler.addTask({
            .name = scene->getName() + ".preRender",
            .phase = TaskPhase::PreRender,
            .execute = [scene, this]() {
                if (scene->getCamera()) {
                    scene->getCamera()->applyTo(*m_vulkanContext);
                }
            },
            .mainThreadOnly = true,
            .dependsOn = preRenderDeps
        });
        preRenderTasks.push_back(task);
    }

    // 9. Render (after all preRender, main thread)
    m_scheduler.addTask({
        .name = "render",
        .phase = TaskPhase::Render,
        .execute = [this, &scenes]() {
            m_vulkanContext->setRenderCallback(
                [this, &scenes](VkCommandBuffer cmd) {
                    (void)cmd;
                    for (auto* scene : scenes) {
                        scene->render();
                    }
                    onRender();
                });
        },
        .mainThreadOnly = true,
        .dependsOn = preRenderTasks
    });
}
```

---

## 9. User-Facing API Examples

### 9.1 Simple Single-Scene with Physics (Backwards Compatible)

```cpp
class PlatformerScene : public vde::Scene {
public:
    void onEnter() override {
        // Enable physics at 60 Hz
        vde::PhysicsConfig physConfig;
        physConfig.fixedTimestep = 1.0f / 60.0f;
        physConfig.gravity = -20.0f;
        enablePhysics(physConfig);

        setCamera(new vde::SideScrollerCamera(vde::Meters(20.0f)));
        setLightBox(new vde::SimpleColorLightBox(vde::Color::white()));

        // Create a physics-enabled sprite
        auto player = addEntity<vde::PhysicsSpriteEntity>();
        player->setTexture(addResource<vde::Texture>("player.png"));
        player->createPhysicsBody({
            .type = vde::PhysicsBodyType::Dynamic,
            .shape = vde::PhysicsShape::Box,
            .position = {0, 5, 0},
            .halfExtents = {0.4f, 0.8f, 0.1f},
            .mass = 70.0f,
            .friction = 0.5f
        });

        // Create a static ground
        auto ground = addEntity<vde::PhysicsSpriteEntity>();
        ground->setColor(vde::Color::green());
        ground->createPhysicsBody({
            .type = vde::PhysicsBodyType::Static,
            .shape = vde::PhysicsShape::Box,
            .position = {0, -2, 0},
            .halfExtents = {20.0f, 0.5f, 0.1f}
        });
    }

    void update(float dt) override {
        auto* input = getInputHandler();
        if (auto* player = getEntity<vde::PhysicsSpriteEntity>(m_playerId)) {
            if (input->isKeyDown(vde::KEY_RIGHT)) {
                player->applyForce({500.0f, 0, 0});
            }
            if (input->isKeyDown(vde::KEY_SPACE)) {
                player->applyImpulse({0, 300.0f, 0});
            }
        }
        vde::Scene::update(dt);
    }
};

// main.cpp — no scheduler config needed, it just works
int main() {
    vde::Game game;
    vde::GameSettings settings;
    settings.gameName = "Platformer";
    game.initialize(settings);
    game.addScene("game", new PlatformerScene());
    game.setActiveScene("game");
    game.run();
}
```

### 9.2 Multi-Scene with Parallel Physics

```cpp
int main() {
    vde::Game game;
    game.initialize(settings);

    // Create scenes
    game.addScene("overworld", new OverworldScene());   // has physics
    game.addScene("dungeon", new DungeonScene());       // has physics
    game.addScene("hud", new HUDScene());               // no physics

    // Activate all three simultaneously
    game.setActiveSceneGroup(vde::SceneGroup::create("gameplay", {
        "overworld", "dungeon", "hud"
    }));

    // Enable parallel physics (2 worker threads)
    game.getScheduler().setWorkerThreadCount(2);

    game.run();
}
```

Frame execution will be:

```
Input.update                              (main)
├── overworld.physics.step ─┐             (worker 1)
├── dungeon.physics.step  ──┘             (worker 2, parallel)
├── overworld.postPhysics                 (main)
├── dungeon.postPhysics                   (main)
├── overworld.gameLogic                   (main)  ← AI, collisions, scoring
├── dungeon.gameLogic                     (main)
├── hud.gameLogic                         (main)
├── overworld.audio ─┐                    (main)  ← queue sounds from events
├── dungeon.audio  ──┤                    (main)
├── hud.audio      ──┘                    (main)
├── audio.global                          (main)  ← AudioManager::update()
├── overworld.visual ─┐                   (main)  ← particles, animation
├── dungeon.visual  ──┤                   (main)
├── hud.visual      ──┘                   (main)
├── PreRender (all scenes)                (main)
└── Render (overworld + dungeon + hud)    (main)
```

### 9.3 Custom Scheduler — Physics at Double Rate with Audio

```cpp
void configureCustomScheduler(vde::Game& game) {
    auto& sched = game.getScheduler();
    sched.clear();

    auto* scene = game.getScene("battle");
    float dt = game.getDeltaTime();

    auto input = sched.addTask({
        .name = "input",
        .phase = vde::TaskPhase::Input,
        .execute = [&game]() { /* input */ },
        .mainThreadOnly = true
    });

    // Run physics twice per frame for higher fidelity
    auto phys1 = sched.addTask({
        .name = "battle.physics.1",
        .phase = vde::TaskPhase::Physics,
        .execute = [scene, dt]() {
            scene->getPhysicsScene()->step(dt * 0.5f);
        },
        .dependsOn = {input}
    });

    auto phys2 = sched.addTask({
        .name = "battle.physics.2",
        .phase = vde::TaskPhase::Physics,
        .execute = [scene, dt]() {
            scene->getPhysicsScene()->step(dt * 0.5f);
        },
        .dependsOn = {phys1}
    });

    auto postPhys = sched.addTask({
        .name = "battle.postPhysics",
        .phase = vde::TaskPhase::PostPhysics,
        .execute = [scene]() { /* sync transforms */ },
        .mainThreadOnly = true,
        .dependsOn = {phys2}
    });

    // Game logic: process collisions, deal damage, update score
    auto logic = sched.addTask({
        .name = "battle.gameLogic",
        .phase = vde::TaskPhase::GameLogic,
        .execute = [scene, dt]() {
            scene->updateGameLogic(dt);
        },
        .mainThreadOnly = true,
        .dependsOn = {postPhys}
    });

    // Audio: submit cues from game events (depends on logic, not visuals)
    auto audio = sched.addTask({
        .name = "battle.audio",
        .phase = vde::TaskPhase::Audio,
        .execute = [scene, dt]() {
            scene->updateAudio(dt);
        },
        .mainThreadOnly = true,
        .dependsOn = {logic}
    });

    // Visuals: particles, animations (depends on logic, not audio)
    auto visual = sched.addTask({
        .name = "battle.visual",
        .phase = vde::TaskPhase::Visual,
        .execute = [scene, dt]() {
            scene->updateVisuals(dt);
        },
        .mainThreadOnly = true,
        .dependsOn = {logic}  // independent of audio
    });

    // Global audio manager tick
    auto audioGlobal = sched.addTask({
        .name = "audio.global",
        .phase = vde::TaskPhase::Audio,
        .execute = []() {
            vde::AudioManager::getInstance().update(dt);
        },
        .mainThreadOnly = true,
        .dependsOn = {audio}
    });

    // PreRender + Render depend on both audio.global and visual
    auto preRender = sched.addTask({
        .name = "battle.preRender",
        .phase = vde::TaskPhase::PreRender,
        .execute = [scene, &game]() {
            scene->getCamera()->applyTo(*game.getVulkanContext());
        },
        .mainThreadOnly = true,
        .dependsOn = {audioGlobal, visual}
    });

    sched.addTask({
        .name = "render",
        .phase = vde::TaskPhase::Render,
        .execute = [scene, &game]() { /* render */ },
        .mainThreadOnly = true,
        .dependsOn = {preRender}
    });
}
```

---

## 10. Implementation Plan

### Phase 1: Foundation (No Physics)

**Goal:** Multi-scene update/render with a basic scheduler, no threading.

| Task | Files | Notes |
|------|-------|-------|
| Add `SceneGroup` struct | `include/vde/api/SceneGroup.h` | Simple data struct |
| Add `Scheduler` with single-threaded execution | `include/vde/api/Scheduler.h`, `src/api/Scheduler.cpp` | Task graph, topological sort, single-thread dispatch |
| Add `Game::setActiveSceneGroup()` | `include/vde/api/Game.h`, `src/api/Game.cpp` | Builds default graph with multiple scenes |
| Add `Scene::setContinueInBackground()` | `include/vde/api/Scene.h`, `src/api/Scene.cpp` | Flag for background updates |
| Split Scene callbacks | `include/vde/api/Scene.h`, `src/api/Scene.cpp` | `updateGameLogic`, `updateAudio`, `updateVisuals` with `enablePhaseCallbacks()` |
| Add `AudioEvent` queue to Scene | `include/vde/api/Scene.h`, `src/api/Scene.cpp` | `queueAudioEvent()`, `playSFX()`, `playSFXAt()` |
| Move `AudioManager::update()` into scheduler | `src/api/Game.cpp` | `audio.global` task after per-scene audio tasks |
| Refactor `Game::run()` to use Scheduler | `src/api/Game.cpp` | Replace inline loop body with `m_scheduler.execute()` |
| Update multi-scene demo | `examples/multi_scene_demo/` | Demonstrate simultaneous scenes |

**Backwards compatibility:** `setActiveScene("name")` creates a single-scene group internally. Scenes that only override `update()` get a single combined `GameLogic` task — identical to current behavior.

### Phase 2: Physics Scene

**Goal:** Fixed-timestep physics simulation per scene, no threading.

| Task | Files | Notes |
|------|-------|-------|
| Create `PhysicsScene` | `include/vde/api/PhysicsScene.h`, `src/api/PhysicsScene.cpp` | Accumulator, body management, collision detection |
| Create `PhysicsBody` internals | `src/api/PhysicsBody.cpp` | Position integration, AABB broadphase |
| Add `Scene::enablePhysics()` | `include/vde/api/Scene.h`, `src/api/Scene.cpp` | Creates/owns PhysicsScene |
| Update Scheduler default graph | `src/api/Game.cpp` | Insert physics tasks when scene has physics |
| Create physics demo | `examples/physics_demo/` | Falling boxes, collision |

**Physics backend:** Start with a simple custom 2D physics engine (AABB + impulse resolution). This keeps dependencies minimal and is sufficient for 2D games. A future phase could integrate Box2D or Jolt for 3D.

### Phase 3: Physics Entities

**Goal:** Bind visual entities to physics bodies.

| Task | Files | Notes |
|------|-------|-------|
| Create `PhysicsEntity` | `include/vde/api/PhysicsEntity.h`, `src/api/PhysicsEntity.cpp` | Base class with sync logic |
| Create `PhysicsMeshEntity` | Same file or separate | 3D physics entity |
| Create `PhysicsSpriteEntity` | Same file or separate | 2D physics entity |
| Implement `syncFromPhysics()` | `src/api/PhysicsEntity.cpp` | Interpolated transform copy |
| Add PostPhysics scheduler phase | `src/api/Game.cpp` | Auto-sync all PhysicsEntities |
| Update physics demo | `examples/physics_demo/` | Interactive physics sprites |

### Phase 4: Thread Pool & Parallel Execution

**Goal:** Run independent scene physics on worker threads.

| Task | Files | Notes |
|------|-------|-------|
| Create `ThreadPool` | `include/vde/api/ThreadPool.h`, `src/api/ThreadPool.cpp` | Worker-thread pool |
| Integrate into Scheduler | `src/api/Scheduler.cpp` | Dispatch non-main-thread tasks to pool |
| Add `Scheduler::setWorkerThreadCount()` | Already in API | Wire up thread pool |
| Thread-safety audit | `PhysicsScene`, `Scene` | Ensure no shared mutable state between scenes |
| Performance benchmark | `examples/` or `tests/` | Compare single vs multi-threaded physics |

**Thread safety rules:**
- Each `PhysicsScene` is fully independent — no shared state between scenes during physics step
- Entity access during physics step is read-only (forces queued before step)
- PostPhysics sync and all rendering happen on the main thread
- The `ThreadPool` only runs `TaskPhase::Physics` tasks by default

### Phase 5: Advanced Features

**Goal:** Collision callbacks, raycasting, constraints.

| Task | Notes |
|------|-------|
| Collision callbacks (`onCollisionBegin`/`onCollisionEnd`) | Per-body and per-scene callbacks |
| Raycast queries | For line-of-sight, ground detection |
| AABB queries | For area triggers |
| Joint/constraint system | Distance, revolute, prismatic joints |
| 3D physics backend option | Integrate Jolt Physics for full 3D support |

---

## 11. File Structure

New and modified files:

```
include/vde/api/
├── GameAPI.h               (modified: add new includes)
├── Game.h                  (modified: add SceneGroup, Scheduler access)
├── Scene.h                 (modified: add PhysicsScene ownership, phase callbacks,
│                            AudioEvent queue, updateGameLogic/Audio/Visuals)
├── Scheduler.h             (NEW)
├── SceneGroup.h            (NEW)
├── AudioEvent.h            (NEW: AudioEvent struct, event types)
├── PhysicsScene.h          (NEW)
├── PhysicsEntity.h         (NEW)
├── PhysicsTypes.h          (NEW: PhysicsBodyDef, PhysicsShape, etc.)
└── ThreadPool.h            (NEW)

src/api/
├── Game.cpp                (modified: scheduler integration, audio.global task)
├── Scene.cpp               (modified: physics enable/disable, phase callbacks,
│                            audio queue drain in default updateAudio())
├── Scheduler.cpp           (NEW)
├── PhysicsScene.cpp        (NEW)
├── PhysicsEntity.cpp       (NEW)
└── ThreadPool.cpp          (NEW)

tests/
├── Scheduler_test.cpp      (NEW)
├── PhysicsScene_test.cpp   (NEW)
├── PhysicsEntity_test.cpp  (NEW)
├── AudioEvent_test.cpp     (NEW)
└── ThreadPool_test.cpp     (NEW)

examples/
├── physics_demo/           (NEW)
│   ├── CMakeLists.txt
│   └── main.cpp
└── audio_physics_demo/     (NEW: demonstrates collision→audio chain)
    ├── CMakeLists.txt
    └── main.cpp
```

---

## 12. Thread Safety Model

### Shared-Nothing During Physics

The critical design decision: **scenes share nothing during physics execution**. Each `PhysicsScene` is a self-contained island:

```
Thread 1 (Worker)           Thread 2 (Worker)           Main Thread
┌────────────────┐         ┌────────────────┐         ┌──────────────┐
│ Scene A physics│         │ Scene B physics│         │  (waiting)   │
│ - Read A bodies│         │ - Read B bodies│         │              │
│ - Write A state│         │ - Write B state│         │              │
│ No access to B │         │ No access to A │         │              │
└────────────────┘         └────────────────┘         └──────────────┘
        │                          │                         │
        └────────── barrier ───────┘                         │
                                                             │
                                                    ┌──────────────┐
                                                    │ PostPhysics: │
                                                    │ Sync A + B   │
                                                    │ Update A + B │
                                                    │ Render A + B │
                                                    └──────────────┘
```

### Inter-Scene Communication

If scenes need to communicate (e.g., a portal between overworld and dungeon), they do so through a message queue that is drained **between** physics steps on the main thread:

```cpp
// Future API concept (Phase 5+)
class SceneMessageBus {
public:
    void send(const std::string& targetScene, const Message& msg);
    std::vector<Message> drain(const std::string& sceneName);
};
```

---

## 13. Migration Guide

### Existing single-scene games

**Zero changes required.** The default scheduler graph for a single scene without physics is functionally identical to the current hard-coded loop. `setActiveScene("main")` internally creates a group of one.

### Adding physics to an existing scene

```cpp
// Before (visual only)
auto entity = addEntity<vde::SpriteEntity>(textureId);
entity->setPosition(0, 5, 0);

// After (with physics)
enablePhysics({.gravity = -9.81f});  // in onEnter()
auto entity = addEntity<vde::PhysicsSpriteEntity>();
entity->setTexture(textureId);
entity->createPhysicsBody({
    .type = vde::PhysicsBodyType::Dynamic,
    .shape = vde::PhysicsShape::Box,
    .position = {0, 5, 0},
    .halfExtents = {0.5f, 0.5f, 0.1f}
});
// Position is now driven by physics — no manual setPosition needed
```

### Activating multiple scenes

```cpp
// Before
game.setActiveScene("gameplay");

// After — multiple scenes active simultaneously
game.setActiveSceneGroup(vde::SceneGroup::create("all", {
    "gameplay", "hud"
}));
```

---

## 14. Open Questions

1. **Physics backend**: Start with custom minimal 2D physics, or immediately integrate an established library (Box2D for 2D, Jolt for 3D)?
   - *Recommendation*: Start custom for 2D (keeps dependency-free, matches VDE philosophy), add Jolt adapter later.

2. **Multi-viewport rendering**: This design handles multi-scene update and render, but all scenes render to the same full viewport. The viewport/split-screen system from `API_SUGGESTION_MULTI_SCENE.md` is orthogonal and can be layered on top.
   - *Recommendation*: Implement viewports as a separate feature that composes with scene groups.

3. **Physics determinism**: Should physics be exactly deterministic across platforms (requires fixed-point math)?
   - *Recommendation*: Not initially. IEEE 754 float with fixed timestep gives "good enough" determinism for single-player games.

4. **Entity ownership**: Should `PhysicsEntity` use multiple inheritance, composition, or a component pattern?
   - *Recommendation*: Inheritance (as designed above) for simplicity, matching the existing Entity hierarchy. A full ECS refactor could come later.

5. **Scheduler persistence**: Should the scheduler graph be rebuilt each frame or persist until scenes change?
   - *Recommendation*: Persist and only rebuild when scene group changes. The task `execute` lambdas capture scene pointers which remain stable.

6. **Audio latency vs. correctness**: The design defers audio cue submission to after game logic. This adds at most one frame of latency (~16ms at 60fps) compared to firing sounds immediately in an `update()` callback. Is this acceptable?
   - *Recommendation*: Yes. 16ms is below human audio perception threshold (~20-30ms for click-to-sound). The benefit of deterministic ordering outweighs the sub-perceptual delay. Power users who need lower latency can submit directly to `AudioManager` from `updateGameLogic()`.

7. **Per-scene audio listeners**: With multiple scenes active simultaneously (e.g., overworld + dungeon), which scene's camera drives the 3D audio listener position?
   - *Recommendation*: Designate one scene as the "primary audio scene" via `Scene::setPrimaryAudioListener(true)`. Default: the first scene in the group. Sounds from non-primary scenes still play but without 3D spatialization (2D fallback), or use a configurable attenuation factor per scene.

8. **Audio event queue thread safety**: If game logic ever moves to worker threads (Phase 4+), the audio event queue needs synchronization.
   - *Recommendation*: Use a simple `std::mutex` around `m_audioEventQueue`. The queue is append-only during game logic and drain-only during audio phase, so contention is minimal. Alternatively, use a lock-free SPSC queue since there's one producer (game logic) and one consumer (audio phase).

---

## 15. Summary

| Feature | Design Choice | Rationale |
|---------|--------------|-----------|
| Multi-scene | SceneGroup + Scheduler | Declarative, extensible, backwards compatible |
| Physics timestep | Fixed accumulator with interpolation | Deterministic, framerate-independent |
| Physics ownership | One PhysicsScene per Scene | Clean separation, no shared mutable state |
| Entity binding | PhysicsEntity inherits Entity | Matches existing hierarchy, simple API |
| Parallelism | ThreadPool + task dependencies | Opt-in, safe by construction (no shared state) |
| Scheduling | Task graph with phases | Flexible ordering, supports custom configurations |
| Game logic phase | Separate from audio and visual | Establishes clear data-flow: physics→logic→audio/visuals |
| Audio cues | Event queue drained in Audio phase | Deterministic ordering, thread-safe, debuggable |
| Audio global tick | Single task after all scene audio | One `AudioManager::update()` per frame, after all cues submitted |
| Update split | `updateGameLogic` / `updateAudio` / `updateVisuals` | Opt-in per scene; legacy `update()` still works unchanged |
| Default behavior | Identical to current single-scene loop | Zero migration cost for existing games |
