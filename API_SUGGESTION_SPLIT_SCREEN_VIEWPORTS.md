# API Suggestion: Split-Screen Viewports & Per-Scene Rendering

## Problem

The engine's multi-scene system (`SceneGroup`, Phase 2) supports updating multiple scenes simultaneously, but **all scenes render through a single full-window viewport using only the primary scene's camera**. This makes true split-screen rendering (4 independent 3D views) impossible through the high-level API.

Attempting to create a demo with 4 simultaneous 3D scenes — each with its own camera, its own perspective, and independent input focus — reveals several interconnected gaps.

## Identified Deficiencies

### 1. No Per-Scene Viewport/Scissor Rect

**Current behavior:** `VulkanContext::recordCommandBuffer()` sets a single `VkViewport` and `VkRect2D` scissor covering the entire swapchain extent before invoking the render callback. All scenes in a `SceneGroup` render to this same full-screen area.

**Impact:** There is no way to confine a scene's rendering to a sub-region of the window (e.g., top-left quadrant). The only workaround is to place all content in a single scene and manually position entities — which eliminates the benefit of separate scenes with independent cameras and coordinate systems.

**Proposed API:**
```cpp
// On Scene:
void setViewportRect(float x, float y, float width, float height);
// Normalized [0,1] coordinates, or pixel coordinates with enum selector.

// Or on SceneGroup:
struct SceneGroupEntry {
    std::string sceneName;
    ViewportRect viewport;  // {x, y, width, height} in normalized coords
};
```

### 2. No Per-Scene Camera Application During Multi-Scene Render

**Current behavior:** The `scene.preRender` scheduler task applies only the **primary** scene's camera to the `VulkanContext`. When multiple scenes are rendered in the render callback loop, all use the same view/projection matrices uploaded in the uniform buffer.

**Impact:** Even if per-scene viewports were added, each scene's entities would still be rendered with the wrong camera transform. Each scene needs its camera applied (UBO updated) immediately before its `render()` call.

**Proposed API:**
```cpp
// In Game::rebuildSchedulerGraph() render task, for each scene:
//   1. Apply scene's camera to VulkanContext
//   2. Set scene's viewport/scissor on the command buffer
//   3. Call scene->render()
//   4. Restore full viewport if needed
```

### 3. No Render Callback Access to Command Buffer in Scene

**Current behavior:** `Scene::render()` takes no parameters. It iterates entities and calls `entity->render()`. Entity render implementations access the `VulkanContext` command buffer indirectly through `Game` accessors. There is no mechanism for a scene to inject `vkCmdSetViewport` / `vkCmdSetScissor` calls before its entities draw.

**Impact:** Even at the advanced-user level, there is no hook to change the viewport between scenes within a single render pass.

**Proposed API:**
```cpp
// Option A: Scene receives command buffer
virtual void render(VkCommandBuffer cmd);

// Option B: Scene has a pre-render hook
virtual void onPreRender(VkCommandBuffer cmd);  // called before entity rendering

// Option C: Game internally handles viewport per scene (no user-facing change)
```

### 4. No Input Focus / Routing Per Viewport

**Current behavior:** A single `InputHandler` is set on the `Game`. While `Scene` has `setInputHandler()`, all keyboard and mouse events go to the global handler. There is no concept of "which viewport is the mouse over" or "which viewport has focus."

**Impact:** In a split-screen scenario, pressing WASD should only affect the focused viewport's camera. Mouse clicks should be routed to the viewport under the cursor. Currently, all input goes everywhere.

**Proposed API:**
```cpp
// Focus tracking on Game:
void setFocusedScene(const std::string& sceneName);
Scene* getFocusedScene();

// Mouse-to-viewport mapping:
Scene* getSceneAtScreenPosition(double mouseX, double mouseY);

// Per-scene input handler (already exists but needs focus gating):
// Input dispatch: only the focused scene's handler receives events,
// OR all handlers receive events with a flag indicating focus state.
```

### 5. No Visual Viewport Decorations (Borders, Focus Indicators)

**Current behavior:** The rendering pipeline provides no mechanism to draw viewport borders, focus indicators, or overlays between scene renders.

**Impact:** Users cannot visually indicate which viewport is active. The only workaround is to draw border sprites as entities within a scene, which doesn't work correctly with 3D cameras (borders would be in world space, not screen space).

**Proposed API:**
```cpp
// Option A: Built-in viewport decoration support
struct ViewportStyle {
    Color borderColor = Color::transparent();
    float borderWidth = 0.0f;  // pixels
};
scene->setViewportStyle(style);

// Option B: Post-render overlay callback on Game
virtual void onPostSceneRender(VkCommandBuffer cmd, const std::string& sceneName);
```

### 6. Per-Scene Clear Color Requires Per-Scene Render Target

**Current behavior:** The clear color is set once at the start of the render pass (`VkRenderPassBeginInfo`). Subsequent scenes in the group cannot set a different clear color — they render on top of the primary scene's cleared framebuffer.

**Impact:** In split-screen mode, each quadrant should have its own background color. With a single render pass begin, only the primary scene's clear color takes effect.

**Proposed Options:**
- Use multiple render passes (one per viewport) — significant architectural change
- Draw a fullscreen quad per viewport before scene entities — simpler but wasteful
- Accept that secondary scenes render atop the primary's background — current workaround

## Current Workaround

Without the above features, split-screen is simulated using the approach from `quad_viewport_demo`:

1. **Single scene** containing all entities from all "virtual viewports"
2. **2D camera** covering the full window, with entities manually positioned in quadrants
3. **Sprite entities** used as colored backgrounds and borders per quadrant
4. **Input routing** simulated by tracking which quadrant is "selected" in user code

This workaround has major limitations:
- **No independent 3D cameras** — all entities share one camera, so true 3D perspective per quadrant is impossible
- **No independent coordinate systems** — entities must be positioned in absolute screen-space coordinates
- **Manual layout math** — quadrant positions and sizes are hardcoded constants
- **No depth buffer separation** — entities in different quadrants can z-fight

## Priority Assessment

| Deficiency | Priority | Reason |
|-----------|----------|--------|
| Per-scene viewport rect | **High** | Fundamental for split-screen; Vulkan already supports dynamic viewports |
| Per-scene camera in render | **High** | Mandatory companion to viewport support |
| Command buffer access in Scene | **Medium** | Needed for viewport changes; could be handled internally by Game |
| Input focus/routing | **Medium** | Essential for interactive split-screen; can be approximated in user code |
| Visual viewport decorations | **Low** | Nice to have; can be approximated with sprites |
| Per-scene clear color | **Low** | Can be worked around with background quads |

## Implementation Sketch

The minimal change for split-screen support would be:

1. Add `ViewportRect` to `Scene` (normalized 0-1 coordinates)
2. In `Game`'s render task, for each scene in the group:
   - Call `vkCmdSetViewport` / `vkCmdSetScissor` using the scene's viewport rect
   - Apply the scene's camera to the UBO
   - Call `scene->render()`
3. Add `setFocusedScene()` to `Game` for input routing
4. Draw border quads after each scene's render for focus indication

This approach requires no new render passes and works within the existing single-pass architecture.
