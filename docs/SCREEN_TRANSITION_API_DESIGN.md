# Screen Transition API Design

## Overview

The Screen Transition system provides a mechanism for visually transitioning between scenes in VDE. During a transition, the source scene and destination scene are both rendered to offscreen textures, and a transition effect composites them into the final frame. Transitions support both full-screen and partial-screen (viewport-scoped) modes.

The system is designed around a base `Transition` class that custom transitions extend. Transitions are primarily shader-driven (a fullscreen-quad fragment shader that samples the source and destination textures), but the architecture does not preclude 3D-geometry-based effects in the future.

## Goals

- **Simple API** — Trigger a transition with one call: `game.transitionToScene("menu", transition, duration)`
- **Extensible** — New effects are created by subclassing `Transition` and providing a shader (or custom geometry)
- **Duration-based** — Every transition has a configurable duration; the engine drives `progress` from 0→1
- **Callback-driven** — Transitions receive an `update()` callback each frame so they can animate uniforms
- **Composable** — Transitions work with single-scene and multi-scene/split-screen setups
- **Non-intrusive** — Source and destination scenes are unaware of the transition; their `update()` / `render()` calls continue normally

## Architecture

```
Game
 └── TransitionManager (owns active transition state)
       ├── Offscreen render targets (source / destination)
       ├── Active Transition instance
       └── Fullscreen-quad pipeline (shader + vertex buffer)

Transition (base class)
 ├── FadeTransition
 ├── WipeTransition
 ├── CircleRevealTransition
 ├── ... (user-defined)
 └── GeometryTransition (future: 3D-geometry-based effects)
```

### Render Flow During a Transition

```
1. Render source scene → offscreen texture A
2. Render destination scene → offscreen texture B
3. Bind transition shader (or geometry)
4. Draw fullscreen quad sampling A + B with progress uniform
5. Present composited result
```

When no transition is active, the render path is unchanged from today's `renderSingleViewport` / `renderMultiViewport`.

## Public API

### Transition (base class)

**Header**: `<vde/api/Transition.h>`

```cpp
namespace vde {

/// Direction hint for transitions that are not symmetric.
enum class TransitionDirection : uint8_t {
    Left,
    Right,
    Up,
    Down,
    Center  // e.g. circle expand from center
};

/// Parameters passed to a transition each frame.
struct TransitionUpdateContext {
    float progress;       // 0.0 → 1.0 over the transition duration
    float deltaTime;      // Frame delta
    float elapsed;        // Wall-clock time since transition start
    float duration;       // Total transition duration (seconds)
    uint32_t frameWidth;  // Render target width in pixels
    uint32_t frameHeight; // Render target height in pixels
};

/// GPU-side uniform block written by the transition each frame.
/// The base class provides the standard fields; subclasses may
/// extend via push constants or additional descriptor sets.
struct TransitionUniforms {
    float progress  = 0.0f;
    float direction = 0.0f;   // encoded TransitionDirection
    float param0    = 0.0f;   // effect-specific
    float param1    = 0.0f;   // effect-specific
};

/**
 * @brief Base class for all screen transitions.
 *
 * Subclass and override `update()` to animate uniforms.
 * Override `getFragmentShaderPath()` to supply a custom
 * fragment shader that samples `sourceTexture` (binding 0)
 * and `destTexture` (binding 1) with a `progress` uniform.
 *
 * For 3D-geometry-based transitions, override
 * `usesCustomGeometry()` and `renderCustomGeometry()`.
 */
class Transition {
  public:
    virtual ~Transition() = default;

    // ---- Identity ----

    /// Human-readable name (for debug UI / logging).
    virtual const char* getName() const = 0;

    // ---- Shader path ----

    /// Path to the GLSL fragment shader for this transition.
    /// The shader receives:
    ///   layout(binding = 0) uniform sampler2D sourceTexture;
    ///   layout(binding = 1) uniform sampler2D destTexture;
    ///   layout(push_constant) TransitionUniforms uniforms;
    virtual std::string getFragmentShaderPath() const = 0;

    /// Vertex shader — the default fullscreen triangle is
    /// usually sufficient.  Override only for custom geometry.
    virtual std::string getVertexShaderPath() const;

    // ---- Per-frame callback ----

    /// Called every frame while the transition is active.
    /// Update `outUniforms` to drive the shader.
    /// The default implementation sets `outUniforms.progress = ctx.progress`.
    virtual void update(const TransitionUpdateContext& ctx,
                        TransitionUniforms& outUniforms);

    // ---- Lifecycle hooks ----

    /// Called once when the transition begins.
    virtual void onStart() {}

    /// Called once when the transition completes (progress == 1).
    virtual void onComplete() {}

    // ---- Geometry override (advanced) ----

    /// Return true if this transition renders its own geometry
    /// instead of using the default fullscreen quad.
    virtual bool usesCustomGeometry() const { return false; }

    /// Record custom draw commands.  `sourceTexture` and
    /// `destTexture` descriptor sets are already bound.
    virtual void renderCustomGeometry(VkCommandBuffer cmd,
                                      const TransitionUpdateContext& ctx) {
        (void)cmd;
        (void)ctx;
    }

    // ---- Configuration helpers ----

    void setDirection(TransitionDirection dir) { m_direction = dir; }
    TransitionDirection getDirection() const { return m_direction; }

  protected:
    TransitionDirection m_direction = TransitionDirection::Center;
};
```

