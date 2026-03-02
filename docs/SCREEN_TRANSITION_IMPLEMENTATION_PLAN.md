# Screen Transition Implementation Plan

This document describes the step-by-step plan for adding the Screen Transition system to VDE. Each phase is designed to produce a buildable, testable increment.

Reference: [SCREEN_TRANSITION_API_DESIGN.md](SCREEN_TRANSITION_API_DESIGN.md)

---

## Phase 1: Offscreen Render Targets ✅ COMPLETE

**Goal:** Render a scene to an offscreen texture and sample it in a later pass.

### Tasks

1. **Create `OffscreenRenderTarget` utility class**
   - File: `include/vde/OffscreenRenderTarget.h`, `src/OffscreenRenderTarget.cpp`
   - Creates a VkImage + VkImageView + VkFramebuffer + VkSampler at a given resolution
   - Uses the same color format as the swapchain and includes a depth attachment
   - Supports `recreate(width, height)` for swapchain resize
   - RAII cleanup in destructor
   - Register in root `CMakeLists.txt` (`VDE_PUBLIC_HEADERS` + `VDE_SOURCES`)

2. **Add offscreen render pass to `VulkanContext`**
   - Create a dedicated `VkRenderPass` (`m_offscreenRenderPass`) that renders to `VK_IMAGE_LAYOUT_SHADER_READ_OPTIMAL` (color) + depth
   - This render pass clears on load and stores the result
   - Expose via `VulkanContext::getOffscreenRenderPass()`

3. **Write unit tests**
   - File: `tests/OffscreenRenderTarget_test.cpp`
   - Verify creation, recreation at different sizes, and cleanup without leaking Vulkan handles
   - Register in `tests/CMakeLists.txt`

4. **Build and verify** — `scripts: build` + `scripts: test`

### Acceptance Criteria
- `OffscreenRenderTarget` can be created and destroyed without validation errors
- Render pass exists and is compatible with the offscreen target's attachments

---

## Phase 2: Transition Base Class and Built-in Effects ✅ COMPLETE

**Goal:** Define the `Transition` class hierarchy and write the transition shaders.

### Tasks

1. **Create `Transition` base class**
   - File: `include/vde/api/Transition.h`
   - Define `Transition`, `TransitionDirection`, `TransitionUpdateContext`, `TransitionUniforms` as specified in the design doc
   - Pure virtual: `getName()`, `getFragmentShaderPath()`
   - Virtual with defaults: `update()`, `onStart()`, `onComplete()`, `getVertexShaderPath()`, `usesCustomGeometry()`, `renderCustomGeometry()`

2. **Create built-in transition subclasses**
   - File: `include/vde/api/Transition.h` (same header, inline definitions) or separate files if they grow
   - `FadeTransition` — cross-fade
   - `WipeTransition` — directional wipe (accepts `TransitionDirection`)
   - `CircleRevealTransition` — expanding circle from center
   - Implementation file: `src/api/Transition.cpp`

3. **Write transition shaders**
   - `shaders/transition_fullscreen.vert` — fullscreen triangle, no vertex buffer
   - `shaders/transition_fade.frag` — `mix(src, dst, progress)`
   - `shaders/transition_wipe.frag` — directional edge wipe
   - `shaders/transition_circle_reveal.frag` — circle distance function

4. **Register files in CMake**
   - Add header + source to root `CMakeLists.txt`
   - Add shaders to the shader compilation list (if one exists) or ensure `ShaderCache` can find them

5. **Write unit tests**
   - File: `tests/Transition_test.cpp`
   - Test base class defaults, subclass `getName()` / `getFragmentShaderPath()`, direction setters
   - Test `update()` produces expected uniform values for known progress inputs
   - Register in `tests/CMakeLists.txt`

6. **Build and verify**

### Acceptance Criteria
- All three built-in transitions instantiate and return valid shader paths
- `update()` correctly maps progress to uniforms for each transition type
- Shaders compile without errors through `ShaderCache`

---

## Phase 3: TransitionManager (Engine Internal) ✅ COMPLETE

**Goal:** Wire up the offscreen render targets, transition lifecycle, and compositing pipeline.

### Tasks

