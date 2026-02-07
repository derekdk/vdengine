# API Suggestion: Multi-Scene Improvements

## Overview

While building the `multi_scene_demo` example, several API limitations were identified around multi-scene workflows. The current API supports basic scene switching (`setActiveScene`, `pushScene`/`popScene`) well, but more advanced patterns require workarounds.

---

## 1. No Background Scene Updates

### Problem

Only the active scene receives `update()` and `render()` calls. When a scene is exited (via `setActiveScene`) or paused (via `pushScene`), its logic completely stops. There is no way to have a scene's physics, animations, or AI continue running while the user views a different scene.

### Current Workaround

The `multi_scene_demo` records a timestamp on `onExit()`/`onPause()` and calculates the elapsed background time on `onEnter()`/`onResume()`. It then drains that accumulated time in capped increments (0.5s per frame) during `updateScene()` to simulate catch-up. This is imprecise — it can't replicate frame-by-frame physics — but it creates the illusion of continued simulation.

### Proposed Improvement

Add a `Scene::setContinueInBackground(bool)` flag recognized by the `Game` loop. When enabled, the scene's `update(deltaTime)` is called every frame even when it's not the active scene (but `render()` is NOT called).

```cpp
// Scene.h - new method
void setContinueInBackground(bool enabled);
bool getContinueInBackground() const;

// Game.cpp - in game loop, after active scene update:
for (auto& [name, scene] : m_scenes) {
    if (scene.get() != m_activeScene && scene->getContinueInBackground()) {
        scene->update(m_deltaTime);
    }
}
```

### Benefits
- Accurate physics simulation across scene switches
- Enables "living world" patterns where NPCs/environments evolve off-screen
- Simple opt-in per scene, no breaking changes

---

## 2. No Multi-Viewport / Split-Screen Rendering

### Problem

The engine renders one scene at a time using the full swapchain viewport. There is no way to render multiple scenes simultaneously in different regions of the screen (e.g., 4 scenes in a 2x2 grid). This prevents:
- Split-screen multiplayer
- Mini-map or picture-in-picture views
- Debug viewports showing different camera angles
- The "stretch goal" of this demo: four scenes visible at once

### Current Workaround

No practical workaround exists within the current API. You would need to render to offscreen framebuffers and composite them, which requires direct Vulkan access outside the API layer.

### Proposed Improvement

Add a `Viewport` concept that maps a scene to a screen region:

```cpp
// New class: Viewport
struct Viewport {
    float x, y;          // Top-left corner (0-1 normalized)
    float width, height; // Size (0-1 normalized)
    Scene* scene;        // Scene to render in this viewport
};

// Game.h - new methods
void addViewport(const std::string& name, const Viewport& viewport);
void removeViewport(const std::string& name);
void setViewportLayout(ViewportLayout layout); // e.g., SINGLE, SPLIT_2, QUAD

// Predefined layouts
enum class ViewportLayout {
    SINGLE,      // One scene, full screen
    SPLIT_H,     // Two scenes, left/right
    SPLIT_V,     // Two scenes, top/bottom
    QUAD         // Four scenes, 2x2 grid
};
```

### Implementation Notes
- Each viewport sets `vkCmdSetViewport`/`vkCmdSetScissor` before rendering its scene
- Camera aspect ratios must be updated to match viewport dimensions
- Only one render pass needed — viewports are drawn sequentially within the same frame
- Clear color handling needs consideration (may need per-viewport clear regions)

---

## 3. No Scene Transition Effects

### Problem

Scene transitions are instantaneous. There are no fade-in, fade-out, crossfade, or slide transitions. This can be jarring for users, especially when scenes have very different visual styles.

### Current Workaround

None practical. You could manually overlay a black sprite and animate its alpha, but this is cumbersome and doesn't work for crossfade transitions.

### Proposed Improvement

Add a transition system:

```cpp
// Scene transition types
enum class TransitionType {
    NONE,        // Instant switch (current behavior)
    FADE_BLACK,  // Fade to black, then fade in
    CROSSFADE,   // Blend old and new scenes
    SLIDE_LEFT,  // Slide new scene in from right
    SLIDE_RIGHT, // Slide new scene in from left
};

// Game.h - modified setActiveScene
void setActiveScene(const std::string& name,
                    TransitionType transition = TransitionType::NONE,
                    float duration = 0.5f);
```

### Implementation Notes
- Requires rendering two scenes simultaneously during transition (related to multi-viewport)
- Fade-to-black is simplest: render old scene with darkening overlay, then new scene with lightening overlay
- Crossfade requires offscreen render targets for both scenes

---

## 4. Scene `onExit`/`onEnter` Called Redundantly on Re-entry

### Problem

When using `setActiveScene()` to switch away from a scene and later switch back, `onExit()` and then `onEnter()` are called. For scenes with heavy setup in `onEnter()` (loading resources, creating entities), this means teardown and re-creation every time, which is wasteful.

### Current Workaround

Scenes can use an `m_initialized` flag to skip setup in subsequent `onEnter()` calls if entities are still alive. The `multi_scene_demo` relies on this pattern.

### Proposed Improvement

Add `onFirstEnter()` / `onReEnter()` lifecycle hooks, or a flag:

```cpp
class Scene {
public:
    virtual void onFirstEnter() {}  // Called only the first time
    virtual void onReEnter() {}     // Called on subsequent activations

    bool hasBeenEntered() const;    // Check if onEnter has been called before
};
```

Alternatively, ensure `setActiveScene()` does not destroy scene state — only deactivate/reactivate (similar to `onPause`/`onResume` but for direct switching).

---

## 5. No Per-Scene Input Handler Stack

### Problem

Input handling falls through from scene to game. When a HUD overlay is pushed, the underlying scene's input handler may also receive events. There's no clean way to block input propagation to paused scenes.

### Current Workaround

Scenes check `getInputHandler()` which falls back to the game's handler. The HUD scene and underlying scene both receive the same input events. The HUD must carefully avoid consuming game-control keys.

### Proposed Improvement

Add input blocking to pushed scenes:

```cpp
// Scene.h
void setBlocksInput(bool blocks); // When true, input doesn't reach scenes below in the stack
bool getBlocksInput() const;

// Game.cpp - input processing
// Only deliver input to top scene if it blocks, otherwise deliver to all active stack scenes
```

---

## Summary of Priorities

| Priority | Feature | Complexity | Impact |
|----------|---------|-----------|--------|
| **High** | Background scene updates | Low | Enables living worlds |
| **High** | Multi-viewport rendering | Medium-High | Enables split-screen, PiP |
| **Medium** | Scene transitions | Medium | Better UX |
| **Medium** | Scene re-entry optimization | Low | Performance |
| **Low** | Per-scene input blocking | Low | Cleaner input handling |

---

## Files

- **Example**: `examples/multi_scene_demo/main.cpp`
- **This document**: `API_SUGGESTION_MULTI_SCENE.md`