### Built-in Transitions

```cpp
/// Cross-fade (alpha blend) between source and destination.
class FadeTransition : public Transition {
  public:
    const char* getName() const override { return "Fade"; }
    std::string getFragmentShaderPath() const override;
};

/// Linear wipe in the configured direction.
class WipeTransition : public Transition {
  public:
    explicit WipeTransition(TransitionDirection dir = TransitionDirection::Left);
    const char* getName() const override { return "Wipe"; }
    std::string getFragmentShaderPath() const override;
};

/// Expanding circle from the center revealing the destination.
class CircleRevealTransition : public Transition {
  public:
    const char* getName() const override { return "CircleReveal"; }
    std::string getFragmentShaderPath() const override;
    void update(const TransitionUpdateContext& ctx,
                TransitionUniforms& outUniforms) override;
};
```

### TransitionManager (internal engine class)

**Header**: `<vde/api/TransitionManager.h>`

```cpp
namespace vde {

/**
 * @brief Manages the lifecycle and rendering of screen transitions.
 *
 * This class is internal to the engine and is owned by Game.
 * Users interact with it indirectly through Game's public API.
 */
class TransitionManager {
  public:
    explicit TransitionManager(VulkanContext* context);
    ~TransitionManager();

    // Non-copyable, non-movable
    TransitionManager(const TransitionManager&) = delete;
    TransitionManager& operator=(const TransitionManager&) = delete;

    /// Begin a transition.
    /// @param transition    The transition effect (TransitionManager takes ownership)
    /// @param duration      Duration in seconds
    /// @param onComplete    Callback invoked when the transition finishes
    void start(std::unique_ptr<Transition> transition,
               float duration,
               std::function<void()> onComplete = nullptr);

    /// Drive the transition forward by deltaTime.
    void update(float deltaTime);

    /// Returns true while a transition is in progress.
    bool isActive() const;

    /// Cancel the current transition immediately.
    void cancel();

    /// Get progress [0, 1] of the current transition.
    float getProgress() const;

    // ---- Render-target management ----

    /// Create / recreate offscreen render targets to match swapchain size.
    void recreateRenderTargets(uint32_t width, uint32_t height);

    /// Get the source offscreen render target (scene being transitioned FROM).
    VkFramebuffer getSourceFramebuffer() const;

    /// Get the destination offscreen render target (scene being transitioned TO).
    VkFramebuffer getDestFramebuffer() const;

    /// Render the composited transition frame into the command buffer.
    /// Binds source + dest textures, sets uniforms, and draws the
    /// fullscreen quad (or delegates to custom geometry).
    void renderComposite(VkCommandBuffer cmd);

  private:
    struct OffscreenTarget {
        VkImage image = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkImageView imageView = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkSampler sampler = VK_NULL_HANDLE;
        VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
        uint32_t width = 0;
        uint32_t height = 0;
    };

    void createOffscreenTarget(OffscreenTarget& target, uint32_t w, uint32_t h);
    void destroyOffscreenTarget(OffscreenTarget& target);
    void createFullscreenQuadPipeline();
    void destroyPipeline();

    VulkanContext* m_context = nullptr;

    // Offscreen targets
    OffscreenTarget m_source;
    OffscreenTarget m_dest;

    // Transition state
    std::unique_ptr<Transition> m_activeTransition;
    float m_duration = 0.0f;
    float m_elapsed = 0.0f;
    std::function<void()> m_onComplete;

    // Vulkan resources for the fullscreen quad pipeline
    VkPipeline m_pipeline = VK_NULL_HANDLE;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;
    VkRenderPass m_renderPass = VK_NULL_HANDLE;
};

} // namespace vde
```