1. **Create `TransitionManager`**
   - File: `include/vde/api/TransitionManager.h`, `src/api/TransitionManager.cpp`
   - Owns two `OffscreenRenderTarget` instances (source, destination)
   - Manages active `Transition` instance, elapsed time, completion callback
   - Public API: `start()`, `update()`, `isActive()`, `cancel()`, `getProgress()`, `recreateRenderTargets()`, `renderComposite()`

2. **Create fullscreen-quad graphics pipeline**
   - Descriptor set layout: binding 0 = source sampler, binding 1 = dest sampler
   - Pipeline layout with push constants (TransitionUniforms)
   - Pipeline created from `transition_fullscreen.vert` + the active transition's fragment shader
   - Cache compiled pipelines keyed by fragment shader path to avoid per-transition rebuilds

3. **Implement `renderComposite()`**
   - Bind the fullscreen-quad pipeline
   - Bind source + destination texture descriptor sets
   - Push `TransitionUniforms`
   - Draw 3 vertices (fullscreen triangle)
   - If `usesCustomGeometry()`, call `renderCustomGeometry()` instead

4. **Handle swapchain resize**
   - `TransitionManager::recreateRenderTargets()` called from `Game` when the window resizes

5. **Write unit tests**
   - File: `tests/TransitionManager_test.cpp`
   - Test lifecycle: start → update → progress → complete → callback fired
   - Test cancel mid-transition
   - Test start-while-active replaces previous transition
   - Test duration ≤ 0 completes immediately
   - Register in `tests/CMakeLists.txt`

6. **Register files in CMake, build and verify**

### Acceptance Criteria
- `TransitionManager` drives progress 0→1 over the specified duration
- Completion callback fires when progress reaches 1
- Offscreen render targets are created at swapchain resolution

---

## Phase 4: Game Integration ✅ COMPLETE

**Goal:** Expose the transition API on `Game` and hook it into the scheduler and render loop.

### Tasks

1. **Add `TransitionManager` member to `Game`**
   - Constructed after `VulkanContext` initialization
   - Destroyed before `VulkanContext` cleanup

2. **Add public transition methods to `Game`**
   - `transitionToScene(name, transition, duration)`
   - `transitionToSceneGroup(group, transition, duration)`
   - `isTransitioning()`
   - `cancelTransition()`
   - `getTransitionProgress()`

3. **Implement `transitionToScene` logic**
   - Validate destination scene exists
   - If duration ≤ 0 → instant `setActiveScene()`, return
   - Store pending destination scene name
   - Call `onEnter()` on destination scene
   - Call `TransitionManager::start()`
   - On completion callback: `setActiveScene()` to destination, call `onExit()` on source

4. **Extend `rebuildSchedulerGraph()` for transition frames**

   The existing per-frame task chain ends with:
   ```
   scene.preRender (PreRender) → scene.render (Render)
   ```
   When `isTransitioning()` is true, the graph becomes:
   ```
   scene.preRender (PreRender)
       └─> transition.update (PreRender)   ← NEW task
               └─> scene.render (Render)   ← now depends on transition.update
   ```

   - Add task `transition.update` at `TaskPhase::PreRender`, depending on `scene.preRender`
     - Calls `m_transitionManager->update(m_deltaTime)` — advances elapsed time, computes `progress`, writes `TransitionUniforms`
   - Make `scene.render` depend on `transition.update` instead of `scene.preRender` while transitioning
   - Both the source and destination scenes are added to the update scene list for this frame so they both receive `update()` calls
   - **Compositing is not a separate scheduler task** — it is folded into `scene.render` (see step 5)

5. **Modify `renderSingleViewport` / `renderMultiViewport`**
   - When `isTransitioning()`, redirect both scene render calls to their respective offscreen targets instead of the swapchain framebuffer
   - After both scenes have rendered, call `TransitionManager::renderComposite()` as a **third render pass in the same command buffer**, writing the final composite to the swapchain framebuffer
   - All three render passes are recorded and submitted in a single `vkQueueSubmit` call, preserving the existing frame-pacing and semaphore behavior:
     ```
     vkCmdBeginRenderPass(offscreenA) → source scene → vkCmdEndRenderPass
     vkCmdBeginRenderPass(offscreenB) → dest scene   → vkCmdEndRenderPass
     vkCmdBeginRenderPass(swapchain)  → composite    → vkCmdEndRenderPass
     vkQueueSubmit
     ```
   - When not transitioning, the code path is identical to today (single render pass, swapchain framebuffer)

