# VDE Audit Remediation Plan

Findings from the April 2025 project audit, organized into phases that can be completed and verified independently. Each phase has its own acceptance criteria and can be merged separately.

---

## Phase 1 — Project Hygiene (no code changes) ✅ COMPLETE

Low-risk cleanup that touches no engine logic.

### 1.1 Add `imgui.ini` to `.gitignore`

`imgui.ini` is a runtime-generated Dear ImGui window-state file sitting in the workspace root. It is not source-controlled intentionally and should be ignored.

- Add `imgui.ini` to `.gitignore`
- Remove `imgui.ini` from tracking if it is currently committed (`git rm --cached imgui.ini`)

### 1.2 Remove orphan `test_audio.cpp`

`test_audio.cpp` in the workspace root is a 52-line standalone miniaudio smoke test that is not referenced by any CMakeLists.txt and includes a header from a build-internal `_deps` path. It serves no ongoing purpose.

- Delete `test_audio.cpp`

### 1.3 Resolve stale TODO comments in `Entity.cpp`

Three TODO comments in `src/api/Entity.cpp` (lines 165, 186, 356) say *"when resource management is implemented"* but `ResourceManager` already exists.

- For each TODO, either:
  - Wire the resource lookup through `ResourceManager` (if the ID-based path is intended to work), or
  - Remove the dead branch and TODO if the direct-reference path is the only supported approach

### Acceptance criteria

- Build passes (no source changes to compiled code beyond the Entity.cpp TODOs)
- All existing unit tests pass
- `git status` shows no untracked `imgui.ini`

---

## Phase 2 — CMake Consolidation ✅ COMPLETE

Structural build-system improvement. No runtime behavior changes.

### 2.1 Move ImGui FetchContent to root `CMakeLists.txt`

ImGui is declared identically in both `examples/CMakeLists.txt` (lines 6-12) and `tools/CMakeLists.txt` (lines 8-14). Consolidate to a single `FetchContent_Declare` + `FetchContent_MakeAvailable` in the root `CMakeLists.txt`, guarded by `if(NOT TARGET imgui)`.

Move the `imgui_backend` static library target (Vulkan + GLFW backends) to root as well so both examples and tools link against the same target.

- Remove duplicate `FetchContent_Declare(imgui ...)` from `examples/CMakeLists.txt` and `tools/CMakeLists.txt`
- Remove duplicate `imgui_backend` target creation from both files
- Add single declaration in root `CMakeLists.txt` after the existing dependency block

### 2.2 Make `add_vde_example()` accept optional parameters

`add_vde_example()` in `examples/CMakeLists.txt` (line 70) hardcodes ImGui linkage and `VDE_EXAMPLE_USE_IMGUI`. The triangle example bypasses it manually.

- Add optional `NO_IMGUI` keyword argument so non-ImGui examples can use the same function
- Convert the triangle example to use `add_vde_example(... NO_IMGUI)`

### Acceptance criteria

- Full rebuild succeeds (both MSVC and Ninja generators)
- All unit tests pass
- All examples compile and link
- Smoke tests pass

---

## Phase 3 — Resource Safety

Fix Vulkan resource leak paths. Changes are localized to two files.

### 3.1 RAII staging buffer in `Texture::uploadToGPU()`

In `src/Texture.cpp` (lines 114-169), a staging `VkBuffer` + `VkDeviceMemory` are allocated at line 139. Six subsequent operations (lines 150-168) can throw, leaking both resources.

- Wrap the staging buffer/memory pair in a small RAII guard (local struct or scope-exit lambda) that calls `vkDestroyBuffer` + `vkFreeMemory` on destruction
- Remove the manual cleanup at the end of the function (it will be handled by the guard)

### 3.2 Split `VulkanContext::createRenderPass()`

In `src/VulkanContext.cpp` (lines 639-788), a single 150-line function creates three render passes (`m_renderPass`, `m_renderPassLoad`, `m_offscreenRenderPass`) with duplicated attachment descriptions.

- Extract three private methods: `createMainRenderPass()`, `createLoadRenderPass()`, `createOffscreenRenderPass()`
- Share common attachment setup via a helper that returns a populated `VkAttachmentDescription`
- `createRenderPass()` becomes a thin dispatcher calling the three helpers

### Acceptance criteria

- Build passes
- All unit tests pass
- Smoke tests pass (render passes are exercised by most examples)

---

## Phase 4 — `processInputScript` Refactor

Decompose the largest function in the codebase. Purely internal refactor with no API changes.

### 4.1 Extract `resolveInputHandler()` helper

The pattern below appears at lines 348-354 and 360-368 in `src/api/Game.cpp`, and is used by most command handlers via the shared `handler` variable:

```cpp
InputHandler* handler = nullptr;
Scene* focused = getFocusedScene();
if (focused) { handler = focused->getInputHandler(); }
if (!handler) { handler = m_inputHandler; }
```

- Add private method `InputHandler* Game::resolveInputHandler() const`
- Replace all inline instances with a call to the new method

### 4.2 Extract per-command handler methods