### Game API Extensions

New methods on `vde::Game`:

```cpp
class Game {
  public:
    // ... existing API ...

    // ---- Transitions ----

    /**
     * @brief Transition to a scene with a visual effect.
     *
     * The source scene is the currently active scene. The destination
     * scene must already be registered with addScene().
     *
     * @param sceneName   Name of the destination scene
     * @param transition  Transition effect (Game takes ownership)
     * @param duration    Duration of the transition in seconds
     */
    void transitionToScene(const std::string& sceneName,
                           std::unique_ptr<Transition> transition,
                           float duration);

    /**
     * @brief Transition to a scene group with a visual effect.
     *
     * @param group       Destination scene group
     * @param transition  Transition effect (Game takes ownership)
     * @param duration    Duration in seconds
     */
    void transitionToSceneGroup(const SceneGroup& group,
                                std::unique_ptr<Transition> transition,
                                float duration);

    /**
     * @brief Check if a transition is currently in progress.
     */
    bool isTransitioning() const;

    /**
     * @brief Cancel the current transition immediately.
     *
     * The destination scene becomes active without completing the effect.
     */
    void cancelTransition();

    /**
     * @brief Get progress of the active transition [0, 1].
     * @return 0 if no transition is active.
     */
    float getTransitionProgress() const;
};
```

## Shader Contract

All transition fragment shaders receive:

```glsl
#version 450

// Inputs from fullscreen-triangle vertex shader
layout(location = 0) in vec2 fragUV;

// Scene textures
layout(binding = 0) uniform sampler2D sourceTexture;
layout(binding = 1) uniform sampler2D destTexture;

// Transition uniforms
layout(push_constant) uniform TransitionPC {
    float progress;   // 0.0 to 1.0
    float direction;  // encoded TransitionDirection
    float param0;     // effect-specific
    float param1;     // effect-specific
} transition;

layout(location = 0) out vec4 outColor;
```

### Example: Fade Shader (`transition_fade.frag`)

```glsl
void main() {
    vec4 src = texture(sourceTexture, fragUV);
    vec4 dst = texture(destTexture, fragUV);
    outColor = mix(src, dst, transition.progress);
}
```

### Example: Wipe Shader (`transition_wipe.frag`)

```glsl
void main() {
    vec4 src = texture(sourceTexture, fragUV);
    vec4 dst = texture(destTexture, fragUV);

    // direction: 0=Left, 1=Right, 2=Up, 3=Down
    float edge;
    if (transition.direction < 0.5)
        edge = fragUV.x;           // Left wipe
    else if (transition.direction < 1.5)
        edge = 1.0 - fragUV.x;    // Right wipe
    else if (transition.direction < 2.5)
        edge = 1.0 - fragUV.y;    // Up wipe
    else
        edge = fragUV.y;           // Down wipe

    float t = step(edge, transition.progress);
    outColor = mix(src, dst, t);
}
```