6. **Handle resize during transition**
   - In the existing resize callback, call `m_transitionManager->recreateRenderTargets(w, h)`

7. **Update `include/vde/api/GameAPI.h`**
   - Include `Transition.h` so users get it automatically

8. **Register files in CMake, build and verify**

### Acceptance Criteria
- `game.transitionToScene("menu", std::make_unique<FadeTransition>(), 1.0f)` produces a 1-second cross-fade
- Both scenes update during the transition
- `onEnter()` / `onExit()` lifecycle is correct
- Instant scene switch still works when duration ≤ 0

---

## Phase 5: Partial-Screen (Viewport) Transitions ✅ COMPLETE

**Goal:** Support transitions scoped to a single viewport in multi-scene layouts.

### Tasks

1. **Add `ViewportRect` overload to `transitionToScene`**
   - `transitionToScene(name, transition, duration, ViewportRect region)`
   - `TransitionManager` stores the viewport region and applies scissor during compositing

2. **Render only the viewport region to offscreen targets**
   - Source/dest offscreen targets can be full-resolution, but the compositing pass uses the viewport scissor

3. **Test with split-screen example**
   - Transition one viewport while the other remains static

4. **Build and verify**

### Acceptance Criteria
- Viewport-scoped transition only affects the specified region
- Other viewports render normally during the transition

---

## Phase 6: Transition Demo ✅ COMPLETE

**Goal:** Build and validate a runnable demo that exercises every built-in transition and confirms end-to-end functionality.

### Tasks

1. **Create `transition_demo` example**
   - File: `examples/transition_demo/main.cpp`
   - Three scenes with distinct visual content:
     - `MainMenuScene` — title text + solid background color
     - `GameScene` — several moving entities
     - `CreditsScene` — static text layout
   - On-screen HUD (ImGui or simple text rendering) showing:
     - Currently active transition name and current progress value
     - Keybinding cheat sheet
   - Keyboard controls to trigger each built-in transition:
     - `1` → Fade to GameScene
     - `2` → Wipe Left to CreditsScene
     - `3` → Wipe Right back to MainMenuScene
     - `4` → Circle Reveal to GameScene
     - `+` / `-` — increase / decrease transition duration (0.25 s steps, min 0.25 s)
     - `C` — cancel in-flight transition (instant cut)
   - Confirm `onEnter()` / `onExit()` fire correctly by logging to console
   - Add to `examples/CMakeLists.txt`

2. **Add smoke test script**
   - File: `scripts/input/transition_demo_smoke.vdescript`
   - Launch app, wait 1 s, press `1` to trigger fade, wait for completion, press `4` for circle reveal, wait, exit
   - Ensures the demo launches and completes transitions without crashing

3. **Manual test checklist** (record results in a PR comment)
   - [ ] Fade renders a smooth blend with no tearing
   - [ ] Wipe Left and Wipe Right travel in opposite directions
   - [ ] Circle Reveal expands from center outward
   - [ ] Duration changes via `+` / `-` take effect on the next transition
   - [ ] Cancelling mid-transition with `C` snaps to the source scene cleanly
   - [ ] Window resize during transition does not crash or corrupt render targets
   - [ ] Instant switch (duration = 0) works when duration is wound down to zero

4. **Build and smoke-test**

### Acceptance Criteria
- `transition_demo` compiles and launches without Vulkan validation errors
- All four transitions are visually correct
- Smoke test passes in CI
- Manual checklist completed and all items pass

---

## Phase 7: Documentation ✅ COMPLETE

**Goal:** Update all project documentation to reflect the new transition API.

### Tasks

1. **Update `API-DOC.md`**
   - Add Transitions section with usage examples
   - Document `Transition` base class, all built-in transitions, and `Game` methods

2. **Update `docs/API.md`**
   - Add `Transition`, `TransitionManager`, and easing functions to the reference

3. **Update using-api skill**
   - Add transition workflow to `.github/skills/using-api/SKILL.md`

4. **Update `writing-examples` skill**
   - Note that new examples should test transitions where scene switching is relevant