`processInputScript()` (lines 333-705) contains a switch with 21 cases. Extract each case into a dedicated private method:

| Case | New method |
|------|-----------|
| `WaitStartup` | `scriptWaitStartup(state, cmd)` |
| `WaitMs` | `scriptWaitMs(state, cmd)` |
| `Press` | `scriptPress(state, cmd)` |
| `KeyDown` | `scriptKeyDown(state, cmd)` |
| `KeyUp` | `scriptKeyUp(state, cmd)` |
| `Click` | `scriptClick(state, cmd)` |
| `ClickRight` | `scriptClickRight(state, cmd)` |
| `MouseDown` | `scriptMouseDown(state, cmd)` |
| `MouseUp` | `scriptMouseUp(state, cmd)` |
| `MouseMove` | `scriptMouseMove(state, cmd)` |
| `Scroll` | `scriptScroll(state, cmd)` |
| `Screenshot` | `scriptScreenshot(state, cmd)` |
| `Print` | `scriptPrint(state, cmd)` |
| `Label` | `scriptLabel(state, cmd)` |
| `Loop` | `scriptLoop(state, cmd)` |
| `Exit` | `scriptExit(state, cmd)` |
| `WaitFrames` | `scriptWaitFrames(state, cmd)` |
| `AssertSceneCount` | `scriptAssertSceneCount(state, cmd)` |
| `AssertScene` | `scriptAssertScene(state, cmd)` |
| `Compare` | `scriptCompare(state, cmd)` |
| `Set` | `scriptSet(state, cmd)` |

Each method receives `InputScriptState& state` and `const InputCommand& cmd` and returns a `bool` (true = continue processing, false = yield until next frame).

`processInputScript()` becomes a thin loop that dispatches to the correct handler.

### 4.3 Declare new private methods in `Game.h`

Add the `resolveInputHandler()` and `script*()` method declarations to `include/vde/api/Game.h` in a private section.

### Acceptance criteria

- Build passes
- All unit tests pass (especially `InputScript_test`)
- Smoke tests pass (scripted input is exercised by every smoke test)
- `processInputScript()` is under 60 lines; no individual handler method exceeds 50 lines

---

## Phase 5 — Consistency & Error Quality

Small targeted fixes across several files.

### 5.1 Improve exception context in `Texture.cpp`

In `src/Texture.cpp` line 517, change:
```cpp
throw std::invalid_argument("Unsupported layout transition!");
```
to include the source and destination layouts in the message.

### 5.2 Fix documentation example in `Game.h`

In `include/vde/api/Game.h` line 61, the doc comment shows raw `new`:
```cpp
game.addScene("main", new MainScene());
```
Change to:
```cpp
game.addScene("main", std::make_unique<MainScene>());
```

### 5.3 Consolidate default constants

`Camera.h` uses `m_farPlane = 200.0f`; `GameCamera.h` uses `m_farPlane = 1000.0f`; both default to 1920×1080 in separate locations. The values are intentionally different (low-level vs game camera), but the screen-size defaults should be single-sourced.

- Add `include/vde/api/Defaults.h` with:
  ```cpp
  namespace vde::defaults {
      inline constexpr float DefaultScreenWidth  = 1920.0f;
      inline constexpr float DefaultScreenHeight = 1080.0f;
  }
  ```
- Update `CameraBounds.h` and `GameCamera.h` to use these constants instead of inline literals

### Acceptance criteria

- Build passes
- All unit tests pass
- Smoke tests pass

---

## Phase 6 — Test Coverage Expansion

Add missing tests for core API classes.

### 6.1 `Game_test.cpp`

Add a unit test covering:
- Construction and destruction without a Vulkan context (mock/headless if needed)
- `addScene()` / `getScene()` / `removeScene()` lifecycle
- Scene count assertions
- Input handler registration

### 6.2 `AudioManager_test.cpp`

Add a unit test covering:
- Singleton access
- `playSFX()` / `playMusic()` with invalid paths (error path)
- Volume get/set
- Mute/unmute state

### 6.3 `BitmapFont_test.cpp`

Add a unit test covering:
- Font construction and glyph metrics
- Layout/measurement functions
- Edge cases (empty string, unknown glyph)

### Acceptance criteria

- Build passes
- All new and existing tests pass
- No test depends on GPU availability (must run in CI headless)

---

## Phase Summary

| Phase | Risk | Files touched | Theme |
|-------|------|---------------|-------|
| 1 | Minimal | `.gitignore`, `test_audio.cpp`, `Entity.cpp` | Cleanup |
| 2 | Low | Root + examples + tools `CMakeLists.txt` | Build dedup |
| 3 | Medium | `Texture.cpp`, `VulkanContext.cpp` | Safety |
| 4 | Medium | `Game.cpp`, `Game.h` | Maintainability |
| 5 | Low | `Texture.cpp`, `Game.h`, `CameraBounds.h`, `GameCamera.h`, new `Defaults.h` | Polish |
| 6 | Low | New test files only | Coverage |

Phases are ordered so that earlier phases reduce noise for later ones (e.g., cleaning stale files before refactoring, fixing CMake before adding new test targets).