### Example: Circle Reveal Shader (`transition_circle_reveal.frag`)

```glsl
void main() {
    vec4 src = texture(sourceTexture, fragUV);
    vec4 dst = texture(destTexture, fragUV);

    // Distance from center, normalized so corners = 1.0
    vec2 center = vec2(0.5);
    float maxDist = length(center); // ~0.707
    float dist = length(fragUV - center) / maxDist;

    // Expand circle from center
    float radius = transition.progress;
    float t = smoothstep(radius - 0.02, radius + 0.02, dist);
    outColor = mix(dst, src, t);
}
```

## Fullscreen Vertex Shader

A single vertex shader is shared by all shader-based transitions. It generates a fullscreen triangle without vertex buffers:

```glsl
// transition_fullscreen.vert
#version 450

layout(location = 0) out vec2 fragUV;

void main() {
    // Fullscreen triangle trick: 3 vertices, no vertex buffer
    fragUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    gl_Position = vec4(fragUV * 2.0 - 1.0, 0.0, 1.0);
    fragUV.y = 1.0 - fragUV.y; // Flip Y for Vulkan
}
```

## Partial-Screen (Viewport-Scoped) Transitions

For multi-scene/split-screen setups, transitions can be scoped to a single viewport. The `transitionToScene` method accepts an optional `ViewportRect`:

```cpp
/// Transition a specific viewport region.
void transitionToScene(const std::string& sceneName,
                       std::unique_ptr<Transition> transition,
                       float duration,
                       const ViewportRect& region);
```

The `TransitionManager` renders the composited effect only within the specified scissor region. Source/destination textures still capture the full framebuffer, but the fragment shader only executes within the viewport scissor.

## Scene Lifecycle During Transitions

| Phase      | Source Scene              | Destination Scene          |
|------------|---------------------------|----------------------------|
| Start      | `update()` continues      | `onEnter()` called         |
| In-progress| `update()` + `render()`   | `update()` + `render()`    |
| Complete   | `onExit()` called         | Becomes active sole scene  |

Both scenes are alive and updating during the transition so that gameplay, animations, and physics remain smooth.

## Configuration via TransitionConfig (Optional)

For transitions that need additional parameters beyond direction:

```cpp
struct TransitionConfig {
    TransitionDirection direction = TransitionDirection::Center;
    float edgeSoftness = 0.02f;      // Anti-aliased edge width
    Color borderColor = Color::black(); // Optional border/edge color
    float borderWidth = 0.0f;         // Border width (0 = none)
    // Effect-specific parameters
    float param0 = 0.0f;
    float param1 = 0.0f;
};
```

## 3D Geometry Transitions (Future)

The `usesCustomGeometry()` / `renderCustomGeometry()` virtual methods allow transitions that use 3D geometry instead of a fullscreen quad. Example use cases:

- **Page turn** — A subdivided quad deformed with a curl shader
- **Cube rotation** — Source and destination mapped onto cube faces
- **Shatter** — Source image broken into triangles that fall away

These transitions would:
1. Override `usesCustomGeometry()` → `true`
2. Override `renderCustomGeometry(cmd, ctx)` to record their own draw calls
3. Potentially provide their own vertex + fragment shaders via `getVertexShaderPath()`
4. Manage their own mesh data (vertex/index buffers) as members

## Easing Functions

`TransitionUpdateContext::progress` is linear by default (0→1 over the duration). Transitions can apply easing internally:

```cpp
void update(const TransitionUpdateContext& ctx,
            TransitionUniforms& outUniforms) override {
    // Apply ease-in-out
    float t = ctx.progress;
    float eased = t < 0.5f ? 2.0f * t * t : 1.0f - pow(-2.0f * t + 2.0f, 2.0f) / 2.0f;
    outUniforms.progress = eased;
}
```