### Acceptance Criteria
- All public API symbols introduced in Phases 1–6 are documented
- Code examples in docs compile against the current headers

---

## Phase 8: Polish & Extensions (Optional / Future)

**Goal:** Quality-of-life improvements and advanced features.

### Tasks

1. **Easing functions**
   - Add `include/vde/api/Easing.h` with common presets (linear, quadratic, cubic, elastic, bounce)
   - Convenience overload: `transitionToScene(name, transition, duration, EasingFunction)`

2. **Transition chaining**
   - Queue multiple transitions: `game.queueTransition(scene1, fade, 0.5f); game.queueTransition(scene2, wipe, 0.5f);`

3. **Built-in edge effects**
   - Configurable border color and softness on wipe/circle transitions

4. **3D geometry transition example**
   - Page-turn or cube-rotate transition using `usesCustomGeometry()`

5. **ImGui debug overlay**
   - Show active transition name, progress bar, render target previews

---

## File Summary

| File | Phase | Type |
|------|-------|------|
| `include/vde/OffscreenRenderTarget.h` | 1 | Public header |
| `src/OffscreenRenderTarget.cpp` | 1 | Implementation |
| `tests/OffscreenRenderTarget_test.cpp` | 1 | Unit test |
| `include/vde/api/Transition.h` | 2 | Public header |
| `src/api/Transition.cpp` | 2 | Implementation |
| `shaders/transition_fullscreen.vert` | 2 | Shader |
| `shaders/transition_fade.frag` | 2 | Shader |
| `shaders/transition_wipe.frag` | 2 | Shader |
| `shaders/transition_circle_reveal.frag` | 2 | Shader |
| `tests/Transition_test.cpp` | 2 | Unit test |
| `include/vde/api/TransitionManager.h` | 3 | Internal header |
| `src/api/TransitionManager.cpp` | 3 | Implementation |
| `tests/TransitionManager_test.cpp` | 3 | Unit test |
| `include/vde/api/Game.h` | 4 | Modified |
| `src/api/Game.cpp` | 4 | Modified |
| `include/vde/api/GameAPI.h` | 4 | Modified |
| `examples/transition_demo/main.cpp` | 6 | Example |
| `examples/CMakeLists.txt` | 6 | Modified |
| `scripts/input/transition_demo_smoke.vdescript` | 6 | Smoke test |
| `API-DOC.md` | 7 | Modified |
| `docs/API.md` | 7 | Modified |
| `include/vde/api/Easing.h` | 8 | Public header (optional) |

---

## Dependency Graph

```
Phase 1 (Offscreen Render Targets)
   │
   ▼
Phase 2 (Transition Base Class + Shaders)
   │
   ▼
Phase 3 (TransitionManager)
   │
   ▼
Phase 4 (Game Integration)      ← Core feature complete
   │
   ├──▶ Phase 5 (Viewport Transitions)
   │
   ├──▶ Phase 6 (Transition Demo)
   │
   └──▶ Phase 7 (Documentation)
            │
            ▼
        Phase 8 (Polish / Future)
```

---

## Risk Assessment

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Offscreen render target format mismatch with swapchain | Medium | High | Use `m_swapChainImageFormat` from VulkanContext; test on multiple GPU vendors |
| Shader compilation failure at runtime | Low | Medium | ShaderCache pre-compiles; fallback to instant cut on failure |
| Performance regression from double-scene rendering during transitions | Low | Medium | Transitions are short-lived; offscreen targets match swapchain resolution |
| Swapchain resize during transition | Medium | Medium | `recreateRenderTargets()` in resize callback; test with window dragging |
| Interaction with multi-scene scheduler | Medium | Medium | Phase 4 extends the task graph carefully; Phase 5 validates split-screen |

---

## Estimated Effort

| Phase | Effort |
|-------|--------|
| Phase 1 — Offscreen Render Targets | 1–2 days |
| Phase 2 — Transition Classes + Shaders | 1 day |
| Phase 3 — TransitionManager | 2–3 days |
| Phase 4 — Game Integration | 2–3 days |
| Phase 5 — Viewport Transitions | 1 day |
| Phase 6 — Transition Demo | 1–2 days |
| Phase 7 — Documentation | 0.5 day |
| **Total (Phases 1–7)** | **8.5–12.5 days** |