An `EasingFunction` type alias and common presets could be added to the API:

```cpp
using EasingFunction = std::function<float(float)>;

namespace Easing {
    EasingFunction linear();
    EasingFunction easeInQuad();
    EasingFunction easeOutQuad();
    EasingFunction easeInOutQuad();
    EasingFunction easeInOutCubic();
}
```

## Scheduler Integration

VDE's task-graph scheduler runs tasks in topologically sorted order with `TaskPhase` as a tiebreaker. The full per-frame chain is:

```
Input            input.script
GameLogic        game.update → scene.update / scene.gameLogic+visuals
Audio            scene.audio (per-scene) → audio.global
Physics          scene.physics (per-scene)
PostPhysics      scene.postPhysics (per-scene)
PreRender        scene.preRender          (applies clear color)
Render           scene.render             (records + submits command buffer)
```

### Transition tasks added to the graph

Two changes are made when a transition is active:

| New task | Phase | Depends on | What it does |
|----------|-------|------------|--------------|
| `transition.update` | `PreRender` | `scene.preRender` | Calls `TransitionManager::update(dt)` — advances elapsed time, computes `progress`, writes `TransitionUniforms` |
| — | — | — | Compositing is **not** a separate task |

The `scene.render` task is **modified** to depend on `transition.update` when a transition is active. Compositing is folded into the existing `scene.render` task (inside `renderSingleViewport()` / `renderMultiViewport()`) so that all three render passes — source scene, destination scene, composite — are recorded into the **same Vulkan command buffer** and submitted in a single `vkQueueSubmit`:

```
PreRender   scene.preRender
               └─> transition.update   (new; only when transition active)
Render                 └─> scene.render
                               │  ① vkCmdBeginRenderPass(offscreenA) → source scene → vkCmdEndRenderPass
                               │  ② vkCmdBeginRenderPass(offscreenB) → dest scene   → vkCmdEndRenderPass
                               │  ③ vkCmdBeginRenderPass(swapchain)  → composite     → vkCmdEndRenderPass
                               └─ vkQueueSubmit (one submission)
```

When no transition is active the scheduler graph and command buffer structure are identical to today: `scene.render` depends on `scene.preRender` and records a single render pass.

All transition tasks are main-thread-only (`mainThreadOnly = true`) because they touch Vulkan state.

## Thread Safety

- `TransitionManager::update()` is called from the `PreRender`-phase task `transition.update` (main thread only)
- `TransitionManager::renderComposite()` is called from inside the `Render`-phase task `scene.render` — it is not a separate task
- No concurrent access to transition state from worker threads
- Scene `update()` calls continue on their normal scheduler threads; the transition system only reads the rendered output

## Error Handling

| Condition | Behavior |
|-----------|----------|
| `transitionToScene` with unknown scene name | `std::runtime_error` thrown |
| `transitionToScene` while already transitioning | Previous transition cancelled, new one starts |
| Transition duration ≤ 0 | Immediate scene switch (no visual effect) |
| Swapchain resize during transition | `TransitionManager::recreateRenderTargets()` called; transition continues |
| Transition shader fails to compile | Falls back to instant cut; error logged |

## Summary

The Screen Transition system adds a visually polished scene-switching mechanism to VDE with minimal impact on the existing rendering pipeline. The key design decisions are:

1. **Offscreen render targets** — Both scenes render to textures, keeping the transition compositing clean and independent of scene internals
2. **Base class + shader** — Most transitions are just a fragment shader; the `Transition` base class handles boilerplate
3. **Engine-driven progress** — `TransitionManager` drives the 0→1 progress; transitions just map it to uniforms
4. **Geometry escape hatch** — `usesCustomGeometry()` supports future 3D effects without redesigning the system
5. **Scheduler integration** — Transition update/render slots fit naturally into the existing task-graph scheduler
